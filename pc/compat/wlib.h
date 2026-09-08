/*  wlib.h - PC replacement for the SIBO window-server header.

    draw.c, player.c, sprite.c and psion3d.h all include <wlib.h>, but the
    portable modules use nothing from it; the only WLIB and GDI calls in the
    program live in psion3d.c and debug.c, neither of which is in the PC build.
    So this header exists purely to satisfy the includes. */

#ifndef WLIB_H
#define WLIB_H

#include <plib.h>

#endif
