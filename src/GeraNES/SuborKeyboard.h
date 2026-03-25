#pragma once

#include <array>
#include <cstdint>

#include "IExpansionDevice.h"

class SuborKeyboard : public IExpansionDevice
{
public:
    static constexpr size_t INPUT_KEY_COUNT = 99;

    enum class Button : uint8_t
    {
        A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
        Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,
        F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
        Numpad0, Numpad1, Numpad2, Numpad3, Numpad4, Numpad5, Numpad6, Numpad7, Numpad8, Numpad9,
        NumpadEnter, NumpadDot, NumpadPlus, NumpadMultiply, NumpadDivide, NumpadMinus, NumLock,
        Comma, Dot, SemiColon, Apostrophe,
        Slash, Backslash,
        Equal, Minus, Grave,
        LeftBracket, RightBracket,
        CapsLock, Pause,
        Ctrl, Shift, Alt,
        Space, Backspace, Tab, Esc, Enter,
        End, Home,
        Ins, Delete,
        PageUp, PageDown,
        Up, Down, Left, Right,
        Unknown1, Unknown2, Unknown3, None
    };

    using KeyStateArray = IExpansionDevice::SuborKeyboardKeys;

private:
    static constexpr std::array<int, 104> KEYBOARD_MATRIX = {
        static_cast<int>(Button::Num4), static_cast<int>(Button::G), static_cast<int>(Button::F), static_cast<int>(Button::C), static_cast<int>(Button::F2), static_cast<int>(Button::E), static_cast<int>(Button::Num5), static_cast<int>(Button::V),
        static_cast<int>(Button::Num2), static_cast<int>(Button::D), static_cast<int>(Button::S), static_cast<int>(Button::End), static_cast<int>(Button::F1), static_cast<int>(Button::W), static_cast<int>(Button::Num3), static_cast<int>(Button::X),
        static_cast<int>(Button::Ins), static_cast<int>(Button::Backspace), static_cast<int>(Button::PageDown), static_cast<int>(Button::Right), static_cast<int>(Button::F8), static_cast<int>(Button::PageUp), static_cast<int>(Button::Delete), static_cast<int>(Button::Home),
        static_cast<int>(Button::Num9), static_cast<int>(Button::I), static_cast<int>(Button::L), static_cast<int>(Button::Comma), static_cast<int>(Button::F5), static_cast<int>(Button::O), static_cast<int>(Button::Num0), static_cast<int>(Button::Dot),
        static_cast<int>(Button::RightBracket), static_cast<int>(Button::Enter), static_cast<int>(Button::Up), static_cast<int>(Button::Left), static_cast<int>(Button::F7), static_cast<int>(Button::LeftBracket), static_cast<int>(Button::Backslash), static_cast<int>(Button::Down),
        static_cast<int>(Button::Q), static_cast<int>(Button::CapsLock), static_cast<int>(Button::Z), static_cast<int>(Button::Tab), static_cast<int>(Button::Esc), static_cast<int>(Button::A), static_cast<int>(Button::Num1), static_cast<int>(Button::Ctrl),
        static_cast<int>(Button::Num7), static_cast<int>(Button::Y), static_cast<int>(Button::K), static_cast<int>(Button::M), static_cast<int>(Button::F4), static_cast<int>(Button::U), static_cast<int>(Button::Num8), static_cast<int>(Button::J),
        static_cast<int>(Button::Minus), static_cast<int>(Button::SemiColon), static_cast<int>(Button::Apostrophe), static_cast<int>(Button::Slash), static_cast<int>(Button::F6), static_cast<int>(Button::P), static_cast<int>(Button::Equal), static_cast<int>(Button::Shift),
        static_cast<int>(Button::T), static_cast<int>(Button::H), static_cast<int>(Button::N), static_cast<int>(Button::Space), static_cast<int>(Button::F3), static_cast<int>(Button::R), static_cast<int>(Button::Num6), static_cast<int>(Button::B),
        static_cast<int>(Button::Numpad6), static_cast<int>(Button::NumpadEnter), static_cast<int>(Button::Numpad4), static_cast<int>(Button::Numpad8), static_cast<int>(Button::None), static_cast<int>(Button::Unknown1), static_cast<int>(Button::Unknown2), static_cast<int>(Button::Unknown3),
        static_cast<int>(Button::Alt), static_cast<int>(Button::Numpad4), static_cast<int>(Button::Numpad7), static_cast<int>(Button::F11), static_cast<int>(Button::F12), static_cast<int>(Button::Numpad1), static_cast<int>(Button::Numpad2), static_cast<int>(Button::Numpad8),
        static_cast<int>(Button::NumpadMinus), static_cast<int>(Button::NumpadPlus), static_cast<int>(Button::NumpadMultiply), static_cast<int>(Button::Numpad9), static_cast<int>(Button::F10), static_cast<int>(Button::Numpad5), static_cast<int>(Button::NumpadDivide), static_cast<int>(Button::NumLock),
        static_cast<int>(Button::Grave), static_cast<int>(Button::Numpad6), static_cast<int>(Button::Pause), static_cast<int>(Button::Space), static_cast<int>(Button::F9), static_cast<int>(Button::Numpad3), static_cast<int>(Button::NumpadDot), static_cast<int>(Button::Numpad0)
    };

    uint8_t m_row = 0;
    uint8_t m_column = 0;
    bool m_enabled = false;
    bool m_strobe = false;
    KeyStateArray m_pressed = {};

    uint8_t activeKeys(uint8_t row, uint8_t column) const
    {
        uint8_t result = 0;
        const size_t baseIndex = static_cast<size_t>(row) * 8 + (column ? 4 : 0);

        for(int i = 0; i < 4; ++i) {
            const int keyIndex = KEYBOARD_MATRIX[baseIndex + i];
            if(keyIndex >= 0 && static_cast<size_t>(keyIndex) < INPUT_KEY_COUNT && m_pressed[static_cast<size_t>(keyIndex)]) {
                result |= static_cast<uint8_t>(1u << i);
            }
        }

        if(row == 9 && column != 0) {
            result |= 0x01;
        }

        return result;
    }

    void resetScanState()
    {
        m_row = 0;
        m_column = 0;
    }

public:
    void write4016(uint8_t data) override
    {
        const bool prevStrobe = m_strobe;
        const uint8_t prevColumn = m_column;

        m_strobe = (data & 0x01) != 0;
        if(prevStrobe && !m_strobe) {
            resetScanState();
        }

        m_column = static_cast<uint8_t>((data >> 1) & 0x01);
        m_enabled = (data & 0x04) != 0;

        if(m_enabled && prevColumn == 1 && m_column == 0) {
            m_row = static_cast<uint8_t>((m_row + 1) % 13);
        }
    }

    uint8_t read4017(bool) override
    {
        if(!m_enabled) {
            return 0x1E;
        }

        return static_cast<uint8_t>(((~activeKeys(m_row, m_column)) << 1) & 0x1E);
    }

    void setSuborKeyboardKeys(const KeyStateArray& keys) override
    {
        m_pressed = keys;
    }

    void serialization(SerializationBase& s) override
    {
        SERIALIZEDATA(s, m_row);
        SERIALIZEDATA(s, m_column);
        SERIALIZEDATA(s, m_enabled);
        SERIALIZEDATA(s, m_strobe);
        for(bool& pressed : m_pressed) {
            SERIALIZEDATA(s, pressed);
        }
    }
};
