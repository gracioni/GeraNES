#pragma once

#ifdef ENABLE_NSF_PLAYER

#include <array>
#include <memory>
#include <cstring>
#include <cstdint>
#include "BaseMapper.h"
#include "NsfExpansionAudio/NsfExpansionAudioFactory.h"
#include "NsfExpansionAudio/NsfExpansionAudioBase.h"
#include "GeraNES/NesCartridgeData/_NsfFormat.h"
#include "GeraNES/util/NesAssembler.h"

class MapperNSF : public BaseMapper
{
private:
    static constexpr uint16_t DRIVER_ADDR = 0x7F00;
    static constexpr uint16_t DRIVER_PLAY_ADDR = 0x7F40;
    static constexpr int PLAY_ACK_REG = 0x1FF6;
    static constexpr int INIT_DONE_REG = 0x1FF7;
    _NsfFormat& m_nsf;
    std::array<uint8_t, 8> m_bankRegs = {0, 1, 2, 3, 4, 5, 6, 7};
    uint8_t m_songIndex = 0; // 0-based
    bool m_isPlaying = true;
    bool m_initDone = false;
    bool m_playRequestPending = false;
    bool m_playInFlight = false;
    uint32_t m_playCounter = 0;
    std::unique_ptr<NsfExpansionAudioBase> m_audioOwner;
    NsfExpansionAudioBase* m_audio = nullptr;

    GERANES_INLINE uint8_t readMappedNsfData(uint16_t cpuAddr) const
    {
        const int rel = static_cast<int>(cpuAddr) - 0x8000;
        if(rel < 0) return 0;

        if(m_nsf.usesBankSwitch()) {
            const int slot = rel >> 12; // 4KB slot in $8000-$FFFF
            const int bank = static_cast<int>(m_bankRegs[static_cast<size_t>(slot)]) & 0xFF;
            const int padding = static_cast<int>(m_nsf.loadAddress() & 0x0FFF);
            const int offset = (bank << 12) | (rel & 0x0FFF);
            const int payloadOffset = offset - padding;
            if(payloadOffset < 0) {
                return 0;
            }
            if(payloadOffset < m_nsf.prgSize()) {
                return m_nsf.readPrg(payloadOffset);
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
        //   LDA #$01
        //   STA $5FF7    ; mark init completed
        // loop:
        //   JMP loop
        //
        // $7F40 play:
        //   JSR play
        //   STA $5FF6    ; clear/reload IRQ timer
        //   JMP loop
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
        resetDriver.ldaImm(0x01);
        resetDriver.staAbs(static_cast<uint16_t>(0x4000 + INIT_DONE_REG));
        const uint16_t loopAddr = resetDriver.position();
        resetDriver.jmp(loopAddr);

        NesAssembler playDriver(emitDriverByte, DRIVER_PLAY_ADDR);
        playDriver.jsr(playAddr);
        playDriver.ldaImm(0x00);
        playDriver.staAbs(static_cast<uint16_t>(0x4000 + PLAY_ACK_REG));
        playDriver.jmp(loopAddr);
    }

    uint32_t getPlayReloadValue() const
    {
        const uint32_t speedUs = m_nsf.playSpeedNtsc();
        const double clocks = static_cast<double>(speedUs) * (1789773.0 / 1000000.0);
        return static_cast<uint32_t>(clocks);
    }

    void clearAndReloadPlayTimer()
    {
        m_playRequestPending = false;
        m_playCounter = getPlayReloadValue();
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
        , m_songIndex(static_cast<uint8_t>((m_nsf.startSong() > 0 ? m_nsf.startSong() : 1) - 1))
        , m_audioOwner(NsfExpansionAudioFactory::createForNsf(m_nsf.soundChipFlags()))
        , m_audio(m_audioOwner.get())
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
        m_initDone = false;
        m_playRequestPending = false;
        m_playInFlight = false;
        m_playCounter = 0;
        if(m_audio != nullptr) {
            m_audio->reset(1789773);
        }
        initBanks();
        installDriver();
    }

    uint8_t readPrg(int addr) override
    {
        const uint16_t cpuAddr = static_cast<uint16_t>(0x8000 + (addr & 0x7FFF));

        // Vectors
        if(cpuAddr == 0xFFFC) return static_cast<uint8_t>(DRIVER_ADDR & 0xFF);
        if(cpuAddr == 0xFFFD) return static_cast<uint8_t>((DRIVER_ADDR >> 8) & 0xFF);

        return readMappedNsfData(cpuAddr);
    }

    void cycle() override
    {
        if(m_audio != nullptr) {
            m_audio->clock();
        }

        if(!m_isPlaying || !m_initDone || m_playInFlight) return;
        if(m_playCounter > 0) {
            --m_playCounter;
            if(m_playCounter == 0) {
                m_playRequestPending = true;
            }
        }
    }

    bool getInterruptFlag() override
    {
        return false;
    }

    uint8_t readMapperRegisterAbsolute(uint16_t cpuAddr, uint8_t openBusData) override
    {
        if(m_audio != nullptr && m_audio->handlesRegister(cpuAddr)) {
            return m_audio->readRegister(cpuAddr, openBusData);
        }

        return openBusData;
    }

    void writeMapperRegisterAbsolute(uint16_t cpuAddr, uint8_t data) override
    {
        if(m_audio != nullptr && m_audio->handlesRegister(cpuAddr)) {
            m_audio->writeRegister(cpuAddr, data);
            return;
        }

        if(cpuAddr == static_cast<uint16_t>(0x4000 + PLAY_ACK_REG)) {
            m_playInFlight = false;
            if(m_isPlaying) {
                clearAndReloadPlayTimer();
            }
            return;
        }

        if(cpuAddr == static_cast<uint16_t>(0x4000 + INIT_DONE_REG)) {
            m_initDone = true;
            if(m_isPlaying && !m_playInFlight) {
                clearAndReloadPlayTimer();
            }
            return;
        }

        if(cpuAddr >= 0x5FF8 && cpuAddr <= 0x5FFF) {
            m_bankRegs[static_cast<size_t>(cpuAddr & 0x07)] = data;
        }
    }

    void writeSaveRam(int addr, uint8_t data) override
    {
        BaseMapper::writeSaveRam(addr, data);
    }

    float getExpansionAudioSample() override
    {
        return m_audio != nullptr ? m_audio->getSample() : 0.0f;
    }

    float getMixWeight() const override
    {
        return m_audio != nullptr ? m_audio->getMixWeight() : 0.0f;
    }

    float getExpansionOutputGain() const override
    {
        return m_audio != nullptr ? m_audio->getOutputGain() : 1.0f;
    }

    std::string getAudioChannelsJson() const override
    {
        return m_audio != nullptr ? m_audio->getAudioChannelsJson() : "{\"channels\":[]}";
    }

    bool setAudioChannelVolumeById(const std::string& id, float volume) override
    {
        return m_audio != nullptr && m_audio->setAudioChannelVolumeById(id, volume);
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        s.array(m_bankRegs.data(), 1, static_cast<int>(m_bankRegs.size()));
        SERIALIZEDATA(s, m_songIndex);
        SERIALIZEDATA(s, m_isPlaying);
        SERIALIZEDATA(s, m_initDone);
        SERIALIZEDATA(s, m_playRequestPending);
        SERIALIZEDATA(s, m_playInFlight);
        SERIALIZEDATA(s, m_playCounter);
        if(m_audio != nullptr) {
            m_audio->serialization(s);
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
            m_playRequestPending = false;
            m_playCounter = 0;
        } else if(m_initDone && !m_playInFlight && m_playCounter == 0 && !m_playRequestPending) {
            clearAndReloadPlayTimer();
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

    bool consumeNsfPlayerInstructionRedirect(uint16_t& cpuAddr) override
    {
        if(!m_isPlaying || !m_initDone || m_playInFlight || !m_playRequestPending) {
            return false;
        }

        m_playRequestPending = false;
        m_playInFlight = true;
        cpuAddr = DRIVER_PLAY_ADDR;
        return true;
    }

    void preloadNsfMemory(uint8_t* cpuRam, size_t cpuRamSize) override
    {
        if(cpuRam == nullptr || cpuRamSize == 0) {
            return;
        }

        const uint16_t loadAddr = m_nsf.loadAddress();
        if(loadAddr >= 0x8000) {
            return;
        }

        const size_t lowSegmentSize = std::min<size_t>(
            static_cast<size_t>(0x8000 - loadAddr),
            static_cast<size_t>(m_nsf.prgSize())
        );

        uint8_t* const workRam = saveRamData();
        const size_t workRamSize = saveRamSize();

        for(size_t i = 0; i < lowSegmentSize; ++i) {
            const uint16_t targetAddr = static_cast<uint16_t>(loadAddr + i);
            const uint8_t value = m_nsf.readPrg(static_cast<int>(i));

            if(targetAddr < 0x2000) {
                cpuRam[targetAddr & static_cast<uint16_t>(cpuRamSize - 1)] = value;
            }
            else if(targetAddr >= 0x6000 && targetAddr < 0x8000 && workRam != nullptr && workRamSize > 0) {
                workRam[targetAddr - 0x6000] = value;
            }
        }
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

#endif
