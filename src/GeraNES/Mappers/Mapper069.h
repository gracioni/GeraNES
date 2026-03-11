#pragma once

#include <memory>
#include <algorithm>
#include <sstream>
#include <cstring>

#include "BaseMapper.h"

//Sunsoft FME-7 5A and 5B

class Mapper069 : public BaseMapper
{
private:
    static constexpr uint32_t MAX_PRGRAM_SIZE = 512 * 1024;

    uint8_t m_PRGREGMask = 0;
    uint8_t m_CHRREGMask = 0;

    uint8_t m_PRGREG[4] = {0};
    uint8_t m_CHRREG[8] = {0};

    bool m_PRGRAMEnable = false;
    bool m_PRGRAMSelect = false;

    bool m_IRQEnable = false;
    bool m_IRQCounterEnable = false;

    uint16_t m_IRQCounter = 0;

    bool m_interruptFlag = false;

    uint8_t m_mirroring = 0;

    uint32_t m_currentRAMSize = 0x2000;
    std::unique_ptr<uint8_t[]> m_PRGRAM;

    uint8_t command = 0;

    // Sunsoft 5B (AY-3-8910 compatible) audio
    uint8_t m_audioSelectedReg = 0;
    uint8_t m_audioRegs[16] = {0};
    int m_audioClockDiv = 8; // AY clock ~= CPU/8 for expected pitch
    int m_audioClockCounter = 0;

    uint16_t m_toneCounter[3] = {1, 1, 1};
    bool m_toneOutput[3] = {true, true, true};

    uint16_t m_noiseCounter = 1;
    uint32_t m_noiseLfsr = 1;
    bool m_noiseOutput = true;

    uint16_t m_envCounter = 1;
    uint8_t m_envVolume = 0;
    bool m_envAttack = false;
    bool m_envContinue = false;
    bool m_envAlternate = false;
    bool m_envHold = false;
    bool m_envHolding = false;
    int m_envDirection = 1;

    float m_expansionAudioSample = 0.0f;
    float m_audioChannelVolA = 1.0f;
    float m_audioChannelVolB = 1.0f;
    float m_audioChannelVolC = 1.0f;

    static constexpr float VOLUME_TABLE[16] = {
        0.0f, 0.0045f, 0.0071f, 0.0106f,
        0.0159f, 0.0240f, 0.0345f, 0.0500f,
        0.0709f, 0.1000f, 0.1414f, 0.1995f,
        0.2818f, 0.3981f, 0.5623f, 1.0f
    };

    void setPRGRAMSize(uint32_t newSize) {
        if(newSize == 0) newSize = 0x2000;
        if(newSize == m_currentRAMSize && m_PRGRAM) return;

        auto aux = std::unique_ptr<uint8_t[]>(new uint8_t[newSize]);
        memset(aux.get(), 0, newSize);

        if(m_PRGRAM) {
            uint32_t copySize = std::min(newSize, m_currentRAMSize);
            memcpy(aux.get(), m_PRGRAM.get(), copySize);
        }

        m_currentRAMSize = newSize;
        m_PRGRAM = std::move(aux);
    }

    GERANES_INLINE uint16_t tonePeriod(int ch) const
    {
        const uint16_t p = static_cast<uint16_t>(
            (m_audioRegs[ch * 2] | ((m_audioRegs[ch * 2 + 1] & 0x0F) << 8)));
        return (p == 0) ? 1 : p;
    }

    GERANES_INLINE uint16_t noisePeriod() const
    {
        const uint16_t p = static_cast<uint16_t>(m_audioRegs[6] & 0x1F);
        return (p == 0) ? 1 : p;
    }

    GERANES_INLINE uint16_t envPeriod() const
    {
        const uint16_t p = static_cast<uint16_t>(m_audioRegs[11] | (m_audioRegs[12] << 8));
        return (p == 0) ? 1 : p;
    }

    void restartEnvelope()
    {
        const uint8_t shape = m_audioRegs[13] & 0x0F;
        m_envContinue = (shape & 0x08) != 0;
        m_envAttack = (shape & 0x04) != 0;
        m_envAlternate = (shape & 0x02) != 0;
        m_envHold = (shape & 0x01) != 0;
        m_envHolding = false;
        m_envCounter = envPeriod();
        m_envVolume = m_envAttack ? 0 : 15;
        m_envDirection = m_envAttack ? 1 : -1;
    }

    void tickEnvelope()
    {
        if(m_envHolding) return;

        if(--m_envCounter > 0) return;
        m_envCounter = envPeriod();

        int next = static_cast<int>(m_envVolume) + m_envDirection;
        if(next >= 0 && next <= 15) {
            m_envVolume = static_cast<uint8_t>(next);
            return;
        }

        if(!m_envContinue) {
            m_envVolume = 0;
            m_envHolding = true;
            return;
        }

        if(m_envHold) {
            m_envVolume = m_envAttack ? 15 : 0;
            m_envHolding = true;
            return;
        }

        if(m_envAlternate) {
            m_envDirection = -m_envDirection;
            m_envAttack = !m_envAttack;
        }

        m_envVolume = m_envAttack ? 0 : 15;
    }

    void tickPSG()
    {
        for(int ch = 0; ch < 3; ++ch) {
            if(--m_toneCounter[ch] == 0) {
                m_toneCounter[ch] = tonePeriod(ch);
                m_toneOutput[ch] = !m_toneOutput[ch];
            }
        }

        if(--m_noiseCounter == 0) {
            m_noiseCounter = noisePeriod();
            const uint32_t feedback = (m_noiseLfsr ^ (m_noiseLfsr >> 3)) & 0x01;
            m_noiseLfsr = (m_noiseLfsr >> 1) | (feedback << 16);
            m_noiseOutput = (m_noiseLfsr & 0x01) != 0;
        }

        tickEnvelope();

        const uint8_t mixer = m_audioRegs[7];

        float mix = 0.0f;
        const float gains[3] = {m_audioChannelVolA, m_audioChannelVolB, m_audioChannelVolC};
        for(int ch = 0; ch < 3; ++ch) {
            const bool toneDisabled = ((mixer >> ch) & 0x01) != 0;
            const bool noiseDisabled = ((mixer >> (ch + 3)) & 0x01) != 0;
            const bool toneOk = toneDisabled || m_toneOutput[ch];
            const bool noiseOk = noiseDisabled || m_noiseOutput;
            if(!(toneOk && noiseOk)) continue;

            const uint8_t volReg = m_audioRegs[8 + ch];
            const uint8_t volIndex = ((volReg & 0x10) != 0) ? m_envVolume : (volReg & 0x0F);
            mix += VOLUME_TABLE[volIndex] * gains[ch];
        }

        // 3 channels normalized to [-1, 1]
        m_expansionAudioSample = (mix / 3.0f) * 2.0f - 1.0f;
        m_expansionAudioSample = std::clamp(m_expansionAudioSample, -1.0f, 1.0f);
    }

    void writeAudioData(uint8_t data)
    {
        const uint8_t reg = m_audioSelectedReg & 0x0F;
        m_audioRegs[reg] = data;

        switch(reg) {
            case 13:
                restartEnvelope();
                break;
            default:
                break;
        }
    }


    template<BankSize bs>
    GERANES_INLINE uint8_t readCHRRAM(int bank, int addr)
    {
        if(m_currentRAMSize == 0 || !m_PRGRAM) return 0;

        const uint32_t index =
            (static_cast<uint32_t>(bank) << log2(bs)) +
            static_cast<uint32_t>(addr & (static_cast<int>(bs) - 1));

        return m_PRGRAM[index % m_currentRAMSize];
    }

    template<BankSize bs>
    GERANES_INLINE void writeCHRRAM(int bank, int addr, uint8_t data)
    {
        if(m_currentRAMSize == 0 || !m_PRGRAM) return;

        const uint32_t index =
            (static_cast<uint32_t>(bank) << log2(bs)) +
            static_cast<uint32_t>(addr & (static_cast<int>(bs) - 1));

        m_PRGRAM[index % m_currentRAMSize] = data;
    }


public:

    Mapper069(ICartridgeData& cd) : BaseMapper(cd), m_PRGRAM()
    {
        if(cd.foundInDatabase() && cd.saveRamSize() > 0)
            m_currentRAMSize = static_cast<uint32_t>(cd.saveRamSize());
        else
            m_currentRAMSize = MAX_PRGRAM_SIZE;

        setPRGRAMSize(m_currentRAMSize);
        m_PRGREGMask = calculateMask(cd.numberOfPRGBanks<BankSize::B8K>());
        m_CHRREGMask = calculateMask(cd.numberOfCHRBanks<BankSize::B1K>());
        restartEnvelope();
    }

    void reset() override
    {
        command = 0;
        m_PRGRAMEnable = false;
        m_PRGRAMSelect = false;
        m_mirroring = 0;
        m_PRGREG[0] = 0;
        m_PRGREG[1] = 0;
        m_PRGREG[2] = 0;
        m_PRGREG[3] = 0;
        for(uint8_t& r : m_CHRREG) r = 0;

        m_interruptFlag = false;
        m_IRQEnable = false;
        m_IRQCounterEnable = false;
        m_IRQCounter = 0;

        m_audioSelectedReg = 0;
        memset(m_audioRegs, 0, sizeof(m_audioRegs));
        m_audioClockCounter = 0;

        m_toneCounter[0] = m_toneCounter[1] = m_toneCounter[2] = 1;
        m_toneOutput[0] = m_toneOutput[1] = m_toneOutput[2] = true;
        m_noiseCounter = 1;
        m_noiseLfsr = 1;
        m_noiseOutput = true;
        restartEnvelope();

        m_expansionAudioSample = 0.0f;
        m_audioChannelVolA = 1.0f;
        m_audioChannelVolB = 1.0f;
        m_audioChannelVolC = 1.0f;
    }

    GERANES_HOT void writePrg(int addr, uint8_t data) override
    {
        //Command Register ($8000-$9FFF)
        //Parameter Register ($A000-$BFFF)
        //Sound Command ($C000-$DFFF)
        //Sound Data ($E000-$FFFF)

        if(addr < 0x2000) command = data & 0x0F;
        else if(addr < 0x4000){

            switch(command) {
            case 0: m_CHRREG[0] = data & m_CHRREGMask; break;
            case 1: m_CHRREG[1] = data & m_CHRREGMask; break;
            case 2: m_CHRREG[2] = data & m_CHRREGMask; break;
            case 3: m_CHRREG[3] = data & m_CHRREGMask; break;
            case 4: m_CHRREG[4] = data & m_CHRREGMask; break;
            case 5: m_CHRREG[5] = data & m_CHRREGMask; break;
            case 6: m_CHRREG[6] = data & m_CHRREGMask; break;
            case 7: m_CHRREG[7] = data & m_CHRREGMask; break;

            case 8:
                m_PRGREG[0] = data & 0x3F;  //ram bank, dont mask with m_PRGREGMask
                m_PRGRAMSelect = data & 0x40;
                m_PRGRAMEnable = data & 0x80;
                break;


            case 9: m_PRGREG[1] = data & 0x3F & m_PRGREGMask; break;
            case 0xA: m_PRGREG[2] = data & 0x3F & m_PRGREGMask; break;
            case 0xB: m_PRGREG[3] = data & 0x3F & m_PRGREGMask; break;
            case 0xC: m_mirroring = data & 0x03; break;

            case 0xD:
                m_IRQEnable = data & 0x01;
                m_IRQCounterEnable = data & 0x80;
                m_interruptFlag = false;
                break;

            case 0xE:
                m_IRQCounter &= 0xFF00;
                m_IRQCounter |= data;
                break;

            case 0xF:
                m_IRQCounter &= 0x00FF;
                m_IRQCounter |= static_cast<uint16_t>(data) << 8;
                break;

            }
        }
        else if(addr < 0x6000) {
            m_audioSelectedReg = data & 0x0F;
        }
        else {
            writeAudioData(data);
        }

    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        switch(addr>>13) {
        case 0: return cd().readPrg<BankSize::B8K>(m_PRGREG[1],addr);
        case 1: return cd().readPrg<BankSize::B8K>(m_PRGREG[2],addr);
        case 2: return cd().readPrg<BankSize::B8K>(m_PRGREG[3],addr);
        case 3: return cd().readPrg<BankSize::B8K>(cd().numberOfPRGBanks<BankSize::B8K>()-1,addr);
        }

        return 0;
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasChrRam()) return BaseMapper::readChr(addr);

        int index = addr >> 10; // addr/0x400
        return cd().readChr<BankSize::B1K>(m_CHRREG[index],addr);
    }

    GERANES_HOT MirroringType mirroringType() override
    {
        switch(m_mirroring){
        case 0: return MirroringType::VERTICAL;
        case 1: return MirroringType::HORIZONTAL;
        case 2: return MirroringType::SINGLE_SCREEN_A;
        case 3: return MirroringType::SINGLE_SCREEN_B;
        }

        return MirroringType::FOUR_SCREEN;
    }

    GERANES_HOT void cycle() override
    {
        if(m_IRQCounterEnable) {

            if(m_IRQEnable && m_IRQCounter == 0) m_interruptFlag = true;
            --m_IRQCounter;
        }

        if(++m_audioClockCounter >= m_audioClockDiv) {
            m_audioClockCounter = 0;
            tickPSG();
        }
    }

    GERANES_HOT bool getInterruptFlag() override {
        return m_interruptFlag;
    }

    GERANES_HOT float getExpansionAudioSample() override
    {
        return m_expansionAudioSample;
    }

    std::string getAudioChannelsJson() const override
    {
        std::ostringstream os;
        os << "{\"channels\":["
           << "{\"id\":\"sunsoft5b.a\",\"label\":\"Sunsoft 5B A\",\"volume\":" << m_audioChannelVolA << ",\"min\":0.0,\"max\":1.0},"
           << "{\"id\":\"sunsoft5b.b\",\"label\":\"Sunsoft 5B B\",\"volume\":" << m_audioChannelVolB << ",\"min\":0.0,\"max\":1.0},"
           << "{\"id\":\"sunsoft5b.c\",\"label\":\"Sunsoft 5B C\",\"volume\":" << m_audioChannelVolC << ",\"min\":0.0,\"max\":1.0}"
           << "]}";
        return os.str();
    }

    bool setAudioChannelVolumeById(const std::string& id, float volume) override
    {
        const float v = std::clamp(volume, 0.0f, 1.0f);
        if(id == "sunsoft5b.a") { m_audioChannelVolA = v; return true; }
        if(id == "sunsoft5b.b") { m_audioChannelVolB = v; return true; }
        if(id == "sunsoft5b.c") { m_audioChannelVolC = v; return true; }
        return false;
    }

    GERANES_HOT void writeSaveRam(int addr, uint8_t data) override
    {
        if(m_PRGRAMEnable && m_PRGRAMSelect) writeCHRRAM<BankSize::B8K>(m_PRGREG[0], addr, data);
    }

    GERANES_HOT uint8_t readSaveRam(int addr) override
    {
        if(m_PRGRAMEnable && m_PRGRAMSelect)
            return readCHRRAM<BankSize::B8K>(m_PRGREG[0],addr);

        return cd().readPrg<BankSize::B8K>(m_PRGREG[0],addr);
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);

        SERIALIZEDATA(s, m_PRGREGMask);
        SERIALIZEDATA(s, m_CHRREGMask);

        s.array(m_PRGREG, 1, 4);
        s.array(m_CHRREG, 1, 8);


        SERIALIZEDATA(s, m_PRGRAMEnable);
        SERIALIZEDATA(s, m_PRGRAMSelect);

        SERIALIZEDATA(s, m_IRQEnable);
        SERIALIZEDATA(s, m_IRQCounterEnable);

        SERIALIZEDATA(s, m_IRQCounter);

        SERIALIZEDATA(s, m_interruptFlag);

        SERIALIZEDATA(s, m_mirroring);

        SERIALIZEDATA(s, m_currentRAMSize);
        setPRGRAMSize(m_currentRAMSize);
        s.array(m_PRGRAM.get(), 1, m_currentRAMSize);

        SERIALIZEDATA(s, command);

        SERIALIZEDATA(s, m_audioSelectedReg);
        s.array(m_audioRegs, 1, 16);
        SERIALIZEDATA(s, m_audioClockDiv);
        SERIALIZEDATA(s, m_audioClockCounter);
        s.array(reinterpret_cast<uint8_t*>(m_toneCounter), sizeof(uint16_t), 3);
        s.array(reinterpret_cast<uint8_t*>(m_toneOutput), 1, 3);
        SERIALIZEDATA(s, m_noiseCounter);
        SERIALIZEDATA(s, m_noiseLfsr);
        SERIALIZEDATA(s, m_noiseOutput);
        SERIALIZEDATA(s, m_envCounter);
        SERIALIZEDATA(s, m_envVolume);
        SERIALIZEDATA(s, m_envAttack);
        SERIALIZEDATA(s, m_envContinue);
        SERIALIZEDATA(s, m_envAlternate);
        SERIALIZEDATA(s, m_envHold);
        SERIALIZEDATA(s, m_envHolding);
        SERIALIZEDATA(s, m_envDirection);
        SERIALIZEDATA(s, m_expansionAudioSample);
        SERIALIZEDATA(s, m_audioChannelVolA);
        SERIALIZEDATA(s, m_audioChannelVolB);
        SERIALIZEDATA(s, m_audioChannelVolC);

    }

};
