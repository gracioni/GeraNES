#pragma once

#include "BaseMapper.h"
#include <cstring>
#include <vector>

// FFE F4xxx (Mapper 6)
class Mapper006 : public BaseMapper
{
private:

    uint8_t m_prgStartPage = 0; // 8K page
    uint8_t m_chrStartPage = 0; // 1K page

    uint8_t m_prgMask = 0; // 8K mask
    uint8_t m_chrMask = 0; // 1K mask

    bool m_singleScreen = false;
    bool m_singleScreenB = false;
    bool m_verticalMirroring = false;
    bool m_ffeAltMode = true;

    bool m_irqEnabled = false;
    bool m_interruptFlag = false;
    uint16_t m_irqCounter = 0;

    std::vector<uint8_t> m_workRam;
    std::vector<uint8_t> m_chrRamOverride;
    bool m_useChrRamOverride = false;
    bool m_bootstrapVectorPending = false;
    bool m_bootstrapConsumed = false;

    void applyDefaultMirroringFromHeader()
    {
        m_singleScreen = false;
        m_singleScreenB = false;
        m_verticalMirroring = false;

        switch(cd().mirroringType()) {
        case MirroringType::VERTICAL:
            m_verticalMirroring = true;
            break;
        case MirroringType::SINGLE_SCREEN_A:
            m_singleScreen = true;
            m_singleScreenB = false;
            break;
        case MirroringType::SINGLE_SCREEN_B:
            m_singleScreen = true;
            m_singleScreenB = true;
            break;
        case MirroringType::HORIZONTAL:
        default:
            break;
        }
    }

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

        // Ensure trainer bytes are mapped at $7000-$71FF even when bootstrap is
        // prepared lazily (before/without an explicit mapper reset callback).
        for(int i = 0; i < 512; ++i) {
            ram[(0x1000 + i) % ramSize] = cd().readTrainer(i);
        }

        // Real reset vector from fixed upper PRG area (8K page 15).
        const uint8_t fixedPage = 15 & m_prgMask;
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

        // Force only first reset vector fetch to point to $6000.
        m_bootstrapVectorPending = true;
        m_bootstrapConsumed = false;
    }

    template<BankSize bs>
    GERANES_INLINE uint8_t readChrBank(int bank, int addr) {
        if(hasChrRam()) {
            if(m_useChrRamOverride) {
                int index = (bank << log2(bs)) + (addr & (static_cast<int>(bs) - 1));
                return m_chrRamOverride[index % static_cast<int>(m_chrRamOverride.size())];
            }
            return readChrRam<bs>(bank, addr);
        }
        return cd().readChr<bs>(bank, addr);
    }

    template<BankSize bs>
    GERANES_INLINE void writeChrBank(int bank, int addr, uint8_t data) {
        if(hasChrRam()) {
            if(m_useChrRamOverride) {
                int index = (bank << log2(bs)) + (addr & (static_cast<int>(bs) - 1));
                m_chrRamOverride[index % static_cast<int>(m_chrRamOverride.size())] = data;
            }
            else {
                writeChrRam<bs>(bank, addr, data);
            }
        }
    }

public:

    Mapper006(ICartridgeData& cd) : BaseMapper(cd)
    {
        m_prgMask = calculateMask(cd.numberOfPRGBanks<BankSize::B8K>());

        if(hasChrRam()) {
            int chrRamSize = cd.chrRamSize();
            // For known DB-matched mapper 6 carts, mirror Mesen behavior and force 32KB CHR-RAM.
            // For unknown/hack dumps, keep header size to avoid over-banking artifacts.
            if(cd.foundInDatabase() && chrRamSize != (32 * 1024)) {
                m_useChrRamOverride = true;
                m_chrRamOverride.resize(32 * 1024, 0x00);
                chrRamSize = static_cast<int>(m_chrRamOverride.size());
            }
            m_chrMask = calculateMask(chrRamSize / static_cast<int>(BankSize::B1K));
        }
        else {
            m_chrMask = calculateMask(cd.numberOfCHRBanks<BankSize::B1K>());
        }

        // Mapper 6 commonly relies on non-battery WRAM at $6000-$7FFF.
        // BaseMapper only allocates RAM from saveRamSize(), so allocate
        // work RAM here when needed.
        if(cd.ramSize() > 0 && cd.saveRamSize() == 0) {
            m_workRam.resize(static_cast<size_t>(cd.ramSize()), 0x00);
        }

        // iNES trainer is mapped at $7000-$71FF.
        // NOTE: saveRamData() is only valid after BaseMapper::init(), so
        // we always apply trainer data again in reset().
        if(cd.hasTrainer() && !m_workRam.empty()) {
            for(int i = 0; i < 512; ++i) {
                m_workRam[(0x1000 + i) % static_cast<int>(m_workRam.size())] = cd.readTrainer(i);
            }
        }

        applyDefaultMirroringFromHeader();
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        // Some startup paths don't call mapper::reset() before first reset-vector read.
        // Lazily prepare bootstrap on first $FFFC/$FFFD access when trainer is present.
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

        uint8_t v;
        int page8k;

        switch((addr >> 13) & 0x03) {
        case 0:
            page8k = m_prgStartPage & m_prgMask;
            break;
        case 1:
            page8k = (m_prgStartPage + 1) & m_prgMask;
            break;
        case 2:
            page8k = 14 & m_prgMask;
            break;
        default:
            page8k = 15 & m_prgMask;
            break;
        }
        v = cd().readPrg<BankSize::B8K>(page8k, addr);

        return v;
    }

    GERANES_HOT void writePrg(int /*addr*/, uint8_t data) override
    {
        uint8_t chrValue = data;

        // FrontFareast (mapper 6): if CHR-RAM or alt mode,
        // high bits select PRG and low 2 bits select CHR 8K page.
        if(hasChrRam() || m_ffeAltMode) {
            m_prgStartPage = ((data & 0xFC) >> 1) & m_prgMask;
            chrValue &= 0x03;
        }
        m_chrStartPage = ((chrValue & 0x03) << 3) & m_chrMask;
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        int page = (m_chrStartPage + ((addr >> 10) & 0x07)) & m_chrMask;
        return readChrBank<BankSize::B1K>(page, addr);
    }

    GERANES_HOT void writeChr(int addr, uint8_t data) override
    {
        int page = (m_chrStartPage + ((addr >> 10) & 0x07)) & m_chrMask;
        writeChrBank<BankSize::B1K>(page, addr, data);
    }

    GERANES_HOT void writeMapperRegister(int addr, uint8_t data) override
    {
        int reg = addr & 0x1FFF;
        if(reg < 0x02FE || reg > 0x0517) return;

        switch(reg) {

        case 0x02FE:
            // FrontFareast:
            // bit7: alt mode enabled when 0
            // bit4: 0=ScreenA, 1=ScreenB
            m_ffeAltMode = (data & 0x80) == 0x00;
            m_singleScreen = true;
            m_singleScreenB = (data & 0x10) != 0;
            break;

        case 0x02FF:
            // bit4: 0=Vertical, 1=Horizontal
            m_singleScreen = false;
            m_verticalMirroring = (data & 0x10) == 0;
            break;

        case 0x0501:
            m_irqEnabled = false;
            m_interruptFlag = false;
            break;

        case 0x0502:
            m_irqCounter = (m_irqCounter & 0xFF00) | data;
            break;

        case 0x0503:
            m_irqCounter = (m_irqCounter & 0x00FF) | (static_cast<uint16_t>(data) << 8);
            m_irqEnabled = true;
            m_interruptFlag = false;
            break;
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

        if(m_irqCounter == 0xFFFF) {
            m_irqCounter = 0x0000;
            m_interruptFlag = true;
            m_irqEnabled = false;
        }
        else {
            ++m_irqCounter;
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
        m_prgStartPage = 0;
        m_chrStartPage = 0;
        applyDefaultMirroringFromHeader();
        m_ffeAltMode = true;
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
            // Mapper 6 trainer must be present at $7000-$71FF.
            // saveRamData is the backing storage used for $6000-$7FFF.
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

        SERIALIZEDATA(s, m_prgStartPage);
        SERIALIZEDATA(s, m_chrStartPage);

        SERIALIZEDATA(s, m_prgMask);
        SERIALIZEDATA(s, m_chrMask);

        SERIALIZEDATA(s, m_singleScreen);
        SERIALIZEDATA(s, m_singleScreenB);
        SERIALIZEDATA(s, m_verticalMirroring);
        SERIALIZEDATA(s, m_ffeAltMode);

        SERIALIZEDATA(s, m_irqEnabled);
        SERIALIZEDATA(s, m_interruptFlag);
        SERIALIZEDATA(s, m_irqCounter);

        SERIALIZEDATA(s, m_useChrRamOverride);
        if(m_useChrRamOverride) {
            s.array(m_chrRamOverride.data(), 1, static_cast<int>(m_chrRamOverride.size()));
        }

        if(!m_workRam.empty()) {
            s.array(m_workRam.data(), 1, static_cast<int>(m_workRam.size()));
        }
    }

};
