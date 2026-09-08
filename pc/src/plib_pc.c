/*  plib_pc.c - the handful of PLIB services the portable game modules use,
    implemented natively.

    Only the parts of PLIB that are already a decent portable API live here:
    stream file I/O, the segment handle/offset/copy interface, string helpers
    and the tick counter. WLIB's event model is deliberately not emulated. It
    is replaced by the Qt host instead. */

#include <plib.h>
#include "plib_pc.h"

#include "pcclock.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

/* ------------------------------------------------------------------ paths */

#ifndef PSION3D_DEFAULT_ASSET_ROOT
#define PSION3D_DEFAULT_ASSET_ROOT "."
#endif

static char g_assetRoot[512] = PSION3D_DEFAULT_ASSET_ROOT;
static int  g_ioVerbose = 0;

void pcSetIoVerbose(int on)
{
	g_ioVerbose = on;
}

void pcSetAssetRoot(const char *root)
{
	if(root && *root)
	{
		strncpy(g_assetRoot, root, sizeof(g_assetRoot) - 1);
		g_assetRoot[sizeof(g_assetRoot) - 1] = 0;
	}
}

const char *pcGetAssetRoot(void)
{
	return g_assetRoot;
}

/* The device tree maps onto the repo layout. Two entries, one place. */
static const struct { const char *from; const char *to; } g_pathAlias[] =
{
	{ "IMG/MAP/", "map/" },
	{ "IMG/SPR/", "spr/" }
};

static int prefixEqualNoCase(const char *s, const char *prefix)
{
	while(*prefix)
	{
		char a = *s++;
		char b = *prefix++;

		if(a >= 'a' && a <= 'z') a = (char)(a - 'a' + 'A');
		if(b >= 'a' && b <= 'z') b = (char)(b - 'a' + 'A');

		if(a != b)
			return 0;
	}

	return 1;
}

/*  LOC::M:\IMG\MAP\map1.map  ->  <root>/map/map1.map  */
static void resolvePath(const char *psionPath, char *out, size_t outLen)
{
	char norm[256];
	const char *p = psionPath;
	size_t i = 0;
	size_t a;

	if(prefixEqualNoCase(p, "LOC::"))
		p += 5;

	/* Drop a drive letter such as "M:". */
	if(p[0] && p[1] == ':')
		p += 2;

	while(*p == '\\' || *p == '/')
		p++;

	while(*p && i < sizeof(norm) - 1)
	{
		norm[i++] = (*p == '\\') ? '/' : *p;
		p++;
	}

	norm[i] = 0;

	for(a = 0; a < sizeof(g_pathAlias) / sizeof(g_pathAlias[0]); a++)
	{
		if(prefixEqualNoCase(norm, g_pathAlias[a].from))
		{
			snprintf(out, outLen, "%s/%s%s", g_assetRoot, g_pathAlias[a].to,
			         norm + strlen(g_pathAlias[a].from));
			return;
		}
	}

	snprintf(out, outLen, "%s/%s", g_assetRoot, norm);
}

/* --------------------------------------------------------------- file i/o */

INT p_open(VOID **handle, const TEXT *name, UINT mode)
{
	char resolved[768];
	FILE *f;

	(void)mode;                 /* P_FOPEN and P_FSTREAM are both zero. */

	resolvePath(name, resolved, sizeof(resolved));

	f = fopen(resolved, "rb");

	if(!f)
	{
		/*  Off by default, because a failed open is usually not a fault:
		    loadSprite() finds a sprite's frame count by opening base0.spr,
		    base1.spr and so on until one is missing, so the last probe of
		    every slot lands here. When something really is absent, the
		    resolved path is what you want, hence -v. */
		if(g_ioVerbose)
			fprintf(stderr, "p_open: cannot open %s (from %s)\n", resolved, name);

		*handle = NULL_D;
		return -1;
	}

	*handle = (VOID *)f;
	return 0;
}

INT p_read(VOID *handle, VOID *buf, UINT len)
{
	size_t got = fread(buf, 1, (size_t)len, (FILE *)handle);

	/* PLIB signals end of file with E_FILE_EOF, not 0. game_map.c:121 and
	   sprite.c:282 both test for it, so reproduce it exactly. */
	if(got == 0 && len != 0)
		return E_FILE_EOF;

	return (INT)got;
}

INT p_close(VOID *handle)
{
	if(handle)
		fclose((FILE *)handle);

	return 0;
}

/* ---------------------------------------------------------------- strings */

void p_atos(TEXT *dst, const TEXT *fmt, ...)
{
	/* Callers pass fixed 64-byte buffers and no length. Format into a scratch
	   buffer first so an overrun is a diagnostic rather than a stomp. */
	char scratch[512];
	va_list ap;
	int n;

	va_start(ap, fmt);
	n = vsnprintf(scratch, sizeof(scratch), fmt, ap);
	va_end(ap);

	if(n < 0)
	{
		dst[0] = 0;
		return;
	}

	if(n >= 64)
		fprintf(stderr, "p_atos: %d bytes formatted from \"%s\"; callers use 64-byte buffers\n",
		        n, fmt);

	memcpy(dst, scratch, (size_t)n + 1);
}

UINT p_slen(const TEXT *s)
{
	return (UINT)strlen(s);
}

/* ------------------------------------------------------------------ ticks */

static double g_tickOrigin = -1.0;   /* wall clock reading at tick zero */
static double g_pausedAt   = 0.0;
static int    g_paused     = 0;

static double tickNowMs(void)
{
	if(g_tickOrigin < 0.0)
		g_tickOrigin = pcMonotonicMs();

	if(g_paused)
		return g_pausedAt - g_tickOrigin;

	return pcMonotonicMs() - g_tickOrigin;
}

/*  32 Hz, and truncated to 16 bits so that tickDelta/tickElapsed wrap exactly
    as they do on the device, every 2048 seconds. Widening this would silently
    change the semantics of the whole timing layer. */
UINT p_returntickcount(void)
{
	double ms = tickNowMs();

	if(ms < 0.0)
		ms = 0.0;

	return (UINT)((unsigned long)(ms * 32.0 / 1000.0) & 0xFFFFu);
}

void pcTickSetPaused(int paused)
{
	if(paused == g_paused)
		return;

	if(g_tickOrigin < 0.0)
		g_tickOrigin = pcMonotonicMs();

	if(paused)
		g_pausedAt = pcMonotonicMs();
	else
		g_tickOrigin += pcMonotonicMs() - g_pausedAt;   /* discard the paused span */

	g_paused = paused;
}

void pcTickResync(void)
{
	g_tickOrigin = pcMonotonicMs();
	g_pausedAt = g_tickOrigin;
}

void pcTickAdvance(unsigned short ticks)
{
	double ms = (double)ticks * 1000.0 / 32.0;

	g_tickOrigin -= ms;
	g_pausedAt += ms;
}

void pcTickSetStart(unsigned short start)
{
	if(g_tickOrigin < 0.0)
		g_tickOrigin = pcMonotonicMs();

	g_tickOrigin -= (double)start * 1000.0 / 32.0;
}

/* --------------------------------------------------------------- segments */

/*  On the device sprite frames live in named far segments because the small
    memory model cannot dereference them directly. That is pointless on a PC,
    but the segment API is only a handle/offset/copy interface, and keeping it
    means sprite.c's LRU cache - which contains real logic worth debugging -
    runs here exactly as it does on the device. */

#define PC_SEG_MAX  16
#define PC_SEG_NAME 16

typedef struct
{
	TEXT           name[PC_SEG_NAME];
	unsigned char *data;
	unsigned long  size;
	int            used;
} pc_seg_t;

static pc_seg_t g_segs[PC_SEG_MAX];

static pc_seg_t *segFromHandle(HANDLE h)
{
	if(h <= 0 || h > PC_SEG_MAX || !g_segs[h - 1].used)
		return NULL;

	return &g_segs[h - 1];
}

HANDLE p_sgcreate(const TEXT *name, INT paragraphs, INT flags)
{
	int i;
	unsigned long size = (unsigned long)(unsigned short)paragraphs * 16uL;

	(void)flags;

	/* The device would silently hand back a second segment with the same name.
	   loadSprite() never closes the previous one, which is invisible today
	   because loadMap(1) runs once, but would leak on any reload. Say so. */
	for(i = 0; i < PC_SEG_MAX; i++)
		if(g_segs[i].used && strncmp(g_segs[i].name, name, PC_SEG_NAME - 1) == 0)
			fprintf(stderr, "p_sgcreate: segment \"%s\" already exists; the previous one leaks\n",
			        name);

	for(i = 0; i < PC_SEG_MAX; i++)
	{
		if(!g_segs[i].used)
		{
			g_segs[i].data = (unsigned char *)calloc(1, size ? size : 1);

			if(!g_segs[i].data)
				return 0;

			strncpy(g_segs[i].name, name, PC_SEG_NAME - 1);
			g_segs[i].name[PC_SEG_NAME - 1] = 0;
			g_segs[i].size = size;
			g_segs[i].used = 1;

			return (HANDLE)(i + 1);     /* 1-based: sprite.c tests for <= 0. */
		}
	}

	fprintf(stderr, "p_sgcreate: out of segment slots (%d)\n", PC_SEG_MAX);
	return 0;
}

HANDLE p_sgopen(const TEXT *name)
{
	int i;

	for(i = 0; i < PC_SEG_MAX; i++)
		if(g_segs[i].used && strncmp(g_segs[i].name, name, PC_SEG_NAME - 1) == 0)
			return (HANDLE)(i + 1);

	return 0;
}

INT p_sgclose(HANDLE h)
{
	pc_seg_t *s = segFromHandle(h);

	if(!s)
		return -1;

	free(s->data);
	memset(s, 0, sizeof(*s));

	return 0;
}

/*  Both copies bounds-check. On the device an overrun quietly corrupts a
    neighbouring segment; here it is a loud, locatable failure. */
static int segRangeOk(const pc_seg_t *s, long int offset, UINT bytes, const char *who)
{
	if(offset < 0 || (unsigned long)offset + bytes > s->size)
	{
		fprintf(stderr, "%s: %u bytes at %ld overruns segment \"%s\" (%lu bytes)\n",
		        who, (unsigned)bytes, offset, s->name, s->size);
		return 0;
	}

	return 1;
}

INT p_sgcopyto(HANDLE h, long int offset, VOID *src, UINT bytes)
{
	pc_seg_t *s = segFromHandle(h);

	if(!s || !segRangeOk(s, offset, bytes, "p_sgcopyto"))
		return -1;

	memcpy(s->data + offset, src, bytes);

	return 0;
}

INT p_sgcopyfr(HANDLE h, long int offset, VOID *dst, UINT bytes)
{
	pc_seg_t *s = segFromHandle(h);

	if(!s || !segRangeOk(s, offset, bytes, "p_sgcopyfr"))
		return -1;

	memcpy(dst, s->data + offset, bytes);

	return 0;
}

void pcSegReleaseAll(void)
{
	int i;

	for(i = 0; i < PC_SEG_MAX; i++)
	{
		if(g_segs[i].used)
		{
			free(g_segs[i].data);
			memset(&g_segs[i], 0, sizeof(g_segs[i]));
		}
	}
}
