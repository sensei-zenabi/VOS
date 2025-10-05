#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>

struct GlyphTexture {
    SDL_Texture* texture{nullptr};
    int width{0};
    int height{0};
    int advance{0};
    int bearingX{0};
    int bearingY{0};
};

class FontAtlas {
public:
    enum Style : uint8_t {
        Regular = 0,
        Bold = 1 << 0,
        Italic = 1 << 1
    };

    FontAtlas(SDL_Renderer* renderer, int pixelSize);
    ~FontAtlas();

    FontAtlas(const FontAtlas&) = delete;
    FontAtlas& operator=(const FontAtlas&) = delete;

    int cellWidth() const { return m_cellWidth; }
    int cellHeight() const { return m_cellHeight; }
    int ascent() const { return m_ascent; }
    int descent() const { return m_descent; }

    const GlyphTexture& glyph(char32_t codepoint, uint8_t style);

private:
    std::string resolveFontPath() const;
    TTF_Font* loadFont(uint8_t styleFlags);
    SDL_Texture* createGlyphTexture(TTF_Font* font, char32_t codepoint, GlyphTexture& glyphInfo);

    SDL_Renderer* m_renderer{nullptr};
    std::map<uint8_t, TTF_Font*> m_fonts;
    std::map<std::tuple<char32_t, uint8_t>, GlyphTexture> m_glyphCache;
    int m_cellWidth{0};
    int m_cellHeight{0};
    int m_ascent{0};
    int m_descent{0};
    int m_pixelSize{0};
    std::string m_fontPath;
};

