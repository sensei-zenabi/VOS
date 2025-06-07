#define _POSIX_C_SOURCE 200112L
#include "window.h"
#include "icon.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

#define TITLEBAR_H   30
#define BORDER       4
#define CLOSE_SZ     18
#define MIN_W        120
#define MIN_H        80
#define ICON_SZ      48
#define STEP         70

/* quick helper */
static inline int inside(int px,int py,SDL_Rect r)
{ return px>=r.x && px<=r.x+r.w && py>=r.y && py<=r.y+r.h; }

/* ------------------------------------------------------------ */
void gui_window_init(GUIWindow *w,const char *path,
                     int x,int y,int w_px,int h_px)
{
    w->x=x;w->y=y;w->width=w_px;w->height=h_px;
    w->dragging=w->resizing=0;w->resize_dir=0;
    w->visible=1;
    strncpy(w->path,path,sizeof(w->path));
    w->next=NULL;
}

/* helper for icon placement ---------------------------------- */
static void icon_rect(SDL_Rect *out,int idx,int cols,int ox,int oy)
{
    int col=idx%cols,row=idx/cols;
    *out=(SDL_Rect){ ox+10+col*STEP, oy+TITLEBAR_H+10+row*STEP,
                     ICON_SZ, ICON_SZ };
}

/* ------------------------------------------------------------ */
int gui_window_handle_event(GUIWindow *w,
                            SDL_Event *e,
                            GUIWindow **winlist)
{
    if(!w || !w->visible) return 0;

    int mx=(e->type==SDL_MOUSEMOTION)?e->motion.x:e->button.x;
    int my=(e->type==SDL_MOUSEMOTION)?e->motion.y:e->button.y;
    int consumed=0;

    SDL_Rect winrect={w->x,w->y,w->width,w->height};

    /* ----------------------------------------------------------------
       BEGIN  mouse-button-down  (only if cursor inside window bounds)
       ----------------------------------------------------------------*/
    if(e->type==SDL_MOUSEBUTTONDOWN && e->button.button==SDL_BUTTON_LEFT
       && inside(mx,my,winrect))                                       /* FIX */
    {
        /* close btn */
        SDL_Rect close={ w->x+w->width-CLOSE_SZ-BORDER,
                         w->y+(TITLEBAR_H-CLOSE_SZ)/2,
                         CLOSE_SZ,CLOSE_SZ };
        if(inside(mx,my,close)){ w->visible=0; return 1; }

        /* resize zones */
        SDL_Rect right ={w->x+w->width-BORDER,w->y,BORDER,w->height};
        SDL_Rect bottom={w->x,w->y+w->height-BORDER,w->width,BORDER};
        SDL_Rect corner={right.x,bottom.y,BORDER,BORDER};

        if(inside(mx,my,corner)){
            w->resizing=1;w->resize_dir=RESIZE_RIGHT|RESIZE_BOTTOM;
            w->resize_off_x=mx-(w->x+w->width);
            w->resize_off_y=my-(w->y+w->height);
        }else if(inside(mx,my,right)){
            w->resizing=1;w->resize_dir=RESIZE_RIGHT;
            w->resize_off_x=mx-(w->x+w->width);
        }else if(inside(mx,my,bottom)){
            w->resizing=1;w->resize_dir=RESIZE_BOTTOM;
            w->resize_off_y=my-(w->y+w->height);
        }
        /* drag bar */
        else if(inside(mx,my,(SDL_Rect){w->x,w->y,w->width,TITLEBAR_H})){
            w->dragging=1;
            w->drag_off_x=mx-w->x;
            w->drag_off_y=my-w->y;
        }
        consumed=1;                                                    /* only if inside */
    }

    /* ----------------------------------------------------------------
       END  mouse-button-down
       ----------------------------------------------------------------*/

    /* release drag/resize if active */
    if(e->type==SDL_MOUSEBUTTONUP && e->button.button==SDL_BUTTON_LEFT){
        w->dragging=w->resizing=0;
        if(w->dragging||w->resizing) consumed=1;                       /* FIX */
    }

    /* motion updates (only while dragging/resizing) */
    if(e->type==SDL_MOUSEMOTION && (w->dragging||w->resizing)){        /* FIX */
        if(w->dragging){ w->x=mx-w->drag_off_x; w->y=my-w->drag_off_y; }
        else if(w->resizing){
            if(w->resize_dir&RESIZE_RIGHT){
                int nw=mx-w->x-w->resize_off_x; if(nw>=MIN_W) w->width=nw;
            }
            if(w->resize_dir&RESIZE_BOTTOM){
                int nh=my-w->y-w->resize_off_y; if(nh>=MIN_H) w->height=nh;
            }
        }
        consumed=1;
    }

    /* desktop-click pass-through if not inside window */
    if(!inside(mx,my,winrect) && !(w->dragging||w->resizing))          /* FIX */
        return consumed;

    /* ----------------------------------------------------------------
       icon clicks (only when click began inside window client area)
       ----------------------------------------------------------------*/
    if(e->type==SDL_MOUSEBUTTONDOWN && e->button.button==SDL_BUTTON_LEFT
       && inside(mx,my,winrect))                                       /* FIX */
    {
        GUIIcon *arr=NULL;int cnt=0;
        if(load_icons(w->path,&arr,&cnt)==0){
            int cols=(w->width-20)/STEP; if(cols<1) cols=1;
            for(int i=0;i<cnt;++i){
                SDL_Rect r; icon_rect(&r,i,cols,w->x,w->y);
                if(inside(mx,my,r)){
                    handle_icon_click(&arr[i],w->path,winlist);
                    consumed=1; break;
                }
            }
            free(arr);
        }
    }
    return consumed;
}

/* ------------------------------------------------------------ */
void gui_window_draw(GUIWindow *w,
                     SDL_Renderer *r,
                     TTF_Font *font)
{
    if(!w||!w->visible) return;

    SDL_SetRenderDrawColor(r,60,60,220,255);
    SDL_Rect body={w->x,w->y,w->width,w->height}; SDL_RenderFillRect(r,&body);

    SDL_SetRenderDrawColor(r,30,30,120,255);
    SDL_Rect bar={w->x,w->y,w->width,TITLEBAR_H}; SDL_RenderFillRect(r,&bar);

    SDL_SetRenderDrawColor(r,0,0,0,255); SDL_RenderDrawRect(r,&body);

    SDL_SetRenderDrawColor(r,200,40,40,255);
    SDL_Rect close={ w->x+w->width-CLOSE_SZ-BORDER,
                     w->y+(TITLEBAR_H-CLOSE_SZ)/2,
                     CLOSE_SZ,CLOSE_SZ };
    SDL_RenderFillRect(r,&close); SDL_RenderDrawRect(r,&close);

    GUIIcon *arr=NULL;int cnt=0;
    if(load_icons(w->path,&arr,&cnt)==0){
        int cols=(w->width-20)/STEP; if(cols<1) cols=1;
        for(int i=0;i<cnt;++i){
            GUIIcon tmp=arr[i];
            icon_rect(&tmp.rect,i,cols,w->x,w->y);
            draw_icon(r,font,&tmp);
        }
        free(arr);
    }
}
