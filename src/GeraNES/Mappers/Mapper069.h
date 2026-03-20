#pragma once

#include <memory>
#include <algorithm>
#include <cstring>

#include "BaseMapper.h"
#include "Audio/Sunsoft5BAudio.h"

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

    Sunsoft5BAudio m_audio;

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
        m_audio.reset();
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

        m_audio.reset();
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
            m_audio.writeAddress(data);
        }
        else {
            m_audio.writeData(data);
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

        m_audio.clock();
    }

    GERANES_HOT bool getInterruptFlag() override {
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

    std::string getAudioChannelsJson() const override
    {
        return m_audio.getAudioChannelsJson();
    }

    bool setAudioChannelVolumeById(const std::string& id, float volume) override
    {
        return m_audio.setAudioChannelVolumeById(id, volume);
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

        m_audio.serialization(s);

    }

};
