#ifndef WINDOW_H
#define WINDOW_H

#include <SDL2/SDL.h>

typedef struct {
    int x, y;
    int width, height;
    int dragging;
    int drag_offset_x, drag_offset_y;
} GUIWindow;

void gui_window_init(GUIWindow *win, int x, int y, int w, int h);
void gui_window_draw(GUIWindow *win, SDL_Renderer *renderer);
void gui_window_handle_event(GUIWindow *win, SDL_Event *e);

#endif
