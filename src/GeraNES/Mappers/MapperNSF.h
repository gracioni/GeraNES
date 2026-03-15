#pragma once

#include <array>
#include <cstdint>
#include <sstream>

#include "BaseMapper.h"
#include "GeraNES/NesCartridgeData/_NsfFormat.h"
#include "logger/logger.h"

class MapperNSF : public BaseMapper
{
private:
    static constexpr uint16_t DRIVER_ADDR = 0x7F00;
    static constexpr uint16_t DRIVER_NMI_ADDR = 0x7F20;
    static constexpr uint16_t DRIVER_IRQ_ADDR = 0x7F3F;

    _NsfFormat& m_nsf;
    std::array<uint8_t, 8> m_bankRegs = {0, 1, 2, 3, 4, 5, 6, 7};
    uint8_t m_songIndex = 0; // 0-based
    bool m_isPlaying = true;

    static constexpr uint16_t NMI_JSR_PLAY_ADDR = static_cast<uint16_t>(DRIVER_NMI_ADDR + 5);

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
        // $7F00:
        //   sei
        //   cld
        //   ldx #$ff
        //   txs
        //   ldx #$00      ; NTSC
        //   ldy #$00
        //   lda #songIndex
        //   jsr init
        //   lda #$80
        //   sta $2000     ; enable NMI
        //   cli
        // loop:
        //   jmp loop
        //
        // $7F20 NMI handler:
        //   pha
        //   txa
        //   pha
        //   tya
        //   pha
        //   jsr play
        //   pla
        //   tay
        //   pla
        //   tax
        //   pla
        //   rti
        const uint16_t initAddr = m_nsf.initAddress();
        const uint16_t playAddr = m_nsf.playAddress();

        uint16_t p = DRIVER_ADDR;
        writeDriverByte(p++, 0x78); // SEI
        writeDriverByte(p++, 0xD8); // CLD
        writeDriverByte(p++, 0xA2); // LDX #$FF
        writeDriverByte(p++, 0xFF);
        writeDriverByte(p++, 0x9A); // TXS
        writeDriverByte(p++, 0xA2); // LDX #$00 (NTSC region for INIT)
        writeDriverByte(p++, 0x00);
        writeDriverByte(p++, 0xA0); // LDY #$00
        writeDriverByte(p++, 0x00);
        writeDriverByte(p++, 0xA9); // LDA #songIndex
        writeDriverByte(p++, m_songIndex);
        writeDriverByte(p++, 0x20); // JSR init
        writeDriverByte(p++, static_cast<uint8_t>(initAddr & 0xFF));
        writeDriverByte(p++, static_cast<uint8_t>((initAddr >> 8) & 0xFF));
        writeDriverByte(p++, 0xA9); // LDA #$80
        writeDriverByte(p++, 0x80);
        writeDriverByte(p++, 0x8D); // STA $2000
        writeDriverByte(p++, 0x00);
        writeDriverByte(p++, 0x20);
        const uint16_t loopAddr = p;
        writeDriverByte(p++, 0x4C); // JMP loop
        writeDriverByte(p++, static_cast<uint8_t>(loopAddr & 0xFF));
        writeDriverByte(p++, static_cast<uint8_t>((loopAddr >> 8) & 0xFF));

        p = DRIVER_NMI_ADDR;
        writeDriverByte(p++, 0x48); // PHA
        writeDriverByte(p++, 0x8A); // TXA
        writeDriverByte(p++, 0x48); // PHA
        writeDriverByte(p++, 0x98); // TYA
        writeDriverByte(p++, 0x48); // PHA
        if(m_isPlaying) {
            writeDriverByte(p++, 0x20); // JSR play
            writeDriverByte(p++, static_cast<uint8_t>(playAddr & 0xFF));
            writeDriverByte(p++, static_cast<uint8_t>((playAddr >> 8) & 0xFF));
        }
        else {
            writeDriverByte(p++, 0xEA); // NOP
            writeDriverByte(p++, 0xEA); // NOP
            writeDriverByte(p++, 0xEA); // NOP
        }
        writeDriverByte(p++, 0x68); // PLA
        writeDriverByte(p++, 0xA8); // TAY
        writeDriverByte(p++, 0x68); // PLA
        writeDriverByte(p++, 0xAA); // TAX
        writeDriverByte(p++, 0x68); // PLA
        writeDriverByte(p++, 0x40); // RTI

        writeDriverByte(DRIVER_IRQ_ADDR, 0x40); // RTI
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

    void logState(const char* prefix) const
    {
        std::ostringstream ss;
        ss << prefix
           << " song=" << (static_cast<int>(m_songIndex) + 1)
           << "/" << static_cast<int>(m_nsf.totalSongs())
           << " init=$" << std::hex << std::uppercase << m_nsf.initAddress()
           << " play=$" << std::hex << std::uppercase << m_nsf.playAddress()
           << " load=$" << std::hex << std::uppercase << m_nsf.loadAddress()
           << " bankswitch=" << (m_nsf.usesBankSwitch() ? "on" : "off")
           << " playing=" << (m_isPlaying ? "on" : "off");
        Logger::instance().log(ss.str(), Logger::Type::DEBUG);
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
        initBanks();
        installDriver();
        logState("NSF mapper reset:");
    }

    uint8_t readPrg(int addr) override
    {
        const uint16_t cpuAddr = static_cast<uint16_t>(0x8000 + (addr & 0x7FFF));

        // Vectors
        if(cpuAddr == 0xFFFA) return static_cast<uint8_t>(DRIVER_NMI_ADDR & 0xFF);
        if(cpuAddr == 0xFFFB) return static_cast<uint8_t>((DRIVER_NMI_ADDR >> 8) & 0xFF);
        if(cpuAddr == 0xFFFC) return static_cast<uint8_t>(DRIVER_ADDR & 0xFF);
        if(cpuAddr == 0xFFFD) return static_cast<uint8_t>((DRIVER_ADDR >> 8) & 0xFF);
        if(cpuAddr == 0xFFFE) return static_cast<uint8_t>(DRIVER_IRQ_ADDR & 0xFF);
        if(cpuAddr == 0xFFFF) return static_cast<uint8_t>((DRIVER_IRQ_ADDR >> 8) & 0xFF);

        return readMappedNsfData(cpuAddr);
    }

    void writeMapperRegister(int addr, uint8_t data) override
    {
        // NSF bankswitch registers at $5FF8-$5FFF.
        // In this core mapper regs use relative addresses ($4000-$5FFF => $0000-$1FFF).
        if(addr >= 0x1FF8 && addr <= 0x1FFF) {
            m_bankRegs[static_cast<size_t>(addr & 0x07)] = data;
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
        const uint16_t playAddr = m_nsf.playAddress();
        if(m_isPlaying) {
            writeDriverByte(NMI_JSR_PLAY_ADDR + 0, 0x20);
            writeDriverByte(NMI_JSR_PLAY_ADDR + 1, static_cast<uint8_t>(playAddr & 0xFF));
            writeDriverByte(NMI_JSR_PLAY_ADDR + 2, static_cast<uint8_t>((playAddr >> 8) & 0xFF));
        }
        else {
            writeDriverByte(NMI_JSR_PLAY_ADDR + 0, 0xEA);
            writeDriverByte(NMI_JSR_PLAY_ADDR + 1, 0xEA);
            writeDriverByte(NMI_JSR_PLAY_ADDR + 2, 0xEA);
        }
        logState("NSF setPlaying:");
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
        logState("NSF setSong:");
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
