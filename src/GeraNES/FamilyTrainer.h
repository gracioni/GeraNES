#pragma once

#include <array>
#include <cstdint>

#include "IExpansionDevice.h"

class FamilyTrainer : public IExpansionDevice
{
private:
    std::array<bool, 12> m_pressed = {};
    bool m_sideB = false;
    uint8_t m_rowMask = 0x07;

    bool isPressedAt(int row, int col) const
    {
        const int logicalIndex = row * 4 + col;
        const int sourceIndex = m_sideB ? ((row * 4) + (3 - col)) : logicalIndex;
        return m_pressed[static_cast<size_t>(sourceIndex)];
    }

public:
    explicit FamilyTrainer(bool sideB) : m_sideB(sideB)
    {
    }

    bool isSideB() const
    {
        return m_sideB;
    }

    void write4016(uint8_t data) override
    {
        m_rowMask = static_cast<uint8_t>(data & 0x07);
    }

    uint8_t read4017(bool) override
    {
        uint8_t output = 0x1E;

        for(int col = 0; col < 4; ++col) {
            bool anyPressedInSelectedRows = false;

            for(int row = 0; row < 3; ++row) {
                const bool rowSelected = ((m_rowMask & (1 << (2 - row))) == 0);
                if(rowSelected && isPressedAt(row, col)) {
                    anyPressedInSelectedRows = true;
                    break;
                }
            }

            if(anyPressedInSelectedRows) {
                output = static_cast<uint8_t>(output & ~(1 << (4 - col)));
            }
        }

        return output;
    }

    void setPowerPadButtons(const std::array<bool, 12>& buttons) override
    {
        m_pressed = buttons;
    }

    void serialization(SerializationBase& s) override
    {
        for(bool& pressed : m_pressed) {
            SERIALIZEDATA(s, pressed);
        }
        SERIALIZEDATA(s, m_sideB);
        SERIALIZEDATA(s, m_rowMask);
    }
};
