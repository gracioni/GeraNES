#pragma once

#include "BaseMapper.h"

//BxROM or NINA-001, 2 incompatible mappers
class Mapper034 : public BaseMapper
{
private:

    uint8_t m_PRGMask = 0;
    uint8_t m_PRGReg = 0;

    uint8_t m_CHRReg[2] = {0};
    uint8_t m_CHRMask = 0;

public:

    Mapper034(ICartridgeData& cd) : BaseMapper(cd)
    {
        m_PRGMask = calculateMask(m_cd.numberOfPRGBanks<WindowSize::W32K>());

        m_CHRMask = calculateMask(m_cd.numberOfCHRBanks<WindowSize::W4K>());
    }

    GERANES_HOT bool isNINA() {
        return !hasChrRam();
    }

    GERANES_HOT void writePrg(int addr, uint8_t data) override
    {
        if(!isNINA()) m_PRGReg = data & m_PRGMask;
        else BaseMapper::writePrg(addr,data);
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        return m_cd.readPrg<WindowSize::W32K>(m_PRGReg,addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasChrRam()) return BaseMapper::readChr(addr);
        else {

            switch(addr>>12){
            case 0: return m_cd.readChr<WindowSize::W4K>(m_CHRReg[0],addr);
            case 1: return m_cd.readChr<WindowSize::W4K>(m_CHRReg[1],addr);
            }

        }

        return 0;
    }

    GERANES_HOT void writeSaveRam(int addr, uint8_t data) override
    {
        if(isNINA()){
            switch(addr){
            case 0x1FFD: m_PRGReg = data & m_PRGMask; break;
            case 0x1FFE: m_CHRReg[0] = data & m_CHRMask; break;
            case 0x1FFF: m_CHRReg[1] = data & m_CHRMask; break;
            }
        }

        BaseMapper::writeSaveRam(addr,data);
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);    

        SERIALIZEDATA(s, m_PRGMask);
        SERIALIZEDATA(s, m_PRGReg);

        s.array(m_CHRReg,1,2);
        SERIALIZEDATA(s, m_CHRMask);
    }

};
