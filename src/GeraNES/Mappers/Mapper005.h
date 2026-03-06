#pragma once

#include <array>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>

#include "BaseMapper.h"
#include "../APU/APUCommon.h"

// MMC5
// Implemented features in this mapper:
// - PRG banking modes ($5100, $5113-$5117)
// - CHR banking modes ($5101, $5120-$512B, $5130)
// - Extended nametable mapping (CIRAM/ExRAM/fill) ($5104-$5107)
// - Scanline IRQ core registers ($5203/$5204)
// - 8x8 multiplier ($5205/$5206)
// - Expansion audio (2 pulse channels + PCM DAC) ($5000-$5015)
class Mapper005 : public BaseMapper
{
private: // GERA VIADÃO!!!!!!!
    uint8_t m_prgMode = 3;
    uint8_t m_chrMode = 3;
    uint8_t m_chrModeByte = 3;
    bool m_chrRamEnable = false;

    uint8_t m_exRamMode = 0;

    uint8_t m_prgRamProtect1 = 0;
    uint8_t m_prgRamProtect2 = 0;

    uint8_t m_nameTableMap[4] = {0, 0, 0, 0};

    uint8_t m_fillTile = 0;
    uint8_t m_fillAttribute = 0;

    uint8_t m_prgRamBank6000 = 0;

    uint8_t m_prgRegs[4] = {0x80, 0x80, 0x80, 0xFF};

    uint16_t m_chrSpriteRegs[8] = {0};
    uint16_t m_chrBgRegs[4] = {0};
    uint16_t m_chrMapA[8] = {0};
    uint16_t m_chrMapB[8] = {0};
    uint8_t m_chrUpperBits = 0;
    uint8_t m_abMode = 0;

    uint8_t m_irqScanline = 0;
    bool m_irqEnable = false;
    bool m_irqPending = false;
    bool m_inFrame = false;
    uint8_t m_irqCounter = 0;
    uint8_t m_matchCount = 0;
    bool m_ppuReading = false;
    uint16_t m_ppuReadAddress = 0;
    uint16_t m_lastPpuReadAddress = 0xFFFF;
    uint8_t m_idleCount = 0;

    uint8_t m_mulA = 0xFF;
    uint8_t m_mulB = 0xFF;
    uint8_t m_audioPulseRegs[2][4] = {{0}, {0}};
    uint8_t m_audioPcmControl = 0;
    uint8_t m_audioPcmValue = 0;
    uint8_t m_audioStatus = 0;
    bool m_audioPcmLatched = false;

    struct ExpansionPulseState {
        uint16_t timerCounter = 0;
        uint8_t dutyStep = 0;
        uint8_t lengthCounter = 0;
        uint8_t envelopeDivider = 0;
        uint8_t envelopeVolume = 0;
        bool envelopeStart = false;
    };
    ExpansionPulseState m_expPulse[2] = {};
    int m_expQuarterCounter = 0;
    int m_expHalfCounter = 0;
    float m_expansionAudioSample = 0.0f;

    static constexpr uint8_t MMC5_DUTY_TABLE[4][8] = {
        {0, 1, 0, 0, 0, 0, 0, 0},
        {0, 1, 1, 0, 0, 0, 0, 0},
        {0, 1, 1, 1, 1, 0, 0, 0},
        {1, 0, 0, 1, 1, 1, 1, 1}
    };
    uint8_t m_mmc5aRegs[2] = {0}; // $5207/$5208
    uint16_t m_mmc5aTimerCounter = 0; // $5209/$520A
    bool m_mmc5aTimerActive = false;
    bool m_mmc5aTimerIrqFlag = false;

    uint8_t m_prgRom8kMask = 0;
    uint16_t m_chrRom1kMask = 0;
    uint16_t m_chrRam1kMask = 0;
    uint16_t m_chr1kMask = 0;
    bool m_isSpriteFetch = false;
    bool m_sprite8x16 = false;
    bool m_renderingEnabled = false;
    bool m_substitutionsEnabled = false;
    uint8_t m_extAttrLatch = 0;
    uint8_t m_splitAttrLatch = 0;
    uint8_t m_splitMode = 0;
    uint8_t m_splitScroll = 0;
    uint8_t m_splitBank = 0;
    uint8_t m_splitVScroll = 0;
    bool m_splitActive = false;
    uint8_t m_splitTileCount = 0;
    uint8_t m_splitScanline = 0;
    int m_curTile = -1;
    int16_t m_lineCounter = -2;
    uint16_t m_lastNtRead = 0xFFFF;
    uint8_t m_sameNtReadCount = 0;
    bool m_lastSplitActiveLogged = false;
    bool m_lastSplitStateValid = false;
    uint16_t m_lastLoggedChrBank = 0xFFFF;
    uint8_t m_lastLoggedChrPage = 0xFF;
    bool m_traceInitDone = false;
    bool m_traceEnabled = false;
    bool m_traceChr = false;
    uint32_t m_traceLines = 0;
    std::ofstream m_traceFile;
    static constexpr uint32_t TRACE_MAX_LINES = 200000;

    std::array<uint8_t, 0x400> m_exRam = {};
    std::array<uint8_t, 0x400> m_mmc5aRam = {};
    std::array<uint8_t, 0x10000> m_prgRam = {};

    GERANES_INLINE uint16_t pulsePeriod(int channel) const
    {
        return static_cast<uint16_t>(
            (static_cast<uint16_t>(m_audioPulseRegs[channel][3] & 0x07) << 8) |
            m_audioPulseRegs[channel][2]);
    }

    GERANES_INLINE uint8_t pulseDuty(int channel) const
    {
        return static_cast<uint8_t>((m_audioPulseRegs[channel][0] >> 6) & 0x03);
    }

    GERANES_INLINE bool pulseLengthHalt(int channel) const
    {
        return (m_audioPulseRegs[channel][0] & 0x20) != 0;
    }

    GERANES_INLINE bool pulseConstantVolumeMode(int channel) const
    {
        return (m_audioPulseRegs[channel][0] & 0x10) != 0;
    }

    GERANES_INLINE uint8_t pulseVolumeParam(int channel) const
    {
        return static_cast<uint8_t>(m_audioPulseRegs[channel][0] & 0x0F);
    }

    GERANES_INLINE void clockPulseEnvelope(int channel)
    {
        ExpansionPulseState& p = m_expPulse[channel];

        if(p.envelopeStart) {
            p.envelopeStart = false;
            p.envelopeVolume = 0x0F;
            p.envelopeDivider = static_cast<uint8_t>(pulseVolumeParam(channel) + 1);
            return;
        }

        if(p.envelopeDivider > 0) {
            --p.envelopeDivider;
        }

        if(p.envelopeDivider == 0) {
            p.envelopeDivider = static_cast<uint8_t>(pulseVolumeParam(channel) + 1);
            if(p.envelopeVolume > 0) {
                --p.envelopeVolume;
            }
            else if(pulseLengthHalt(channel)) {
                p.envelopeVolume = 0x0F;
            }
        }
    }

    GERANES_INLINE void clockPulseLength(int channel)
    {
        if((m_audioStatus & (1 << channel)) == 0) {
            return;
        }

        ExpansionPulseState& p = m_expPulse[channel];
        if(!pulseLengthHalt(channel) && p.lengthCounter > 0) {
            --p.lengthCounter;
        }
    }

    GERANES_INLINE void stepPulseTimer(int channel)
    {
        ExpansionPulseState& p = m_expPulse[channel];
        const uint16_t period = pulsePeriod(channel);
        uint16_t timerPeriod = static_cast<uint16_t>((period + 1) << 1);
        if(timerPeriod < 2) {
            timerPeriod = 2;
        }

        if(p.timerCounter == 0) {
            p.timerCounter = static_cast<uint16_t>(timerPeriod - 1);
            p.dutyStep = static_cast<uint8_t>((p.dutyStep + 1) & 0x07);
        }
        else {
            --p.timerCounter;
        }
    }

    GERANES_INLINE float pulseOutput(int channel) const
    {
        if((m_audioStatus & (1 << channel)) == 0) {
            return 0.0f;
        }

        const ExpansionPulseState& p = m_expPulse[channel];
        if(p.lengthCounter == 0) {
            return 0.0f;
        }

        if(pulsePeriod(channel) < 8) {
            return 0.0f;
        }

        const uint8_t duty = pulseDuty(channel);
        const uint8_t dutyBit = MMC5_DUTY_TABLE[duty][p.dutyStep];
        const uint8_t rawVolume = pulseConstantVolumeMode(channel) ? pulseVolumeParam(channel) : p.envelopeVolume;
        const float volume = static_cast<float>(rawVolume) / 15.0f;

        return dutyBit ? volume : -volume;
    }

    GERANES_INLINE void updateExpansionAudioSample()
    {
        const float pulseMix = (pulseOutput(0) + pulseOutput(1)) * 0.5f;
        const float pcm = m_audioPcmLatched ? (static_cast<float>(static_cast<int>(m_audioPcmValue) - 128) / 128.0f) : 0.0f;

        float out = pulseMix * 0.60f + pcm * 0.25f;
        if(out > 1.0f) out = 1.0f;
        else if(out < -1.0f) out = -1.0f;

        m_expansionAudioSample = out;
    }

    GERANES_INLINE void writePulseRegister(int channel, int reg, uint8_t data)
    {
        m_audioPulseRegs[channel][reg] = data;
        ExpansionPulseState& p = m_expPulse[channel];

        if(reg == 3) {
            if(m_audioStatus & (1 << channel)) {
                p.lengthCounter = LENGTH_TABLE[(data >> 3) & 0x1F];
            }
            p.envelopeStart = true;
            p.dutyStep = 0;
            uint16_t timerPeriod = static_cast<uint16_t>((pulsePeriod(channel) + 1) << 1);
            if(timerPeriod < 2) timerPeriod = 2;
            p.timerCounter = static_cast<uint16_t>(timerPeriod - 1);
        }
    }

    GERANES_INLINE uint16_t calculateMask16(int nBanks) const
    {
        uint16_t mask = 0;
        int n = nBanks - 1;
        while(n > 0) {
            mask = static_cast<uint16_t>((mask << 1) | 1);
            n >>= 1;
        }
        return mask;
    }

    void initTraceIfNeeded()
    {
        if(m_traceInitDone) {
            return;
        }
        m_traceInitDone = true;

        const char* enabled = std::getenv("GERANES_MMC5_TRACE");
        if(enabled == nullptr || enabled[0] == '0') {
            return;
        }

        const char* path = std::getenv("GERANES_MMC5_TRACE_FILE");
        if(path == nullptr || path[0] == '\0') {
            path = "/tmp/geranes-mmc5-trace.log";
        }

        m_traceFile.open(path, std::ios::out | std::ios::trunc);
        m_traceEnabled = m_traceFile.is_open();
        const char* traceChr = std::getenv("GERANES_MMC5_TRACE_CHR");
        m_traceChr = (traceChr != nullptr && traceChr[0] != '0');

        if(m_traceEnabled) {
            m_traceFile << "MMC5 trace start\n";
            m_traceFile.flush();
        }
    }

    void traceLine(const std::string& s)
    {
        initTraceIfNeeded();
        if(!m_traceEnabled || m_traceLines >= TRACE_MAX_LINES) {
            return;
        }
        m_traceFile << s << '\n';
        ++m_traceLines;
        if((m_traceLines & 0x3FF) == 0) {
            m_traceFile.flush();
        }
    }

    GERANES_INLINE uint8_t fillAttributeByte() const
    {
        uint8_t v = m_fillAttribute & 0x03;
        return static_cast<uint8_t>(v | (v << 2) | (v << 4) | (v << 6));
    }

    GERANES_INLINE static uint8_t expandPaletteBits(uint8_t p)
    {
        p &= 0x03;
        return static_cast<uint8_t>(p | (p << 2) | (p << 4) | (p << 6));
    }

    GERANES_INLINE bool prgRamWriteEnabled() const
    {
        return (m_prgRamProtect1 & 0x03) == 0x02 && (m_prgRamProtect2 & 0x03) == 0x01;
    }

    GERANES_INLINE uint8_t readPrgRam8k(uint8_t bank8k, int addr)
    {
        uint8_t bank = bank8k & 0x07;
        return m_prgRam[(bank << log2(BankSize::B8K)) + (addr & (static_cast<int>(BankSize::B8K) - 1))];
    }

    GERANES_INLINE void writePrgRam8k(uint8_t bank8k, int addr, uint8_t data)
    {
        if(!prgRamWriteEnabled()) {
            return;
        }

        uint8_t bank = bank8k & 0x07;
        m_prgRam[(bank << log2(BankSize::B8K)) + (addr & (static_cast<int>(BankSize::B8K) - 1))] = data;
    }

    GERANES_INLINE uint8_t readPrgRom8k(uint8_t bank8k, int addr)
    {
        return m_cd.readPrg<BankSize::B8K>(bank8k & m_prgRom8kMask, addr);
    }

    struct PrgWindow {
        bool ram = false;
        bool writable = false;
        uint8_t bank8k = 0;
    };

    GERANES_INLINE PrgWindow resolvePrgWindow(int addr)
    {
        PrgWindow w;

        uint8_t slot = static_cast<uint8_t>((addr >> 13) & 0x03);

        auto decode8k = [&](uint8_t reg, bool forceRom) {
            w.ram = !forceRom && ((reg & 0x80) == 0);
            w.writable = w.ram;
            w.bank8k = reg & 0x7F;
        };

        auto decode16k = [&](uint8_t reg, uint8_t half, bool forceRom) {
            w.ram = !forceRom && ((reg & 0x80) == 0);
            w.writable = w.ram;
            // In 16K modes, register bit 0 is ignored and CPU A13 selects the 8K half.
            w.bank8k = static_cast<uint8_t>((reg & 0x7E) | (half & 0x01));
        };

        auto decode32k = [&](uint8_t reg, uint8_t quarter, bool forceRom) {
            w.ram = !forceRom && ((reg & 0x80) == 0);
            w.writable = w.ram;
            // In 32K mode, register bits 1..0 are ignored and CPU A14..A13 select 8K quarter.
            w.bank8k = static_cast<uint8_t>((reg & 0x7C) | (quarter & 0x03));
        };

        switch(m_prgMode & 0x03) {
        case 0:
            decode32k(m_prgRegs[3], slot, true);
            break;

        case 1:
            if(slot < 2) decode16k(m_prgRegs[1], slot & 0x01, false);
            else decode16k(m_prgRegs[3], slot & 0x01, true);
            break;

        case 2:
            if(slot < 2) decode16k(m_prgRegs[1], slot & 0x01, false);
            else if(slot == 2) decode8k(m_prgRegs[2], false);
            else decode8k(m_prgRegs[3], true);
            break;

        default:
            if(slot == 0) decode8k(m_prgRegs[0], false);
            else if(slot == 1) decode8k(m_prgRegs[1], false);
            else if(slot == 2) decode8k(m_prgRegs[2], false);
            else decode8k(m_prgRegs[3], true);
            break;
        }

        return w;
    }

    GERANES_INLINE void updateChrMaps()
    {
        switch(m_chrMode & 0x03) {
        case 0:
            for(int i = 0; i < 8; ++i) {
                m_chrMapA[i] = static_cast<uint16_t>((m_chrSpriteRegs[7] << 3) | i);
                m_chrMapB[i] = static_cast<uint16_t>((m_chrBgRegs[3] << 3) | i);
            }
            break;
        case 1:
            for(int i = 0; i < 4; ++i) {
                m_chrMapA[i] = static_cast<uint16_t>((m_chrSpriteRegs[3] << 2) | i);
                m_chrMapA[i + 4] = static_cast<uint16_t>((m_chrSpriteRegs[7] << 2) | i);
                m_chrMapB[i] = static_cast<uint16_t>((m_chrBgRegs[3] << 2) | i);
                m_chrMapB[i + 4] = static_cast<uint16_t>((m_chrBgRegs[3] << 2) | i);
            }
            break;
        case 2:
            for(int i = 0; i < 2; ++i) {
                m_chrMapA[i] = static_cast<uint16_t>((m_chrSpriteRegs[1] << 1) | i);
                m_chrMapA[i + 2] = static_cast<uint16_t>((m_chrSpriteRegs[3] << 1) | i);
                m_chrMapA[i + 4] = static_cast<uint16_t>((m_chrSpriteRegs[5] << 1) | i);
                m_chrMapA[i + 6] = static_cast<uint16_t>((m_chrSpriteRegs[7] << 1) | i);
                m_chrMapB[i] = static_cast<uint16_t>((m_chrBgRegs[1] << 1) | i);
                m_chrMapB[i + 2] = static_cast<uint16_t>((m_chrBgRegs[3] << 1) | i);
                m_chrMapB[i + 4] = static_cast<uint16_t>((m_chrBgRegs[1] << 1) | i);
                m_chrMapB[i + 6] = static_cast<uint16_t>((m_chrBgRegs[3] << 1) | i);
            }
            break;
        default:
            for(int i = 0; i < 8; ++i) {
                m_chrMapA[i] = m_chrSpriteRegs[i];
                m_chrMapB[i] = m_chrBgRegs[i & 0x03];
            }
            break;
        }
    }

    GERANES_INLINE uint16_t resolveChr1kBank(int addr) const
    {
        uint8_t page = static_cast<uint8_t>((addr >> 10) & 0x07);
        uint16_t bank = 0;

        // Split mode CHR mapping takes priority over extended attributes in the split region.
        if(!m_isSpriteFetch && m_renderingEnabled && m_substitutionsEnabled && m_splitActive && (m_splitMode & 0x80)) {
            uint16_t bank4k = m_splitBank;
            bank = static_cast<uint16_t>((bank4k << 2) | (page & 0x03));
            return bank & m_chr1kMask;
        }

        // In extended attribute mode, background CHR uses ExRAM per-tile 4K bank selection.
        if(!m_isSpriteFetch && m_renderingEnabled && m_substitutionsEnabled && m_exRamMode == 1) {
            uint16_t bank4k = static_cast<uint16_t>(((m_chrUpperBits & 0x03) << 6) | (m_extAttrLatch & 0x3F));
            bank = static_cast<uint16_t>((bank4k << 2) | (page & 0x03));
            return bank & m_chr1kMask;
        }

        // MMC5: in 8x16 sprite mode while rendering, CHR-A is sprite-source and CHR-B is bg-source.
        // Otherwise CHR source follows the last CHR register group written ($5120-$5127 or $5128-$512B).
        bool useA = (m_abMode == 0);
        if(m_sprite8x16 && m_renderingEnabled && m_substitutionsEnabled) {
            useA = m_isSpriteFetch;
        }

        bank = useA ? m_chrMapA[page] : m_chrMapB[page];

        return bank & m_chr1kMask;
    }

    GERANES_INLINE bool hasChrRom() const
    {
        return m_cd.chrSize() > 0;
    }

    GERANES_INLINE bool useChrRam() const
    {
        if(!hasChrRam()) {
            return false;
        }
        if(!hasChrRom()) {
            return true;
        }
        return m_chrRamEnable;
    }

    GERANES_INLINE void refreshChrMask()
    {
        m_chr1kMask = useChrRam() ? m_chrRam1kMask : m_chrRom1kMask;
    }

    GERANES_INLINE bool splitBackgroundActive() const
    {
        return !m_isSpriteFetch && m_renderingEnabled && m_substitutionsEnabled && m_splitActive && (m_splitMode & 0x80);
    }

    GERANES_INLINE uint8_t splitFineY() const
    {
        return static_cast<uint8_t>(m_splitVScroll & 0x07);
    }

    GERANES_INLINE uint8_t splitCoarseY() const
    {
        return static_cast<uint8_t>((m_splitVScroll >> 3) & 0x1F);
    }

    GERANES_INLINE int applySplitPatternRow(int addr) const
    {
        if(splitBackgroundActive()) {
            return (addr & ~0x07) | splitFineY();
        }
        return addr;
    }

public:
    Mapper005(ICartridgeData& cd) : BaseMapper(cd)
    {
        m_prgRom8kMask = calculateMask(m_cd.numberOfPRGBanks<BankSize::B8K>());

        if(hasChrRom()) {
            m_chrRom1kMask = calculateMask16(m_cd.numberOfCHRBanks<BankSize::B1K>());
        }
        if(hasChrRam()) {
            m_chrRam1kMask = calculateMask16(m_cd.chrRamSize() / static_cast<int>(BankSize::B1K));
        }
        refreshChrMask();

        updateChrMaps();
    }

    void reset() override
    {
        // Keep mapper register/RAM contents, but clear runtime detector/split state.
        // This is a practical approximation for reset behavior.
        m_irqPending = false;
        m_inFrame = false;
        m_irqCounter = 0;
        m_matchCount = 0;
        m_ppuReading = false;
        m_ppuReadAddress = 0;
        m_lastPpuReadAddress = 0xFFFF;
        m_idleCount = 0;
        m_splitActive = false;
        m_splitTileCount = 0;
        m_splitScanline = 0;
        m_splitVScroll = 0;
        m_curTile = -1;
        m_lineCounter = -2;
        m_lastNtRead = 0xFFFF;
        m_sameNtReadCount = 0;
        m_lastSplitStateValid = false;
        m_extAttrLatch = 0;
        m_splitAttrLatch = 0;

        // Multiplier power-on defaults per wiki notes.
        m_mulA = 0xFF;
        m_mulB = 0xFF;

        m_mmc5aTimerCounter = 0;
        m_mmc5aTimerActive = false;
        m_mmc5aTimerIrqFlag = false;
        m_audioPcmLatched = false;
        m_audioStatus = 0;
        m_expQuarterCounter = 0;
        m_expHalfCounter = 0;
        m_expansionAudioSample = 0.0f;
        for(int i = 0; i < 2; ++i) {
            m_expPulse[i] = ExpansionPulseState();
        }
        memset(m_audioPulseRegs, 0, sizeof(m_audioPulseRegs));
        refreshChrMask();
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        PrgWindow w = resolvePrgWindow(addr);

        if(w.ram) {
            return readPrgRam8k(w.bank8k, addr);
        }

        return readPrgRom8k(w.bank8k, addr);
    }

    GERANES_HOT void writePrg(int addr, uint8_t data) override
    {
        PrgWindow w = resolvePrgWindow(addr);

        if(w.ram && w.writable) {
            writePrgRam8k(w.bank8k, addr, data);
        }
    }

    GERANES_HOT uint8_t readSaveRam(int addr) override
    {
        return readPrgRam8k(m_prgRamBank6000, addr);
    }

    GERANES_HOT void writeSaveRam(int addr, uint8_t data) override
    {
        writePrgRam8k(m_prgRamBank6000, addr, data);
    }

    GERANES_HOT void writeChr(int addr, uint8_t data) override
    {
        if(!useChrRam()) {
            return;
        }

        int effectiveAddr = applySplitPatternRow(addr);
        uint16_t bank = resolveChr1kBank(effectiveAddr);
        writeChrRam<BankSize::B1K>(bank, effectiveAddr, data);
    }

    GERANES_HOT MirroringType mirroringType() override
    {
        return MirroringType::CUSTOM;
    }

    GERANES_HOT uint8_t customMirroring(uint8_t blockIndex) override
    {
        uint8_t mode = m_nameTableMap[blockIndex & 0x03] & 0x03;
        if(mode <= 1) {
            return mode;
        }

        // ExRAM/fill are handled via custom nametable hooks.
        return 0;
    }

    GERANES_HOT bool useCustomNameTable(uint8_t index) override
    {
        return (m_nameTableMap[index & 0x03] & 0x03) >= 2;
    }

    GERANES_HOT uint8_t readCustomNameTable(uint8_t index, uint16_t addr) override
    {
        uint8_t mapMode = m_nameTableMap[index & 0x03] & 0x03;
        addr &= 0x03FF;

        if(mapMode == 2) {
            if(m_exRamMode >= 2) {
                return 0;
            }
            return m_exRam[addr];
        }

        if(mapMode == 3) {
            if(addr < 0x03C0) {
                return m_fillTile;
            }
            return fillAttributeByte();
        }

        return 0;
    }

    GERANES_HOT void writeCustomNameTable(uint8_t index, uint16_t addr, uint8_t data) override
    {
        uint8_t mapMode = m_nameTableMap[index & 0x03] & 0x03;
        addr &= 0x03FF;

        if(mapMode == 2) {
            if((m_exRamMode == 0 || m_exRamMode == 1) && !m_renderingEnabled) {
                m_exRam[addr] = data;
            }
        }
    }

    GERANES_HOT void writeMapperRegister(int addr, uint8_t data) override
    {
        uint16_t absolute = static_cast<uint16_t>(addr + 0x4000);

        if(absolute >= 0x5C00 && absolute <= 0x5FFF) {
            // MMC5Plus/Nintendulator behavior:
            // mode 0/1: writes are only effective in-frame, otherwise write 0
            // mode 2: read/write RAM
            // mode 3: read-only (writes ignored)
            if(m_exRamMode != 3) {
                if(m_exRamMode == 2 || m_inFrame) {
                    m_exRam[absolute & 0x03FF] = data;
                }
                else {
                    m_exRam[absolute & 0x03FF] = 0;
                }
            }
            return;
        }

        if(absolute >= 0x5800 && absolute <= 0x5BFF) {
            m_mmc5aRam[absolute & 0x03FF] = data;
            return;
        }

        switch(absolute) {
        case 0x5000:
        case 0x5001:
        case 0x5002:
        case 0x5003: {
            int reg = absolute - 0x5000;
            writePulseRegister(0, reg, data);
            break;
        }

        case 0x5004:
        case 0x5005:
        case 0x5006:
        case 0x5007: {
            int reg = absolute - 0x5004;
            writePulseRegister(1, reg, data);
            break;
        }

        case 0x5010:
            m_audioPcmControl = data;
            break;

        case 0x5011:
            m_audioPcmValue = data;
            m_audioPcmLatched = true;
            break;

        case 0x5015:
            m_audioStatus = static_cast<uint8_t>(data & 0x03);
            if((m_audioStatus & 0x01) == 0) {
                m_expPulse[0].lengthCounter = 0;
            }
            if((m_audioStatus & 0x02) == 0) {
                m_expPulse[1].lengthCounter = 0;
            }
            break;

        case 0x5100:
            m_prgMode = data & 0x03;
            break;

        case 0x5101:
            m_chrModeByte = data;
            m_chrMode = data & 0x03;
            m_chrRamEnable = (data & 0x80) != 0;
            refreshChrMask();
            updateChrMaps();
            traceLine(
                "reg 5101 chrMode=" + std::to_string(m_chrMode) +
                " chrRamEnable=" + std::to_string(m_chrRamEnable ? 1 : 0));
            break;

        case 0x5102:
            m_prgRamProtect1 = data;
            break;

        case 0x5103:
            m_prgRamProtect2 = data;
            break;

        case 0x5104:
            m_exRamMode = data & 0x03;
            traceLine("reg 5104 exRamMode=" + std::to_string(m_exRamMode));
            break;

        case 0x5105:
            m_nameTableMap[0] = data & 0x03;
            m_nameTableMap[1] = (data >> 2) & 0x03;
            m_nameTableMap[2] = (data >> 4) & 0x03;
            m_nameTableMap[3] = (data >> 6) & 0x03;
            traceLine(
                "reg 5105 ntMap=" + std::to_string(data) +
                " [" + std::to_string(m_nameTableMap[0]) +
                "," + std::to_string(m_nameTableMap[1]) +
                "," + std::to_string(m_nameTableMap[2]) +
                "," + std::to_string(m_nameTableMap[3]) + "]");
            break;

        case 0x5106:
            m_fillTile = data;
            break;

        case 0x5107:
            m_fillAttribute = data & 0x03;
            break;

        case 0x5113:
            m_prgRamBank6000 = data & 0x07;
            break;

        case 0x5114:
            m_prgRegs[0] = data;
            break;

        case 0x5115:
            m_prgRegs[1] = data;
            break;

        case 0x5116:
            m_prgRegs[2] = data;
            break;

        case 0x5117:
            m_prgRegs[3] = data;
            break;

        case 0x5120:
        case 0x5121:
        case 0x5122:
        case 0x5123:
        case 0x5124:
        case 0x5125:
        case 0x5126:
        case 0x5127:
            m_abMode = 0;
            m_chrSpriteRegs[absolute - 0x5120] = static_cast<uint16_t>(data | ((m_chrUpperBits & 0x03) << 8));
            updateChrMaps();
            traceLine("reg " + std::to_string(absolute) + " chra[" + std::to_string(absolute - 0x5120) + "]=" + std::to_string(data));
            break;

        case 0x5128:
        case 0x5129:
        case 0x512A:
        case 0x512B:
            m_abMode = 1;
            m_chrBgRegs[absolute - 0x5128] = static_cast<uint16_t>(data | ((m_chrUpperBits & 0x03) << 8));
            updateChrMaps();
            traceLine("reg " + std::to_string(absolute) + " chrb[" + std::to_string(absolute - 0x5128) + "]=" + std::to_string(data));
            break;

        case 0x5130:
            m_chrUpperBits = data & 0x03;
            break;

        case 0x5203:
            m_irqScanline = data;
            break;

        case 0x5204:
            m_irqEnable = (data & 0x80) != 0;
            break;

        case 0x5205:
            m_mulA = data;
            break;

        case 0x5206:
            m_mulB = data;
            break;

        case 0x5207:
        case 0x5208:
            m_mmc5aRegs[absolute - 0x5207] = data;
            break;

        case 0x5209:
            m_mmc5aTimerCounter = static_cast<uint16_t>((m_mmc5aTimerCounter & 0xFF00) | data);
            m_mmc5aTimerActive = (m_mmc5aTimerCounter != 0);
            break;

        case 0x520A:
            m_mmc5aTimerCounter = static_cast<uint16_t>((m_mmc5aTimerCounter & 0x00FF) | (static_cast<uint16_t>(data) << 8));
            break;

        case 0x5200:
            m_splitMode = data;
            traceLine("reg 5200 splitMode=" + std::to_string(m_splitMode));
            break;

        case 0x5201:
            m_splitScroll = data;
            traceLine("reg 5201 splitScroll=" + std::to_string(m_splitScroll));
            break;

        case 0x5202:
            m_splitBank = data;
            traceLine("reg 5202 splitBank=" + std::to_string(m_splitBank));
            break;
        }
    }

    GERANES_HOT uint8_t readMapperRegister(int addr, uint8_t openBusData) override
    {
        uint16_t absolute = static_cast<uint16_t>(addr + 0x4000);

        if(absolute >= 0x5C00 && absolute <= 0x5FFF) {
            if(m_exRamMode <= 1) {
                return openBusData;
            }
            return m_exRam[absolute & 0x03FF];
        }

        if(absolute >= 0x5800 && absolute <= 0x5BFF) {
            return m_mmc5aRam[absolute & 0x03FF];
        }

        switch(absolute) {
        case 0x5015:
            return static_cast<uint8_t>(
                (m_expPulse[0].lengthCounter > 0 ? 0x01 : 0x00) |
                (m_expPulse[1].lengthCounter > 0 ? 0x02 : 0x00));

        case 0x5204: {
            uint8_t ret = 0;
            if(m_irqPending) ret |= 0x80;
            if(m_inFrame) ret |= 0x40;
            m_irqPending = false;
            return ret;
        }

        case 0x5205:
            return static_cast<uint8_t>((m_mulA * m_mulB) & 0x00FF);

        case 0x5206:
            return static_cast<uint8_t>(((m_mulA * m_mulB) >> 8) & 0x00FF);

        case 0x5207:
        case 0x5208:
            return m_mmc5aRegs[absolute - 0x5207];

        case 0x5209: {
            if(m_mmc5aTimerActive) {
                return 0;
            }
            uint8_t ret = static_cast<uint8_t>(m_mmc5aTimerIrqFlag ? 0x80 : 0x00);
            m_mmc5aTimerIrqFlag = false;
            return ret;
        }

        case 0x520A:
            return openBusData;
        }

        return openBusData;
    }

    GERANES_HOT bool getInterruptFlag() override
    {
        return (m_irqEnable && m_irqPending) || m_mmc5aTimerIrqFlag;
    }

    GERANES_HOT void onScanlineStart(bool renderingEnabled, int scanline) override
    {
        m_renderingEnabled = renderingEnabled;

        if(!renderingEnabled) {
            m_inFrame = false;
            m_splitActive = false;
            m_splitTileCount = 0;
            m_splitScanline = 0;
            m_splitAttrLatch = 0;
            return;
        }

        m_splitActive = false;
        m_splitTileCount = 0;
        m_splitAttrLatch = 0;
        if(scanline >= 0 && scanline < 240) {
            m_splitScanline = static_cast<uint8_t>(scanline);
        }
        else {
            m_splitScanline = 0;
        }
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);

        SERIALIZEDATA(s, m_prgMode);
        SERIALIZEDATA(s, m_chrMode);
        SERIALIZEDATA(s, m_chrModeByte);
        SERIALIZEDATA(s, m_chrRamEnable);
        SERIALIZEDATA(s, m_exRamMode);

        SERIALIZEDATA(s, m_prgRamProtect1);
        SERIALIZEDATA(s, m_prgRamProtect2);

        s.array(m_nameTableMap, 1, 4);

        SERIALIZEDATA(s, m_fillTile);
        SERIALIZEDATA(s, m_fillAttribute);

        SERIALIZEDATA(s, m_prgRamBank6000);

        s.array(m_prgRegs, 1, 4);
        s.array(reinterpret_cast<uint8_t*>(m_chrSpriteRegs), 1, static_cast<int>(sizeof(m_chrSpriteRegs)));
        s.array(reinterpret_cast<uint8_t*>(m_chrBgRegs), 1, static_cast<int>(sizeof(m_chrBgRegs)));
        s.array(reinterpret_cast<uint8_t*>(m_chrMapA), 1, static_cast<int>(sizeof(m_chrMapA)));
        s.array(reinterpret_cast<uint8_t*>(m_chrMapB), 1, static_cast<int>(sizeof(m_chrMapB)));
        SERIALIZEDATA(s, m_chrUpperBits);
        SERIALIZEDATA(s, m_abMode);

        SERIALIZEDATA(s, m_irqScanline);
        SERIALIZEDATA(s, m_irqEnable);
        SERIALIZEDATA(s, m_irqPending);
        SERIALIZEDATA(s, m_inFrame);
        SERIALIZEDATA(s, m_irqCounter);
        SERIALIZEDATA(s, m_matchCount);
        SERIALIZEDATA(s, m_ppuReading);
        SERIALIZEDATA(s, m_ppuReadAddress);
        SERIALIZEDATA(s, m_lastPpuReadAddress);
        SERIALIZEDATA(s, m_idleCount);

        SERIALIZEDATA(s, m_mulA);
        SERIALIZEDATA(s, m_mulB);
        s.array(reinterpret_cast<uint8_t*>(m_audioPulseRegs), 1, static_cast<int>(sizeof(m_audioPulseRegs)));
        SERIALIZEDATA(s, m_audioPcmControl);
        SERIALIZEDATA(s, m_audioPcmValue);
        SERIALIZEDATA(s, m_audioStatus);
        SERIALIZEDATA(s, m_audioPcmLatched);
        for(int i = 0; i < 2; ++i) {
            SERIALIZEDATA(s, m_expPulse[i].timerCounter);
            SERIALIZEDATA(s, m_expPulse[i].dutyStep);
            SERIALIZEDATA(s, m_expPulse[i].lengthCounter);
            SERIALIZEDATA(s, m_expPulse[i].envelopeDivider);
            SERIALIZEDATA(s, m_expPulse[i].envelopeVolume);
            SERIALIZEDATA(s, m_expPulse[i].envelopeStart);
        }
        SERIALIZEDATA(s, m_expQuarterCounter);
        SERIALIZEDATA(s, m_expHalfCounter);
        SERIALIZEDATA(s, m_expansionAudioSample);
        s.array(reinterpret_cast<uint8_t*>(m_mmc5aRegs), 1, static_cast<int>(sizeof(m_mmc5aRegs)));
        SERIALIZEDATA(s, m_mmc5aTimerCounter);
        SERIALIZEDATA(s, m_mmc5aTimerActive);
        SERIALIZEDATA(s, m_mmc5aTimerIrqFlag);

        SERIALIZEDATA(s, m_prgRom8kMask);
        SERIALIZEDATA(s, m_chrRom1kMask);
        SERIALIZEDATA(s, m_chrRam1kMask);
        SERIALIZEDATA(s, m_chr1kMask);
        SERIALIZEDATA(s, m_isSpriteFetch);
        SERIALIZEDATA(s, m_sprite8x16);
        SERIALIZEDATA(s, m_renderingEnabled);
        SERIALIZEDATA(s, m_extAttrLatch);
        SERIALIZEDATA(s, m_splitAttrLatch);
        SERIALIZEDATA(s, m_splitMode);
        SERIALIZEDATA(s, m_splitScroll);
        SERIALIZEDATA(s, m_splitBank);
        SERIALIZEDATA(s, m_splitVScroll);
        SERIALIZEDATA(s, m_splitActive);
        SERIALIZEDATA(s, m_splitTileCount);
        SERIALIZEDATA(s, m_splitScanline);
        SERIALIZEDATA(s, m_curTile);
        SERIALIZEDATA(s, m_lineCounter);
        SERIALIZEDATA(s, m_lastNtRead);
        SERIALIZEDATA(s, m_sameNtReadCount);

        s.array(m_exRam.data(), 1, static_cast<int>(m_exRam.size()));
        s.array(m_mmc5aRam.data(), 1, static_cast<int>(m_mmc5aRam.size()));
        s.array(m_prgRam.data(), 1, static_cast<int>(m_prgRam.size()));
        refreshChrMask();
    }

    GERANES_HOT void setPpuFetchSource(bool isSpriteFetch) override
    {
        m_isSpriteFetch = isSpriteFetch;
    }

    GERANES_HOT void setSpriteSize8x16(bool sprite8x16) override
    {
        m_sprite8x16 = sprite8x16;
        traceLine(std::string("ppu sprite8x16=") + (m_sprite8x16 ? "1" : "0"));
    }

    GERANES_HOT void setPpuReadAffectsBus(bool affectsBus) override
    {
        (void)affectsBus;
    }

    GERANES_HOT void setPpuMask(uint8_t mask) override
    {
        bool newEnabled = (mask & 0x18) != 0;
        traceLine(
            "ppu mask=" + std::to_string(mask) +
            " bg=" + std::to_string((mask & 0x08) ? 1 : 0) +
            " spr=" + std::to_string((mask & 0x10) ? 1 : 0) +
            " subs=" + std::to_string(newEnabled ? 1 : 0));
        m_substitutionsEnabled = newEnabled;

        // MMC5+ behavior (mirrors Nintendulator): when both BG and sprites are disabled,
        // reset frame/read detector state.
        if(!newEnabled) {
            m_inFrame = false;
            m_lineCounter = -2;
            m_splitActive = false;
            m_splitTileCount = 0;
            m_splitAttrLatch = 0;
            m_curTile = -1;
        }
    }

    GERANES_HOT void onPpuStatusRead(bool vblankSet) override
    {
        (void)vblankSet;
    }

    GERANES_HOT void onCpuRead(uint16_t addr) override
    {
        (void)addr;
    }

    GERANES_HOT void onPpuRead(uint16_t addr) override
    {
        m_ppuReadAddress = static_cast<uint16_t>(addr & 0x3FFF);
    }

    GERANES_HOT void cycle() override
    {
        if(m_mmc5aTimerActive && m_mmc5aTimerCounter > 0) {
            if(m_mmc5aTimerCounter == 1) {
                m_mmc5aTimerCounter = 0;
                m_mmc5aTimerActive = false;
                m_mmc5aTimerIrqFlag = true;
            }
            else {
                --m_mmc5aTimerCounter;
            }
        }

        stepPulseTimer(0);
        stepPulseTimer(1);

        if(++m_expQuarterCounter >= 7457) {
            m_expQuarterCounter -= 7457;
            clockPulseEnvelope(0);
            clockPulseEnvelope(1);
        }

        if(++m_expHalfCounter >= 14914) {
            m_expHalfCounter -= 14914;
            clockPulseLength(0);
            clockPulseLength(1);
        }

        updateExpansionAudioSample();
    }

    GERANES_HOT void onPpuCycle(int scanline, int cycle, bool isRendering, bool isPreLine) override
    {
        if(scanline == 240 && cycle == 0) {
            m_splitActive = false;
            m_splitTileCount = 0;
            m_splitAttrLatch = 0;
            m_curTile = -1;
            m_lineCounter = -2;
            m_inFrame = false;
        }

        if(!isRendering) {
            return;
        }

        if(cycle == 0) {
            if(scanline == 0) {
                m_irqPending = false;
            }
            if(scanline == 1) {
                m_inFrame = true;
            }

            if(m_lineCounter < 0x7FFF) {
                ++m_lineCounter;
            }
            if(m_irqScanline != 0 && m_lineCounter == static_cast<int16_t>(m_irqScanline)) {
                m_irqPending = true;
            }
        }

        if(cycle == 320) {
            m_curTile = -1;
            m_splitTileCount = 0;

            if(isPreLine) {
                m_splitVScroll = m_splitScroll;
                if(m_splitVScroll >= 240) {
                    m_splitVScroll = static_cast<uint8_t>(m_splitVScroll - 16);
                }
            }
            else if(scanline >= 0 && scanline < 240) {
                m_splitVScroll = static_cast<uint8_t>(m_splitVScroll + 1);
            }

            if(m_splitVScroll >= 240) {
                m_splitVScroll = static_cast<uint8_t>(m_splitVScroll - 240);
            }
        }

        if((cycle & 0x07) == 0 && cycle < 336) {
            ++m_curTile;

            bool splitEnabled = (m_splitMode & 0x80) && ((m_exRamMode & 0x02) == 0);
            if(!splitEnabled) {
                m_splitActive = false;
                return;
            }

            uint8_t threshold = m_splitMode & 0x1F;
            bool splitRight = (m_splitMode & 0x40) != 0;
            if(splitRight) {
                if(m_curTile == 0) {
                    m_splitActive = false;
                }
                else if(m_curTile == threshold) {
                    m_splitActive = true;
                }
                else if(m_curTile == 34) {
                    m_splitActive = false;
                }
            }
            else {
                if(m_curTile == 0) {
                    m_splitActive = true;
                }
                else if(m_curTile == threshold) {
                    m_splitActive = false;
                }
                else if(m_curTile >= 34) {
                    m_splitActive = false;
                }
            }
        }
    }

    GERANES_HOT uint8_t transformNameTableRead(uint8_t index, uint16_t addr, uint8_t value) override
    {
        if(!m_isSpriteFetch && m_renderingEnabled && m_substitutionsEnabled && addr < 0x03C0) {
            ++m_splitTileCount;
            int curTile = m_curTile;
            if(curTile < 0) {
                curTile = static_cast<int>(m_splitTileCount) - 1;
            }
            uint8_t fetchTileX = static_cast<uint8_t>(curTile & 0x1F);

            bool splitEnabled = (m_splitMode & 0x80) && ((m_exRamMode & 0x02) == 0);
            if(!splitEnabled) {
                m_splitActive = false;
            }

            if(m_splitActive && splitEnabled) {
                uint8_t tileY = splitCoarseY();
                uint8_t splitTileX = fetchTileX;
                uint16_t exAddr = static_cast<uint16_t>(((tileY << 5) | splitTileX) & 0x03FF);
                uint16_t attrAddr = static_cast<uint16_t>((0x03C0 | ((tileY >> 2) << 3) | (splitTileX >> 2)) & 0x03FF);
                m_extAttrLatch = m_exRam[exAddr];
                uint8_t attrByte = m_exRam[attrAddr];
                uint8_t shift = static_cast<uint8_t>(((tileY & 0x02) << 1) | (splitTileX & 0x02));
                m_splitAttrLatch = expandPaletteBits(static_cast<uint8_t>((attrByte >> shift) & 0x03));
                value = m_extAttrLatch;
                if(fetchTileX == 0 || fetchTileX == 16 || fetchTileX == 31) {
                    traceLine(
                        "split nt idx=" + std::to_string(index) +
                        " x=" + std::to_string(fetchTileX) +
                        " y=" + std::to_string(m_splitScanline) +
                        " ex=" + std::to_string(exAddr) +
                        " exv=" + std::to_string(m_extAttrLatch));
                }
            }
            else {
                m_extAttrLatch = m_exRam[addr & 0x03FF];
                m_splitAttrLatch = 0;
            }

            if(splitEnabled && (!m_lastSplitStateValid || m_lastSplitActiveLogged != m_splitActive)) {
                m_lastSplitStateValid = true;
                m_lastSplitActiveLogged = m_splitActive;
                traceLine(
                    "split state active=" + std::to_string(m_splitActive ? 1 : 0) +
                    " mode=" + std::to_string(m_splitMode) +
                    " x=" + std::to_string(fetchTileX) +
                    " nt=" + std::to_string(index));
            }
        }
        else {
            if(m_splitMode & 0x80) {
                m_lastSplitStateValid = false;
            }
        }

        if(!m_isSpriteFetch && m_renderingEnabled && m_substitutionsEnabled && addr >= 0x03C0 && m_splitActive && (m_splitMode & 0x80)) {
            return m_splitAttrLatch;
        }

        if(!m_isSpriteFetch && m_renderingEnabled && m_substitutionsEnabled && m_exRamMode == 1 && addr >= 0x03C0) {
            uint8_t p = (m_extAttrLatch >> 6) & 0x03;
            return expandPaletteBits(p);
        }

        return value;
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        int effectiveAddr = applySplitPatternRow(addr);
        uint16_t bank = resolveChr1kBank(effectiveAddr);
        uint8_t page = static_cast<uint8_t>((effectiveAddr >> 10) & 0x07);

        if(m_traceChr && (page != m_lastLoggedChrPage || bank != m_lastLoggedChrBank)) {
            m_lastLoggedChrPage = page;
            m_lastLoggedChrBank = bank;
            traceLine(
                "chr page=" + std::to_string(page) +
                " bank=" + std::to_string(bank) +
                " mode=" + std::to_string(m_chrMode) +
                " spr=" + std::to_string(m_isSpriteFetch ? 1 : 0) +
                " spr8x16=" + std::to_string(m_sprite8x16 ? 1 : 0) +
                " exMode=" + std::to_string(m_exRamMode) +
                " split=" + std::to_string(m_splitActive ? 1 : 0));
        }

        if(useChrRam()) {
            return readChrRam<BankSize::B1K>(bank, effectiveAddr);
        }

        return m_cd.readChr<BankSize::B1K>(bank, effectiveAddr);
    }

    GERANES_HOT float getExpansionAudioSample() override
    {
        return m_expansionAudioSample;
    }
};
