#define _POSIX_C_SOURCE 200112L
#include "edit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define TITLEBAR_H   30
#define BORDER        4
#define CLOSE_SZ     18
#define MIN_W       200
#define MIN_H       120
#define PAD          10
#define SCRL_SZ      12          /* scrollbar thickness */

static int inside(int px,int py,SDL_Rect r)
{ return px>=r.x && px<=r.x+r.w && py>=r.y && py<=r.y+r.h; }

/* small local strdup (portable) */
static char *dupstr(const char *s)
{
    size_t L = strlen(s)+1;
    char *d  = malloc(L);
    if(d) memcpy(d,s,L);
    return d;
}

/* ------------------------------------------------------------ */
static char **file_to_lines(const char *file,
                            int *count,int *max_w,
                            TTF_Font *font)
{
    *count = 0; *max_w = 0;
    FILE *fp = fopen(file,"r"); if(!fp) return NULL;

    int cap = 128;
    char **arr = malloc(sizeof(char*)*cap);
    char buf[4096];

    while(fgets(buf,sizeof(buf),fp)){
        char *nl = strchr(buf,'\n'); if(nl) *nl = '\0';
        if(*count >= cap){
            cap *= 2;
            arr  = realloc(arr,sizeof(char*)*cap);
        }
        arr[*count] = dupstr(buf);

        int wpx; TTF_SizeText(font, arr[*count], &wpx, NULL);
        if(wpx > *max_w) *max_w = wpx;

        (*count)++;
    }
    fclose(fp);
    return arr;
}

/* ------------------------------------------------------------ */
GUIEdit *edit_window_create(const char *file,int x,int y,int w,int h)
{
    TTF_Font *font = TTF_OpenFont(
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",12);
    if(!font) return NULL;

    GUIEdit *ed = calloc(1,sizeof(GUIEdit));
    gui_window_init(&ed->win, file, x,y,w,h);

    ed->line_h = TTF_FontHeight(font);
    ed->lines  = file_to_lines(file,&ed->line_count,&ed->max_w_px,font);
    TTF_CloseFont(font);

    if(!ed->lines){
        free(ed);
        return NULL;
    }
    return ed;
}

/* ------------------------------------------------------------ */
void edit_window_destroy(GUIEdit *ed)
{
    if(!ed) return;
    for(int i=0;i<ed->line_count;++i) free(ed->lines[i]);
    free(ed->lines);
    free(ed);
}

/* ------------------------------------------------------------ */
static void clamp_scroll(GUIEdit *ed)
{
    int view_w = ed->win.width  - SCRL_SZ - 2*PAD;
    int view_h = ed->win.height - TITLEBAR_H - SCRL_SZ - 2*PAD;

    int max_x = ed->max_w_px - view_w;         if(max_x < 0) max_x = 0;
    int max_y = ed->line_h*ed->line_count - view_h; if(max_y < 0) max_y = 0;

    if(ed->scroll_x < 0)        ed->scroll_x = 0;
    if(ed->scroll_y < 0)        ed->scroll_y = 0;
    if(ed->scroll_x > max_x)    ed->scroll_x = max_x;
    if(ed->scroll_y > max_y)    ed->scroll_y = max_y;
}

/* ------------------------------------------------------------ */
int edit_window_handle_event(GUIEdit *ed,
                             SDL_Event *e,
                             GUIEdit **edit_list)
{
    if(!ed || !ed->win.visible) return 0;

    /* let GUIWindow manage drag/resize/close */
    gui_window_handle_event(&ed->win, e, NULL, edit_list);

    int mx = (e->type==SDL_MOUSEMOTION)? e->motion.x : e->button.x;
    int my = (e->type==SDL_MOUSEMOTION)? e->motion.y : e->button.y;

    /* client view metrics */
    int view_x = ed->win.x + PAD;
    int view_y = ed->win.y + TITLEBAR_H + PAD;
    int view_w = ed->win.width  - SCRL_SZ - 2*PAD;
    int view_h = ed->win.height - SCRL_SZ - TITLEBAR_H - 2*PAD;
    int max_x  = ed->max_w_px - view_w;
    int max_y  = ed->line_h*ed->line_count - view_h;

    /* wheel scroll ------------------------------------------------ */
    if(e->type==SDL_MOUSEWHEEL &&
       mx>=view_x && mx<=view_x+view_w &&
       my>=view_y && my<=view_y+view_h)
    {
        ed->scroll_y -= e->wheel.y * ed->line_h * 3;
        clamp_scroll(ed);
        return 1;
    }

    /* scroll-bars ------------------------------------------------- */
    SDL_Rect vbar={ ed->win.x+ed->win.width-SCRL_SZ-BORDER,
                    ed->win.y+TITLEBAR_H,
                    SCRL_SZ,
                    ed->win.height-TITLEBAR_H-SCRL_SZ };

    SDL_Rect hbar={ ed->win.x+PAD,
                    ed->win.y+ed->win.height-SCRL_SZ-BORDER,
                    ed->win.width-SCRL_SZ-2*PAD,
                    SCRL_SZ };

    int vsz = (max_y<=0)? vbar.h : (view_h*vbar.h)/(view_h+max_y);
    if(vsz<20) vsz=20;
    int vpos = (max_y<=0)? 0 : ( (long long)ed->scroll_y*(vbar.h-vsz))/max_y;
    SDL_Rect vslider={ vbar.x, vbar.y+vpos, SCRL_SZ, vsz };

    int hsz = (max_x<=0)? hbar.w : (view_w*hbar.w)/(view_w+max_x);
    if(hsz<20) hsz=20;
    int hpos = (max_x<=0)? 0 : ( (long long)ed->scroll_x*(hbar.w-hsz))/max_x;
    SDL_Rect hslider={ hbar.x+hpos, hbar.y, hsz, SCRL_SZ };

    /* start drag */
    if(e->type==SDL_MOUSEBUTTONDOWN && e->button.button==SDL_BUTTON_LEFT){
        if(inside(mx,my,vslider)){ ed->v_dragging=1; ed->drag_off=my-vslider.y; return 1; }
        if(inside(mx,my,hslider)){ ed->h_dragging=1; ed->drag_off=mx-hslider.x; return 1; }
    }
    /* stop drag */
    if(e->type==SDL_MOUSEBUTTONUP && e->button.button==SDL_BUTTON_LEFT){
        ed->v_dragging = ed->h_dragging = 0;
    }
    /* motion drag */
    if(e->type==SDL_MOUSEMOTION){
        if(ed->v_dragging && max_y>0){
            int new_y = my - ed->drag_off - vbar.y;
            if(new_y<0) new_y=0;
            if(new_y>vbar.h-vsz) new_y=vbar.h-vsz;
            ed->scroll_y = (long long)new_y*max_y/(vbar.h-vsz);
            clamp_scroll(ed);
            return 1;
        }
        if(ed->h_dragging && max_x>0){
            int new_x = mx - ed->drag_off - hbar.x;
            if(new_x<0) new_x=0;
            if(new_x>hbar.w-hsz) new_x=hbar.w-hsz;
            ed->scroll_x = (long long)new_x*max_x/(hbar.w-hsz);
            clamp_scroll(ed);
            return 1;
        }
    }
    return 0;
}

/* ------------------------------------------------------------ */
void edit_window_draw(GUIEdit *ed,
                      SDL_Renderer *ren,
                      TTF_Font *font)
{
    if(!ed || !ed->win.visible) return;

    /* window chrome */
    gui_window_draw(&ed->win, ren, font);

    /* view metrics */
    int view_x = ed->win.x + PAD;
    int view_y = ed->win.y + TITLEBAR_H + PAD;
    int view_w = ed->win.width  - SCRL_SZ - 2*PAD;
    int view_h = ed->win.height - SCRL_SZ - TITLEBAR_H - 2*PAD;

    /* clip to client */
    SDL_RenderSetClipRect(ren,(SDL_Rect[]){ {view_x,view_y,view_w,view_h} });

    /* draw visible lines */
    int first = ed->scroll_y / ed->line_h;
    int yoff  = -(ed->scroll_y % ed->line_h);
    for(int i=first;i<ed->line_count;++i){
        int y = view_y + yoff + (i-first)*ed->line_h;
        if(y > view_y + view_h) break;
        SDL_Surface *s = TTF_RenderText_Solid(font,ed->lines[i],
                                              (SDL_Color){255,255,255});
        if(s){
            SDL_Texture *t = SDL_CreateTextureFromSurface(ren,s);
            SDL_Rect dst={ view_x - ed->scroll_x, y, s->w, s->h };
            SDL_RenderCopy(ren,t,NULL,&dst);
            SDL_FreeSurface(s); SDL_DestroyTexture(t);
        }
    }
    SDL_RenderSetClipRect(ren,NULL);

    /* scroll-bar tracks */
    SDL_SetRenderDrawColor(ren,80,80,80,255);
    SDL_Rect vbar={ ed->win.x+ed->win.width-SCRL_SZ-BORDER,
                    ed->win.y+TITLEBAR_H,
                    SCRL_SZ,
                    ed->win.height-TITLEBAR_H-SCRL_SZ };
    SDL_Rect hbar={ ed->win.x+PAD,
                    ed->win.y+ed->win.height-SCRL_SZ-BORDER,
                    ed->win.width-SCRL_SZ-2*PAD,
                    SCRL_SZ };
    SDL_RenderFillRect(ren,&vbar);
    SDL_RenderFillRect(ren,&hbar);

    /* slider thumbs */
    int max_x = ed->max_w_px - view_w;
    int max_y = ed->line_h*ed->line_count - view_h;
    int vsz = (max_y<=0)? vbar.h : (view_h*vbar.h)/(view_h+max_y); if(vsz<20)vsz=20;
    int vpos= (max_y<=0)? 0 : ( (long long)ed->scroll_y*(vbar.h-vsz))/max_y;
    int hsz = (max_x<=0)? hbar.w : (view_w*hbar.w)/(view_w+max_x); if(hsz<20)hsz=20;
    int hpos= (max_x<=0)? 0 : ( (long long)ed->scroll_x*(hbar.w-hsz))/max_x;

    SDL_SetRenderDrawColor(ren,200,200,200,255);
    SDL_Rect vslider={ vbar.x, vbar.y+vpos, SCRL_SZ, vsz };
    SDL_Rect hslider={ hbar.x+hpos, hbar.y, hsz, SCRL_SZ };
    SDL_RenderFillRect(ren,&vslider);
    SDL_RenderFillRect(ren,&hslider);
}
