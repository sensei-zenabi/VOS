#define _POSIX_C_SOURCE 200112L
#include "icon.h"
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdio.h>

#define ICON_SZ       48
#define STEP          70
#define DBLCLICK_MS  400

/* ------------------------------------------------------------ */
int load_icons(const char *path, GUIIcon **icons, int *count)
{
    DIR *d = opendir(path);
    if (!d) return -1;

    int cap = 32;  *icons = malloc(sizeof(GUIIcon)*cap);  *count = 0;
    int x = 10, y = 10;  struct dirent *e;

    while ((e = readdir(d))) {
        if (!strcmp(e->d_name,".")||!strcmp(e->d_name,"..")) continue;

        if (*count >= cap) { cap*=2; *icons = realloc(*icons,sizeof(GUIIcon)*cap); }
        GUIIcon *ic = &(*icons)[(*count)++];
        strncpy(ic->name, e->d_name, sizeof(ic->name));

        char full[512];  struct stat st;
        snprintf(full,sizeof(full),"%s/%s",path,e->d_name);
        ic->is_dir = (!stat(full,&st) && S_ISDIR(st.st_mode));

        ic->rect = (SDL_Rect){x,y,ICON_SZ,ICON_SZ};

        x += STEP;  if (x > 520) { x = 10; y += STEP; }
    }
    closedir(d);  return 0;
}

/* ------------------------------------------------------------ */
void draw_icon(SDL_Renderer *r, TTF_Font *font, GUIIcon *ic)
{
    SDL_SetRenderDrawColor(r, ic->is_dir?80:160, 80, 200,255);
    SDL_RenderFillRect(r, &ic->rect);

    SDL_Color c={255,255,255};
    SDL_Surface *s=TTF_RenderText_Solid(font, ic->name, c);
    if (s){
        SDL_Texture *t=SDL_CreateTextureFromSurface(r,s);
        SDL_Rect dst={ ic->rect.x, ic->rect.y+ICON_SZ+4, s->w, s->h };
        SDL_RenderCopy(r,t,NULL,&dst);
        SDL_FreeSurface(s); SDL_DestroyTexture(t);
    }
}

/* ------------------------------------------------------------ */
void handle_icon_click(GUIIcon *ic,
                       const char *dirpath,
                       GUIWindow  **win_list)
{
    static Uint32 last_ms=0;  static char last_path[1024]="";

    char full[1024];
    snprintf(full,sizeof(full),"%s/%s",dirpath,ic->name);

    Uint32 now = SDL_GetTicks();
    int dbl   = (now-last_ms < DBLCLICK_MS) && !strcmp(full,last_path);

    last_ms = now;  strncpy(last_path, full, sizeof(last_path));

    if (!dbl) return;                      /* wait for second click */

    if (ic->is_dir) {                      /* open folder window */
        GUIWindow *nw = malloc(sizeof(GUIWindow));
        gui_window_init(nw, full, 120,120, 320,240);

        /* append to END of list to appear topmost */
        if (!*win_list) { *win_list = nw; }
        else {
            GUIWindow *p=*win_list; while(p->next) p=p->next; p->next=nw;
        }
    } else {                               /* execute file */
        if (fork()==0) { execlp(full,full,(char*)NULL); perror("exec"); _exit(1); }
    }
}
