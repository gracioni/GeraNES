#ifndef MAPPER019_H
#define MAPPER019_H

//TODO: expansion sound

#include <memory>

#include "IMapper.h"

#define SOUND_RAM_SIZE 128

class Mapper019 : public IMapper
{

private:

    std::unique_ptr<uint8_t[]> m_CHRRAM; //8k

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
    uint8_t m_soundRAM[SOUND_RAM_SIZE];

    template<int WindowSize>
    GERANES_INLINE uint8_t readCHRRAM(int bank, int addr)
    {
        addr = bank*WindowSize + (addr&(WindowSize-1));
        return m_CHRRAM[addr];
    }

    template<int WindowSize>
    GERANES_INLINE void writeCHRRAM(int bank, int addr, uint8_t data)
    {
        addr = bank*WindowSize + (addr&(WindowSize-1));
        m_CHRRAM[addr] = data;
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


public:

    Mapper019(ICartridgeData& cd) : IMapper(cd)
    {
        m_PRGREGMask = calculateMask(m_cartridgeData.numberOfPRGBanks<W8K>());
        m_CHRREGMask = calculateMask(m_cartridgeData.numberOfCHRBanks<W1K>());

        m_CHRRAM.reset(new uint8_t[0x2000]); //8k
    }

    GERANES_HOT void writePRG32k(int addr, uint8_t data) override
    {
        switch(addr) {

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
            break;

        }

    }

    GERANES_HOT uint8_t readPRG32k(int addr) override
    {
        switch(addr>>13) { // addr/8192
        case 0: return m_cartridgeData.readPrg<W8K>(m_PRGReg[0],addr);
        case 1: return m_cartridgeData.readPrg<W8K>(m_PRGReg[1],addr);
        case 2: return m_cartridgeData.readPrg<W8K>(m_PRGReg[2],addr);
        case 3: return m_cartridgeData.readPrg<W8K>(m_cartridgeData.numberOfPRGBanks<W8K>()-1,addr);
        }

        return 0;
    }

    GERANES_HOT uint8_t readCHR8k(int addr) override
    {
        if(has8kVRAM()) return IMapper::readCHR8k(addr);

        int index = addr >> 10; // addr/0x400

        if(index < 4) {
            if(m_CHRReg[index] >= 0xE0 && !m_lowCHRRAMDisable) return readCHRRAM<W1K>(m_CHRReg[index]-0xE0,addr);
        }
        else {
            if(m_CHRReg[index] >= 0xE0 && !m_highCHRRAMDisable) return readCHRRAM<W1K>(m_CHRReg[index]-0xE0,addr);
        }

        return m_cartridgeData.readChr<W1K>(m_CHRReg[index]&m_CHRREGMask,addr);
    }

    GERANES_HOT void writeCHR8k(int addr, uint8_t data) override
    {
        if(has8kVRAM()) {
            IMapper::writeCHR8k(addr, data);
            return;
         }

        int index = addr >> 10; // addr/0x400

        if(addr < 0x1000) {
            if(m_CHRReg[index] >= 0xE0 && !m_lowCHRRAMDisable)
                return writeCHRRAM<W1K>(m_CHRReg[index]-0xE0,addr,data);
        }
        else {
            if(m_CHRReg[index] >= 0xE0 && !m_highCHRRAMDisable)
                return writeCHRRAM<W1K>(m_CHRReg[index]-0xE0,addr,data);
        }
    }

    GERANES_HOT void write0x4000(int addr, uint8_t data) override
    {
        switch(addr) {
        case 0x0000: break;
        case 0x0800: //Sound Data port
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

    GERANES_HOT uint8_t read0x4000(int addr, uint8_t openBusData) override
    {
        switch(addr) {
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
        return m_cartridgeData.readChr<W1K>(bank,addr);
    }
    
    GERANES_HOT MirroringType mirroringType() override
    {
        return IMapper::CUSTOM;
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

    GERANES_HOT void cycle() override
    {
        if(m_IRQEnable) {
            if( (m_IRQCounter&0x7FFF) == 0x7FFF) {
                m_interruptFlag = true;
                //m_IRQCounter &= 0x8000; //m_IRQCounter = 0 + keep enable bit
            }
            else ++m_IRQCounter;

        }
    }

    void serialization(SerializationBase& s) override
    {
        IMapper::serialization(s);

        s.array(m_CHRRAM.get(),1,0x2000); //8k

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

    }

};

#endif
