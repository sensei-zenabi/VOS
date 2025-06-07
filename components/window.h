#ifndef WINDOW_H
#define WINDOW_H
#define _POSIX_C_SOURCE 200112L

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

struct GUIEdit;                          /* forward decl */

#define RESIZE_RIGHT  1
#define RESIZE_BOTTOM 2       /* corner = 3 */

typedef struct GUIWindow GUIWindow;

struct GUIWindow {
    int x,y,width,height;

    int dragging, drag_off_x, drag_off_y;
    int resizing, resize_dir, resize_off_x, resize_off_y;

    int  visible;
    char path[512];

    struct GUIWindow *next;
};

void gui_window_init(GUIWindow *w,
                     const char *path,
                     int x,int y,int w_px,int h_px);

int  gui_window_handle_event(GUIWindow *w,
                             SDL_Event *e,
                             GUIWindow **win_list,
                             struct GUIEdit **edit_list);

void gui_window_draw(GUIWindow *w,
                     SDL_Renderer *ren,
                     TTF_Font *font);

#endif /* WINDOW_H */
