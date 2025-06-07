#ifndef ICON_H
#define ICON_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "window.h"

typedef struct {
    SDL_Rect rect;
    char     name[256];
    int      is_dir;
} GUIIcon;

int  load_icons(const char *path, GUIIcon **icons, int *count);
void draw_icon(SDL_Renderer *r, TTF_Font *font, GUIIcon *ic);
void handle_icon_click(GUIIcon *ic,
                       const char *dirpath,
                       GUIWindow  **win_list);

#endif
