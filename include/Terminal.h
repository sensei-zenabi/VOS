#pragma once

#include <SDL2/SDL.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "AnsiParser.h"
#include "CRTEffect.h"
#include "FontAtlas.h"

struct TerminalAttributes {
    SDL_Color foreground{0x80, 0xff, 0x80, 0xff};
    SDL_Color background{0x02, 0x08, 0x02, 0xff};
    bool bold{false};
    bool italic{false};
    bool underline{false};
    bool inverse{false};
};

struct TerminalCell {
    char32_t codepoint{U' '};
    SDL_Color foreground{0x80, 0xff, 0x80, 0xff};
    SDL_Color background{0x02, 0x08, 0x02, 0xff};
    bool bold{false};
    bool italic{false};
    bool underline{false};
};

class TerminalScreen {
public:
    virtual ~TerminalScreen() = default;

    virtual void putChar(char32_t codepoint) = 0;
    virtual void carriageReturn() = 0;
    virtual void lineFeed(bool newLine) = 0;
    virtual void backspace() = 0;
    virtual void tab() = 0;

    virtual void cursorUp(int amount) = 0;
    virtual void cursorDown(int amount) = 0;
    virtual void cursorForward(int amount) = 0;
    virtual void cursorBackward(int amount) = 0;
    virtual void cursorNextLine(int amount) = 0;
    virtual void cursorPrevLine(int amount) = 0;
    virtual void setCursorColumn(int column) = 0;
    virtual void setCursorPosition(int row, int column) = 0;

    virtual void eraseInDisplay(int mode) = 0;
    virtual void eraseInLine(int mode) = 0;
    virtual void insertLines(int amount) = 0;
    virtual void deleteLines(int amount) = 0;
    virtual void scrollUp(int amount) = 0;
    virtual void scrollDown(int amount) = 0;

    virtual void setAttributes(const TerminalAttributes& attributes) = 0;
    virtual TerminalAttributes& attributes() = 0;
    virtual const TerminalAttributes& defaultAttributes() const = 0;
    virtual void reset() = 0;

    virtual void saveCursor() = 0;
    virtual void restoreCursor() = 0;
    virtual void setCursorVisible(bool visible) = 0;

    virtual int columns() const = 0;
    virtual int rows() const = 0;
};

class Terminal : public TerminalScreen {
public:
    Terminal(SDL_Renderer* renderer, int width, int height);
    ~Terminal() override;

    Terminal(const Terminal&) = delete;
    Terminal& operator=(const Terminal&) = delete;

    void update();
    void render();

    void handleWindowSize(int width, int height);

    void handleTextInput(const char* text);
    void handleKeyPress(const SDL_Keysym& keysym, bool repeat);

    bool isChildAlive() const;

    // TerminalScreen implementation
    void putChar(char32_t codepoint) override;
    void carriageReturn() override;
    void lineFeed(bool newLine) override;
    void backspace() override;
    void tab() override;

    void cursorUp(int amount) override;
    void cursorDown(int amount) override;
    void cursorForward(int amount) override;
    void cursorBackward(int amount) override;
    void cursorNextLine(int amount) override;
    void cursorPrevLine(int amount) override;
    void setCursorColumn(int column) override;
    void setCursorPosition(int row, int column) override;

    void eraseInDisplay(int mode) override;
    void eraseInLine(int mode) override;
    void insertLines(int amount) override;
    void deleteLines(int amount) override;
    void scrollUp(int amount) override;
    void scrollDown(int amount) override;

    void setAttributes(const TerminalAttributes& attributes) override;
    TerminalAttributes& attributes() override;
    const TerminalAttributes& defaultAttributes() const override;
    void reset() override;

    void saveCursor() override;
    void restoreCursor() override;
    void setCursorVisible(bool visible) override;

    int columns() const override { return m_columns; }
    int rows() const override { return m_rows; }

private:
    void initializePty();
    void shutdownPty();
    void allocateGrid(int width, int height);
    void applyWinsize();
    void writeToPty(std::string_view text);
    void flushPendingInput();
    void updateCursorBlink();
    void drawCells();
    void drawCell(int row, int column, const TerminalCell& cell);
    void drawCursor();

    struct CursorState {
        int row{0};
        int column{0};
    };

    SDL_Renderer* m_renderer{nullptr};
    SDL_Texture* m_renderTarget{nullptr};
    FontAtlas m_fontAtlas;
    CRTEffect m_crtEffect;
    AnsiParser m_parser;

    int m_columns{0};
    int m_rows{0};
    std::vector<TerminalCell> m_cells;

    CursorState m_cursor{};
    CursorState m_savedCursor{};
    TerminalAttributes m_currentAttributes{};
    TerminalAttributes m_savedAttributes{};
    TerminalAttributes m_defaultAttributes{};
    bool m_cursorVisible{true};
    bool m_cursorBlinkState{true};
    uint32_t m_lastBlinkTicks{0};

    int m_width{0};
    int m_height{0};

    int m_masterFd{-1};
    pid_t m_childPid{-1};

    std::string m_pendingInput;
    bool m_needsFullRedraw{true};
};

