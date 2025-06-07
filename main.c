#define _POSIX_C_SOURCE 200112L
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>
#include "components/window.h"
#include "components/icon.h"

/* ----- helpers ------------------------------------------------------ */
static void append_front(GUIWindow **list, GUIWindow *w)
{
    /* detach */
    GUIWindow *prev=NULL,*cur=*list;
    while(cur && cur!=w){ prev=cur; cur=cur->next; }
    if (!cur) return;                      /* not found */

    if (prev) prev->next = cur->next;      /* cut out */
    else      *list      = cur->next;

    /* append at END */
    cur->next = NULL;
    if (!*list) { *list = cur; }
    else {
        GUIWindow *p=*list; while(p->next) p=p->next; p->next=cur;
    }
}
/* ------------------------------------------------------------------- */
int main(void)
{
    if (SDL_Init(SDL_INIT_VIDEO)!=0 || TTF_Init()!=0){
        fprintf(stderr,"SDL/TTF init failed\n"); return 1;
    }

    SDL_Window *win=SDL_CreateWindow("SDL File Browser",
                                     SDL_WINDOWPOS_CENTERED,
                                     SDL_WINDOWPOS_CENTERED,
                                     800,600, SDL_WINDOW_FULLSCREEN);
    SDL_Renderer *ren=SDL_CreateRenderer(win,-1,SDL_RENDERER_ACCELERATED);
    SDL_RenderSetLogicalSize(ren,640,480);

    TTF_Font *font=TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",12);
    if(!font){ fprintf(stderr,"Font not found\n"); return 1; }

    /* desktop icons -------------------------------------------------- */
    GUIIcon *desk_icons=NULL; int desk_cnt=0;
    load_icons(".", &desk_icons, &desk_cnt);

    /* window list (desktop has no window) ---------------------------- */
    GUIWindow *windows=NULL;

    int quit=0; SDL_Event e;
    while(!quit){
        while(SDL_PollEvent(&e)){
            if(e.type==SDL_QUIT ||
               (e.type==SDL_KEYDOWN && e.key.keysym.sym==SDLK_ESCAPE))
                quit=1;

            int handled=0;

            /* pass to windows, top-most first (iterate tail→head) */
            int n=0; for(GUIWindow *p=windows;p;p=p->next) ++n;
            GUIWindow **stack = alloca(sizeof(GUIWindow*)*n);
            int i=0; for(GUIWindow *p=windows;p;p=p->next) stack[i++]=p;

            for(int k=n-1;k>=0 && !handled;--k){
                if(gui_window_handle_event(stack[k],&e,&windows)){
                    append_front(&windows, stack[k]);   /* raise */
                    handled=1;
                }
            }

            /* desktop icons if not handled */
            if(!handled && e.type==SDL_MOUSEBUTTONDOWN &&
               e.button.button==SDL_BUTTON_LEFT){
                for(int idx=0; idx<desk_cnt; ++idx){
                    if (e.button.x >= desk_icons[idx].rect.x &&
                        e.button.x <= desk_icons[idx].rect.x + desk_icons[idx].rect.w &&
                        e.button.y >= desk_icons[idx].rect.y &&
                        e.button.y <= desk_icons[idx].rect.y + desk_icons[idx].rect.h){
                        handle_icon_click(&desk_icons[idx], ".", &windows);
                        break;
                    }
                }
            }
        }

        /* remove closed windows */
        GUIWindow **pp=&windows;
        while(*pp){
            if(!(*pp)->visible){
                GUIWindow *del=*pp; *pp=del->next; free(del);
            } else pp=&(*pp)->next;
        }

        /* ---------------- draw frame --------------------------------*/
        SDL_SetRenderDrawColor(ren,20,20,20,255);
        SDL_RenderClear(ren);

        /* desktop icons */
        for(int i=0;i<desk_cnt;++i)
            draw_icon(ren,font,&desk_icons[i]);

        /* windows (back→front) */
        for(GUIWindow *p=windows;p;p=p->next)
            gui_window_draw(p,ren,font);

        SDL_RenderPresent(ren);
        SDL_Delay(16);
    }

    free(desk_icons);
    SDL_DestroyRenderer(ren); SDL_DestroyWindow(win);
    TTF_Quit(); SDL_Quit();
    return 0;
}
