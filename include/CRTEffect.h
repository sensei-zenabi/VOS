#pragma once

#include <SDL2/SDL.h>

#include <random>

class CRTEffect {
public:
    CRTEffect();
    ~CRTEffect();

    CRTEffect(const CRTEffect&) = delete;
    CRTEffect& operator=(const CRTEffect&) = delete;

    void ensureResources(SDL_Renderer* renderer, int width, int height);
    void apply(SDL_Renderer* renderer, SDL_Texture* source);

private:
    void updateNoise(int width, int height);
    void drawVignette(SDL_Renderer* renderer, int width, int height);
    void drawScanlines(SDL_Renderer* renderer, int width, int height);

    SDL_Texture* m_noiseTexture{nullptr};
    SDL_Texture* m_vignetteTexture{nullptr};
    std::mt19937 m_rng;
};

