#ifndef SETTINGS_H
#define SETTINGS_H

#include "fp_types.h"

/*  The player's settings, as the Options screen edits them. They live for
    the process: there is no save file yet (TASKS.md task 1), so every launch
    starts from the defaults below. */

#define SOUND_LEVEL_MAX 10

/*  0 .. SOUND_LEVEL_MAX. Stored and shown; nothing plays a sound yet
    (TASKS.md task 12). */
extern u8 soundLevel;

/*  Whether the frame counter is drawn: the debug window's fps line on the
    device today, the HUD's FPS row once there is a HUD. */
extern u8 showFps;

#endif
