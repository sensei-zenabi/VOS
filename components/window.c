#define _POSIX_C_SOURCE 200112L
#include "window.h"
#include "icon.h"
#include "edit.h"
#include <string.h>
#include <stdlib.h>

#define TITLEBAR_H   30
#define BORDER        4
#define CLOSE_SZ     18
#define MIN_W       120
#define MIN_H        80
#define ICON_SZ      48
#define STEP         70          /* icon grid spacing */

/* ------------------------------------------------------------ */
static inline int inside(int px,int py, SDL_Rect r)
{
    return px >= r.x && px <= r.x + r.w &&
           py >= r.y && py <= r.y + r.h;
}

/* ------------------------------------------------------------ */
void gui_window_init(GUIWindow *w,
                     const char *path,
                     int x,int y,
                     int w_px,int h_px)
{
    w->x = x;  w->y = y;
    w->width = w_px;  w->height = h_px;

    w->dragging = w->resizing = 0;
    w->resize_dir = 0;
    w->visible = 1;

    strncpy(w->path,path,sizeof(w->path));
    w->next = NULL;
}

/* helper: icon position inside window ------------------------ */
static void icon_rect(SDL_Rect *out,int idx,int cols,
                      int win_x,int win_y)
{
    int col = idx % cols;
    int row = idx / cols;
    out->x = win_x + 10 + col*STEP;
    out->y = win_y + TITLEBAR_H + 10 + row*STEP;
    out->w = ICON_SZ;
    out->h = ICON_SZ;
}

/* ------------------------------------------------------------ */
int gui_window_handle_event(GUIWindow *w,
                            SDL_Event *e,
                            GUIWindow **win_list,
                            GUIEdit   **edit_list)
{
    if(!w || !w->visible) return 0;

    int mx = (e->type==SDL_MOUSEMOTION)? e->motion.x : e->button.x;
    int my = (e->type==SDL_MOUSEMOTION)? e->motion.y : e->button.y;

    SDL_Rect winrect = { w->x, w->y, w->width, w->height };
    int consumed = 0;

    /* ---------- mouse down: only if inside bounds --------------- */
    if(e->type==SDL_MOUSEBUTTONDOWN && e->button.button==SDL_BUTTON_LEFT &&
       inside(mx,my,winrect))
    {
        /* close button */
        SDL_Rect close={ w->x+w->width-CLOSE_SZ-BORDER,
                         w->y+(TITLEBAR_H-CLOSE_SZ)/2,
                         CLOSE_SZ,CLOSE_SZ };
        if(inside(mx,my,close)){ w->visible = 0; return 1; }

        /* resize handles */
        SDL_Rect right ={ w->x+w->width-BORDER, w->y,
                          BORDER, w->height };
        SDL_Rect bottom={ w->x, w->y+w->height-BORDER,
                          w->width, BORDER };
        SDL_Rect corner={ right.x, bottom.y, BORDER, BORDER };

        if(inside(mx,my,corner)){
            w->resizing = 1; w->resize_dir = RESIZE_RIGHT|RESIZE_BOTTOM;
            w->resize_off_x = mx - (w->x + w->width);
            w->resize_off_y = my - (w->y + w->height);
        }else if(inside(mx,my,right)){
            w->resizing = 1; w->resize_dir = RESIZE_RIGHT;
            w->resize_off_x = mx - (w->x + w->width);
        }else if(inside(mx,my,bottom)){
            w->resizing = 1; w->resize_dir = RESIZE_BOTTOM;
            w->resize_off_y = my - (w->y + w->height);
        }
        /* drag title bar */
        else if(inside(mx,my,(SDL_Rect){w->x,w->y,w->width,TITLEBAR_H})){
            w->dragging = 1;
            w->drag_off_x = mx - w->x;
            w->drag_off_y = my - w->y;
        }
        consumed = 1;
    }

    /* ---------- mouse up ---------------------------------------- */
    if(e->type==SDL_MOUSEBUTTONUP && e->button.button==SDL_BUTTON_LEFT){
        if(w->dragging || w->resizing) consumed = 1;
        w->dragging = w->resizing = 0;
    }

    /* ---------- motion while dragging / resizing ---------------- */
    if(e->type==SDL_MOUSEMOTION && (w->dragging || w->resizing)){
        if(w->dragging){
            w->x = mx - w->drag_off_x;
            w->y = my - w->drag_off_y;
        }else{  /* resizing */
            if(w->resize_dir & RESIZE_RIGHT){
                int nw = mx - w->x - w->resize_off_x;
                if(nw >= MIN_W) w->width = nw;
            }
            if(w->resize_dir & RESIZE_BOTTOM){
                int nh = my - w->y - w->resize_off_y;
                if(nh >= MIN_H) w->height = nh;
            }
        }
        consumed = 1;
    }

    /* if cursor not inside window and not dragging/resizing,
       let event fall through to desktop */
    if(!inside(mx,my,winrect) && !(w->dragging || w->resizing))
        return consumed;

    /* ---------- client-area icon clicks ------------------------- */
    if(e->type==SDL_MOUSEBUTTONDOWN && e->button.button==SDL_BUTTON_LEFT &&
       inside(mx,my,winrect))
    {
        GUIIcon *arr=NULL; int cnt=0;
        if(load_icons(w->path,&arr,&cnt)==0){
            int cols = (w->width-20)/STEP; if(cols<1) cols=1;
            for(int i=0;i<cnt;++i){
                SDL_Rect r; icon_rect(&r,i,cols,w->x,w->y);
                if(inside(mx,my,r)){
                    handle_icon_click(&arr[i], w->path,
                                      win_list, edit_list);
                    consumed = 1;
                    break;
                }
            }
            free(arr);
        }
    }
    return consumed;
}

/* ------------------------------------------------------------ */
void gui_window_draw(GUIWindow *w,
                     SDL_Renderer *ren,
                     TTF_Font *font)
{
    if(!w || !w->visible) return;

    /* body */
    SDL_SetRenderDrawColor(ren, 60,60,220,255);
    SDL_Rect body={ w->x, w->y, w->width, w->height };
    SDL_RenderFillRect(ren, &body);

    /* title bar */
    SDL_SetRenderDrawColor(ren, 30,30,120,255);
    SDL_Rect bar={ w->x, w->y, w->width, TITLEBAR_H };
    SDL_RenderFillRect(ren, &bar);

    /* border */
    SDL_SetRenderDrawColor(ren, 0,0,0,255);
    SDL_RenderDrawRect(ren, &body);

    /* close button */
    SDL_SetRenderDrawColor(ren, 200,40,40,255);
    SDL_Rect close={ w->x+w->width-CLOSE_SZ-BORDER,
                     w->y+(TITLEBAR_H-CLOSE_SZ)/2,
                     CLOSE_SZ, CLOSE_SZ };
    SDL_RenderFillRect(ren, &close);
    SDL_RenderDrawRect(ren, &close);

    /* icons ------------------------------------------------------ */
    GUIIcon *arr=NULL; int cnt=0;
    if(load_icons(w->path,&arr,&cnt)==0){
        int cols = (w->width-20)/STEP; if(cols<1) cols=1;
        for(int i=0;i<cnt;++i){
            GUIIcon tmp = arr[i];
            icon_rect(&tmp.rect,i,cols,w->x,w->y);
            draw_icon(ren, font, &tmp);
        }
        free(arr);
    }
}
