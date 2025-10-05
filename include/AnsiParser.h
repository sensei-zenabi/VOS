#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

class TerminalScreen;

class AnsiParser {
public:
    explicit AnsiParser(TerminalScreen& screen);
    void process(std::string_view data);
    void reset();

private:
    void handleControl(uint8_t byte);
    void dispatchCsi(char finalByte);
    void executeCsiCommand(char finalByte, const std::vector<int>& parameters, bool privateMode);
    void executeOscCommand(const std::string& payload);
    void applySgr(const std::vector<int>& parameters);

    TerminalScreen& m_screen;

    enum class State {
        Ground,
        Escape,
        CsiEntry,
        CsiParam,
        OscString,
        SosPmApcString
    };

    State m_state{State::Ground};
    std::vector<int> m_parameters;
    bool m_privateMode{false};
    std::string m_intermediate;
    std::string m_oscBuffer;

    class Utf8Decoder {
    public:
        void push(uint8_t byte, const std::function<void(char32_t)>& emit);
        void reset();

    private:
        uint32_t m_codepoint{0};
        int m_expected{0};
    } m_decoder;
};

