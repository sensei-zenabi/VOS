#include "Terminal.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include <exception>
#include <iostream>
#include <memory>

namespace {
constexpr int kInitialWidth = 1920;
constexpr int kInitialHeight = 1080;
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER) != 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
        return 1;
    }

    if (TTF_Init() != 0) {
        std::cerr << "TTF_Init failed: " << TTF_GetError() << "\n";
        SDL_Quit();
        return 1;
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

    SDL_Window* window = SDL_CreateWindow(
        "Cool Retro Terminal Clone",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        kInitialWidth,
        kInitialHeight,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);

    if (!window) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << "\n";
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << "\n";
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    try {
        Terminal terminal(renderer, kInitialWidth, kInitialHeight);
        SDL_StartTextInput();

        bool running = true;
        SDL_Event event;
        while (running) {
            while (SDL_PollEvent(&event)) {
                switch (event.type) {
                case SDL_QUIT:
                    running = false;
                    break;
                case SDL_WINDOWEVENT:
                    if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                        terminal.handleWindowSize(event.window.data1, event.window.data2);
                    }
                    break;
                case SDL_TEXTINPUT:
                    terminal.handleTextInput(event.text.text);
                    break;
                case SDL_KEYDOWN:
                    terminal.handleKeyPress(event.key.keysym, event.key.repeat != 0);
                    break;
                default:
                    break;
                }
            }

            terminal.update();
            terminal.render();

            if (!terminal.isChildAlive()) {
                running = false;
            }

            SDL_Delay(16);
        }

        SDL_StopTextInput();
    } catch (const std::exception& ex) {
        std::cerr << "Fatal error: " << ex.what() << "\n";
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    return 0;
}

