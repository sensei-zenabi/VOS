#define _POSIX_C_SOURCE 200112L
#include "window.h"

void gui_window_init(GUIWindow *win, int x, int y, int w, int h) {
    win->x = x;
    win->y = y;
    win->width = w;
    win->height = h;
    win->dragging = 0;
    win->drag_offset_x = 0;
    win->drag_offset_y = 0;
}

void gui_window_handle_event(GUIWindow *win, SDL_Event *e) {
    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
        int mx = e->button.x;
        int my = e->button.y;
        if (mx >= win->x && mx <= win->x + win->width &&
            my >= win->y && my <= win->y + 30) { // drag only from top 30px
            win->dragging = 1;
            win->drag_offset_x = mx - win->x;
            win->drag_offset_y = my - win->y;
        }
    } else if (e->type == SDL_MOUSEBUTTONUP && e->button.button == SDL_BUTTON_LEFT) {
        win->dragging = 0;
    } else if (e->type == SDL_MOUSEMOTION && win->dragging) {
        win->x = e->motion.x - win->drag_offset_x;
        win->y = e->motion.y - win->drag_offset_y;
    }
}

void gui_window_draw(GUIWindow *win, SDL_Renderer *renderer) {
    SDL_Rect body = { win->x, win->y, win->width, win->height };
    SDL_Rect titlebar = { win->x, win->y, win->width, 30 };

    SDL_SetRenderDrawColor(renderer, 50, 50, 200, 255); // body color
    SDL_RenderFillRect(renderer, &body);

    SDL_SetRenderDrawColor(renderer, 20, 20, 120, 255); // titlebar color
    SDL_RenderFillRect(renderer, &titlebar);

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); // border
    SDL_RenderDrawRect(renderer, &body);
}
