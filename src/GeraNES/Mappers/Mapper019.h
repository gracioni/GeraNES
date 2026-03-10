#pragma once

#include <memory>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>

#include "BaseMapper.h"
#include "logger/logger.h"

#define SOUND_RAM_SIZE 128
#ifndef GERANES_M019_AUDIO_DEBUG
#define GERANES_M019_AUDIO_DEBUG 0
#endif

class Mapper019 : public BaseMapper
{

private:

    uint8_t m_PRGREGMask = 0;
    uint8_t m_CHRREGMask = 0;

    uint8_t m_CHRReg[8] = {0};
    uint8_t m_PRGReg[3] = {0};

    uint8_t m_MirroringReg[4] = {0};

    bool m_highCHRRAMDisable = false;
    bool m_lowCHRRAMDisable = false;

    uint16_t m_IRQCounter = 0;
    bool m_interruptFlag = false;
    bool m_IRQEnable = false;

    uint8_t m_soundRAMAddress = 0;
    bool m_soundAutoIncrement = false;
    uint8_t m_soundRAM[SOUND_RAM_SIZE] = {0};
    bool m_soundDisable = false;

    // Namco 163 expansion audio
    int m_audioClockDiv = 15;
    int m_audioClockCounter = 0;
    float m_expansionAudioSample = 0.0f;
    float m_expansionAudioPrev = 0.0f;
    float m_expansionAudioTarget = 0.0f;
    float m_expansionAudioFiltered = 0.0f;
    uint32_t m_audioPhaseRemainder[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    float m_audioChannelVol[8] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
#if GERANES_M019_AUDIO_DEBUG
    int m_dbgWriteCounter = 0;
    int m_dbgCycleCounter = 0;
    int m_dbgLastFirstChannel = -1;
#endif

    template<BankSize bs>
    GERANES_INLINE uint8_t readChrRam(int bank, int addr)
    {
        addr = (bank << log2(bs)) + (addr&(static_cast<int>(bs)-1));
        return chrRam()[addr];
    }

    template<BankSize bs>
    GERANES_INLINE void writeChrRam(int bank, int addr, uint8_t data)
    {
        addr = (bank << log2(bs)) + (addr&(static_cast<int>(bs)-1));
        chrRam()[addr] = data;
    }

    GERANES_INLINE void writeSoundRAM(uint8_t data)
    {
        m_soundRAM[m_soundRAMAddress] = data;
        incrementSoundRAMAddress();
    }

    GERANES_INLINE uint8_t readSoundRAM()
    {
        uint8_t ret = m_soundRAM[m_soundRAMAddress];

        incrementSoundRAMAddress();

        return ret;
    }

    GERANES_INLINE void incrementSoundRAMAddress()
    {
        if(m_soundAutoIncrement) m_soundRAMAddress++;
        m_soundRAMAddress &= SOUND_RAM_SIZE-1;
    }

    GERANES_INLINE int firstActiveChannel() const
    {
        const int c = static_cast<int>((m_soundRAM[0x7F] >> 4) & 0x07);
        return std::clamp(7 - c, 0, 7);
    }

    GERANES_INLINE int activeChannelCount() const
    {
        return 8 - firstActiveChannel();
    }

    GERANES_INLINE uint8_t readWaveNibble(uint8_t nibbleIndex) const
    {
        const uint8_t byteVal = m_soundRAM[(nibbleIndex >> 1) & (SOUND_RAM_SIZE - 1)];
        return (nibbleIndex & 0x01) ? static_cast<uint8_t>((byteVal >> 4) & 0x0F)
                                    : static_cast<uint8_t>(byteVal & 0x0F);
    }

    GERANES_INLINE float updateChannelAndGetSample(int channel, int channels)
    {
        const int base = 0x40 + (channel << 3);

        const uint32_t freq =
            static_cast<uint32_t>(m_soundRAM[base + 0]) |
            (static_cast<uint32_t>(m_soundRAM[base + 2]) << 8) |
            (static_cast<uint32_t>(m_soundRAM[base + 4] & 0x03) << 16);

        uint32_t phase =
            static_cast<uint32_t>(m_soundRAM[base + 1]) |
            (static_cast<uint32_t>(m_soundRAM[base + 3]) << 8) |
            (static_cast<uint32_t>(m_soundRAM[base + 5]) << 16);

        const uint32_t stepNumerator = freq + m_audioPhaseRemainder[channel];
        const uint32_t step = stepNumerator / static_cast<uint32_t>(channels);
        m_audioPhaseRemainder[channel] = stepNumerator % static_cast<uint32_t>(channels);

        phase += step;
        phase &= 0x00FFFFFF;

        const uint32_t waveLen = static_cast<uint32_t>(256 - (m_soundRAM[base + 4] & 0xFC));
        const uint32_t waveSpan = waveLen << 16;
        if(waveSpan != 0) {
            phase %= waveSpan;
        }

        m_soundRAM[base + 1] = static_cast<uint8_t>(phase & 0xFF);
        m_soundRAM[base + 3] = static_cast<uint8_t>((phase >> 8) & 0xFF);
        m_soundRAM[base + 5] = static_cast<uint8_t>((phase >> 16) & 0xFF);

        const uint8_t waveAddr = m_soundRAM[base + 6];
        const uint8_t wavePos = static_cast<uint8_t>((phase >> 16) & 0xFF);
        const uint8_t sampleIndex = static_cast<uint8_t>(waveAddr + wavePos);
        const uint8_t sample4 = readWaveNibble(sampleIndex);
        const int sampleCentered = static_cast<int>(sample4) - 8;
        const int volume = static_cast<int>(m_soundRAM[base + 7] & 0x0F);

        const float out = static_cast<float>(sampleCentered * volume) / 120.0f;
        return out * m_audioChannelVol[channel];
    }

    GERANES_INLINE void tickExpansionAudio()
    {
        if(m_soundDisable) {
            m_expansionAudioTarget = 0.0f;
            return;
        }

        const int first = firstActiveChannel();
        const int channels = std::max(1, activeChannelCount());

        float mix = 0.0f;
        for(int ch = first; ch <= 7; ++ch) {
            mix += updateChannelAndGetSample(ch, channels);
        }

        // "Smooth" approximation for this emulator pipeline:
        // normalize by channel energy (sqrt) so multi-channel songs keep a usable level.
        const float channelNorm = std::sqrt(static_cast<float>(channels));
        const float raw = std::clamp(mix / std::max(1.0f, channelNorm), -1.0f, 1.0f);
        const float alpha = (channels >= 6) ? 0.18f : 0.30f;
        m_expansionAudioFiltered += alpha * (raw - m_expansionAudioFiltered);
        m_expansionAudioTarget = std::clamp(m_expansionAudioFiltered, -1.0f, 1.0f);

#if GERANES_M019_AUDIO_DEBUG
        if(m_dbgLastFirstChannel != first) {
            m_dbgLastFirstChannel = first;
            std::ostringstream ss;
            ss << "[M019][AUDIO] active_first=" << first
               << " channels=" << channels
               << " reg7f=0x" << std::uppercase << std::hex << std::setw(2) << std::setfill('0')
               << static_cast<int>(m_soundRAM[0x7F]);
            Logger::instance().log(ss.str(), Logger::Type::DEBUG);
        }
#endif
    }

#if GERANES_M019_AUDIO_DEBUG
    void debugLogAddrPortWrite(uint8_t data)
    {
        std::ostringstream ss;
        ss << "[M019][F800] data=0x" << std::uppercase << std::hex << std::setw(2) << std::setfill('0')
           << static_cast<int>(data)
           << " autoInc=" << ((data & 0x80) ? 1 : 0)
           << " addr=0x" << std::setw(2) << static_cast<int>(data & 0x7F);
        Logger::instance().log(ss.str(), Logger::Type::DEBUG);
    }

    void debugLogDataPortWrite(uint8_t addr, uint8_t data)
    {
        if(addr >= 0x40) {
            std::ostringstream ss;
            ss << "[M019][4800] ram[0x" << std::uppercase << std::hex << std::setw(2) << std::setfill('0')
               << static_cast<int>(addr) << "]=0x" << std::setw(2) << static_cast<int>(data);
            Logger::instance().log(ss.str(), Logger::Type::DEBUG);
        }
    }

    void debugLogWaveRegsSnapshot()
    {
        std::ostringstream ss;
        ss << "[M019][SNAP] 40-7F";
        for(int i = 0x40; i <= 0x7F; ++i) {
            if(((i - 0x40) % 16) == 0) {
                ss << "\n  ";
            }
            ss << std::uppercase << std::hex << std::setw(2) << std::setfill('0')
               << static_cast<int>(m_soundRAM[i]) << ' ';
        }
        Logger::instance().log(ss.str(), Logger::Type::DEBUG);
    }
#endif


public:

    Mapper019(ICartridgeData& cd) : BaseMapper(cd)
    {
        m_PRGREGMask = calculateMask(cd.numberOfPRGBanks<BankSize::B8K>());
        m_CHRREGMask = calculateMask(cd.numberOfCHRBanks<BankSize::B1K>());
    }

    GERANES_HOT void writePrg(int addr, uint8_t data) override
    {
        switch(addr & 0x7800) {

        case 0x0000: m_CHRReg[0] = data; break;
        case 0x0800: m_CHRReg[1] = data; break;
        case 0x1000: m_CHRReg[2] = data; break;
        case 0x1800: m_CHRReg[3] = data; break;
        case 0x2000: m_CHRReg[4] = data; break;
        case 0x2800: m_CHRReg[5] = data; break;
        case 0x3000: m_CHRReg[6] = data; break;
        case 0x3800: m_CHRReg[7] = data; break;

        //Mirroring Regs   (mapper 019 only)
        case 0x4000: m_MirroringReg[0] = data; break;
        case 0x4800: m_MirroringReg[1] = data; break;
        case 0x5000: m_MirroringReg[2] = data; break;
        case 0x5800: m_MirroringReg[3] = data; break;

        case 0x6000:
            m_PRGReg[0] = data & 0x3F & m_PRGREGMask;
            m_soundDisable = (data & 0x40) != 0;
            break;

        case 0x6800:
            m_PRGReg[1] = data & 0x3F & m_PRGREGMask;
            m_highCHRRAMDisable = data >> 7;
            m_lowCHRRAMDisable = (data >> 6) & 0x01;
            break;

        case 0x7000:
            m_PRGReg[2] = data & 0x3F & m_PRGREGMask;
            break;

        case 0x7800: //sound
            m_soundAutoIncrement = data & 0x80;
            m_soundRAMAddress = data & 0x7F;
#if GERANES_M019_AUDIO_DEBUG
            debugLogAddrPortWrite(data);
#endif
            break;

        }

    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        switch(addr>>13) { // addr/8192
        case 0: return cd().readPrg<BankSize::B8K>(m_PRGReg[0],addr);
        case 1: return cd().readPrg<BankSize::B8K>(m_PRGReg[1],addr);
        case 2: return cd().readPrg<BankSize::B8K>(m_PRGReg[2],addr);
        case 3: return cd().readPrg<BankSize::B8K>(cd().numberOfPRGBanks<BankSize::B8K>()-1,addr);
        }

        return 0;
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasChrRam()) return BaseMapper::readChr(addr);

        int index = addr >> 10; // addr/0x400

        if(index < 4) {
            if(m_CHRReg[index] >= 0xE0 && !m_lowCHRRAMDisable) return readChrRam<BankSize::B1K>(m_CHRReg[index]-0xE0,addr);
        }
        else {
            if(m_CHRReg[index] >= 0xE0 && !m_highCHRRAMDisable) return readChrRam<BankSize::B1K>(m_CHRReg[index]-0xE0,addr);
        }

        return cd().readChr<BankSize::B1K>(m_CHRReg[index]&m_CHRREGMask,addr);
    }

    GERANES_HOT void writeChr(int addr, uint8_t data) override
    {
        if(hasChrRam()) {
            BaseMapper::writeChr(addr, data);
            return;
         }

        int index = addr >> 10; // addr/0x400

        if(addr < 0x1000) {
            if(m_CHRReg[index] >= 0xE0 && !m_lowCHRRAMDisable)
                return writeChrRam<BankSize::B1K>(m_CHRReg[index]-0xE0,addr,data);
        }
        else {
            if(m_CHRReg[index] >= 0xE0 && !m_highCHRRAMDisable)
                return writeChrRam<BankSize::B1K>(m_CHRReg[index]-0xE0,addr,data);
        }
    }

    GERANES_HOT void writeMapperRegister(int addr, uint8_t data) override
    {
        switch(addr & 0x1800) {
        case 0x0000: break;
        case 0x0800: //Sound Data port
#if GERANES_M019_AUDIO_DEBUG
            debugLogDataPortWrite(m_soundRAMAddress, data);
            if((++m_dbgWriteCounter % 256) == 0) {
                debugLogWaveRegsSnapshot();
            }
#endif
            writeSoundRAM(data);
            break;
        case 0x1000:
            m_IRQCounter &= 0xFF00;
            m_IRQCounter |= data;
            m_interruptFlag = false;
            break;
        case 0x1800:
            m_IRQCounter &= 0x00FF;
            m_IRQCounter |= static_cast<uint16_t>(data) << 8;
            m_IRQEnable = data & 0x80;
            m_interruptFlag = false;
            break;
        }
    }

    GERANES_HOT uint8_t readMapperRegister(int addr, uint8_t openBusData) override
    {
        switch(addr & 0x1800) {
        case 0x0000: return openBusData;
        case 0x0800: //Sound Data port
            return readSoundRAM();
        case 0x1000:
            m_interruptFlag = false;
            return static_cast<uint8_t>(m_IRQCounter);
        case 0x1800:
            m_interruptFlag = false;
            return static_cast<uint8_t>(m_IRQCounter>>8);
        }

        return openBusData;
    }

    GERANES_HOT bool useCustomNameTable(uint8_t index) override
    {
        return m_MirroringReg[index] < 0xE0;
    }

    GERANES_HOT uint8_t readCustomNameTable(uint8_t index, uint16_t addr) override
    {        
        uint8_t bank = m_MirroringReg[index];
        return cd().readChr<BankSize::B1K>(bank,addr);
    }
    
    GERANES_HOT MirroringType mirroringType() override
    {
        return MirroringType::CUSTOM;
    }

    GERANES_HOT uint8_t customMirroring(uint8_t blockIndex) override
    {
        uint8_t value = m_MirroringReg[blockIndex];
        return value < 0xE0 ? value : value&0x03;
    }

    GERANES_HOT bool getInterruptFlag() override
    {
        return m_interruptFlag;
    }

    GERANES_HOT float getExpansionAudioSample() override
    {
        return m_expansionAudioSample;
    }

    std::string getAudioChannelsJson() const override
    {
        std::ostringstream ss;
        ss << "{\"channels\":[";
        for(int i = 0; i < 8; ++i) {
            if(i > 0) ss << ",";
            ss << "{\"id\":\"n163.ch" << (i + 1) << "\",\"label\":\"N163 Ch " << (i + 1)
               << "\",\"volume\":" << m_audioChannelVol[i] << ",\"min\":0.0,\"max\":1.0}";
        }
        ss << "]}";
        return ss.str();
    }

    bool setAudioChannelVolumeById(const std::string& id, float volume) override
    {
        const float v = std::clamp(volume, 0.0f, 1.0f);
        for(int i = 0; i < 8; ++i) {
            const std::string chId = std::string("n163.ch") + std::to_string(i + 1);
            if(id == chId) {
                m_audioChannelVol[i] = v;
                return true;
            }
        }
        return false;
    }

    void reset() override
    {
        m_audioClockCounter = 0;
        m_expansionAudioSample = 0.0f;
        m_expansionAudioPrev = 0.0f;
        m_expansionAudioTarget = 0.0f;
        m_expansionAudioFiltered = 0.0f;
        m_soundDisable = false;
        for(uint32_t& r : m_audioPhaseRemainder) r = 0;
        for(float& v : m_audioChannelVol) v = 1.0f;
#if GERANES_M019_AUDIO_DEBUG
        m_dbgWriteCounter = 0;
        m_dbgCycleCounter = 0;
        m_dbgLastFirstChannel = -1;
#endif
    }

    GERANES_HOT void cycle() override
    {
        if(m_IRQEnable) {
            if( (m_IRQCounter&0x7FFF) == 0x7FFF) {
                m_interruptFlag = true;
                //m_IRQCounter &= 0x8000; //m_IRQCounter = 0 + keep enable bit
            }
            else ++m_IRQCounter;

        }

        if(++m_audioClockCounter >= m_audioClockDiv) {
            m_audioClockCounter = 0;
            m_expansionAudioPrev = m_expansionAudioTarget;
            tickExpansionAudio();
        }

        const float t = static_cast<float>(m_audioClockCounter + 1) / static_cast<float>(m_audioClockDiv);
        const float interpolated = m_expansionAudioPrev + (m_expansionAudioTarget - m_expansionAudioPrev) * t;

        // De-click guard without long-term lag:
        // only soften very large discontinuities, otherwise pass through unchanged.
        const float prevOut = m_expansionAudioSample;
        const float jump = std::fabs(interpolated - prevOut);
        if(jump > 0.55f) {
            m_expansionAudioSample = std::clamp(prevOut + (interpolated - prevOut) * 0.35f, -1.0f, 1.0f);
        }
        else {
            m_expansionAudioSample = std::clamp(interpolated, -1.0f, 1.0f);
        }

#if GERANES_M019_AUDIO_DEBUG
        if((++m_dbgCycleCounter % 4096) == 0) {
            debugLogWaveRegsSnapshot();
        }
#endif
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);

        SERIALIZEDATA(s, m_PRGREGMask);
        SERIALIZEDATA(s, m_CHRREGMask);

        s.array(m_CHRReg,1,8);
        s.array(m_PRGReg,1,3);

        s.array(m_MirroringReg,1,4);

        SERIALIZEDATA(s, m_highCHRRAMDisable);
        SERIALIZEDATA(s, m_lowCHRRAMDisable);

        SERIALIZEDATA(s, m_IRQCounter);
        SERIALIZEDATA(s, m_interruptFlag);
        SERIALIZEDATA(s, m_IRQEnable);

        SERIALIZEDATA(s, m_soundRAMAddress);
        SERIALIZEDATA(s, m_soundAutoIncrement);

        s.array(m_soundRAM,1,SOUND_RAM_SIZE);

        SERIALIZEDATA(s, m_soundDisable);
        SERIALIZEDATA(s, m_audioClockDiv);
        SERIALIZEDATA(s, m_audioClockCounter);
        SERIALIZEDATA(s, m_expansionAudioSample);
        SERIALIZEDATA(s, m_expansionAudioPrev);
        SERIALIZEDATA(s, m_expansionAudioTarget);
        SERIALIZEDATA(s, m_expansionAudioFiltered);
        s.array(reinterpret_cast<uint8_t*>(m_audioPhaseRemainder), sizeof(uint32_t), 8);
        s.array(reinterpret_cast<uint8_t*>(m_audioChannelVol), sizeof(float), 8);

    }

};
