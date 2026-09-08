/*  plib_pc.h - PC-only controls for the PLIB shim. Not visible to game code. */

#ifndef PLIB_PC_H
#define PLIB_PC_H

#ifdef __cplusplus
extern "C" {
#endif

/* Where LOC::M:\IMG\... is resolved from. Defaults to PSION3D_DEFAULT_ASSET_ROOT. */
void        pcSetAssetRoot(const char *root);
const char *pcGetAssetRoot(void);

/* Report every failed p_open. Off by default: loadSprite probes past the last
   frame of every sprite, so failures are routine rather than faults. */
void pcSetIoVerbose(int on);

/* The tick source behind p_returntickcount() is virtual so that pausing the
   host does not cause an avalanche of catch-up ticks on resume. */
void pcTickSetPaused(int paused);
void pcTickResync(void);                    /* re-origin to now, on focus regained */
void pcTickAdvance(unsigned short ticks);   /* nudge forward, for frame stepping */
void pcTickSetStart(unsigned short start);  /* seed the counter, to exercise wrap */

/* Frees every segment. Only for shutdown and hot reload. */
void pcSegReleaseAll(void);

#ifdef __cplusplus
}
#endif

#endif
