#include "AnsiParser.h"

#include "Terminal.h"

#include <SDL2/SDL.h>

#include <algorithm>
#include <charconv>
#include <cctype>
#include <stdexcept>

namespace {
constexpr int kDefaultParamValue = 1;
}

AnsiParser::AnsiParser(TerminalScreen& screen)
    : m_screen(screen) {}

void AnsiParser::process(std::string_view data) {
    for (uint8_t byte : data) {
        switch (m_state) {
        case State::Ground:
            if (byte == 0x1b) {
                m_state = State::Escape;
                m_parameters.clear();
                m_intermediate.clear();
                m_privateMode = false;
            } else if (byte < 0x20) {
                handleControl(byte);
            } else {
                m_decoder.push(byte, [this](char32_t cp) { m_screen.putChar(cp); });
            }
            break;
        case State::Escape:
            if (byte == '[') {
                m_state = State::CsiEntry;
                m_parameters.clear();
                m_intermediate.clear();
                m_privateMode = false;
            } else if (byte == ']') {
                m_state = State::OscString;
                m_oscBuffer.clear();
            } else if (byte == '7') {
                m_screen.saveCursor();
                m_state = State::Ground;
            } else if (byte == '8') {
                m_screen.restoreCursor();
                m_state = State::Ground;
            } else if (byte == 'c') {
                m_screen.reset();
                m_state = State::Ground;
            } else if (byte >= 0x20 && byte <= 0x2f) {
                m_intermediate.push_back(static_cast<char>(byte));
            } else if (byte >= 0x30 && byte <= 0x7e) {
                // Unsupported escape sequences
                m_state = State::Ground;
            }
            break;
        case State::CsiEntry:
            if (byte == '?') {
                m_privateMode = true;
            } else if (byte >= '0' && byte <= '9') {
                m_parameters.emplace_back(byte - '0');
                m_state = State::CsiParam;
            } else if (byte == ';') {
                m_parameters.emplace_back(0);
                m_state = State::CsiParam;
            } else if (byte >= 0x40 && byte <= 0x7e) {
                dispatchCsi(static_cast<char>(byte));
                m_state = State::Ground;
            }
            break;
        case State::CsiParam:
            if (byte >= '0' && byte <= '9') {
                if (m_parameters.empty()) {
                    m_parameters.emplace_back(0);
                }
                m_parameters.back() = m_parameters.back() * 10 + (byte - '0');
            } else if (byte == ';') {
                m_parameters.emplace_back(0);
            } else if (byte >= 0x40 && byte <= 0x7e) {
                dispatchCsi(static_cast<char>(byte));
                m_state = State::Ground;
            }
            break;
        case State::OscString:
            if (byte == 0x07) {
                executeOscCommand(m_oscBuffer);
                m_state = State::Ground;
            } else if (byte == 0x1b) {
                m_state = State::SosPmApcString;
            } else {
                m_oscBuffer.push_back(static_cast<char>(byte));
            }
            break;
        case State::SosPmApcString:
            if (byte == '\\') {
                executeOscCommand(m_oscBuffer);
                m_state = State::Ground;
            }
            break;
        }
    }
}

void AnsiParser::reset() {
    m_state = State::Ground;
    m_parameters.clear();
    m_intermediate.clear();
    m_privateMode = false;
    m_oscBuffer.clear();
    m_decoder.reset();
}

void AnsiParser::handleControl(uint8_t byte) {
    switch (byte) {
    case 0x07:
        m_screen.setCursorVisible(true);
        break;
    case 0x08:
        m_screen.backspace();
        break;
    case 0x09:
        m_screen.tab();
        break;
    case 0x0a:
        m_screen.lineFeed(true);
        break;
    case 0x0d:
        m_screen.carriageReturn();
        break;
    default:
        break;
    }
}

void AnsiParser::dispatchCsi(char finalByte) {
    executeCsiCommand(finalByte, m_parameters, m_privateMode);
    m_parameters.clear();
    m_privateMode = false;
}

void AnsiParser::executeCsiCommand(char finalByte, const std::vector<int>& parameters, bool privateMode) {
    auto param = [&parameters](size_t index, int defaultValue) {
        if (index < parameters.size() && parameters[index] != 0) {
            return parameters[index];
        }
        return defaultValue;
    };

    switch (finalByte) {
    case 'A':
        m_screen.cursorUp(param(0, kDefaultParamValue));
        break;
    case 'B':
        m_screen.cursorDown(param(0, kDefaultParamValue));
        break;
    case 'C':
        m_screen.cursorForward(param(0, kDefaultParamValue));
        break;
    case 'D':
        m_screen.cursorBackward(param(0, kDefaultParamValue));
        break;
    case 'E':
        m_screen.cursorNextLine(param(0, kDefaultParamValue));
        break;
    case 'F':
        m_screen.cursorPrevLine(param(0, kDefaultParamValue));
        break;
    case 'G':
        m_screen.setCursorColumn(std::max(1, param(0, 1)) - 1);
        break;
    case 'H':
    case 'f':
        m_screen.setCursorPosition(std::max(1, param(0, 1)) - 1, std::max(1, param(1, 1)) - 1);
        break;
    case 'J':
        m_screen.eraseInDisplay(parameters.empty() ? 0 : parameters[0]);
        break;
    case 'K':
        m_screen.eraseInLine(parameters.empty() ? 0 : parameters[0]);
        break;
    case 'L':
        m_screen.insertLines(param(0, 1));
        break;
    case 'M':
        m_screen.deleteLines(param(0, 1));
        break;
    case 'S':
        m_screen.scrollUp(param(0, 1));
        break;
    case 'T':
        m_screen.scrollDown(param(0, 1));
        break;
    case 'm':
        applySgr(parameters);
        break;
    case 'h':
    case 'l':
        if (privateMode) {
            if (!parameters.empty() && parameters[0] == 25) {
                m_screen.setCursorVisible(finalByte == 'h');
            }
        }
        break;
    default:
        break;
    }
}

void AnsiParser::executeOscCommand(const std::string& payload) {
    if (payload.empty()) {
        return;
    }
    // OSC commands are currently ignored but parsed to keep state consistent.
    (void)payload;
}

void AnsiParser::applySgr(const std::vector<int>& parameters) {
    auto attrs = m_screen.attributes();

    auto applyColor = [&attrs](int param, bool foreground) {
        auto setRgb = [&attrs, foreground](uint8_t r, uint8_t g, uint8_t b) {
            SDL_Color color{r, g, b, 0xff};
            if (foreground) {
                attrs.foreground = color;
            } else {
                attrs.background = color;
            }
        };

        const SDL_Color palette[16] = {
            {0x00, 0x00, 0x00, 0xff}, {0xaa, 0x00, 0x00, 0xff}, {0x00, 0xaa, 0x00, 0xff}, {0xaa, 0x55, 0x00, 0xff},
            {0x00, 0x00, 0xaa, 0xff}, {0xaa, 0x00, 0xaa, 0xff}, {0x00, 0xaa, 0xaa, 0xff}, {0xaa, 0xaa, 0xaa, 0xff},
            {0x55, 0x55, 0x55, 0xff}, {0xff, 0x55, 0x55, 0xff}, {0x55, 0xff, 0x55, 0xff}, {0xff, 0xff, 0x55, 0xff},
            {0x55, 0x55, 0xff, 0xff}, {0xff, 0x55, 0xff, 0xff}, {0x55, 0xff, 0xff, 0xff}, {0xff, 0xff, 0xff, 0xff},
        };

        if (param >= 30 && param <= 37) {
            setRgb(palette[param - 30].r, palette[param - 30].g, palette[param - 30].b);
        } else if (param >= 40 && param <= 47) {
            setRgb(palette[param - 40].r, palette[param - 40].g, palette[param - 40].b);
        } else if (param >= 90 && param <= 97) {
            setRgb(palette[param - 90 + 8].r, palette[param - 90 + 8].g, palette[param - 90 + 8].b);
        } else if (param >= 100 && param <= 107) {
            setRgb(palette[param - 100 + 8].r, palette[param - 100 + 8].g, palette[param - 100 + 8].b);
        }
    };

    if (parameters.empty()) {
        m_screen.setAttributes(m_screen.defaultAttributes());
        return;
    }

    for (size_t i = 0; i < parameters.size(); ++i) {
        int param = parameters[i];
        switch (param) {
        case 0:
            attrs = m_screen.defaultAttributes();
            break;
        case 1:
            attrs.bold = true;
            break;
        case 3:
            attrs.italic = true;
            break;
        case 4:
            attrs.underline = true;
            break;
        case 7:
            attrs.inverse = true;
            break;
        case 22:
            attrs.bold = false;
            break;
        case 23:
            attrs.italic = false;
            break;
        case 24:
            attrs.underline = false;
            break;
        case 27:
            attrs.inverse = false;
            break;
        case 38:
        case 48: {
            bool foreground = (param == 38);
            if (i + 1 < parameters.size()) {
                int mode = parameters[++i];
                if (mode == 5) {
                    if (i + 1 < parameters.size()) {
                        int colorIndex = parameters[++i];
                        auto expand = [](int idx) -> SDL_Color {
                            if (idx < 16) {
                                const SDL_Color palette[16] = {
                                    {0x00, 0x00, 0x00, 0xff}, {0xaa, 0x00, 0x00, 0xff}, {0x00, 0xaa, 0x00, 0xff}, {0xaa, 0x55, 0x00, 0xff},
                                    {0x00, 0x00, 0xaa, 0xff}, {0xaa, 0x00, 0xaa, 0xff}, {0x00, 0xaa, 0xaa, 0xff}, {0xaa, 0xaa, 0xaa, 0xff},
                                    {0x55, 0x55, 0x55, 0xff}, {0xff, 0x55, 0x55, 0xff}, {0x55, 0xff, 0x55, 0xff}, {0xff, 0xff, 0x55, 0xff},
                                    {0x55, 0x55, 0xff, 0xff}, {0xff, 0x55, 0xff, 0xff}, {0x55, 0xff, 0xff, 0xff}, {0xff, 0xff, 0xff, 0xff},
                                };
                                return palette[idx];
                            }
                            if (idx >= 16 && idx <= 231) {
                                int base = idx - 16;
                                int r = (base / 36) % 6;
                                int g = (base / 6) % 6;
                                int b = base % 6;
                                auto scale = [](int component) { return component == 0 ? 0 : component * 40 + 55; };
                                return SDL_Color{static_cast<uint8_t>(scale(r)), static_cast<uint8_t>(scale(g)), static_cast<uint8_t>(scale(b)), 0xff};
                            }
                            int level = (idx - 232) * 10 + 8;
                            level = std::clamp(level, 0, 255);
                            return SDL_Color{static_cast<uint8_t>(level), static_cast<uint8_t>(level), static_cast<uint8_t>(level), 0xff};
                        };
                        SDL_Color color = expand(colorIndex);
                        if (foreground) {
                            attrs.foreground = color;
                        } else {
                            attrs.background = color;
                        }
                    }
                } else if (mode == 2) {
                    if (i + 3 < parameters.size()) {
                        uint8_t r = static_cast<uint8_t>(std::clamp(parameters[++i], 0, 255));
                        uint8_t g = static_cast<uint8_t>(std::clamp(parameters[++i], 0, 255));
                        uint8_t b = static_cast<uint8_t>(std::clamp(parameters[++i], 0, 255));
                        SDL_Color color{r, g, b, 0xff};
                        if (foreground) {
                            attrs.foreground = color;
                        } else {
                            attrs.background = color;
                        }
                    }
                }
            }
            break;
        }
        default: {
            bool handled = false;
            if ((param >= 30 && param <= 37) || (param >= 90 && param <= 97)) {
                applyColor(param, true);
                handled = true;
            } else if ((param >= 40 && param <= 47) || (param >= 100 && param <= 107)) {
                applyColor(param, false);
                handled = true;
            }
            if (!handled) {
                // Unhandled parameter - ignore gracefully
            }
            break;
        }
        }
    }

    m_screen.setAttributes(attrs);
}

void AnsiParser::Utf8Decoder::push(uint8_t byte, const std::function<void(char32_t)>& emit) {
    if (m_expected == 0) {
        if (byte < 0x80) {
            emit(byte);
        } else if ((byte & 0xe0) == 0xc0) {
            m_codepoint = byte & 0x1f;
            m_expected = 1;
        } else if ((byte & 0xf0) == 0xe0) {
            m_codepoint = byte & 0x0f;
            m_expected = 2;
        } else if ((byte & 0xf8) == 0xf0) {
            m_codepoint = byte & 0x07;
            m_expected = 3;
        } else {
            reset();
        }
    } else {
        if ((byte & 0xc0) == 0x80) {
            m_codepoint = (m_codepoint << 6) | (byte & 0x3f);
            --m_expected;
            if (m_expected == 0) {
                emit(static_cast<char32_t>(m_codepoint));
                m_codepoint = 0;
            }
        } else {
            reset();
        }
    }
}

void AnsiParser::Utf8Decoder::reset() {
    m_codepoint = 0;
    m_expected = 0;
}

