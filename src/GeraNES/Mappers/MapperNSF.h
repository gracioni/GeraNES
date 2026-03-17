#pragma once

#include <array>
#include <cstring>
#include <cstdint>
#include <sstream>
#include <iomanip>
#include "BaseMapper.h"
#include "GeraNES/NesCartridgeData/_NsfFormat.h"
#include "GeraNES/util/NesAssembler.h"

class MapperNSF : public BaseMapper
{
private:
    static constexpr uint16_t DRIVER_ADDR = 0x7F00;
    static constexpr uint16_t DRIVER_IRQ_ADDR = 0x7F40;
    static constexpr uint16_t DRIVER_RTI_ADDR = 0x7F47;
    static constexpr int IRQ_ACK_ADDR = 0x1FF0;
    _NsfFormat& m_nsf;
    std::array<uint8_t, 8> m_bankRegs = {0, 1, 2, 3, 4, 5, 6, 7};
    uint8_t m_songIndex = 0; // 0-based
    bool m_isPlaying = true;
    bool m_irqPending = false;
    uint32_t m_irqCounter = 0;

    GERANES_INLINE uint8_t readMappedNsfData(uint16_t cpuAddr) const
    {
        const int rel = static_cast<int>(cpuAddr) - 0x8000;
        if(rel < 0) return 0;

        if(m_nsf.usesBankSwitch()) {
            const int slot = rel >> 12; // 4KB slot in $8000-$FFFF
            const int bank = static_cast<int>(m_bankRegs[static_cast<size_t>(slot)]) & 0xFF;
            const int offset = (bank << 12) | (rel & 0x0FFF);
            if(offset >= 0 && offset < m_nsf.prgSize()) {
                return m_nsf.readPrg(offset);
            }
            return 0;
        }

        const int linear = rel - static_cast<int>(m_nsf.loadAddress() - 0x8000);
        if(linear >= 0 && linear < m_nsf.prgSize()) {
            return m_nsf.readPrg(linear);
        }
        return 0;
    }

    void writeDriverByte(uint16_t cpuAddr, uint8_t value)
    {
        writeSaveRam(cpuAddr - 0x6000, value);
    }

    void installDriver()
    {
        // Mesen-style minimal BIOS in WRAM:
        // $7F00 reset:
        //   CLI
        //   LDX #$FD
        //   TXS
        //   LDA #$00
        //   STA $2000
        //   STA $2001
        //   LDX #$13
        // clear_apu:
        //   STA $4000,X
        //   DEX
        //   BPL clear_apu
        //   STA $4015
        //   LDA #$0F
        //   STA $4015
        //   LDA #$40
        //   STA $4017
        //   LDX #region
        //   LDY #$00
        //   LDA #songIndex
        //   JSR init
        //   STA $7FF0    ; clear/reload IRQ timer
        // loop:
        //   JMP loop
        //
        // $7F10 irq:
        //   STA $7FF0    ; clear/reload IRQ timer
        //   JSR play
        //   RTI
        //
        // $7F17:
        //   RTI          ; NMI vector (unused)
        const uint16_t initAddr = m_nsf.initAddress();
        const uint16_t playAddr = m_nsf.playAddress();
        auto emitDriverByte = [this](uint16_t cpuAddr, uint8_t value) {
            writeDriverByte(cpuAddr, value);
        };

        NesAssembler resetDriver(emitDriverByte, DRIVER_ADDR);
        resetDriver.cli();
        resetDriver.ldxImm(0xFD);
        resetDriver.txs();
        resetDriver.ldaImm(0x00);
        resetDriver.staAbs(0x2000);
        resetDriver.staAbs(0x2001);
        resetDriver.ldxImm(0x13);
        const uint16_t clearApuLoopAddr = resetDriver.position();
        resetDriver.staAbsX(0x4000);
        resetDriver.dex();
        resetDriver.bpl(clearApuLoopAddr);
        resetDriver.staAbs(0x4015);
        resetDriver.ldaImm(0x0F);
        resetDriver.staAbs(0x4015);
        resetDriver.ldaImm(0x40);
        resetDriver.staAbs(0x4017);
        resetDriver.ldxImm(m_nsf.initRegionValue());
        resetDriver.ldyImm(0x00);
        resetDriver.ldaImm(m_songIndex);
        resetDriver.jsr(initAddr);
        resetDriver.staAbs(static_cast<uint16_t>(0x6000 + IRQ_ACK_ADDR));
        const uint16_t loopAddr = resetDriver.position();
        resetDriver.jmp(loopAddr);

        NesAssembler irqDriver(emitDriverByte, DRIVER_IRQ_ADDR);
        irqDriver.staAbs(static_cast<uint16_t>(0x6000 + IRQ_ACK_ADDR));
        irqDriver.jsr(playAddr);
        irqDriver.rti();

        NesAssembler nmiDriver(emitDriverByte, DRIVER_RTI_ADDR);
        nmiDriver.rti();
    }

    uint32_t getIrqReloadValue() const
    {
        const uint32_t speedUs = m_nsf.playSpeedNtsc();
        const double clocks = static_cast<double>(speedUs) * (1789773.0 / 1000000.0);
        return static_cast<uint32_t>(clocks);
    }

    void clearAndReloadIrq()
    {
        m_irqPending = false;
        m_irqCounter = getIrqReloadValue();
    }

    void initBanks()
    {
        if(m_nsf.usesBankSwitch()) {
            for(int i = 0; i < 8; ++i) {
                m_bankRegs[static_cast<size_t>(i)] = m_nsf.bankInitReg(i);
            }
        } else {
            for(int i = 0; i < 8; ++i) {
                m_bankRegs[static_cast<size_t>(i)] = static_cast<uint8_t>(i);
            }
        }
    }

public:
    MapperNSF(ICartridgeData& cd)
        : BaseMapper(cd)
        , m_nsf(*dynamic_cast<_NsfFormat*>(&cd))
    {
    }

    void reset() override
    {
        if(m_nsf.totalSongs() == 0) {
            m_songIndex = 0;
        }
        else if(m_songIndex >= m_nsf.totalSongs()) {
            m_songIndex = static_cast<uint8_t>((m_nsf.startSong() > 0 ? m_nsf.startSong() : 1) - 1);
        }
        if(saveRamData() != nullptr && saveRamSize() > 0) {
            memset(saveRamData(), 0, saveRamSize());
        }
        m_irqPending = false;
        m_irqCounter = 0;
        initBanks();
        installDriver();
    }

    uint8_t readPrg(int addr) override
    {
        const uint16_t cpuAddr = static_cast<uint16_t>(0x8000 + (addr & 0x7FFF));

        // Vectors
        if(cpuAddr == 0xFFFA) return static_cast<uint8_t>(DRIVER_RTI_ADDR & 0xFF);
        if(cpuAddr == 0xFFFB) return static_cast<uint8_t>((DRIVER_RTI_ADDR >> 8) & 0xFF);
        if(cpuAddr == 0xFFFC) return static_cast<uint8_t>(DRIVER_ADDR & 0xFF);
        if(cpuAddr == 0xFFFD) return static_cast<uint8_t>((DRIVER_ADDR >> 8) & 0xFF);
        if(cpuAddr == 0xFFFE) return static_cast<uint8_t>(DRIVER_IRQ_ADDR & 0xFF);
        if(cpuAddr == 0xFFFF) return static_cast<uint8_t>((DRIVER_IRQ_ADDR >> 8) & 0xFF);

        return readMappedNsfData(cpuAddr);
    }

    void cycle() override
    {
        if(!m_isPlaying) return;
        if(m_irqCounter > 0) {
            --m_irqCounter;
            if(m_irqCounter == 0) {
                m_irqPending = true;
                m_irqCounter = getIrqReloadValue();
            }
        }
    }

    bool getInterruptFlag() override
    {
        return m_irqPending;
    }

    void writeMapperRegister(int addr, uint8_t data) override
    {
        // NSF bankswitch registers at $5FF8-$5FFF.
        // In this core mapper regs use relative addresses ($4000-$5FFF => $0000-$1FFF).
        if(addr >= 0x1FF8 && addr <= 0x1FFF) {
            m_bankRegs[static_cast<size_t>(addr & 0x07)] = data;
        }
    }

    void writeSaveRam(int addr, uint8_t data) override
    {
        BaseMapper::writeSaveRam(addr, data);
        if(addr == IRQ_ACK_ADDR) {
            clearAndReloadIrq();
        }
    }

    GERANES_INLINE int totalSongs() const
    {
        return static_cast<int>(m_nsf.totalSongs());
    }

    GERANES_INLINE int currentSong() const
    {
        return static_cast<int>(m_songIndex) + 1;
    }

    GERANES_INLINE bool isPlaying() const
    {
        return m_isPlaying;
    }

    void setPlaying(bool playing)
    {
        m_isPlaying = playing;
        if(!m_isPlaying) {
            m_irqPending = false;
        } else if(m_irqCounter == 0) {
            m_irqCounter = getIrqReloadValue();
        }
    }

    void setSong(int song1Based)
    {
        if(m_nsf.totalSongs() == 0) {
            m_songIndex = 0;
            return;
        }

        int clamped = song1Based;
        if(clamped < 1) clamped = 1;
        if(clamped > static_cast<int>(m_nsf.totalSongs())) clamped = static_cast<int>(m_nsf.totalSongs());
        m_songIndex = static_cast<uint8_t>(clamped - 1);
    }

    void requestSongInit()
    {
    }

    bool songInitPending()
    {
        return false;
    }

    void nextSong()
    {
        if(m_nsf.totalSongs() == 0) return;
        m_songIndex = static_cast<uint8_t>((m_songIndex + 1) % m_nsf.totalSongs());
    }

    void prevSong()
    {
        if(m_nsf.totalSongs() == 0) return;
        if(m_songIndex == 0) m_songIndex = static_cast<uint8_t>(m_nsf.totalSongs() - 1);
        else --m_songIndex;
    }
};
