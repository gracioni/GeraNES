#pragma once

#include "BaseMapper.h"

// J.Y. Company ASIC (iNES Mapper 90)
class Mapper090 : public BaseMapper
{
private:
    enum class IrqSource : uint8_t
    {
        CPU_CLOCK = 0,
        PPU_A12_RISE = 1,
        PPU_READ = 2,
        CPU_WRITE = 3
    };

    uint8_t m_prgReg[4] = {0, 0, 0, 0};
    uint8_t m_chrLowReg[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    uint8_t m_chrHighReg[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    uint8_t m_chrLatch[2] = {0, 4};

    uint8_t m_prgMode = 0;
    bool m_enablePrgAt6000 = false;

    uint8_t m_chrMode = 0;
    bool m_chrBlockMode = false;
    uint8_t m_chrBlock = 0;
    bool m_mirrorChr = false;

    uint8_t m_mirroringReg = 0;
    bool m_advancedNtControl = false;
    bool m_disableNtRam = false;

    uint8_t m_ntRamSelectBit = 0;
    uint8_t m_ntLowReg[4] = {0, 0, 0, 0};
    uint8_t m_ntHighReg[4] = {0, 0, 0, 0};

    bool m_irqEnabled = false;
    IrqSource m_irqSource = IrqSource::CPU_CLOCK;
    uint8_t m_irqCountDirection = 0;
    bool m_irqFunkyMode = false;
    uint8_t m_irqFunkyModeReg = 0;
    bool m_irqSmallPrescaler = false;
    uint8_t m_irqPrescaler = 0;
    uint8_t m_irqCounter = 0;
    uint8_t m_irqXorReg = 0;
    bool m_interruptFlag = false;
    bool m_cpuWriteCycle = false;

    uint8_t m_multiplyValue1 = 0;
    uint8_t m_multiplyValue2 = 0;
    uint8_t m_regRamValue = 0;

    uint16_t m_lastPpuAddr = 0;

    uint8_t m_prgMask = 0; // 8K
    uint16_t m_chrMask = 0; // 1K
    uint16_t m_prgBankCount = 0; // 8K
    uint16_t m_chrBankCount = 0; // 1K

    uint8_t m_prgPage[4] = {0, 0, 0, 0}; // 8K pages at $8000-$FFFF
    uint16_t m_chrPage[8] = {0, 0, 0, 0, 0, 0, 0, 0}; // 1K pages at $0000-$1FFF
    uint16_t m_prg6000Page = 0; // 8K page mapped at $6000-$7FFF when enabled

    static uint16_t calculateMask16(int nBanks)
    {
        uint16_t mask = 0;
        int n = nBanks - 1;

        while(n > 0) {
            mask <<= 1;
            mask |= 1;
            n >>= 1;
        }

        return mask;
    }

    GERANES_INLINE uint16_t mapPrgBank(uint16_t bank) const
    {
        if(m_prgBankCount == 0) return 0;
        if((m_prgBankCount & (m_prgBankCount - 1)) == 0) {
            return static_cast<uint16_t>(bank & m_prgMask);
        }
        return static_cast<uint16_t>(bank % m_prgBankCount);
    }

    GERANES_INLINE uint16_t mapChrBank(uint16_t bank) const
    {
        if(m_chrBankCount == 0) return 0;
        if((m_chrBankCount & (m_chrBankCount - 1)) == 0) {
            return static_cast<uint16_t>(bank & m_chrMask);
        }
        return static_cast<uint16_t>(bank % m_chrBankCount);
    }

    GERANES_INLINE uint8_t invertPrgBits(uint8_t prgReg, bool needInvert) const
    {
        if(!needInvert) return prgReg;
        return static_cast<uint8_t>(
            ((prgReg & 0x01) << 6) |
            ((prgReg & 0x02) << 4) |
            ((prgReg & 0x04) << 2) |
            ((prgReg & 0x10) >> 2) |
            ((prgReg & 0x20) >> 4) |
            ((prgReg & 0x40) >> 6)
        );
    }

    GERANES_INLINE uint16_t getChrReg(int index) const
    {
        if(m_chrMode >= 2 && m_mirrorChr && (index == 2 || index == 3)) {
            index -= 2;
        }

        if(m_chrBlockMode) {
            uint8_t mask = 0;
            uint8_t shift = 0;

            switch(m_chrMode) {
            default:
            case 0: mask = 0x1F; shift = 5; break;
            case 1: mask = 0x3F; shift = 6; break;
            case 2: mask = 0x7F; shift = 7; break;
            case 3: mask = 0xFF; shift = 8; break;
            }

            return static_cast<uint16_t>((m_chrLowReg[index] & mask) | (m_chrBlock << shift));
        }

        return static_cast<uint16_t>(m_chrLowReg[index]) | (static_cast<uint16_t>(m_chrHighReg[index]) << 8);
    }

    void updatePrgState()
    {
        const bool invertBits = (m_prgMode & 0x03) == 0x03;
        const uint8_t prg0 = invertPrgBits(m_prgReg[0], invertBits);
        const uint8_t prg1 = invertPrgBits(m_prgReg[1], invertBits);
        const uint8_t prg2 = invertPrgBits(m_prgReg[2], invertBits);
        const uint8_t prg3 = invertPrgBits(m_prgReg[3], invertBits);
        switch(m_prgMode & 0x03) {
        case 0:
        {
            const uint16_t start = (m_prgMode & 0x04) ? static_cast<uint16_t>(prg3) : 0x3C;
            m_prgPage[0] = static_cast<uint8_t>(start + 0);
            m_prgPage[1] = static_cast<uint8_t>(start + 1);
            m_prgPage[2] = static_cast<uint8_t>(start + 2);
            m_prgPage[3] = static_cast<uint8_t>(start + 3);
            m_prg6000Page = static_cast<uint16_t>(prg3 * 4 + 3);
            break;
        }
        case 1:
        {
            const uint16_t first16k = static_cast<uint16_t>(prg1 << 1);
            const uint16_t last16k = (m_prgMode & 0x04) ? static_cast<uint16_t>(prg3 << 1) : 0x3E;
            m_prgPage[0] = static_cast<uint8_t>(first16k + 0);
            m_prgPage[1] = static_cast<uint8_t>(first16k + 1);
            m_prgPage[2] = static_cast<uint8_t>(last16k + 0);
            m_prgPage[3] = static_cast<uint8_t>(last16k + 1);
            m_prg6000Page = static_cast<uint16_t>(prg3 * 2 + 1);
            break;
        }
        case 2:
        default:
            m_prgPage[0] = prg0;
            m_prgPage[1] = prg1;
            m_prgPage[2] = prg2;
            m_prgPage[3] = (m_prgMode & 0x04) ? prg3 : 0x3F;
            m_prg6000Page = static_cast<uint16_t>(prg3);
            break;
        }

    }

    void updateChrState()
    {
        const uint16_t chr0 = getChrReg(0);
        const uint16_t chr1 = getChrReg(1);
        const uint16_t chr2 = getChrReg(2);
        const uint16_t chr3 = getChrReg(3);
        const uint16_t chr4 = getChrReg(4);
        const uint16_t chr5 = getChrReg(5);
        const uint16_t chr6 = getChrReg(6);
        const uint16_t chr7 = getChrReg(7);

        switch(m_chrMode) {
        case 0:
        {
            const uint16_t start = static_cast<uint16_t>(chr0 << 3);
            for(int i = 0; i < 8; i++) {
                m_chrPage[i] = static_cast<uint16_t>(start + i);
            }
            break;
        }
        case 1:
        {
            const uint16_t left = static_cast<uint16_t>(getChrReg(m_chrLatch[0] & 0x07) << 2);
            const uint16_t right = static_cast<uint16_t>(getChrReg(m_chrLatch[1] & 0x07) << 2);
            m_chrPage[0] = static_cast<uint16_t>(left + 0);
            m_chrPage[1] = static_cast<uint16_t>(left + 1);
            m_chrPage[2] = static_cast<uint16_t>(left + 2);
            m_chrPage[3] = static_cast<uint16_t>(left + 3);
            m_chrPage[4] = static_cast<uint16_t>(right + 0);
            m_chrPage[5] = static_cast<uint16_t>(right + 1);
            m_chrPage[6] = static_cast<uint16_t>(right + 2);
            m_chrPage[7] = static_cast<uint16_t>(right + 3);
            break;
        }
        case 2:
            m_chrPage[0] = static_cast<uint16_t>((chr0 << 1) + 0);
            m_chrPage[1] = static_cast<uint16_t>((chr0 << 1) + 1);
            m_chrPage[2] = static_cast<uint16_t>((chr2 << 1) + 0);
            m_chrPage[3] = static_cast<uint16_t>((chr2 << 1) + 1);
            m_chrPage[4] = static_cast<uint16_t>((chr4 << 1) + 0);
            m_chrPage[5] = static_cast<uint16_t>((chr4 << 1) + 1);
            m_chrPage[6] = static_cast<uint16_t>((chr6 << 1) + 0);
            m_chrPage[7] = static_cast<uint16_t>((chr6 << 1) + 1);
            break;
        case 3:
        default:
            m_chrPage[0] = chr0;
            m_chrPage[1] = chr1;
            m_chrPage[2] = chr2;
            m_chrPage[3] = chr3;
            m_chrPage[4] = chr4;
            m_chrPage[5] = chr5;
            m_chrPage[6] = chr6;
            m_chrPage[7] = chr7;
            break;
        }
    }

    GERANES_INLINE void updateState()
    {
        updatePrgState();
        updateChrState();
    }

    GERANES_INLINE void tickIrqCounter()
    {
        bool clockIrqCounter = false;
        const uint8_t mask = m_irqSmallPrescaler ? 0x07 : 0xFF;
        uint8_t prescaler = static_cast<uint8_t>(m_irqPrescaler & mask);

        if(m_irqCountDirection == 0x01) {
            ++prescaler;
            if((prescaler & mask) == 0) {
                clockIrqCounter = true;
            }
        }
        else if(m_irqCountDirection == 0x02) {
            if(--prescaler == 0) {
                clockIrqCounter = true;
            }
        }

        m_irqPrescaler = static_cast<uint8_t>((m_irqPrescaler & ~mask) | (prescaler & mask));

        if(!clockIrqCounter) return;

        if(m_irqCountDirection == 0x01) {
            ++m_irqCounter;
            if(m_irqCounter == 0 && m_irqEnabled) {
                m_interruptFlag = true;
            }
        }
        else if(m_irqCountDirection == 0x02) {
            --m_irqCounter;
            if(m_irqCounter == 0xFF && m_irqEnabled) {
                m_interruptFlag = true;
            }
        }
    }

public:
    Mapper090(ICartridgeData& cd) : BaseMapper(cd)
    {
        m_prgBankCount = static_cast<uint16_t>(cd.numberOfPRGBanks<BankSize::B8K>());
        m_prgMask = calculateMask(cd.numberOfPRGBanks<BankSize::B8K>());

        if(hasChrRam()) {
            m_chrBankCount = static_cast<uint16_t>(cd.chrRamSize() / static_cast<int>(BankSize::B1K));
            m_chrMask = calculateMask16(m_chrBankCount);
        }
        else {
            m_chrBankCount = static_cast<uint16_t>(cd.numberOfCHRBanks<BankSize::B1K>());
            m_chrMask = calculateMask16(m_chrBankCount);
        }
    }

    GERANES_HOT void writeMapperRegister(int addr, uint8_t data) override
    {
        switch(addr & 0x1803) {
        case 0x1800: m_multiplyValue1 = data; break;
        case 0x1801: m_multiplyValue2 = data; break;
        case 0x1803: m_regRamValue = data; break;
        default: break;
        }
    }

    GERANES_HOT uint8_t readMapperRegister(int addr, uint8_t openBusData) override
    {
        switch(addr & 0x1803) {
        case 0x1000: return 0; // DIP switches
        case 0x1800: return static_cast<uint8_t>((m_multiplyValue1 * m_multiplyValue2) & 0xFF);
        case 0x1801: return static_cast<uint8_t>(((m_multiplyValue1 * m_multiplyValue2) >> 8) & 0xFF);
        case 0x1803: return m_regRamValue;
        default: return openBusData;
        }
    }

    GERANES_HOT void writePrg(int addr, uint8_t data) override
    {
        switch(addr & 0x7007) {
        case 0x0000: case 0x0001: case 0x0002: case 0x0003:
        case 0x0004: case 0x0005: case 0x0006: case 0x0007:
            m_prgReg[addr & 0x03] = static_cast<uint8_t>(data & 0x7F);
            break;

        case 0x1000: case 0x1001: case 0x1002: case 0x1003:
        case 0x1004: case 0x1005: case 0x1006: case 0x1007:
            m_chrLowReg[addr & 0x07] = data;
            break;

        case 0x2000: case 0x2001: case 0x2002: case 0x2003:
        case 0x2004: case 0x2005: case 0x2006: case 0x2007:
            m_chrHighReg[addr & 0x07] = data;
            break;

        case 0x3000: case 0x3001: case 0x3002: case 0x3003:
            m_ntLowReg[addr & 0x03] = data;
            break;

        case 0x3004: case 0x3005: case 0x3006: case 0x3007:
            m_ntHighReg[addr & 0x03] = data;
            break;

        case 0x4000:
            if((data & 0x01) != 0) {
                m_irqEnabled = true;
            }
            else {
                m_irqEnabled = false;
                m_interruptFlag = false;
            }
            break;

        case 0x4001:
            m_irqCountDirection = static_cast<uint8_t>((data >> 6) & 0x03);
            m_irqFunkyMode = (data & 0x08) != 0;
            m_irqSmallPrescaler = ((data >> 2) & 0x01) != 0;
            m_irqSource = static_cast<IrqSource>(data & 0x03);
            break;

        case 0x4002:
            m_irqEnabled = false;
            m_interruptFlag = false;
            break;

        case 0x4003: m_irqEnabled = true; break;
        case 0x4004: m_irqPrescaler = static_cast<uint8_t>(data ^ m_irqXorReg); break;
        case 0x4005: m_irqCounter = static_cast<uint8_t>(data ^ m_irqXorReg); break;
        case 0x4006: m_irqXorReg = data; break;
        case 0x4007: m_irqFunkyModeReg = data; break;

        case 0x5000:
            m_prgMode = static_cast<uint8_t>(data & 0x07);
            m_chrMode = static_cast<uint8_t>((data >> 3) & 0x03);
            m_advancedNtControl = (data & 0x20) != 0;
            m_disableNtRam = (data & 0x40) != 0;
            m_enablePrgAt6000 = (data & 0x80) != 0;
            break;

        case 0x5001: m_mirroringReg = static_cast<uint8_t>(data & 0x03); break;
        case 0x5002: m_ntRamSelectBit = static_cast<uint8_t>(data & 0x80); break;

        case 0x5003:
            m_mirrorChr = (data & 0x80) != 0;
            m_chrBlockMode = (data & 0x20) == 0;
            m_chrBlock = static_cast<uint8_t>(((data & 0x18) >> 2) | (data & 0x01));
            break;
        }

        updateState();
    }

    GERANES_HOT void onCpuWrite(uint16_t /*addr*/, uint8_t /*data*/) override
    {
        m_cpuWriteCycle = true;
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        const uint8_t slot = static_cast<uint8_t>((addr >> 13) & 0x03);
        const uint16_t bank = mapPrgBank(m_prgPage[slot]);
        return cd().readPrg<BankSize::B8K>(bank, addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        const uint8_t slot = static_cast<uint8_t>((addr >> 10) & 0x07);
        const uint16_t bank = mapChrBank(m_chrPage[slot]);

        if(hasChrRam()) return readChrRam<BankSize::B1K>(bank, addr);
        return cd().readChr<BankSize::B1K>(bank, addr);
    }

    GERANES_HOT void writeChr(int addr, uint8_t data) override
    {
        if(!hasChrRam()) return;
        const uint8_t slot = static_cast<uint8_t>((addr >> 10) & 0x07);
        const uint16_t bank = mapChrBank(m_chrPage[slot]);
        writeChrRam<BankSize::B1K>(bank, addr, data);
    }

    GERANES_HOT void onPpuRead(uint16_t addr) override
    {
        if(m_irqSource == IrqSource::PPU_READ && addr < 0x3000) {
            tickIrqCounter();
        }
    }

    GERANES_HOT void setA12State(bool state) override
    {
        const bool wasHigh = (m_lastPpuAddr & 0x1000) != 0;
        if(m_irqSource == IrqSource::PPU_A12_RISE && state && !wasHigh) {
            tickIrqCounter();
        }
        m_lastPpuAddr = state ? 0x1000 : 0x0000;
    }

    GERANES_HOT void cycle() override
    {
        if(
            m_irqSource == IrqSource::CPU_CLOCK ||
            (m_irqSource == IrqSource::CPU_WRITE && m_cpuWriteCycle)
        ) {
            tickIrqCounter();
        }
        m_cpuWriteCycle = false;
    }

    GERANES_HOT void writeSaveRam(int addr, uint8_t data) override
    {
        if(m_enablePrgAt6000) {
            (void)addr;
            (void)data;
            return;
        }
        (void)addr;
        (void)data;
    }

    GERANES_HOT uint8_t readSaveRam(int addr) override
    {
        if(m_enablePrgAt6000) {
            const uint16_t bank = mapPrgBank(m_prg6000Page);
            return cd().readPrg<BankSize::B8K>(bank, addr);
        }
        return 0;
    }

    GERANES_HOT bool getInterruptFlag() override
    {
        return m_interruptFlag;
    }

    GERANES_HOT MirroringType mirroringType() override
    {
        if(cd().useFourScreenMirroring()) return MirroringType::FOUR_SCREEN;

        switch(m_mirroringReg & 0x03) {
        case 0: return MirroringType::VERTICAL;
        case 1: return MirroringType::HORIZONTAL;
        case 2: return MirroringType::SINGLE_SCREEN_A;
        case 3: return MirroringType::SINGLE_SCREEN_B;
        }

        return MirroringType::VERTICAL;
    }

    void reset() override
    {
        m_prgReg[0] = m_prgReg[1] = m_prgReg[2] = m_prgReg[3] = 0;
        m_chrLowReg[0] = m_chrLowReg[1] = m_chrLowReg[2] = m_chrLowReg[3] = 0;
        m_chrLowReg[4] = m_chrLowReg[5] = m_chrLowReg[6] = m_chrLowReg[7] = 0;
        m_chrHighReg[0] = m_chrHighReg[1] = m_chrHighReg[2] = m_chrHighReg[3] = 0;
        m_chrHighReg[4] = m_chrHighReg[5] = m_chrHighReg[6] = m_chrHighReg[7] = 0;
        m_chrLatch[0] = 0;
        m_chrLatch[1] = 4;

        m_prgMode = 0;
        m_enablePrgAt6000 = false;

        m_chrMode = 0;
        m_chrBlockMode = false;
        m_chrBlock = 0;
        m_mirrorChr = false;

        m_mirroringReg = 0;
        m_advancedNtControl = false;
        m_disableNtRam = false;
        m_ntRamSelectBit = 0;
        m_ntLowReg[0] = m_ntLowReg[1] = m_ntLowReg[2] = m_ntLowReg[3] = 0;
        m_ntHighReg[0] = m_ntHighReg[1] = m_ntHighReg[2] = m_ntHighReg[3] = 0;

        m_irqEnabled = false;
        m_irqSource = IrqSource::CPU_CLOCK;
        m_irqCountDirection = 0;
        m_irqFunkyMode = false;
        m_irqFunkyModeReg = 0;
        m_irqSmallPrescaler = false;
        m_irqPrescaler = 0;
        m_irqCounter = 0;
        m_irqXorReg = 0;
        m_interruptFlag = false;
        m_cpuWriteCycle = false;

        m_multiplyValue1 = 0;
        m_multiplyValue2 = 0;
        m_regRamValue = 0;
        m_lastPpuAddr = 0;

        m_prgPage[0] = m_prgPage[1] = m_prgPage[2] = m_prgPage[3] = 0;
        m_chrPage[0] = m_chrPage[1] = m_chrPage[2] = m_chrPage[3] = 0;
        m_chrPage[4] = m_chrPage[5] = m_chrPage[6] = m_chrPage[7] = 0;
        m_prg6000Page = 0;

        updateState();
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);

        s.array(m_prgReg, 1, 4);
        s.array(m_chrLowReg, 1, 8);
        s.array(m_chrHighReg, 1, 8);
        s.array(m_chrLatch, 1, 2);

        SERIALIZEDATA(s, m_prgMode);
        SERIALIZEDATA(s, m_enablePrgAt6000);

        SERIALIZEDATA(s, m_chrMode);
        SERIALIZEDATA(s, m_chrBlockMode);
        SERIALIZEDATA(s, m_chrBlock);
        SERIALIZEDATA(s, m_mirrorChr);

        SERIALIZEDATA(s, m_mirroringReg);
        SERIALIZEDATA(s, m_advancedNtControl);
        SERIALIZEDATA(s, m_disableNtRam);
        SERIALIZEDATA(s, m_ntRamSelectBit);
        s.array(m_ntLowReg, 1, 4);
        s.array(m_ntHighReg, 1, 4);

        SERIALIZEDATA(s, m_irqEnabled);
        SERIALIZEDATA(s, m_irqSource);
        SERIALIZEDATA(s, m_irqCountDirection);
        SERIALIZEDATA(s, m_irqFunkyMode);
        SERIALIZEDATA(s, m_irqFunkyModeReg);
        SERIALIZEDATA(s, m_irqSmallPrescaler);
        SERIALIZEDATA(s, m_irqPrescaler);
        SERIALIZEDATA(s, m_irqCounter);
        SERIALIZEDATA(s, m_irqXorReg);
        SERIALIZEDATA(s, m_interruptFlag);
        SERIALIZEDATA(s, m_cpuWriteCycle);

        SERIALIZEDATA(s, m_multiplyValue1);
        SERIALIZEDATA(s, m_multiplyValue2);
        SERIALIZEDATA(s, m_regRamValue);
        SERIALIZEDATA(s, m_lastPpuAddr);

        SERIALIZEDATA(s, m_prgMask);
        SERIALIZEDATA(s, m_chrMask);
        SERIALIZEDATA(s, m_prgBankCount);
        SERIALIZEDATA(s, m_chrBankCount);

        s.array(m_prgPage, 1, 4);
        s.array(reinterpret_cast<uint8_t*>(m_chrPage), 2, 8);
        SERIALIZEDATA(s, m_prg6000Page);

        updateState();
    }
};
