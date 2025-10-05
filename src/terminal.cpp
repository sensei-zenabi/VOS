#include "Terminal.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdlib>
#include <string_view>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <stdexcept>
#include <system_error>

#include <fcntl.h>
#include <pty.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

namespace {
constexpr int kDefaultFontPixels = 20;
constexpr uint32_t kCursorBlinkInterval = 530;

int clampInt(int value, int minValue, int maxValue) {
    return std::max(minValue, std::min(value, maxValue));
}

TerminalCell makeEmptyCell(const TerminalAttributes& defaults) {
    TerminalCell cell;
    cell.codepoint = U' ';
    cell.foreground = defaults.foreground;
    cell.background = defaults.background;
    cell.bold = defaults.bold;
    cell.italic = defaults.italic;
    cell.underline = defaults.underline;
    return cell;
}
}

Terminal::Terminal(SDL_Renderer* renderer, int width, int height)
    : m_renderer(renderer)
    , m_fontAtlas(renderer, kDefaultFontPixels)
    , m_crtEffect()
    , m_parser(*this) {
    if (!renderer) {
        throw std::runtime_error("Renderer is null");
    }

    m_defaultAttributes = TerminalAttributes{};
    m_currentAttributes = m_defaultAttributes;
    m_savedAttributes = m_defaultAttributes;

    m_renderTarget = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, width, height);
    if (!m_renderTarget) {
        throw std::runtime_error(std::string("Failed to create render target: ") + SDL_GetError());
    }

    SDL_SetTextureBlendMode(m_renderTarget, SDL_BLENDMODE_BLEND);

    allocateGrid(width, height);
    initializePty();
    applyWinsize();
}

Terminal::~Terminal() {
    shutdownPty();
    if (m_renderTarget) {
        SDL_DestroyTexture(m_renderTarget);
    }
}

void Terminal::initializePty() {
    int master = -1;
    int slave = -1;
    if (openpty(&master, &slave, nullptr, nullptr, nullptr) == -1) {
        throw std::system_error(errno, std::generic_category(), "openpty failed");
    }

    m_masterFd = master;

    m_childPid = fork();
    if (m_childPid == -1) {
        throw std::system_error(errno, std::generic_category(), "fork failed");
    }

    if (m_childPid == 0) {
        // Child
        ::close(master);

        if (setsid() == -1) {
            _exit(1);
        }

        if (ioctl(slave, TIOCSCTTY, 0) == -1) {
            _exit(1);
        }

        dup2(slave, STDIN_FILENO);
        dup2(slave, STDOUT_FILENO);
        dup2(slave, STDERR_FILENO);
        if (slave > STDERR_FILENO) {
            ::close(slave);
        }

        struct termios term;
        if (tcgetattr(STDIN_FILENO, &term) == 0) {
            cfmakeraw(&term);
            term.c_lflag |= ECHO | ICANON;
            tcsetattr(STDIN_FILENO, TCSANOW, &term);
        }

        ::setenv("TERM", "xterm-256color", 1);
        const char* shell = std::getenv("SHELL");
        if (!shell) {
            shell = "/bin/bash";
        }
        execl(shell, shell, "-l", nullptr);
        _exit(127);
    }

    ::close(slave);

    int flags = fcntl(master, F_GETFL, 0);
    if (flags != -1) {
        fcntl(master, F_SETFL, flags | O_NONBLOCK);
    }
}

void Terminal::shutdownPty() {
    if (m_masterFd != -1) {
        ::close(m_masterFd);
        m_masterFd = -1;
    }

    if (m_childPid > 0) {
        int status = 0;
        if (waitpid(m_childPid, &status, WNOHANG) == 0) {
            kill(m_childPid, SIGHUP);
            waitpid(m_childPid, &status, 0);
        }
        m_childPid = -1;
    }
}

void Terminal::allocateGrid(int width, int height) {
    m_width = width;
    m_height = height;

    int newColumns = std::max(2, width / std::max(1, m_fontAtlas.cellWidth()));
    int newRows = std::max(2, height / std::max(1, m_fontAtlas.cellHeight()));

    std::vector<TerminalCell> newCells(newColumns * newRows, makeEmptyCell(m_defaultAttributes));

    int copyColumns = std::min(m_columns, newColumns);
    int copyRows = std::min(m_rows, newRows);
    for (int row = 0; row < copyRows; ++row) {
        for (int col = 0; col < copyColumns; ++col) {
            newCells[row * newColumns + col] = m_cells[row * m_columns + col];
        }
    }

    m_cells.swap(newCells);
    m_columns = newColumns;
    m_rows = newRows;

    m_cursor.row = clampInt(m_cursor.row, 0, m_rows - 1);
    m_cursor.column = clampInt(m_cursor.column, 0, m_columns - 1);
    m_needsFullRedraw = true;
}

void Terminal::applyWinsize() {
    if (m_masterFd == -1) {
        return;
    }

    struct winsize ws {
        static_cast<unsigned short>(m_rows),
        static_cast<unsigned short>(m_columns),
        static_cast<unsigned short>(m_height),
        static_cast<unsigned short>(m_width)
    };
    ioctl(m_masterFd, TIOCSWINSZ, &ws);
}

void Terminal::handleWindowSize(int width, int height) {
    if (width == m_width && height == m_height) {
        return;
    }

    if (m_renderTarget) {
        SDL_DestroyTexture(m_renderTarget);
    }

    m_renderTarget = SDL_CreateTexture(m_renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, width, height);
    if (!m_renderTarget) {
        throw std::runtime_error(std::string("Failed to resize render target: ") + SDL_GetError());
    }

    SDL_SetTextureBlendMode(m_renderTarget, SDL_BLENDMODE_BLEND);

    allocateGrid(width, height);
    applyWinsize();
}

void Terminal::update() {
    updateCursorBlink();
    flushPendingInput();

    if (m_masterFd == -1) {
        return;
    }

    char buffer[4096];
    while (true) {
        ssize_t bytes = ::read(m_masterFd, buffer, sizeof(buffer));
        if (bytes > 0) {
            m_parser.process(std::string_view(buffer, static_cast<size_t>(bytes)));
            m_needsFullRedraw = true;
        } else if (bytes == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            break;
        } else if (bytes == -1 && errno == EINTR) {
            continue;
        } else if (bytes == 0) {
            // EOF
            shutdownPty();
            break;
        } else {
            break;
        }
    }

    if (m_childPid > 0) {
        int status = 0;
        if (waitpid(m_childPid, &status, WNOHANG) == m_childPid) {
            m_childPid = -1;
        }
    }
}

void Terminal::render() {
    if (!m_renderTarget) {
        return;
    }

    SDL_SetRenderTarget(m_renderer, m_renderTarget);
    SDL_SetRenderDrawColor(m_renderer, m_defaultAttributes.background.r, m_defaultAttributes.background.g, m_defaultAttributes.background.b, 255);
    SDL_RenderClear(m_renderer);

    drawCells();
    drawCursor();

    SDL_SetRenderTarget(m_renderer, nullptr);
    m_crtEffect.apply(m_renderer, m_renderTarget);
    SDL_RenderPresent(m_renderer);

    m_needsFullRedraw = false;
}

void Terminal::updateCursorBlink() {
    uint32_t ticks = SDL_GetTicks();
    if (ticks - m_lastBlinkTicks >= kCursorBlinkInterval) {
        m_cursorBlinkState = !m_cursorBlinkState;
        m_lastBlinkTicks = ticks;
    }
}

void Terminal::drawCells() {
    const int cellWidth = m_fontAtlas.cellWidth();
    const int cellHeight = m_fontAtlas.cellHeight();

    for (int row = 0; row < m_rows; ++row) {
        for (int col = 0; col < m_columns; ++col) {
            const auto& cell = m_cells[row * m_columns + col];
            drawCell(row, col, cell);
        }
    }
}

void Terminal::drawCell(int row, int column, const TerminalCell& cell) {
    const int cellWidth = m_fontAtlas.cellWidth();
    const int cellHeight = m_fontAtlas.cellHeight();

    SDL_Rect rect{column * cellWidth, row * cellHeight, cellWidth, cellHeight};
    SDL_SetRenderDrawColor(m_renderer, cell.background.r, cell.background.g, cell.background.b, 255);
    SDL_RenderFillRect(m_renderer, &rect);

    if (cell.codepoint == U' ') {
        return;
    }

    uint8_t style = FontAtlas::Style::Regular;
    if (cell.bold) {
        style |= FontAtlas::Style::Bold;
    }
    if (cell.italic) {
        style |= FontAtlas::Style::Italic;
    }

    const auto& glyph = m_fontAtlas.glyph(cell.codepoint, style);
    if (glyph.width == 0 || glyph.height == 0) {
        return;
    }
    int baseline = m_fontAtlas.ascent();
    SDL_Rect dstRect{
        rect.x + glyph.bearingX,
        rect.y + baseline - glyph.bearingY,
        glyph.width,
        glyph.height};
    if (dstRect.x < rect.x - cellWidth) {
        dstRect.x = rect.x - cellWidth;
    }
    if (dstRect.x + dstRect.w > rect.x + cellWidth) {
        dstRect.x = rect.x + cellWidth - dstRect.w;
    }
    if (dstRect.y < rect.y - cellHeight) {
        dstRect.y = rect.y - cellHeight;
    }
    if (dstRect.y + dstRect.h > rect.y + cellHeight) {
        dstRect.y = rect.y + cellHeight - dstRect.h;
    }
    SDL_SetTextureColorMod(glyph.texture, cell.foreground.r, cell.foreground.g, cell.foreground.b);
    SDL_SetTextureAlphaMod(glyph.texture, 255);
    SDL_RenderCopy(m_renderer, glyph.texture, nullptr, &dstRect);

    if (cell.underline) {
        SDL_SetRenderDrawColor(m_renderer, cell.foreground.r, cell.foreground.g, cell.foreground.b, 255);
        SDL_RenderDrawLine(m_renderer, rect.x, rect.y + cellHeight - 2, rect.x + cellWidth, rect.y + cellHeight - 2);
    }
}

void Terminal::drawCursor() {
    if (!m_cursorVisible || !m_cursorBlinkState) {
        return;
    }

    const int cellWidth = m_fontAtlas.cellWidth();
    const int cellHeight = m_fontAtlas.cellHeight();
    SDL_Rect rect{m_cursor.column * cellWidth, m_cursor.row * cellHeight, cellWidth, cellHeight};
    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_ADD);
    SDL_SetRenderDrawColor(m_renderer, 0, 255, 0, 80);
    SDL_RenderFillRect(m_renderer, &rect);
    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
}

void Terminal::handleTextInput(const char* text) {
    if (!text) {
        return;
    }
    writeToPty(text);
}

void Terminal::handleKeyPress(const SDL_Keysym& keysym, bool repeat) {
    if (repeat) {
        return;
    }

    auto send = [this](std::string_view data) { writeToPty(data); };

    switch (keysym.sym) {
    case SDLK_RETURN:
        send("\r");
        break;
    case SDLK_BACKSPACE:
        send("\x7f");
        break;
    case SDLK_TAB:
        send("\t");
        break;
    case SDLK_ESCAPE:
        send("\x1b");
        break;
    case SDLK_UP:
        send("\x1b[A");
        break;
    case SDLK_DOWN:
        send("\x1b[B");
        break;
    case SDLK_RIGHT:
        send("\x1b[C");
        break;
    case SDLK_LEFT:
        send("\x1b[D");
        break;
    case SDLK_PAGEUP:
        send("\x1b[5~");
        break;
    case SDLK_PAGEDOWN:
        send("\x1b[6~");
        break;
    case SDLK_HOME:
        send("\x1b[H");
        break;
    case SDLK_END:
        send("\x1b[F");
        break;
    case SDLK_DELETE:
        send("\x1b[3~");
        break;
    default:
        if (keysym.mod & KMOD_CTRL) {
            char base = static_cast<char>(std::tolower(static_cast<unsigned char>(keysym.sym)));
            if (base >= 'a' && base <= 'z') {
                char control = static_cast<char>(base - 'a' + 1);
                send(std::string_view(&control, 1));
            } else if (keysym.sym == SDLK_SPACE) {
                char nul = 0;
                send(std::string_view(&nul, 1));
            }
        }
        break;
    }
}

bool Terminal::isChildAlive() const {
    return m_childPid > 0;
}

void Terminal::writeToPty(std::string_view text) {
    if (m_masterFd == -1 || text.empty()) {
        return;
    }

    size_t offset = 0;
    while (offset < text.size()) {
        ssize_t written = ::write(m_masterFd, text.data() + offset, text.size() - offset);
        if (written > 0) {
            offset += static_cast<size_t>(written);
        } else if (written == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            m_pendingInput.append(text.substr(offset));
            return;
        } else if (written == -1 && errno == EINTR) {
            continue;
        } else {
            return;
        }
    }
}

void Terminal::flushPendingInput() {
    if (m_pendingInput.empty() || m_masterFd == -1) {
        return;
    }

    std::string pending = std::move(m_pendingInput);
    size_t offset = 0;
    while (offset < pending.size()) {
        ssize_t written = ::write(m_masterFd, pending.data() + offset, pending.size() - offset);
        if (written > 0) {
            offset += static_cast<size_t>(written);
        } else if (written == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            m_pendingInput.assign(pending.begin() + static_cast<std::ptrdiff_t>(offset), pending.end());
            return;
        } else if (written == -1 && errno == EINTR) {
            continue;
        } else {
            return;
        }
    }
}

void Terminal::putChar(char32_t codepoint) {
    if (codepoint == U'\r') {
        carriageReturn();
        return;
    }
    if (codepoint == U'\n') {
        lineFeed(true);
        return;
    }

    auto& cell = m_cells[m_cursor.row * m_columns + m_cursor.column];
    cell.codepoint = codepoint;
    cell.bold = m_currentAttributes.bold;
    cell.italic = m_currentAttributes.italic;
    cell.underline = m_currentAttributes.underline;
    cell.foreground = m_currentAttributes.foreground;
    cell.background = m_currentAttributes.background;
    if (m_currentAttributes.inverse) {
        std::swap(cell.foreground, cell.background);
    }

    if (m_cursor.column + 1 >= m_columns) {
        carriageReturn();
        lineFeed(true);
    } else {
        ++m_cursor.column;
    }
    m_needsFullRedraw = true;
}

void Terminal::carriageReturn() {
    m_cursor.column = 0;
    m_needsFullRedraw = true;
}

void Terminal::lineFeed(bool newLine) {
    if (newLine) {
        if (m_cursor.row == m_rows - 1) {
            scrollUp(1);
        } else {
            ++m_cursor.row;
        }
        m_needsFullRedraw = true;
    }
}

void Terminal::backspace() {
    if (m_cursor.column > 0) {
        --m_cursor.column;
        m_needsFullRedraw = true;
    }
}

void Terminal::tab() {
    int next = ((m_cursor.column / 8) + 1) * 8;
    m_cursor.column = std::min(next, m_columns - 1);
    m_needsFullRedraw = true;
}

void Terminal::cursorUp(int amount) {
    m_cursor.row = clampInt(m_cursor.row - amount, 0, m_rows - 1);
    m_needsFullRedraw = true;
}

void Terminal::cursorDown(int amount) {
    m_cursor.row = clampInt(m_cursor.row + amount, 0, m_rows - 1);
    m_needsFullRedraw = true;
}

void Terminal::cursorForward(int amount) {
    m_cursor.column = clampInt(m_cursor.column + amount, 0, m_columns - 1);
    m_needsFullRedraw = true;
}

void Terminal::cursorBackward(int amount) {
    m_cursor.column = clampInt(m_cursor.column - amount, 0, m_columns - 1);
    m_needsFullRedraw = true;
}

void Terminal::cursorNextLine(int amount) {
    cursorDown(amount);
    carriageReturn();
}

void Terminal::cursorPrevLine(int amount) {
    cursorUp(amount);
    carriageReturn();
}

void Terminal::setCursorColumn(int column) {
    m_cursor.column = clampInt(column, 0, m_columns - 1);
    m_needsFullRedraw = true;
}

void Terminal::setCursorPosition(int row, int column) {
    m_cursor.row = clampInt(row, 0, m_rows - 1);
    m_cursor.column = clampInt(column, 0, m_columns - 1);
    m_needsFullRedraw = true;
}

void Terminal::eraseInDisplay(int mode) {
    switch (mode) {
    case 0: // from cursor to end
        eraseInLine(0);
        for (int row = m_cursor.row + 1; row < m_rows; ++row) {
            for (int col = 0; col < m_columns; ++col) {
                m_cells[row * m_columns + col] = makeEmptyCell(m_defaultAttributes);
            }
        }
        break;
    case 1: // from start to cursor
        for (int row = 0; row < m_cursor.row; ++row) {
            for (int col = 0; col < m_columns; ++col) {
                m_cells[row * m_columns + col] = makeEmptyCell(m_defaultAttributes);
            }
        }
        for (int col = 0; col <= m_cursor.column; ++col) {
            m_cells[m_cursor.row * m_columns + col] = makeEmptyCell(m_defaultAttributes);
        }
        break;
    case 2: // entire screen
    default:
        std::fill(m_cells.begin(), m_cells.end(), makeEmptyCell(m_defaultAttributes));
        break;
    }
    m_needsFullRedraw = true;
}

void Terminal::eraseInLine(int mode) {
    switch (mode) {
    case 0: // from cursor to end
        for (int col = m_cursor.column; col < m_columns; ++col) {
            m_cells[m_cursor.row * m_columns + col] = makeEmptyCell(m_defaultAttributes);
        }
        break;
    case 1: // start to cursor
        for (int col = 0; col <= m_cursor.column; ++col) {
            m_cells[m_cursor.row * m_columns + col] = makeEmptyCell(m_defaultAttributes);
        }
        break;
    case 2: // entire line
    default:
        for (int col = 0; col < m_columns; ++col) {
            m_cells[m_cursor.row * m_columns + col] = makeEmptyCell(m_defaultAttributes);
        }
        break;
    }
    m_needsFullRedraw = true;
}

void Terminal::insertLines(int amount) {
    amount = std::clamp(amount, 0, m_rows - m_cursor.row);
    if (amount == 0) {
        return;
    }

    for (int row = m_rows - 1; row >= m_cursor.row + amount; --row) {
        for (int col = 0; col < m_columns; ++col) {
            m_cells[row * m_columns + col] = m_cells[(row - amount) * m_columns + col];
        }
    }

    for (int row = m_cursor.row; row < m_cursor.row + amount; ++row) {
        for (int col = 0; col < m_columns; ++col) {
            m_cells[row * m_columns + col] = makeEmptyCell(m_defaultAttributes);
        }
    }
    m_needsFullRedraw = true;
}

void Terminal::deleteLines(int amount) {
    amount = std::clamp(amount, 0, m_rows - m_cursor.row);
    if (amount == 0) {
        return;
    }

    for (int row = m_cursor.row; row < m_rows - amount; ++row) {
        for (int col = 0; col < m_columns; ++col) {
            m_cells[row * m_columns + col] = m_cells[(row + amount) * m_columns + col];
        }
    }

    for (int row = m_rows - amount; row < m_rows; ++row) {
        for (int col = 0; col < m_columns; ++col) {
            m_cells[row * m_columns + col] = makeEmptyCell(m_defaultAttributes);
        }
    }
    m_needsFullRedraw = true;
}

void Terminal::scrollUp(int amount) {
    amount = std::clamp(amount, 0, m_rows);
    if (amount == 0) {
        return;
    }

    for (int row = 0; row < m_rows - amount; ++row) {
        for (int col = 0; col < m_columns; ++col) {
            m_cells[row * m_columns + col] = m_cells[(row + amount) * m_columns + col];
        }
    }

    for (int row = m_rows - amount; row < m_rows; ++row) {
        for (int col = 0; col < m_columns; ++col) {
            m_cells[row * m_columns + col] = makeEmptyCell(m_defaultAttributes);
        }
    }
    m_needsFullRedraw = true;
}

void Terminal::scrollDown(int amount) {
    amount = std::clamp(amount, 0, m_rows);
    if (amount == 0) {
        return;
    }

    for (int row = m_rows - 1; row >= amount; --row) {
        for (int col = 0; col < m_columns; ++col) {
            m_cells[row * m_columns + col] = m_cells[(row - amount) * m_columns + col];
        }
    }

    for (int row = 0; row < amount; ++row) {
        for (int col = 0; col < m_columns; ++col) {
            m_cells[row * m_columns + col] = makeEmptyCell(m_defaultAttributes);
        }
    }
    m_needsFullRedraw = true;
}

void Terminal::setAttributes(const TerminalAttributes& attributes) {
    m_currentAttributes = attributes;
    m_needsFullRedraw = true;
}

TerminalAttributes& Terminal::attributes() {
    return m_currentAttributes;
}

const TerminalAttributes& Terminal::defaultAttributes() const {
    return m_defaultAttributes;
}

void Terminal::reset() {
    m_currentAttributes = m_defaultAttributes;
    m_cursor = {};
    m_savedCursor = {};
    m_savedAttributes = m_defaultAttributes;
    std::fill(m_cells.begin(), m_cells.end(), makeEmptyCell(m_defaultAttributes));
    m_parser.reset();
    m_needsFullRedraw = true;
}

void Terminal::saveCursor() {
    m_savedCursor = m_cursor;
    m_savedAttributes = m_currentAttributes;
}

void Terminal::restoreCursor() {
    m_cursor = m_savedCursor;
    m_currentAttributes = m_savedAttributes;
    m_needsFullRedraw = true;
}

void Terminal::setCursorVisible(bool visible) {
    m_cursorVisible = visible;
    m_needsFullRedraw = true;
}

