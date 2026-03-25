#pragma once

#include <array>
#include <cstdint>

#include "IExpansionDevice.h"

class FamilyBasicKeyboard : public IExpansionDevice
{
public:
    enum class Button : uint8_t
    {
        A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
        Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,
        Return, Space, Del, Ins, Esc,
        Ctrl, RightShift, LeftShift,
        RightBracket, LeftBracket,
        Up, Down, Left, Right,
        Dot, Comma, Colon, SemiColon, Underscore, Slash, Minus, Caret,
        F1, F2, F3, F4, F5, F6, F7, F8,
        Yen, Stop, AtSign, Grph, ClrHome, Kana
    };

    using KeyStateArray = IExpansionDevice::FamilyBasicKeyboardKeys;
    static constexpr size_t INPUT_KEY_COUNT = 72;

private:
    static constexpr std::array<int, 72> KEY_MATRIX = {
        static_cast<int>(Button::F8), static_cast<int>(Button::Return), static_cast<int>(Button::LeftBracket), static_cast<int>(Button::RightBracket),
        static_cast<int>(Button::Kana), static_cast<int>(Button::RightShift), static_cast<int>(Button::Yen), static_cast<int>(Button::Stop),
        static_cast<int>(Button::F7), static_cast<int>(Button::AtSign), static_cast<int>(Button::Colon), static_cast<int>(Button::SemiColon),
        static_cast<int>(Button::Underscore), static_cast<int>(Button::Slash), static_cast<int>(Button::Minus), static_cast<int>(Button::Caret),
        static_cast<int>(Button::F6), static_cast<int>(Button::O), static_cast<int>(Button::L), static_cast<int>(Button::K),
        static_cast<int>(Button::Dot), static_cast<int>(Button::Comma), static_cast<int>(Button::P), static_cast<int>(Button::Num0),
        static_cast<int>(Button::F5), static_cast<int>(Button::I), static_cast<int>(Button::U), static_cast<int>(Button::J),
        static_cast<int>(Button::M), static_cast<int>(Button::N), static_cast<int>(Button::Num9), static_cast<int>(Button::Num8),
        static_cast<int>(Button::F4), static_cast<int>(Button::Y), static_cast<int>(Button::G), static_cast<int>(Button::H),
        static_cast<int>(Button::B), static_cast<int>(Button::V), static_cast<int>(Button::Num7), static_cast<int>(Button::Num6),
        static_cast<int>(Button::F3), static_cast<int>(Button::T), static_cast<int>(Button::R), static_cast<int>(Button::D),
        static_cast<int>(Button::F), static_cast<int>(Button::C), static_cast<int>(Button::Num5), static_cast<int>(Button::Num4),
        static_cast<int>(Button::F2), static_cast<int>(Button::W), static_cast<int>(Button::S), static_cast<int>(Button::A),
        static_cast<int>(Button::X), static_cast<int>(Button::Z), static_cast<int>(Button::E), static_cast<int>(Button::Num3),
        static_cast<int>(Button::F1), static_cast<int>(Button::Esc), static_cast<int>(Button::Q), static_cast<int>(Button::Ctrl),
        static_cast<int>(Button::LeftShift), static_cast<int>(Button::Grph), static_cast<int>(Button::Num1), static_cast<int>(Button::Num2),
        static_cast<int>(Button::ClrHome), static_cast<int>(Button::Up), static_cast<int>(Button::Right), static_cast<int>(Button::Left),
        static_cast<int>(Button::Down), static_cast<int>(Button::Space), static_cast<int>(Button::Del), static_cast<int>(Button::Ins)
    };

    uint8_t m_row = 0;
    uint8_t m_column = 0;
    bool m_enabled = false;
    KeyStateArray m_pressed = {};

    uint8_t activeKeys(uint8_t row, uint8_t column) const
    {
        if(row == 9) {
            return 0;
        }

        uint8_t result = 0;
        const size_t baseIndex = static_cast<size_t>(row) * 8 + (column ? 4 : 0);
        for(int i = 0; i < 4; ++i) {
            if(m_pressed[static_cast<size_t>(KEY_MATRIX[baseIndex + i])]) {
                result |= 0x10;
            }
            result >>= 1;
        }
        return result;
    }

public:
    void write4016(uint8_t data) override
    {
        const uint8_t prevColumn = m_column;
        m_column = static_cast<uint8_t>((data >> 1) & 0x01);
        if(prevColumn == 1 && m_column == 0) {
            m_row = static_cast<uint8_t>((m_row + 1) % 10);
        }

        if((data & 0x01) != 0) {
            m_row = 0;
        }

        m_enabled = (data & 0x04) != 0;
    }

    uint8_t read4017(bool) override
    {
        if(!m_enabled) {
            return 0x00;
        }

        return static_cast<uint8_t>(((~activeKeys(m_row, m_column)) << 1) & 0x1E);
    }

    void setFamilyBasicKeyboardKeys(const KeyStateArray& keys) override
    {
        m_pressed = keys;
    }

    void serialization(SerializationBase& s) override
    {
        SERIALIZEDATA(s, m_row);
        SERIALIZEDATA(s, m_column);
        SERIALIZEDATA(s, m_enabled);
        for(bool& pressed : m_pressed) {
            SERIALIZEDATA(s, pressed);
        }
    }
};
