#define _POSIX_C_SOURCE 200112L
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>
#include "components/window.h"
#include "components/icon.h"
#include "components/edit.h"

/* raise a window (or editor) to top ------------------------------- */
static void raise_front(GUIWindow **list, GUIWindow *w)
{
    GUIWindow *prev=NULL,*cur=*list;
    while(cur && cur!=w){ prev=cur; cur=cur->next; }
    if(!cur) return;
    if(prev) prev->next = cur->next;
    else     *list      = cur->next;
    cur->next = NULL;
    if(!*list) *list = cur;
    else{
        GUIWindow *p=*list; while(p->next) p=p->next; p->next=cur;
    }
}
/* ----------------------------------------------------------------- */
int main(void)
{
    if(SDL_Init(SDL_INIT_VIDEO)!=0 || TTF_Init()!=0){
        fprintf(stderr,"SDL/TTF init failed\n"); return 1;
    }
    SDL_Window *win  = SDL_CreateWindow("SDL File Browser",
                                        SDL_WINDOWPOS_CENTERED,
                                        SDL_WINDOWPOS_CENTERED,
                                        800,600, SDL_WINDOW_FULLSCREEN);
    SDL_Renderer *ren = SDL_CreateRenderer(win,-1,SDL_RENDERER_ACCELERATED);
    SDL_RenderSetLogicalSize(ren, 640,480);

    TTF_Font *font = TTF_OpenFont(
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 12);
    if(!font){ fprintf(stderr,"font\n"); return 1; }

    GUIIcon *desk_icons=NULL; int desk_cnt=0;
    load_icons(".", &desk_icons, &desk_cnt);

    GUIWindow *browsers=NULL;   /* folder windows */
    GUIEdit   *edits   = NULL;  /* text editor windows */

    int quit=0; SDL_Event e;
    while(!quit){
        while(SDL_PollEvent(&e)){
            if(e.type==SDL_QUIT ||
               (e.type==SDL_KEYDOWN && e.key.keysym.sym==SDLK_ESCAPE))
                quit=1;

            int handled=0;

            /* ---- editors first (top to bottom) ------------------- */
            int ecnt=0; for(GUIEdit *p=edits;p;p=(GUIEdit*)p->win.next) ++ecnt;
            GUIEdit **estack = alloca(sizeof(GUIEdit*)*ecnt);
            int ix=0; for(GUIEdit *p=edits;p;p=(GUIEdit*)p->win.next) estack[ix++]=p;
            for(int k=ecnt-1;k>=0 && !handled;--k){
                if(edit_window_handle_event(estack[k],&e,&edits)){
                    raise_front((GUIWindow**)&edits,(GUIWindow*)estack[k]);
                    handled=1;
                }
            }

            /* ---- browsers next ----------------------------------- */
            int bcnt=0; for(GUIWindow *p=browsers;p;p=p->next) ++bcnt;
            GUIWindow **bstack = alloca(sizeof(GUIWindow*)*bcnt);
            int iy=0; for(GUIWindow *p=browsers;p;p=p->next) bstack[iy++]=p;
            for(int k=bcnt-1;k>=0 && !handled;--k){
                if(gui_window_handle_event(
                        bstack[k], &e, &browsers, &edits)){
                    raise_front(&browsers, bstack[k]);
                    handled=1;
                }
            }

            /* ---- desktop icons ----------------------------------- */
            if(!handled && e.type==SDL_MOUSEBUTTONDOWN &&
               e.button.button==SDL_BUTTON_LEFT){
                for(int i=0;i<desk_cnt;++i){
                    if(e.button.x >= desk_icons[i].rect.x &&
                       e.button.x <= desk_icons[i].rect.x+desk_icons[i].rect.w &&
                       e.button.y >= desk_icons[i].rect.y &&
                       e.button.y <= desk_icons[i].rect.y+desk_icons[i].rect.h)
                    {
                        handle_icon_click(&desk_icons[i], ".", &browsers, &edits);
                        break;
                    }
                }
            }
        }

        /* purge closed editors */
        GUIEdit **pe=&edits;
        while(*pe){
            if(!(*pe)->win.visible){
                GUIEdit *dead=*pe; *pe=(GUIEdit*)dead->win.next;
                edit_window_destroy(dead);
            }else pe=(GUIEdit**)&(*pe)->win.next;
        }
        /* purge closed browser windows */
        GUIWindow **pb=&browsers;
        while(*pb){
            if(!(*pb)->visible){
                GUIWindow *dead=*pb; *pb=dead->next; free(dead);
            }else pb=&(*pb)->next;
        }

        /* -------------- draw frame ------------------------------ */
        SDL_SetRenderDrawColor(ren,20,20,20,255);
        SDL_RenderClear(ren);

        for(int i=0;i<desk_cnt;++i) draw_icon(ren,font,&desk_icons[i]);
        for(GUIWindow *p=browsers;p;p=p->next) gui_window_draw(p,ren,font);
        for(GUIEdit *p=edits;p;p=(GUIEdit*)p->win.next)
            edit_window_draw(p,ren,font);

        SDL_RenderPresent(ren);
        SDL_Delay(16);
    }
    free(desk_icons);
    SDL_DestroyRenderer(ren); SDL_DestroyWindow(win);
    TTF_Quit(); SDL_Quit();
    return 0;
}
