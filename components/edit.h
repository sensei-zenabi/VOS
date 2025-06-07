#ifndef EDIT_H
#define EDIT_H
#define _POSIX_C_SOURCE 200112L

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "window.h"

typedef struct GUIEdit {
    GUIWindow win;          /* must be first */

    char **lines;           /* text lines */
    int    line_count;
    int    line_h;
    int    max_w_px;

    int scroll_x, scroll_y; /* pixel scroll */

    int v_dragging, h_dragging;
    int drag_off;           /* mouse offset inside slider */
} GUIEdit;

/* create / destroy -------------------------------------------------- */
GUIEdit *edit_window_create(const char *filepath,
                            int x,int y,int w,int h);
void     edit_window_destroy(GUIEdit *ed);

/* events / drawing -------------------------------------------------- */
int  edit_window_handle_event(GUIEdit   *ed,
                              SDL_Event *e,
                              GUIEdit  **edit_list); /* RETURNS 1 if event used */

void edit_window_draw(GUIEdit      *ed,
                      SDL_Renderer *ren,
                      TTF_Font     *font);

#endif /* EDIT_H */
