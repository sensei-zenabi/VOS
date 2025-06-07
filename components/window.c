#define _POSIX_C_SOURCE 200112L
#include "window.h"
#include "icon.h"
#include <string.h>
#include <stdlib.h>

#define TITLEBAR_H     30
#define BORDER         4
#define CLOSE_SZ       18
#define MIN_W          120
#define MIN_H          80

/* ------------------------------------------------------------ */
void gui_window_init(GUIWindow *w, const char *path,
                     int x, int y, int w_px, int h_px)
{
    w->x = x;  w->y = y;  w->width  = w_px;  w->height = h_px;
    w->dragging = w->resizing = 0;   w->resize_dir = 0;
    w->visible  = 1;
    strncpy(w->path, path, sizeof(w->path));
    w->next = NULL;
}

/* ------------------------------------------------------------ */
static int inside(int px, int py, SDL_Rect r)
{   return px >= r.x && px <= r.x + r.w && py >= r.y && py <= r.y + r.h; }

/* ------------------------------------------------------------ */
int gui_window_handle_event(GUIWindow *w,
                            SDL_Event *e,
                            GUIWindow **win_list)
{
    if (!w || !w->visible) return 0;

    int mx = (e->type == SDL_MOUSEMOTION) ? e->motion.x : e->button.x;
    int my = (e->type == SDL_MOUSEMOTION) ? e->motion.y : e->button.y;
    int consumed = 0;

    /* ----------------------------------------------------------------
       mouse-button down
       ----------------------------------------------------------------*/
    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {

        /* close button */
        SDL_Rect close = { w->x + w->width - CLOSE_SZ - BORDER,
                           w->y + (TITLEBAR_H - CLOSE_SZ) / 2,
                           CLOSE_SZ, CLOSE_SZ };
        if (inside(mx, my, close)) { w->visible = 0; return 1; }

        /* resize corners / edges (right, bottom, corner) */
        SDL_Rect right  = { w->x + w->width - BORDER, w->y, BORDER, w->height };
        SDL_Rect bottom = { w->x, w->y + w->height - BORDER, w->width, BORDER };
        SDL_Rect corner = { right.x, bottom.y, BORDER, BORDER };

        if (inside(mx, my, corner)) {
            w->resizing = 1; w->resize_dir = RESIZE_RIGHT | RESIZE_BOTTOM;
            w->resize_off_x = mx - (w->x + w->width);
            w->resize_off_y = my - (w->y + w->height);
        } else if (inside(mx, my, right)) {
            w->resizing = 1; w->resize_dir = RESIZE_RIGHT;
            w->resize_off_x = mx - (w->x + w->width);
        } else if (inside(mx, my, bottom)) {
            w->resizing = 1; w->resize_dir = RESIZE_BOTTOM;
            w->resize_off_y = my - (w->y + w->height);
        }
        /* drag title-bar */
        else if (inside(mx, my, (SDL_Rect){w->x, w->y, w->width, TITLEBAR_H})) {
            w->dragging = 1;
            w->drag_off_x = mx - w->x;
            w->drag_off_y = my - w->y;
        }

        consumed = 1;
    }

    /* ----------------------------------------------------------------
       mouse-button up
       ----------------------------------------------------------------*/
    if (e->type == SDL_MOUSEBUTTONUP && e->button.button == SDL_BUTTON_LEFT) {
        w->dragging = w->resizing = 0;
    }

    /* ----------------------------------------------------------------
       motion
       ----------------------------------------------------------------*/
    if (e->type == SDL_MOUSEMOTION) {
        if (w->dragging) {
            w->x = mx - w->drag_off_x;
            w->y = my - w->drag_off_y;
        } else if (w->resizing) {
            if (w->resize_dir & RESIZE_RIGHT) {
                int nw = mx - w->x - w->resize_off_x;
                if (nw >= MIN_W) w->width = nw;
            }
            if (w->resize_dir & RESIZE_BOTTOM) {
                int nh = my - w->y - w->resize_off_y;
                if (nh >= MIN_H) w->height = nh;
            }
        }
    }

    /* ----------------------------------------------------------------
       icon handling inside client area
       ----------------------------------------------------------------*/
    if (e->type == SDL_MOUSEBUTTONDOWN &&
        e->button.button == SDL_BUTTON_LEFT) {

        GUIIcon *icons = NULL;  int cnt = 0;
        if (load_icons(w->path, &icons, &cnt) == 0) {
            int ox = w->x,  oy = w->y + TITLEBAR_H;
            for (int i = 0; i < cnt; ++i) {
                SDL_Rect scr = { icons[i].rect.x + ox,
                                 icons[i].rect.y + oy,
                                 icons[i].rect.w, icons[i].rect.h };
                if (inside(mx, my, scr)) {
                    handle_icon_click(&icons[i], w->path, win_list);
                    consumed = 1;
                    break;
                }
            }
            free(icons);
        }
    }

    return consumed;
}

/* ------------------------------------------------------------ */
void gui_window_draw(GUIWindow *w, SDL_Renderer *r, TTF_Font *font)
{
    if (!w || !w->visible) return;

    SDL_SetRenderDrawColor(r, 60, 60, 220, 255);
    SDL_Rect body = { w->x, w->y, w->width, w->height };
    SDL_RenderFillRect(r, &body);

    SDL_SetRenderDrawColor(r, 30, 30, 120, 255);
    SDL_Rect bar  = { w->x, w->y, w->width, TITLEBAR_H };
    SDL_RenderFillRect(r, &bar);

    /* border */
    SDL_SetRenderDrawColor(r, 0,0,0,255);
    SDL_RenderDrawRect(r, &body);

    /* close button */
    SDL_Rect close = { w->x + w->width - CLOSE_SZ - BORDER,
                       w->y + (TITLEBAR_H - CLOSE_SZ)/2,
                       CLOSE_SZ, CLOSE_SZ };
    SDL_SetRenderDrawColor(r, 200,40,40,255);
    SDL_RenderFillRect(r, &close);
    SDL_SetRenderDrawColor(r, 0,0,0,255);
    SDL_RenderDrawRect(r, &close);

    /* icons */
    GUIIcon *icons = NULL;  int cnt = 0;
    if (load_icons(w->path, &icons, &cnt) == 0) {
        int ox = w->x,  oy = w->y + TITLEBAR_H;
        for (int i = 0; i < cnt; ++i) {
            GUIIcon tmp = icons[i];
            tmp.rect.x += ox;  tmp.rect.y += oy;
            draw_icon(r, font, &tmp);
        }
        free(icons);
    }
}
