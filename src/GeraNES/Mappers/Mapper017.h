#pragma once

#include "BaseMapper.h"
#include <cstring>
#include <vector>

// FFE F8xxx (Mapper 17)
class Mapper017 : public BaseMapper
{
private:

    uint8_t m_prgPage[4] = {0, 1, 2, 3}; // 8K pages at $8000-$FFFF
    uint8_t m_chrPage[8] = {0, 1, 2, 3, 4, 5, 6, 7}; // 1K pages at $0000-$1FFF

    uint8_t m_prgMask = 0; // 8K page mask
    uint8_t m_chrMask = 0; // 1K page mask

    bool m_singleScreen = false;
    bool m_singleScreenB = false;
    bool m_verticalMirroring = false;

    bool m_irqEnabled = false;
    bool m_interruptFlag = false;
    uint16_t m_irqCounter = 0;

    std::vector<uint8_t> m_workRam;

    bool m_bootstrapVectorPending = false;
    bool m_bootstrapConsumed = false;

    uint8_t* getCpuRamPtr()
    {
        if(!m_workRam.empty()) return m_workRam.data();
        return saveRamData();
    }

    size_t getCpuRamSize() const
    {
        if(!m_workRam.empty()) return m_workRam.size();
        return saveRamSize();
    }

    void setupTrainerBootstrap()
    {
        if(!cd().hasTrainer()) {
            m_bootstrapVectorPending = false;
            return;
        }

        uint8_t* ram = getCpuRamPtr();
        size_t ramSize = getCpuRamSize();
        if(ram == nullptr || ramSize < 6) {
            m_bootstrapVectorPending = false;
            return;
        }

        for(int i = 0; i < 512; ++i) {
            ram[(0x1000 + i) % ramSize] = cd().readTrainer(i);
        }

        // Current reset vector source is always CPU $FFFC/$FFFD, i.e. slot 3.
        const uint8_t fixedPage = m_prgPage[3] & m_prgMask;
        const uint8_t resetLow = cd().readPrg<BankSize::B8K>(fixedPage, 0x7FFC);
        const uint8_t resetHigh = cd().readPrg<BankSize::B8K>(fixedPage, 0x7FFD);

        // Stub at $6000:
        //   JSR $7003
        //   JMP $xxxx (real reset vector)
        ram[0] = 0x20;
        ram[1] = 0x03;
        ram[2] = 0x70;
        ram[3] = 0x4C;
        ram[4] = resetLow;
        ram[5] = resetHigh;

        m_bootstrapVectorPending = true;
        m_bootstrapConsumed = false;
    }

    template<BankSize bs>
    GERANES_INLINE uint8_t readChrBank(int bank, int addr) {
        if(hasChrRam()) return readChrRam<bs>(bank, addr);
        return cd().readChr<bs>(bank, addr);
    }

    template<BankSize bs>
    GERANES_INLINE void writeChrBank(int bank, int addr, uint8_t data) {
        if(hasChrRam()) writeChrRam<bs>(bank, addr, data);
    }

public:

    Mapper017(ICartridgeData& cd) : BaseMapper(cd)
    {
        m_prgMask = calculateMask(cd.numberOfPRGBanks<BankSize::B8K>());

        if(hasChrRam()) {
            m_chrMask = calculateMask(cd.chrRamSize() / static_cast<int>(BankSize::B1K));
        }
        else {
            m_chrMask = calculateMask(cd.numberOfCHRBanks<BankSize::B1K>());
        }

        if(cd.ramSize() > 0 && cd.saveRamSize() == 0) {
            m_workRam.resize(static_cast<size_t>(cd.ramSize()), 0x00);
        }

        // Initial PRG mapping matches FrontFareast mapper 17:
        // SelectPrgPage4x(0, -4)
        const uint8_t pageCount = static_cast<uint8_t>(cd.numberOfPRGBanks<BankSize::B8K>());
        m_prgPage[0] = static_cast<uint8_t>((pageCount - 4) & m_prgMask);
        m_prgPage[1] = static_cast<uint8_t>((pageCount - 3) & m_prgMask);
        m_prgPage[2] = static_cast<uint8_t>((pageCount - 2) & m_prgMask);
        m_prgPage[3] = static_cast<uint8_t>((pageCount - 1) & m_prgMask);
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        if(cd().hasTrainer() && !m_bootstrapConsumed && !m_bootstrapVectorPending
            && (addr == 0x7FFC || addr == 0x7FFD))
        {
            setupTrainerBootstrap();
        }

        if(m_bootstrapVectorPending) {
            if(addr == 0x7FFC) return 0x00;
            if(addr == 0x7FFD) {
                m_bootstrapVectorPending = false;
                m_bootstrapConsumed = true;
                return 0x60;
            }
        }

        const uint8_t slot = static_cast<uint8_t>((addr >> 13) & 0x03);
        return cd().readPrg<BankSize::B8K>(m_prgPage[slot] & m_prgMask, addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        const uint8_t slot = static_cast<uint8_t>((addr >> 10) & 0x07);
        return readChrBank<BankSize::B1K>(m_chrPage[slot] & m_chrMask, addr);
    }

    GERANES_HOT void writeChr(int addr, uint8_t data) override
    {
        const uint8_t slot = static_cast<uint8_t>((addr >> 10) & 0x07);
        writeChrBank<BankSize::B1K>(m_chrPage[slot] & m_chrMask, addr, data);
    }

    GERANES_HOT void writeMapperRegister(int addr, uint8_t data) override
    {
        const int reg = addr & 0x1FFF;
        if(reg < 0x02FE || reg > 0x0517) return;

        switch(reg) {
        case 0x02FE:
            m_singleScreen = true;
            m_singleScreenB = (data & 0x10) != 0;
            break;

        case 0x02FF:
            m_singleScreen = false;
            m_verticalMirroring = (data & 0x10) == 0;
            break;

        case 0x0501:
            m_irqEnabled = false;
            m_interruptFlag = false;
            break;

        case 0x0502:
            m_irqCounter = (m_irqCounter & 0xFF00) | data;
            m_interruptFlag = false;
            break;

        case 0x0503:
            m_irqCounter = (m_irqCounter & 0x00FF) | (static_cast<uint16_t>(data) << 8);
            m_irqEnabled = true;
            m_interruptFlag = false;
            break;

        case 0x0504: m_prgPage[0] = data & m_prgMask; break;
        case 0x0505: m_prgPage[1] = data & m_prgMask; break;
        case 0x0506: m_prgPage[2] = data & m_prgMask; break;
        case 0x0507: m_prgPage[3] = data & m_prgMask; break;

        case 0x0510: m_chrPage[0] = data & m_chrMask; break;
        case 0x0511: m_chrPage[1] = data & m_chrMask; break;
        case 0x0512: m_chrPage[2] = data & m_chrMask; break;
        case 0x0513: m_chrPage[3] = data & m_chrMask; break;
        case 0x0514: m_chrPage[4] = data & m_chrMask; break;
        case 0x0515: m_chrPage[5] = data & m_chrMask; break;
        case 0x0516: m_chrPage[6] = data & m_chrMask; break;
        case 0x0517: m_chrPage[7] = data & m_chrMask; break;
        }
    }

    GERANES_HOT MirroringType mirroringType() override
    {
        if(cd().useFourScreenMirroring()) return MirroringType::FOUR_SCREEN;

        if(m_singleScreen) {
            return m_singleScreenB ? MirroringType::SINGLE_SCREEN_B : MirroringType::SINGLE_SCREEN_A;
        }
        return m_verticalMirroring ? MirroringType::VERTICAL : MirroringType::HORIZONTAL;
    }

    GERANES_HOT void cycle() override
    {
        if(!m_irqEnabled) return;

        ++m_irqCounter;
        if(m_irqCounter == 0) {
            m_interruptFlag = true;
            m_irqEnabled = false;
        }
    }

    GERANES_HOT bool getInterruptFlag() override
    {
        return m_interruptFlag;
    }

    GERANES_HOT void writeSaveRam(int addr, uint8_t data) override
    {
        if(!m_workRam.empty()) {
            m_workRam[addr % static_cast<int>(m_workRam.size())] = data;
            return;
        }
        BaseMapper::writeSaveRam(addr, data);
    }

    GERANES_HOT uint8_t readSaveRam(int addr) override
    {
        if(!m_workRam.empty()) {
            return m_workRam[addr % static_cast<int>(m_workRam.size())];
        }
        return BaseMapper::readSaveRam(addr);
    }

    void reset() override
    {
        const uint8_t pageCount = static_cast<uint8_t>(cd().numberOfPRGBanks<BankSize::B8K>());
        m_prgPage[0] = static_cast<uint8_t>((pageCount - 4) & m_prgMask);
        m_prgPage[1] = static_cast<uint8_t>((pageCount - 3) & m_prgMask);
        m_prgPage[2] = static_cast<uint8_t>((pageCount - 2) & m_prgMask);
        m_prgPage[3] = static_cast<uint8_t>((pageCount - 1) & m_prgMask);

        m_chrPage[0] = 0;
        m_chrPage[1] = 1;
        m_chrPage[2] = 2;
        m_chrPage[3] = 3;
        m_chrPage[4] = 4;
        m_chrPage[5] = 5;
        m_chrPage[6] = 6;
        m_chrPage[7] = 7;

        m_singleScreen = false;
        m_singleScreenB = false;
        m_verticalMirroring = (cd().mirroringType() == MirroringType::VERTICAL);

        m_irqEnabled = false;
        m_interruptFlag = false;
        m_irqCounter = 0;

        m_bootstrapVectorPending = false;
        m_bootstrapConsumed = false;

        if(!m_workRam.empty()) {
            std::fill(m_workRam.begin(), m_workRam.end(), 0x00);
            if(cd().hasTrainer()) {
                for(int i = 0; i < 512; ++i) {
                    m_workRam[(0x1000 + i) % static_cast<int>(m_workRam.size())] = cd().readTrainer(i);
                }
            }
        }
        else if(saveRamData() != nullptr && saveRamSize() > 0) {
            std::memset(saveRamData(), 0x00, saveRamSize());
            if(cd().hasTrainer()) {
                for(int i = 0; i < 512; ++i) {
                    saveRamData()[(0x1000 + i) & (saveRamSize() - 1)] = cd().readTrainer(i);
                }
            }
        }

        setupTrainerBootstrap();
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);

        s.array(m_prgPage, 1, 4);
        s.array(m_chrPage, 1, 8);

        SERIALIZEDATA(s, m_prgMask);
        SERIALIZEDATA(s, m_chrMask);

        SERIALIZEDATA(s, m_singleScreen);
        SERIALIZEDATA(s, m_singleScreenB);
        SERIALIZEDATA(s, m_verticalMirroring);

        SERIALIZEDATA(s, m_irqEnabled);
        SERIALIZEDATA(s, m_interruptFlag);
        SERIALIZEDATA(s, m_irqCounter);

        SERIALIZEDATA(s, m_bootstrapVectorPending);
        SERIALIZEDATA(s, m_bootstrapConsumed);

        if(!m_workRam.empty()) {
            s.array(m_workRam.data(), 1, static_cast<int>(m_workRam.size()));
        }
    }
};
