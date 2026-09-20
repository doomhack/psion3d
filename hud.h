#ifndef HUD_H
#define HUD_H

#include "fp_types.h"

/*  The in-game HUD: the two 120 pixel panels either side of the 240 wide
    game view, from the design's HUD artboard. Health, its ten-segment bar
    and the objectives count on the left, the weapon list with ammo on the
    right, the frame rate at the bottom left when the option is on.

    Drawn through ui.h into the HUD target (a full-screen window under the
    game window on the device, an image the PC host composites the game view
    over). The static parts are drawn once per uncover; the values are cells
    that hudUpdate replaces one at a time as they change, so the window
    server never sees a whole-panel repaint while playing. */

/*  Forget what was last drawn, so the next hudUpdate() repaints in full: at mission
    start, and whenever the HUD window has been covered and uncovered. */
void hudInvalidate(void);

/*  Draw both panels from scratch: the platform's answer to a redraw event.
    Records what it drew. */
void hudDraw(void);

/*  Once a frame while playing, with the HUD target selected: redraw only the
    cells whose values moved since the last draw, directly into the window.
    A quiet frame costs the compares and nothing else. */
void hudUpdate(void);

/*  The frame counter the platform measured this second. */
void hudSetFps(const u8 fps);

#endif
