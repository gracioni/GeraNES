#pragma once

#include "BaseMapper.h"
#include <array>

//bandai

class Mapper016 : public BaseMapper
{
protected:
    enum class EepromMode : uint8_t
    {
        Idle = 0,
        Address = 1,
        Read = 2,
        Write = 3,
        SendAck = 4,
        WaitAck = 5,
        ChipAddress = 6
    };

    enum class EepromType : uint8_t
    {
        None = 0,
        X24C01 = 1,
        C24C02 = 2
    };

    uint8_t m_PRGMask = 0;
    uint8_t m_CHRMask = 0;

    uint8_t m_CHRBank[8];
    uint8_t m_PRGBank = 0;
    uint8_t m_PRGBankSelect = 0;
    uint8_t m_mirroring = 0;
    bool m_enableIRQ = false;
    uint16_t m_IRQCounter = 0;
    uint16_t m_IRQReload = 0;
    bool m_IRQFlag = false;

    EepromType m_eepromType = EepromType::None;
    std::array<uint8_t, 256> m_eepromData = {};
    EepromMode m_eepromMode = EepromMode::Idle;
    EepromMode m_eepromNextMode = EepromMode::Idle;
    uint8_t m_eepromChipAddress = 0;
    uint8_t m_eepromAddress = 0;
    uint8_t m_eepromLatch = 0;
    uint8_t m_eepromCounter = 0;
    uint8_t m_eepromOutput = 1;
    uint8_t m_eepromPrevScl = 0;
    uint8_t m_eepromPrevSda = 1;

    template<BankSize bs>
    GERANES_INLINE uint8_t readChrBank(int bank, int addr)
    {
        if(hasChrRam()) return readChrRam<bs>(bank, addr);
        return cd().readChr<bs>(bank, addr);
    }

    template<BankSize bs>
    GERANES_INLINE void writeChrBank(int bank, int addr, uint8_t data)
    {
        if(hasChrRam()) writeChrRam<bs>(bank, addr, data);
    }

    GERANES_INLINE virtual bool uses6000WriteRange() const
    {
        return cd().mapperId() == 16 && cd().subMapperId() != 5;
    }

    GERANES_INLINE virtual bool uses8000WriteRange() const
    {
        return cd().mapperId() != 16 || cd().subMapperId() != 4;
    }

    GERANES_INLINE virtual bool usesLatchedIrq() const
    {
        return cd().mapperId() != 16 || cd().subMapperId() != 4;
    }

    GERANES_INLINE virtual bool usesChrBankPrgSelect() const
    {
        return cd().mapperId() == 153 || cd().numberOfPRGBanks<BankSize::B16K>() >= 0x20;
    }

    GERANES_INLINE size_t eepromStorageSize() const
    {
        switch(m_eepromType) {
        case EepromType::X24C01: return 128;
        case EepromType::C24C02: return 256;
        default: return 0;
        }
    }

    GERANES_INLINE uint8_t* eepromDataPtr()
    {
        if(saveRamData() != nullptr && saveRamSize() >= eepromStorageSize()) {
            return saveRamData();
        }
        return m_eepromData.data();
    }

    void updatePrgBankSelect()
    {
        if(!usesChrBankPrgSelect()) {
            m_PRGBankSelect = 0;
            return;
        }

        m_PRGBankSelect = 0;
        for(int i = 0; i < 8; ++i) {
            m_PRGBankSelect |= static_cast<uint8_t>((m_CHRBank[i] & 0x01) << 4);
        }
    }

    void writeEepromBitMsb(uint8_t& dest, uint8_t value)
    {
        if(m_eepromCounter < 8) {
            const uint8_t mask = static_cast<uint8_t>(~(1u << (7 - m_eepromCounter)));
            dest = static_cast<uint8_t>((dest & mask) | (value << (7 - m_eepromCounter)));
            ++m_eepromCounter;
        }
    }

    void writeEepromBitLsb(uint8_t& dest, uint8_t value)
    {
        if(m_eepromCounter < 8) {
            const uint8_t mask = static_cast<uint8_t>(~(1u << m_eepromCounter));
            dest = static_cast<uint8_t>((dest & mask) | (value << m_eepromCounter));
            ++m_eepromCounter;
        }
    }

    void readEepromBitMsb()
    {
        if(m_eepromCounter < 8) {
            m_eepromOutput = (m_eepromLatch & (1 << (7 - m_eepromCounter))) ? 1 : 0;
            ++m_eepromCounter;
        }
    }

    void readEepromBitLsb()
    {
        if(m_eepromCounter < 8) {
            m_eepromOutput = (m_eepromLatch & (1 << m_eepromCounter)) ? 1 : 0;
            ++m_eepromCounter;
        }
    }

    void writeEeprom24C02(uint8_t scl, uint8_t sda)
    {
        uint8_t* data = eepromDataPtr();

        if(m_eepromPrevScl && scl && sda < m_eepromPrevSda) {
            m_eepromMode = EepromMode::ChipAddress;
            m_eepromCounter = 0;
            m_eepromOutput = 1;
        }
        else if(m_eepromPrevScl && scl && sda > m_eepromPrevSda) {
            m_eepromMode = EepromMode::Idle;
            m_eepromOutput = 1;
        }
        else if(scl > m_eepromPrevScl) {
            switch(m_eepromMode) {
            default:
                break;

            case EepromMode::ChipAddress:
                writeEepromBitMsb(m_eepromChipAddress, sda);
                break;

            case EepromMode::Address:
                writeEepromBitMsb(m_eepromAddress, sda);
                break;

            case EepromMode::Read:
                readEepromBitMsb();
                break;

            case EepromMode::Write:
                writeEepromBitMsb(m_eepromLatch, sda);
                break;

            case EepromMode::SendAck:
                m_eepromOutput = 0;
                break;

            case EepromMode::WaitAck:
                if(!sda) {
                    m_eepromNextMode = EepromMode::Read;
                    m_eepromLatch = data[m_eepromAddress];
                }
                break;
            }
        }
        else if(scl < m_eepromPrevScl) {
            switch(m_eepromMode) {
            case EepromMode::ChipAddress:
                if(m_eepromCounter == 8) {
                    if((m_eepromChipAddress & 0xA0) == 0xA0) {
                        m_eepromMode = EepromMode::SendAck;
                        m_eepromCounter = 0;
                        m_eepromOutput = 1;

                        if(m_eepromChipAddress & 0x01) {
                            m_eepromNextMode = EepromMode::Read;
                            m_eepromLatch = data[m_eepromAddress];
                        }
                        else {
                            m_eepromNextMode = EepromMode::Address;
                        }
                    }
                    else {
                        m_eepromMode = EepromMode::Idle;
                        m_eepromCounter = 0;
                        m_eepromOutput = 1;
                    }
                }
                break;

            case EepromMode::Address:
                if(m_eepromCounter == 8) {
                    m_eepromCounter = 0;
                    m_eepromMode = EepromMode::SendAck;
                    m_eepromNextMode = EepromMode::Write;
                    m_eepromOutput = 1;
                }
                break;

            case EepromMode::Read:
                if(m_eepromCounter == 8) {
                    m_eepromMode = EepromMode::WaitAck;
                    m_eepromAddress = static_cast<uint8_t>(m_eepromAddress + 1);
                }
                break;

            case EepromMode::Write:
                if(m_eepromCounter == 8) {
                    m_eepromCounter = 0;
                    m_eepromMode = EepromMode::SendAck;
                    m_eepromNextMode = EepromMode::Write;
                    data[m_eepromAddress] = m_eepromLatch;
                    m_eepromAddress = static_cast<uint8_t>(m_eepromAddress + 1);
                }
                break;

            case EepromMode::SendAck:
            case EepromMode::WaitAck:
                m_eepromMode = m_eepromNextMode;
                m_eepromCounter = 0;
                m_eepromOutput = 1;
                break;

            default:
                break;
            }
        }

        m_eepromPrevScl = scl;
        m_eepromPrevSda = sda;
    }

    void writeEeprom24C01(uint8_t scl, uint8_t sda)
    {
        uint8_t* data = eepromDataPtr();

        if(m_eepromPrevScl && scl && sda < m_eepromPrevSda) {
            m_eepromMode = EepromMode::Address;
            m_eepromAddress = 0;
            m_eepromCounter = 0;
            m_eepromOutput = 1;
        }
        else if(m_eepromPrevScl && scl && sda > m_eepromPrevSda) {
            m_eepromMode = EepromMode::Idle;
            m_eepromOutput = 1;
        }
        else if(scl > m_eepromPrevScl) {
            switch(m_eepromMode) {
            case EepromMode::Address:
                if(m_eepromCounter < 7) {
                    writeEepromBitLsb(m_eepromAddress, sda);
                }
                else if(m_eepromCounter == 7) {
                    m_eepromCounter = 8;

                    if(sda) {
                        m_eepromNextMode = EepromMode::Read;
                        m_eepromLatch = data[m_eepromAddress & 0x7F];
                    }
                    else {
                        m_eepromNextMode = EepromMode::Write;
                    }
                }
                break;

            case EepromMode::SendAck:
                m_eepromOutput = 0;
                break;

            case EepromMode::Read:
                readEepromBitLsb();
                break;

            case EepromMode::Write:
                writeEepromBitLsb(m_eepromLatch, sda);
                break;

            case EepromMode::WaitAck:
                if(!sda) {
                    m_eepromNextMode = EepromMode::Idle;
                }
                break;

            default:
                break;
            }
        }
        else if(scl < m_eepromPrevScl) {
            switch(m_eepromMode) {
            case EepromMode::Address:
                if(m_eepromCounter == 8) {
                    m_eepromMode = EepromMode::SendAck;
                    m_eepromOutput = 1;
                }
                break;

            case EepromMode::SendAck:
                m_eepromMode = m_eepromNextMode;
                m_eepromCounter = 0;
                m_eepromOutput = 1;
                break;

            case EepromMode::Read:
                if(m_eepromCounter == 8) {
                    m_eepromMode = EepromMode::WaitAck;
                    m_eepromAddress = static_cast<uint8_t>((m_eepromAddress + 1) & 0x7F);
                }
                break;

            case EepromMode::Write:
                if(m_eepromCounter == 8) {
                    m_eepromMode = EepromMode::SendAck;
                    m_eepromNextMode = EepromMode::Idle;
                    data[m_eepromAddress & 0x7F] = m_eepromLatch;
                    m_eepromAddress = static_cast<uint8_t>((m_eepromAddress + 1) & 0x7F);
                }
                break;

            default:
                break;
            }
        }

        m_eepromPrevScl = scl;
        m_eepromPrevSda = sda;
    }

    void writeEeprom(uint8_t scl, uint8_t sda)
    {
        switch(m_eepromType) {
        case EepromType::C24C02: writeEeprom24C02(scl, sda); break;
        case EepromType::X24C01: writeEeprom24C01(scl, sda); break;
        default: break;
        }
    }

    virtual void handleRegisterD(uint8_t data)
    {
        if(m_eepromType != EepromType::None) {
            const uint8_t scl = static_cast<uint8_t>((data >> 5) & 0x01);
            const uint8_t sda = static_cast<uint8_t>((data >> 6) & 0x01);
            writeEeprom(scl, sda);
        }
    }

    void writeRegister(uint8_t reg, uint8_t data)
    {
        switch(reg & 0x0F)
        {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
            m_CHRBank[reg & 0x07] = data & m_CHRMask;
            updatePrgBankSelect();
            break;

        case 8:
            m_PRGBank = data & 0x0F;
            break;

        case 9:
            m_mirroring = data & 0x03;
            break;

        case 0xA:
            m_enableIRQ = (data & 0x01) != 0;
            m_IRQFlag = false;
            if(usesLatchedIrq()) {
                m_IRQCounter = m_IRQReload;
            }
            break;

        case 0xB:
            if(usesLatchedIrq()) {
                m_IRQReload = static_cast<uint16_t>((m_IRQReload & 0xFF00) | data);
            }
            else {
                m_IRQCounter = static_cast<uint16_t>((m_IRQCounter & 0xFF00) | data);
            }
            break;

        case 0xC:
            if(usesLatchedIrq()) {
                m_IRQReload = static_cast<uint16_t>((m_IRQReload & 0x00FF) | (static_cast<uint16_t>(data) << 8));
            }
            else {
                m_IRQCounter = static_cast<uint16_t>((m_IRQCounter & 0x00FF) | (static_cast<uint16_t>(data) << 8));
            }
            break;

        case 0xD:
            handleRegisterD(data);
            break;
        }
    }

public:

    Mapper016(ICartridgeData& cd) : BaseMapper(cd)
    {
        memset(m_CHRBank, 0x00, 8);

        m_PRGMask = calculateMask(cd.numberOfPRGBanks<BankSize::B16K>());
        if(hasChrRam()) {
            m_CHRMask = calculateMask(cd.chrRamSize() / static_cast<int>(BankSize::B1K));
        }
        else {
            m_CHRMask = calculateMask(cd.numberOfCHRBanks<BankSize::B1K>());
        }

        if(cd.mapperId() == 159) {
            m_eepromType = EepromType::X24C01;
        }
        else if((cd.mapperId() == 16)
            && (cd.subMapperId() == 0 || (cd.subMapperId() == 5 && cd.saveRamSize() >= 256))) {
            m_eepromType = EepromType::C24C02;
        }
    }

    GERANES_HOT void writePrg(int addr, uint8_t data) override
    {
        if(uses8000WriteRange()) writeRegister(static_cast<uint8_t>(addr), data);
    }

    GERANES_HOT void writeMapperRegister(int addr, uint8_t data) override
    {
        if(uses6000WriteRange()) writeRegister(static_cast<uint8_t>(addr), data);
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        const uint8_t prgBank = static_cast<uint8_t>((m_PRGBank | m_PRGBankSelect) & m_PRGMask);
        const uint8_t fixedBank = static_cast<uint8_t>((0x0F | m_PRGBankSelect) & m_PRGMask);

        if(addr < 0x4000) return cd().readPrg<BankSize::B16K>(prgBank,addr);
        if(usesChrBankPrgSelect()) {
            return cd().readPrg<BankSize::B16K>(fixedBank, addr);
        }
        return cd().readPrg<BankSize::B16K>(cd().numberOfPRGBanks<BankSize::B16K>()-1,addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        addr &= 0x1FFF;
        return readChrBank<BankSize::B1K>(m_CHRBank[(addr/0x0400)&0x07], addr);
    }

    GERANES_HOT void writeChr(int addr, uint8_t data) override
    {
        addr &= 0x1FFF;
        writeChrBank<BankSize::B1K>(m_CHRBank[(addr/0x0400)&0x07], addr, data);
    }

    GERANES_HOT uint8_t readMapperRegister(int /*addr*/, uint8_t openBusData) override
    {
        if(m_eepromType == EepromType::None) return openBusData;
        return static_cast<uint8_t>((openBusData & 0xE7) | (m_eepromOutput << 4));
    }

    GERANES_HOT void cycle() override
    {
        if(m_enableIRQ) {
            if(m_IRQCounter == 0) m_IRQFlag = true;
            --m_IRQCounter;
        }
    };

    GERANES_HOT bool getInterruptFlag() override
    {
        return m_IRQFlag;
    };

    GERANES_HOT MirroringType mirroringType() override
    {
        switch(m_mirroring)
        {
        case 0: return MirroringType::VERTICAL; break;
        case 1: return MirroringType::HORIZONTAL; break;
        case 2: return MirroringType::SINGLE_SCREEN_A; break;
        case 3: return MirroringType::SINGLE_SCREEN_B; break;
        }

        return MirroringType::FOUR_SCREEN;
    }

    void reset() override
    {
        memset(m_CHRBank, 0x00, sizeof(m_CHRBank));
        m_PRGBank = 0;
        m_PRGBankSelect = 0;
        m_mirroring = 0;
        m_enableIRQ = false;
        m_IRQCounter = 0;
        m_IRQReload = 0;
        m_IRQFlag = false;
        m_eepromMode = EepromMode::Idle;
        m_eepromNextMode = EepromMode::Idle;
        m_eepromChipAddress = 0;
        m_eepromAddress = 0;
        m_eepromLatch = 0;
        m_eepromCounter = 0;
        m_eepromOutput = 1;
        m_eepromPrevScl = 0;
        m_eepromPrevSda = 1;

        if(saveRamData() == nullptr || saveRamSize() < eepromStorageSize()) {
            m_eepromData.fill(0x00);
        }
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);

        s.array(m_CHRBank, 1, 8);
        SERIALIZEDATA(s, m_PRGMask);
        SERIALIZEDATA(s, m_CHRMask);
        SERIALIZEDATA(s, m_PRGBank);
        SERIALIZEDATA(s, m_PRGBankSelect);
        SERIALIZEDATA(s, m_mirroring);
        SERIALIZEDATA(s, m_enableIRQ);
        SERIALIZEDATA(s, m_IRQCounter);
        SERIALIZEDATA(s, m_IRQReload);
        SERIALIZEDATA(s, m_IRQFlag);

        SERIALIZEDATA(s, m_eepromType);
        if(saveRamData() == nullptr || saveRamSize() < eepromStorageSize()) {
            s.array(m_eepromData.data(), 1, eepromStorageSize());
        }

        SERIALIZEDATA(s, m_eepromMode);
        SERIALIZEDATA(s, m_eepromNextMode);
        SERIALIZEDATA(s, m_eepromChipAddress);
        SERIALIZEDATA(s, m_eepromAddress);
        SERIALIZEDATA(s, m_eepromLatch);
        SERIALIZEDATA(s, m_eepromCounter);
        SERIALIZEDATA(s, m_eepromOutput);
        SERIALIZEDATA(s, m_eepromPrevScl);
        SERIALIZEDATA(s, m_eepromPrevSda);
    }

};
