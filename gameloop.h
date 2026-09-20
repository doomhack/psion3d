#ifndef GAMELOOP_H
#define GAMELOOP_H

#include "fp_types.h"

/*  The parts of the frame that are the same on every target, and the mode
    above it.

    psion3d.c keeps everything platform-specific around this: the WLIB window
    setup and event pump, p_getscancodes, and the blit. The PC host in pc/src
    supplies its own equivalents. Both then call gameRunTicks, so there is only
    one copy of the timing and update order. */

/*  The current KEY_* bitmask, filled by whichever platform is reading input. */
extern u16 keys;

/*  What owns the screen. In GAME_MODE_MENU the platform shows the menu
    window, feeds key events to gameKey and never runs a frame; in
    GAME_MODE_PLAYING it hides the menu, samples the scancodes and runs
    gameRunTicks. The platform watches for the mode changing under a gameKey
    call and switches windows and the tick clock accordingly. */
#define GAME_MODE_MENU 0
#define GAME_MODE_PLAYING 1

extern u8 gameMode;

/*  What a key event did, for the platform. */
#define GAME_KEY_IGNORED 0
#define GAME_KEY_REDRAW 1	/* the menu changed: repaint it */
#define GAME_KEY_QUIT 2	/* leave the program */

/*  Build the mission index and open the main menu. */
void gameInit(void);

/*  Load a level and start playing it from the spawn. */
u16 gameStartMission(const u8 mapId);

/*  A UI_KEY_* event from the platform, in either mode. In play only Esc
    means anything (it pauses); in a menu the key goes to the screen. */
u16 gameKey(const u16 uiKey);

/*  The tick counter is 16 bits and free-running, so it wraps roughly every
    2048 seconds. Both helpers are written to stay correct across that wrap;
    widening either of them would break it. */
s16 tickDelta(const u16 later, const u16 earlier);
u16 tickElapsed(const u16 later, const u16 earlier);

/*  Catch the simulation up to realTime in whole ticks, then render one frame
    into the bitmap. Returns the new game time. Presenting the bitmap is the
    platform's job and is deliberately not done here.

    A tick can end the mission (end cell reached, player dead), on which the
    mode switches to GAME_MODE_MENU with the outcome screen open. The
    platform checks gameMode after this call as it does after gameKey. */
u16 gameRunTicks(u16 gameTime, const u16 realTime);

#endif
