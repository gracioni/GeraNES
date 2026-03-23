#pragma once

#include <array>
#include <cstring>
#include <memory>

#include "BaseMapper.h"
#include "Audio/Vrc7Audio.h"
#include "Helpers/VrcIrq.h"

class Mapper085 : public BaseMapper
{
private:
    uint8_t m_prgMask = 0;
    uint8_t m_chrMask = 0;
    std::array<uint8_t, 3> m_prgRegs = {0, 0, 0};
    std::array<uint8_t, 8> m_chrRegs = {0, 0, 0, 0, 0, 0, 0, 0};
    uint8_t m_controlFlags = 0;
    bool m_useA4Select = true;
    bool m_hasAudio = true;
    VrcIrq m_irq;
    Vrc7Audio m_audio;
    std::unique_ptr<uint8_t[]> m_workRam;

    uint8_t* workRamData()
    {
        return saveRamData() != nullptr ? saveRamData() : m_workRam.get();
    }

    uint16_t normalizeRegisterAddress(int addr) const
    {
        uint16_t absoluteAddr = static_cast<uint16_t>(0x8000 | (addr & 0x7FFF));

        if(m_useA4Select && (absoluteAddr & 0x10) != 0 && (absoluteAddr & 0xF010) != 0x9010) {
            absoluteAddr = static_cast<uint16_t>((absoluteAddr | 0x08) & ~0x10);
        }

        return absoluteAddr;
    }

    bool wramEnabled() const
    {
        return (m_controlFlags & 0x80) != 0;
    }

public:
    Mapper085(ICartridgeData& cd) : BaseMapper(cd)
    {
        m_prgMask = calculateMask(cd.numberOfPRGBanks<BankSize::B8K>());
        m_chrMask = calculateMask(cd.numberOfCHRBanks<BankSize::B1K>());
        m_useA4Select = cd.subMapperId() != 1;
        m_hasAudio = cd.subMapperId() != 1;

        if(saveRamData() == nullptr) {
            m_workRam = std::make_unique<uint8_t[]>(0x2000);
            memset(m_workRam.get(), 0, 0x2000);
        }

        reset();
    }

    void reset() override
    {
        m_prgRegs = {0, 0, 0};
        m_chrRegs.fill(0);
        m_controlFlags = 0;
        m_irq.reset();
        m_audio.reset();
        m_audio.setMuted(!m_hasAudio);

        if(m_workRam) {
            memset(m_workRam.get(), 0, 0x2000);
        }
    }

    GERANES_HOT void writePrg(int addr, uint8_t data) override
    {
        const uint16_t regAddr = normalizeRegisterAddress(addr);

        switch(regAddr & 0xF038) {
        case 0x8000:
            m_prgRegs[0] = data & 0x3F & m_prgMask;
            break;
        case 0x8008:
            m_prgRegs[1] = data & 0x3F & m_prgMask;
            break;
        case 0x9000:
            m_prgRegs[2] = data & 0x3F & m_prgMask;
            break;

        case 0x9010:
            if(m_hasAudio) m_audio.writeRegisterSelect(data);
            break;
        case 0x9030:
            if(m_hasAudio) m_audio.writeRegisterData(data);
            break;

        case 0xA000: m_chrRegs[0] = data & m_chrMask; break;
        case 0xA008: m_chrRegs[1] = data & m_chrMask; break;
        case 0xB000: m_chrRegs[2] = data & m_chrMask; break;
        case 0xB008: m_chrRegs[3] = data & m_chrMask; break;
        case 0xC000: m_chrRegs[4] = data & m_chrMask; break;
        case 0xC008: m_chrRegs[5] = data & m_chrMask; break;
        case 0xD000: m_chrRegs[6] = data & m_chrMask; break;
        case 0xD008: m_chrRegs[7] = data & m_chrMask; break;

        case 0xE000:
            m_controlFlags = data;
            m_audio.setMuted(!m_hasAudio || (data & 0x40) != 0);
            break;

        case 0xE008:
            m_irq.setReloadValue(data);
            break;
        case 0xF000:
            m_irq.setControlValue(data);
            break;
        case 0xF008:
            m_irq.acknowledge();
            break;
        }
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        switch((addr >> 13) & 0x03) {
        case 0: return cd().readPrg<BankSize::B8K>(m_prgRegs[0], addr);
        case 1: return cd().readPrg<BankSize::B8K>(m_prgRegs[1], addr);
        case 2: return cd().readPrg<BankSize::B8K>(m_prgRegs[2], addr);
        default: return cd().readPrg<BankSize::B8K>(cd().numberOfPRGBanks<BankSize::B8K>() - 1, addr);
        }
    }

    GERANES_HOT void writeChr(int addr, uint8_t data) override
    {
        if(!hasChrRam()) return;
        writeChrRam<BankSize::B1K>(m_chrRegs[(addr >> 10) & 0x07], addr, data);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        const uint8_t bank = m_chrRegs[(addr >> 10) & 0x07];
        if(hasChrRam()) return readChrRam<BankSize::B1K>(bank, addr);
        return cd().readChr<BankSize::B1K>(bank, addr);
    }

    GERANES_HOT MirroringType mirroringType() override
    {
        switch(m_controlFlags & 0x03) {
        case 0: return MirroringType::VERTICAL;
        case 1: return MirroringType::HORIZONTAL;
        case 2: return MirroringType::SINGLE_SCREEN_A;
        case 3: return MirroringType::SINGLE_SCREEN_B;
        }

        return MirroringType::VERTICAL;
    }

    GERANES_HOT void writeSaveRam(int addr, uint8_t data) override
    {
        if(!wramEnabled()) return;

        uint8_t* ram = workRamData();
        if(ram != nullptr) {
            ram[addr & 0x1FFF] = data;
        }
    }

    GERANES_HOT uint8_t readSaveRam(int addr) override
    {
        if(!wramEnabled()) return 0;

        uint8_t* ram = workRamData();
        return ram != nullptr ? ram[addr & 0x1FFF] : 0;
    }

    GERANES_HOT void cycle() override
    {
        m_irq.cycle();
        if(m_hasAudio) {
            m_audio.clock(cd().sistem() == GameDatabase::System::NesPal ? 1662607 : 1789773);
        }
    }

    GERANES_HOT bool getInterruptFlag() override
    {
        return m_irq.interruptFlag();
    }

    GERANES_HOT float getExpansionAudioSample() override
    {
        return m_hasAudio ? m_audio.getSample() : 0.0f;
    }

    float getMixWeight() const override
    {
        return m_hasAudio ? m_audio.getMixWeight() : 0.0f;
    }

    std::string getAudioChannelsJson() const override
    {
        return m_hasAudio ? m_audio.getAudioChannelsJson() : "{\"channels\":[]}";
    }

    bool setAudioChannelVolumeById(const std::string& id, float volume) override
    {
        return m_hasAudio && m_audio.setAudioChannelVolumeById(id, volume);
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);

        SERIALIZEDATA(s, m_prgMask);
        SERIALIZEDATA(s, m_chrMask);
        s.array(m_prgRegs.data(), 1, m_prgRegs.size());
        s.array(m_chrRegs.data(), 1, m_chrRegs.size());
        SERIALIZEDATA(s, m_controlFlags);
        SERIALIZEDATA(s, m_useA4Select);
        SERIALIZEDATA(s, m_hasAudio);
        m_irq.serialization(s);
        m_audio.serialization(s);

        bool hasWorkRam = m_workRam != nullptr;
        SERIALIZEDATA(s, hasWorkRam);
        if(hasWorkRam) {
            s.array(m_workRam.get(), 1, 0x2000);
        }
    }
};
