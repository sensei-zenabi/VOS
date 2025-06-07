#define _POSIX_C_SOURCE 200112L
#include "icon.h"
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdio.h>

#define ICON_SZ      48
#define STEP         70
#define DBLCLICK_MS  400
#define DESK_WIDTH   640         /* logical desktop size */

/* geometry constants used here (duplicated from window.c) */
#define MIN_W       120
#define MIN_H        80
#define TITLEBAR_H   30

static int desktop_max_x(void){ return DESK_WIDTH-20; }

/* ---------- enumerate icons in a dir ------------------------------ */
int load_icons(const char *path, GUIIcon **icons, int *count)
{
    DIR *d=opendir(path); if(!d) return -1;

    int cap=32; *icons=malloc(sizeof(GUIIcon)*cap); *count=0;
    int x=10,y=10; struct dirent *e;

    while((e=readdir(d))){
        if(!strcmp(e->d_name,".")||!strcmp(e->d_name,"..")) continue;
        if(*count>=cap){ cap*=2; *icons=realloc(*icons,sizeof(GUIIcon)*cap);}
        GUIIcon *ic=&(*icons)[(*count)++];
        strncpy(ic->name,e->d_name,sizeof(ic->name));

        char full[512]; struct stat st;
        snprintf(full,sizeof(full),"%s/%s",path,e->d_name);
        ic->is_dir = (!stat(full,&st) && S_ISDIR(st.st_mode));

        ic->rect=(SDL_Rect){x,y,ICON_SZ,ICON_SZ};
        x += STEP;
        if(x > desktop_max_x()){ x=10; y += STEP; }
    }
    closedir(d); return 0;
}

/* ---------- draw one icon ----------------------------------------- */
void draw_icon(SDL_Renderer *r, TTF_Font *font, GUIIcon *ic)
{
    SDL_SetRenderDrawColor(r, ic->is_dir?80:160,80,200,255);
    SDL_RenderFillRect(r,&ic->rect);

    SDL_Color c={255,255,255};
    SDL_Surface *s=TTF_RenderText_Solid(font,ic->name,c);
    if(s){
        SDL_Texture *t=SDL_CreateTextureFromSurface(r,s);
        SDL_Rect dst={ic->rect.x,ic->rect.y+ICON_SZ+4,s->w,s->h};
        SDL_RenderCopy(r,t,NULL,&dst);
        SDL_FreeSurface(s); SDL_DestroyTexture(t);
    }
}

/* ---------- double-click handler ---------------------------------- */
void handle_icon_click(GUIIcon *ic,
                       const char *dir,
                       GUIWindow **winlist)
{
    static Uint32 last_ms=0; static char last_path[1024]="";
    char full[1024]; snprintf(full,sizeof(full),"%s/%s",dir,ic->name);

    Uint32 now=SDL_GetTicks();
    int dbl=(now-last_ms<DBLCLICK_MS)&&!strcmp(full,last_path);
    last_ms=now; strncpy(last_path,full,sizeof(last_path));

    if(!dbl) return;                                     /* single click */

    if(ic->is_dir){                                      /* open folder */
        GUIIcon *tmp=NULL; int cnt=0;
        load_icons(full,&tmp,&cnt); free(tmp);

        int cols=(DESK_WIDTH-40)/STEP; if(cols<1) cols=1;
        if(cnt<cols) cols=cnt?cnt:1;
        int rows=(cnt+cols-1)/cols;

        int w = cols*STEP + 20;  if(w<MIN_W) w=MIN_W; if(w>DESK_WIDTH-40) w=DESK_WIDTH-40;
        int h = rows*STEP + TITLEBAR_H + 20; if(h<MIN_H) h=MIN_H; if(h>480-40) h=480-40;

        GUIWindow *nw=malloc(sizeof(GUIWindow));
        gui_window_init(nw,full,120,120,w,h);

        /* push to end to appear topmost */
        if(!*winlist){ *winlist=nw; }
        else{ GUIWindow *p=*winlist; while(p->next)p=p->next; p->next=nw; }
    }else{                                               /* launch file */
        if(fork()==0){ execlp(full,full,(char*)NULL); perror("exec"); _exit(1);}
    }
}
