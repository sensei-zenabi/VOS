#ifndef ICON_H
#define ICON_H
#define _POSIX_C_SOURCE 200112L

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "window.h"
#include "edit.h"

typedef struct {
    SDL_Rect rect;
    char     name[256];
    int      is_dir;
} GUIIcon;

/* load / draw ------------------------------------------------------- */
int  load_icons(const char *path, GUIIcon **icons, int *count);
void draw_icon(SDL_Renderer *ren, TTF_Font *font, GUIIcon *ic);

/* click handler ----------------------------------------------------- */
void handle_icon_click(GUIIcon  *ic,
                       const char *dirpath,
                       GUIWindow **win_list,
                       GUIEdit   **edit_list);

#endif /* ICON_H */
