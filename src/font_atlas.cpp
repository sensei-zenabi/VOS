#include "FontAtlas.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {
TTF_Font* duplicateFontWithStyle(const std::string& path, int pixelSize, uint8_t style) {
    TTF_Font* font = TTF_OpenFont(path.c_str(), pixelSize);
    if (!font) {
        throw std::runtime_error(std::string("Failed to load font: ") + TTF_GetError());
    }

    int sdlStyle = TTF_STYLE_NORMAL;
    if (style & FontAtlas::Bold) {
        sdlStyle |= TTF_STYLE_BOLD;
    }
    if (style & FontAtlas::Italic) {
        sdlStyle |= TTF_STYLE_ITALIC;
    }
    TTF_SetFontStyle(font, sdlStyle);
    TTF_SetFontHinting(font, TTF_HINTING_LIGHT);
    return font;
}
}

FontAtlas::FontAtlas(SDL_Renderer* renderer, int pixelSize)
    : m_renderer(renderer), m_pixelSize(pixelSize) {
    if (!renderer) {
        throw std::runtime_error("Renderer must not be null");
    }

    m_fontPath = resolveFontPath();
    if (m_fontPath.empty()) {
        throw std::runtime_error("Unable to locate a suitable monospace font. Set CRT_FONT_PATH to override.");
    }

    // Warm up regular font to compute metrics
    auto* font = loadFont(Style::Regular);
    int minx = 0;
    int maxx = 0;
    int miny = 0;
    int maxy = 0;
    int advance = 0;
    if (TTF_GlyphMetrics32(font, U'M', &minx, &maxx, &miny, &maxy, &advance) != 0) {
        throw std::runtime_error(std::string("Failed to query glyph metrics: ") + TTF_GetError());
    }
    m_cellWidth = advance;
    m_cellHeight = TTF_FontHeight(font);
    m_ascent = TTF_FontAscent(font);
    m_descent = -TTF_FontDescent(font);
}

FontAtlas::~FontAtlas() {
    for (auto& entry : m_glyphCache) {
        if (entry.second.texture) {
            SDL_DestroyTexture(entry.second.texture);
        }
    }

    for (auto& entry : m_fonts) {
        if (entry.second) {
            TTF_CloseFont(entry.second);
        }
    }
}

const GlyphTexture& FontAtlas::glyph(char32_t codepoint, uint8_t style) {
    auto key = std::make_tuple(codepoint, style);
    auto it = m_glyphCache.find(key);
    if (it != m_glyphCache.end()) {
        return it->second;
    }

    GlyphTexture glyphInfo;
    TTF_Font* font = loadFont(style);
    int minx = 0;
    int maxx = 0;
    int miny = 0;
    int maxy = 0;
    int advance = 0;
    if (TTF_GlyphMetrics32(font, codepoint, &minx, &maxx, &miny, &maxy, &advance) != 0) {
        if (codepoint != U'?') {
            return glyph(U'?', style);
        }
        throw std::runtime_error(std::string("Failed to query glyph metrics: ") + TTF_GetError());
    }

    glyphInfo.advance = advance;
    glyphInfo.bearingX = minx;
    glyphInfo.bearingY = maxy;

    if (!createGlyphTexture(font, codepoint, glyphInfo)) {
        if (codepoint != U'?') {
            return glyph(U'?', style);
        }
        throw std::runtime_error(std::string("Failed to render glyph: ") + TTF_GetError());
    }

    auto [insertedIt, inserted] = m_glyphCache.emplace(key, std::move(glyphInfo));
    return insertedIt->second;
}

std::string FontAtlas::resolveFontPath() const {
    if (const char* overridePath = std::getenv("CRT_FONT_PATH")) {
        if (std::filesystem::exists(overridePath)) {
            return std::string(overridePath);
        }
    }

    const std::vector<std::string> candidates = {
        "assets/fonts/JetBrainsMono-Regular.ttf",
        "/usr/share/fonts/truetype/jetbrains-mono/JetBrainsMono-Regular.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf",
        "/usr/share/fonts/truetype/ubuntu/UbuntuMono-R.ttf",
        "/usr/share/fonts/truetype/freefont/FreeMono.ttf"
    };

    for (const auto& path : candidates) {
        if (std::filesystem::exists(path)) {
            return path;
        }
    }

    return {};
}

TTF_Font* FontAtlas::loadFont(uint8_t styleFlags) {
    auto it = m_fonts.find(styleFlags);
    if (it != m_fonts.end()) {
        return it->second;
    }

    TTF_Font* font = duplicateFontWithStyle(m_fontPath, m_pixelSize, styleFlags);
    m_fonts.emplace(styleFlags, font);
    return font;
}

SDL_Texture* FontAtlas::createGlyphTexture(TTF_Font* font, char32_t codepoint, GlyphTexture& glyphInfo) {
    SDL_Color white{255, 255, 255, 255};
    SDL_Surface* surface = TTF_RenderGlyph32_Blended(font, codepoint, white);
    if (!surface) {
        return nullptr;
    }

    glyphInfo.width = surface->w;
    glyphInfo.height = surface->h;

    SDL_Texture* texture = SDL_CreateTextureFromSurface(m_renderer, surface);
    SDL_FreeSurface(surface);
    if (!texture) {
        return nullptr;
    }

    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    glyphInfo.texture = texture;
    return texture;
}

