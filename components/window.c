#define _POSIX_C_SOURCE 200112L
#include "window.h"

#define TITLEBAR_HEIGHT 30
#define BORDER 4
#define CLOSE_SIZE 20
#define MIN_WIDTH 100
#define MIN_HEIGHT 60

static int point_in_rect(int x, int y, int rx, int ry, int rw, int rh) {
    return x >= rx && x <= rx + rw && y >= ry && y <= ry + rh;
}

void gui_window_init(GUIWindow *win, int x, int y, int w, int h) {
    win->x = x;
    win->y = y;
    win->width = w;
    win->height = h;
    win->dragging = 0;
    win->resizing = 0;
    win->resize_dir = 0;
    win->visible = 1;
}

void gui_window_handle_event(GUIWindow *win, SDL_Event *e) {
    if (!win->visible) return;

    int mx, my;
    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
        mx = e->button.x;
        my = e->button.y;

        // Close button area
        int cx = win->x + win->width - CLOSE_SIZE - BORDER;
        int cy = win->y + (TITLEBAR_HEIGHT - CLOSE_SIZE) / 2;
        if (point_in_rect(mx, my, cx, cy, CLOSE_SIZE, CLOSE_SIZE)) {
            win->visible = 0;
            return;
        }

        // Dragging title bar
        if (point_in_rect(mx, my, win->x, win->y, win->width, TITLEBAR_HEIGHT)) {
            win->dragging = 1;
            win->drag_offset_x = mx - win->x;
            win->drag_offset_y = my - win->y;
            return;
        }

        // Resize logic
        int rx = win->x + win->width - BORDER;
        int ry = win->y + win->height - BORDER;

        if (point_in_rect(mx, my, rx, win->y, BORDER, win->height)) {
            win->resizing = 1;
            win->resize_dir = 1; // right
            win->resize_offset_x = mx - (win->x + win->width);
        } else if (point_in_rect(mx, my, win->x, ry, win->width, BORDER)) {
            win->resizing = 1;
            win->resize_dir = 2; // bottom
            win->resize_offset_y = my - (win->y + win->height);
        } else if (point_in_rect(mx, my, rx, ry, BORDER, BORDER)) {
            win->resizing = 1;
            win->resize_dir = 3; // corner
            win->resize_offset_x = mx - (win->x + win->width);
            win->resize_offset_y = my - (win->y + win->height);
        }

    } else if (e->type == SDL_MOUSEBUTTONUP && e->button.button == SDL_BUTTON_LEFT) {
        win->dragging = 0;
        win->resizing = 0;
        win->resize_dir = 0;
    } else if (e->type == SDL_MOUSEMOTION) {
        mx = e->motion.x;
        my = e->motion.y;

        if (win->dragging) {
            win->x = mx - win->drag_offset_x;
            win->y = my - win->drag_offset_y;
        } else if (win->resizing) {
            if (win->resize_dir == 1 || win->resize_dir == 3) {
                int new_w = mx - win->x - win->resize_offset_x;
                if (new_w >= MIN_WIDTH) win->width = new_w;
            }
            if (win->resize_dir == 2 || win->resize_dir == 3) {
                int new_h = my - win->y - win->resize_offset_y;
                if (new_h >= MIN_HEIGHT) win->height = new_h;
            }
        }
    }
}

void gui_window_draw(GUIWindow *win, SDL_Renderer *renderer) {
    if (!win->visible) return;

    SDL_Rect body = { win->x, win->y, win->width, win->height };
    SDL_Rect titlebar = { win->x, win->y, win->width, TITLEBAR_HEIGHT };
    SDL_Rect close_btn = {
        win->x + win->width - CLOSE_SIZE - BORDER,
        win->y + (TITLEBAR_HEIGHT - CLOSE_SIZE) / 2,
        CLOSE_SIZE, CLOSE_SIZE
    };

    // Body background
    SDL_SetRenderDrawColor(renderer, 60, 60, 220, 255);
    SDL_RenderFillRect(renderer, &body);

    // Title bar
    SDL_SetRenderDrawColor(renderer, 30, 30, 150, 255);
    SDL_RenderFillRect(renderer, &titlebar);

    // Border
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderDrawRect(renderer, &body);

    // Close button (red)
    SDL_SetRenderDrawColor(renderer, 200, 40, 40, 255);
    SDL_RenderFillRect(renderer, &close_btn);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderDrawRect(renderer, &close_btn);
}
