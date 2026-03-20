#pragma once

#include <memory>
#include <algorithm>

#include "BaseMapper.h"
#include "Audio/Namco163Audio.h"

#ifndef GERANES_M019_SUBMAPPER5_GAIN
#define GERANES_M019_SUBMAPPER5_GAIN 2.0f
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

    Namco163Audio m_audio;
    bool m_subMapper5Mix = false;
    float m_subMapperMixGain = 1.0f;

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

public:

    Mapper019(ICartridgeData& cd) : BaseMapper(cd)
    {
        m_PRGREGMask = calculateMask(cd.numberOfPRGBanks<BankSize::B8K>());
        m_CHRREGMask = calculateMask(cd.numberOfCHRBanks<BankSize::B1K>());
        m_subMapper5Mix = (cd.subMapperId() == 5);
        m_subMapperMixGain = m_subMapper5Mix ? GERANES_M019_SUBMAPPER5_GAIN : 1.0f;
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
            m_audio.setDisabled((data & 0x40) != 0);
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
            m_audio.setAddressControl(data);
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
            m_audio.writeData(data);
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
            return m_audio.readData();
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
        return m_audio.getSample();
    }

    float getMixWeight() const override
    {
        return m_audio.getMixWeight();
    }

    float getExpansionOutputGain() const override
    {
        return m_subMapperMixGain;
    }

    std::string getAudioChannelsJson() const override
    {
        return m_audio.getAudioChannelsJson();
    }

    bool setAudioChannelVolumeById(const std::string& id, float volume) override
    {
        return m_audio.setAudioChannelVolumeById(id, volume);
    }

    void reset() override
    {
        for(uint8_t& r : m_CHRReg) r = 0;
        for(uint8_t& r : m_PRGReg) r = 0;
        for(uint8_t& r : m_MirroringReg) r = 0;
        m_highCHRRAMDisable = false;
        m_lowCHRRAMDisable = false;
        m_IRQCounter = 0;
        m_interruptFlag = false;
        m_IRQEnable = false;
        m_audio.reset();
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

        m_audio.clock();
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

        m_audio.serialization(s);
        SERIALIZEDATA(s, m_subMapper5Mix);
        SERIALIZEDATA(s, m_subMapperMixGain);

    }

};
