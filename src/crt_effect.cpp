#include "CRTEffect.h"

#include <SDL2/SDL.h>

#include <algorithm>
#include <array>
#include <vector>

CRTEffect::CRTEffect()
    : m_rng(std::random_device{}()) {}

CRTEffect::~CRTEffect() {
    if (m_noiseTexture) {
        SDL_DestroyTexture(m_noiseTexture);
    }
    if (m_vignetteTexture) {
        SDL_DestroyTexture(m_vignetteTexture);
    }
}

void CRTEffect::ensureResources(SDL_Renderer* renderer, int width, int height) {
    int existingWidth = 0;
    int existingHeight = 0;

    if (m_noiseTexture) {
        SDL_QueryTexture(m_noiseTexture, nullptr, nullptr, &existingWidth, &existingHeight);
        if (existingWidth != width || existingHeight != height) {
            SDL_DestroyTexture(m_noiseTexture);
            m_noiseTexture = nullptr;
        }
    }

    if (!m_noiseTexture) {
        m_noiseTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, width, height);
        SDL_SetTextureBlendMode(m_noiseTexture, SDL_BLENDMODE_BLEND);
    }

    existingWidth = existingHeight = 0;
    if (m_vignetteTexture) {
        SDL_QueryTexture(m_vignetteTexture, nullptr, nullptr, &existingWidth, &existingHeight);
        if (existingWidth != width || existingHeight != height) {
            SDL_DestroyTexture(m_vignetteTexture);
            m_vignetteTexture = nullptr;
        }
    }

    if (!m_vignetteTexture) {
        m_vignetteTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, width, height);
        SDL_SetTextureBlendMode(m_vignetteTexture, SDL_BLENDMODE_MOD);
        drawVignette(renderer, width, height);
    }
}

void CRTEffect::apply(SDL_Renderer* renderer, SDL_Texture* source) {
    int width = 0;
    int height = 0;
    SDL_QueryTexture(source, nullptr, nullptr, &width, &height);
    ensureResources(renderer, width, height);

    SDL_SetRenderTarget(renderer, nullptr);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    SDL_SetTextureColorMod(source, 255, 255, 255);
    SDL_SetTextureAlphaMod(source, 255);
    SDL_RenderCopy(renderer, source, nullptr, nullptr);

    drawScanlines(renderer, width, height);
    updateNoise(width, height);

    if (m_noiseTexture) {
        SDL_SetTextureAlphaMod(m_noiseTexture, 32);
        SDL_RenderCopy(renderer, m_noiseTexture, nullptr, nullptr);
    }

    if (m_vignetteTexture) {
        SDL_RenderCopy(renderer, m_vignetteTexture, nullptr, nullptr);
    }
}

void CRTEffect::updateNoise(int width, int height) {
    if (!m_noiseTexture) {
        return;
    }

    void* pixels = nullptr;
    int pitch = 0;
    if (SDL_LockTexture(m_noiseTexture, nullptr, &pixels, &pitch) != 0) {
        return;
    }

    std::uniform_int_distribution<int> dist(0, 30);
    SDL_PixelFormat* format = SDL_AllocFormat(SDL_PIXELFORMAT_RGBA8888);
    if (!format) {
        SDL_UnlockTexture(m_noiseTexture);
        return;
    }
    auto* row = static_cast<uint8_t*>(pixels);
    for (int y = 0; y < height; ++y) {
        auto* pixel = reinterpret_cast<uint32_t*>(row);
        for (int x = 0; x < width; ++x) {
            uint8_t noise = static_cast<uint8_t>(dist(m_rng));
            pixel[x] = SDL_MapRGBA(format, noise, noise, noise, noise);
        }
        row += pitch;
    }

    SDL_UnlockTexture(m_noiseTexture);
    SDL_FreeFormat(format);
}

void CRTEffect::drawVignette(SDL_Renderer* renderer, int width, int height) {
    if (!m_vignetteTexture) {
        return;
    }

    SDL_SetRenderTarget(renderer, m_vignetteTexture);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderClear(renderer);

    SDL_Rect rect{0, 0, width, height};
    SDL_SetRenderDrawColor(renderer, 30, 30, 30, 0);
    SDL_RenderFillRect(renderer, &rect);

    const int steps = 64;
    for (int i = 0; i < steps; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(steps);
        uint8_t alpha = static_cast<uint8_t>(std::clamp(255 * t * t, 0.0f, 255.0f));
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, alpha);
        SDL_Rect vignetteRect{
            static_cast<int>(t * 0.5f * width),
            static_cast<int>(t * 0.5f * height),
            static_cast<int>(width - t * width),
            static_cast<int>(height - t * height)};
        SDL_RenderDrawRect(renderer, &vignetteRect);
    }

    SDL_SetRenderTarget(renderer, nullptr);
}

void CRTEffect::drawScanlines(SDL_Renderer* renderer, int width, int height) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 60);
    for (int y = 0; y < height; y += 2) {
        SDL_RenderDrawLine(renderer, 0, y, width - 1, y);
    }
}

