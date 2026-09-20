#ifndef UI_SEAM_H
#define UI_SEAM_H

/*  The drawing seam the menu screens sit on.

    menu.c draws every screen through these calls and nothing else, so the
    same module runs on the device, where ui_psion.c implements them with
    WLIB into a full-screen window, and on the PC, where pc/src/menu_pc.cpp
    implements them with QPainter. Menus are not a hot path: every call may
    be a window server message.

    Deliberately includes nothing and uses plain C types. menu_pc.cpp is C++
    and no .cpp in the PC build may include a game header (fp_types.h puns a
    union), so this file has to stand on its own. */

#define UI_W 480
#define UI_H 160

/*  Fonts. Body is the ROM Swiss 13 (the 13px Helvetica of the design), head
    the Swiss 16 bold. Line pitch for body text is 19px. */
#define UI_FONT_BODY 0
#define UI_FONT_BODY_BOLD 1
#define UI_FONT_HEAD_BOLD 2
#define UI_FONT_BIG 3	/* the head face at double height: 32 rows, for the HUD numbers */

/*  Where the primitives draw. The menu window covers the screen while a
    menu is up; the HUD window is the full screen too but sits under the
    game view, and only its two side panels are ever drawn. The platform
    selects one before a menuDraw or hudDraw. */
#define UI_TARGET_MENU 0
#define UI_TARGET_HUD 1

/*  Keys, as the platform reports them from its key events. The play loop
    still reads levels through the KEY_* scancode mask; these are edges. */
#define UI_KEY_NONE 0
#define UI_KEY_UP 1
#define UI_KEY_DOWN 2
#define UI_KEY_LEFT 3
#define UI_KEY_RIGHT 4
#define UI_KEY_ENTER 5
#define UI_KEY_ESC 6
#define UI_KEY_SPACE 7

#ifdef __cplusplus
extern "C" {
#endif

void uiTarget(short target);

/*  Whole window to the LCD background, both planes. */
void uiClear(void);

/*  Rectangles: black plane set, both planes cleared, grey plane set, the
    system dither, and a one pixel black outline just inside the rectangle. */
void uiFillRect(short x, short y, short w, short h);
void uiClearRect(short x, short y, short w, short h);
void uiGreyRect(short x, short y, short w, short h);
void uiPatternRect(short x, short y, short w, short h);
void uiBox(short x, short y, short w, short h);

/*  One pixel black line, inclusive of both ends. */
void uiLine(short x0, short y0, short x1, short y1);

/*  Text at x with y the vertical CENTRE of the row it sits in: the platform
    centres the capitals on it (baseline = y + ascent / 2), which is what
    reads as centred whatever the font's real ascent and descent are - the
    device's Swiss 13 has an 11 row ascent over a 2 row descent, and placing
    its nominal 13px cell by its top left the text riding high in every
    bar. inverse draws white glyphs, for text over a black bar. len is in
    bytes. */
void uiText(short x, short y, short font, short inverse, const char *s, short len);
short uiTextWidth(short font, const char *s, short len);

/*  A text cell: clear the box (or fill it, when inverse) and print s in it,
    capitals centred vertically, aligned left, right or centred. One buffered
    window server call on the device (gPrintBoxText), which is what makes it
    the primitive for anything that changes while playing: a HUD cell
    updates for the cost of one message, no invalidation, no redraw event.
    Black plane only: the box must not sit over grey. */
#define UI_ALIGN_LEFT 0
#define UI_ALIGN_RIGHT 1
#define UI_ALIGN_CENTRE 2

void uiTextBox(short x, short y, short w, short h, short font, short inverse, short align, const char *s, short len);

/*  Copy rows srcY..srcY+h of the game bitmap (screenBm, both planes, x from
    0) into the menu window at dstX, dstY. The automap renders through
    bitmap.c and arrives on screen through this. */
void uiBlitMap(short dstX, short dstY, short srcY, short w, short h);

/*  A short transient notice over whatever is on screen, play included: the
    window server's info message on the device (wInfoMsg, bottom right, gone
    on its own after a moment), a line on stderr on the PC. NUL terminated.
    The one ui call the game makes outside the menus, so it must not assume
    the menu window is up. */
void uiInfoMsg(const char *s);

#ifdef __cplusplus
}
#endif

#endif
