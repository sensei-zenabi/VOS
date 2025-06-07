#define _POSIX_C_SOURCE 200112L
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include "components/window.h"

#define INTERNAL_WIDTH 640
#define INTERNAL_HEIGHT 480

int main(void) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_DisplayMode display_mode;
    if (SDL_GetCurrentDisplayMode(0, &display_mode) != 0) {
        fprintf(stderr, "SDL_GetCurrentDisplayMode failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow("Draggable Window Fullscreen",
                                          SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED,
                                          display_mode.w, display_mode.h,
                                          SDL_WINDOW_FULLSCREEN);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Keep internal 640x480 layout regardless of screen resolution
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1"); // Linear filtering
    SDL_RenderSetLogicalSize(renderer, INTERNAL_WIDTH, INTERNAL_HEIGHT);

    GUIWindow gui;
    gui_window_init(&gui, 100, 100, 300, 200);

    int quit = 0;
    SDL_Event e;
    while (!quit) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT ||
                (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)) {
                quit = 1;
            }

            gui_window_handle_event(&gui, &e);
        }

        SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
        SDL_RenderClear(renderer);

        gui_window_draw(&gui, renderer);

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
