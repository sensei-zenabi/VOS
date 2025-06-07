#ifndef WINDOW_H
#define WINDOW_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

typedef struct GUIWindow GUIWindow;

struct GUIWindow {
    int x, y, width, height;

    /* dragging / resizing */
    int dragging, drag_off_x, drag_off_y;
    int resizing, resize_dir, resize_off_x, resize_off_y;

    int visible;                    /* false → close & free */
    char path[512];

    struct GUIWindow *next;
};

/* resize_dir flags */
#define RESIZE_RIGHT  1
#define RESIZE_BOTTOM 2   /* 3 = corner (right|bottom) */

void gui_window_init(GUIWindow *w,
                     const char *path,
                     int x, int y, int w_px, int h_px);

int  gui_window_handle_event(GUIWindow *w,
                             SDL_Event *e,
                             GUIWindow **win_list);   /* returns 1 if event consumed */

void gui_window_draw(GUIWindow *w,
                     SDL_Renderer *r,
                     TTF_Font *font);

#endif
