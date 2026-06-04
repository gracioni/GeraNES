#pragma once

#include "signal/signal.h"

#include "Settings.h"
#include "Cartridge.h"

#include "Serialization.h"

#include <array>
#include <algorithm>
#include <vector>

namespace GeraNES {

const int VBLANK_CYCLE = 1;

//0xAABBGGRR
const unsigned int NES_PALETTE[64] = {
    0xFF7C7C7C, 0xFFFC0000, 0xFFBC0000, 0xFFBC2844, 0xFF840094, 0xFF2000A8, 0xFF0010A8, 0xFF001488,
    0xFF003050, 0xFF007800, 0xFF006800, 0xFF005800, 0xFF584000, 0xFF000000, 0xFF000000, 0xFF000000,
    0xFFBCBCBC, 0xFFF87800, 0xFFF85800, 0xFFFC4468, 0xFFCC00D8, 0xFF5800E4, 0xFF0038F8, 0xFF105CE4,
    0xFF007CAC, 0xFF00B800, 0xFF00A800, 0xFF44A800, 0xFF888800, 0xFF000000, 0xFF000000, 0xFF000000,
    0xFFF8F8F8, 0xFFFCBC3C, 0xFFFC8868, 0xFFF87898, 0xFFF878F8, 0xFF9858F8, 0xFF5878F8, 0xFF44A0FC,
    0xFF00B8F8, 0xFF18F8B8, 0xFF54D858, 0xFF98F858, 0xFFD8E800, 0xFF787878, 0xFF000000, 0xFF000000,
    0xFFFCFCFC, 0xFFFCE4A4, 0xFFF8B8B8, 0xFFF8B8D8, 0xFFF8B8F8, 0xFFC0A4F8, 0xFFB0D0F0, 0xFFA8E0FC,
    0xFF78D8F8, 0xFF78F8D8, 0xFFB8F8B8, 0xFFD8F8B8, 0xFFFCFC00, 0xFFF8D8F8, 0xFF000000, 0xFF000000
};

// RP2C04 palette permutation LUTs (NESdev PPU programmer reference)
const uint8_t VS_PALETTE_LUT_2C04_0001[64] = {
    0x35,0x23,0x16,0x22,0x1C,0x09,0x1D,0x15,0x20,0x00,0x27,0x05,0x04,0x28,0x08,0x20,
    0x21,0x3E,0x1F,0x29,0x3C,0x32,0x36,0x12,0x3F,0x2B,0x2E,0x1E,0x3D,0x2D,0x24,0x01,
    0x0E,0x31,0x33,0x2A,0x2C,0x0C,0x1B,0x14,0x2E,0x07,0x34,0x06,0x13,0x02,0x26,0x2E,
    0x2E,0x19,0x10,0x0A,0x39,0x03,0x37,0x17,0x0F,0x11,0x0B,0x0D,0x38,0x25,0x18,0x3A
};

const uint8_t VS_PALETTE_LUT_2C04_0002[64] = {
    0x2E,0x27,0x18,0x39,0x3A,0x25,0x1C,0x31,0x16,0x13,0x38,0x34,0x20,0x23,0x3C,0x0B,
    0x0F,0x21,0x06,0x3D,0x1B,0x29,0x1E,0x22,0x1D,0x24,0x0E,0x2B,0x32,0x08,0x2E,0x03,
    0x04,0x36,0x26,0x33,0x11,0x1F,0x10,0x02,0x14,0x3F,0x00,0x09,0x12,0x2E,0x28,0x20,
    0x3E,0x0D,0x2A,0x17,0x0C,0x01,0x15,0x19,0x2E,0x2C,0x07,0x37,0x35,0x05,0x0A,0x2D
};

const uint8_t VS_PALETTE_LUT_2C04_0003[64] = {
    0x14,0x25,0x3A,0x10,0x0B,0x20,0x31,0x09,0x01,0x2E,0x36,0x08,0x15,0x3D,0x3E,0x3C,
    0x22,0x1C,0x05,0x12,0x19,0x18,0x17,0x1B,0x00,0x03,0x2E,0x02,0x16,0x06,0x34,0x35,
    0x23,0x0F,0x0E,0x37,0x0D,0x27,0x26,0x20,0x29,0x04,0x21,0x24,0x11,0x2D,0x2E,0x1F,
    0x2C,0x1E,0x39,0x33,0x07,0x2A,0x28,0x1D,0x0A,0x2E,0x32,0x38,0x13,0x2B,0x3F,0x0C
};

const uint8_t VS_PALETTE_LUT_2C04_0004[64] = {
    0x18,0x03,0x1C,0x28,0x2E,0x35,0x01,0x17,0x10,0x1F,0x2A,0x0E,0x36,0x37,0x0B,0x39,
    0x25,0x1E,0x12,0x34,0x2E,0x1D,0x06,0x26,0x3E,0x1B,0x22,0x19,0x04,0x2E,0x3A,0x21,
    0x05,0x0A,0x07,0x02,0x13,0x14,0x00,0x15,0x0C,0x3D,0x11,0x0F,0x0D,0x38,0x2D,0x24,
    0x33,0x20,0x08,0x16,0x3F,0x2B,0x20,0x3C,0x2E,0x27,0x23,0x31,0x29,0x32,0x2C,0x09
};

const uint8_t POWER_UP_PALETTE[] = {
    0x09, 0x01, 0x00, 0x01, 0x00, 0x02, 0x02, 0x0D, 0x08, 0x10, 0x08, 0x24, 0x00, 0x00, 0x04, 0x2C,
    0x09, 0x01, 0x34, 0x03, 0x00, 0x04, 0x00, 0x14, 0x08, 0x3A, 0x00, 0x02, 0x00, 0x20, 0x2C, 0x08
};

class PPU
{
public:

    static constexpr int SCREEN_WIDTH = 256;
    static constexpr int SCREEN_HEIGHT = 240;

private:

    enum class SpriteHeight : uint8_t {
        H8 = 8,
        H16 = 16
    };

    enum class VramAddressIncrement : uint8_t {
        I1 = 1,
        I32 = 32
    };

    enum class PatternTableAddress : uint16_t {
        X0000 = 0x0000,
        X1000 = 0x1000
    };

    struct Sprite {
        uint8_t y;
        uint8_t indexInPatternTable;
        uint8_t attrib;
        uint8_t x;
    };

    Settings& m_settings;
    Cartridge& m_cartridge;

    uint32_t m_framebuffer[SCREEN_WIDTH*SCREEN_HEIGHT];
    std::array<uint32_t, 64> m_colorPalette = {};
    std::array<uint32_t, 64> m_outputColorPalette = {};

    uint8_t m_currentPixelColorIndex;

    int m_scanline;
    int m_cycle;

    //PPUCTRL
    VramAddressIncrement m_VRAMAddressIncrement;
    PatternTableAddress m_spritePatternTableAddress;
    PatternTableAddress m_backgroundPatternTableAddress;
    SpriteHeight m_spriteHeight;
    bool m_PPUSlave; // false = master true = slave
    bool m_NMIOnVBlank; //false = off on = true

    //PPUMASK
    bool m_monochromeDisplay;
    bool m_showBackgroundLeftmost8Pixels;
    bool m_showSpritesLeftmost8Pixels;
    bool m_backgroundEnabled;
    bool m_spritesEnabled;
    uint8_t m_colorEmphasis;

    bool m_renderingEnabled;

    //PPUSTATUS
    bool m_VBlankHasStarted;
    bool m_sprite0Hit;
    bool m_spriteOverflow;

    //sprites evaluate variables
    uint8_t m_spritesInThisLine;
    bool m_testSprite0HitInThisLine;

    //OAM read/write address
    uint8_t m_oamAddr;
    uint8_t m_oamCopyBuffer;
    bool m_oamCopyDone;

    uint8_t m_secondaryOamAddr;
    bool m_spriteInRange;

    uint8_t m_oamAddrN; //oam[N][M]
    uint8_t m_oamAddrM;

    int m_overflowBugCounter;
    bool m_sprite0Added;
    bool m_corruptOamRow[32];

    //Rendering position
    int m_currentY;
    int m_currentX;

    bool m_interruptFlag;

    bool m_oddFrameFlag;

    /*
    The NES has nametables 0 and 1 in the console, while nametables 2 and 3 are in the cartridge
    when four-screen mirroring is used. They are declared here for simplification.
    */
    uint8_t m_nameTable[4][0x400]; //4x 1KB

    uint8_t m_palette[0x20]; //32 Bytes
    uint32_t m_debugChrGeneration = 0;
    uint32_t m_debugNametableGeneration = 0;
    uint32_t m_debugPaletteGeneration = 0;

    uint8_t m_primaryOam[0x100]; //256 bytes

    uint8_t m_secondaryOam[0x20]; //32 bytes

    uint8_t m_spritesIndexesInThisLine[64];

    struct SpriteFetchEntry {
        uint8_t x;
        uint8_t attr;
        uint8_t lowByte;
        uint8_t highByte;
        uint16_t tileIndex;
        uint16_t patternAddress;
        uint8_t row;
        bool sprite0;
        bool valid;
    };
    struct SpriteRenderEntry {
        uint8_t x;
        uint8_t xCounter;
        uint8_t attr;
        uint8_t lowShift;
        uint8_t highShift;
        uint16_t tileIndex;
        uint16_t patternAddress;
        uint8_t row;
        bool sprite0;
        bool counting;
        bool active;
        bool valid;
    };
    SpriteFetchEntry m_spriteFetchEntries[8];
    SpriteRenderEntry m_spriteRenderEntries[8];
    uint8_t m_spriteFetchCount;

    //write/read internal regs
    uint16_t m_reg_v;
    uint8_t m_reg_x;
    uint16_t m_reg_t;
    bool m_reg_w;

    struct DeferredPpuIoState {
        bool pendingDataLatchUpdate = false;
        uint8_t pendingDataLatchDelay = 0;
        uint16_t pendingDataLatchAddr = 0;
        bool deferredDataLatchArmPending = false;
        bool deferredDataLatchStart = false;
        uint8_t deferredDataLatchStartDelay = 0;
        bool deferredVideoRamIncrementArmPending = false;
        uint8_t deferredVideoRamIncrementDelay = 0;
    };

    uint8_t m_dataLatch;
    DeferredPpuIoState m_deferredPpuIo;

    int m_debugCursorX = 0;
    int m_debugCursorY = 0;

    // background temporary variables
    uint16_t m_tileAddr;
    uint8_t m_paletteOffset;
    uint8_t m_lowTileByte;
    uint8_t m_highTileByte;
    uint16_t m_bgPatternLowShift;
    uint16_t m_bgPatternHighShift;
    uint16_t m_bgAttribLowShift;
    uint16_t m_bgAttribHighShift;
    bool m_bgAttribLowLatch;
    bool m_bgAttribHighLatch;
    uint64_t m_tileData;

public:
    struct DebugModBackgroundPixel {
        uint16_t tileIndex = 0xFFFF;
        uint32_t tileHash = 0;
        uint8_t palette[3] = {};
        uint8_t paletteIndex = 0;
        uint8_t colorLowBits = 0;
        uint8_t offsetX = 0;
        uint8_t offsetY = 0;
        bool valid = false;
    };

    struct DebugModSpriteCandidate {
        uint16_t tileIndex = 0xFFFF;
        uint32_t tileHash = 0;
        uint8_t palette[3] = {};
        uint8_t paletteSlot = 0;
        uint8_t colorLowBits = 0;
        uint8_t offsetX = 0;
        uint8_t offsetY = 0;
        bool behindBackground = false;
        bool horizontalMirror = false;
        bool verticalMirror = false;
        bool synthetic = false;
        bool valid = false;
    };

    struct DebugModSpritePixel {
        uint16_t tileIndex = 0xFFFF;
        uint32_t tileHash = 0;
        uint8_t palette[3] = {};
        uint8_t paletteSlot = 0;
        uint8_t colorLowBits = 0;
        uint8_t offsetX = 0;
        uint8_t offsetY = 0;
        bool behindBackground = false;
        bool horizontalMirror = false;
        bool verticalMirror = false;
        bool synthetic = false;
        bool valid = false;
        uint8_t count = 0;
        std::array<DebugModSpriteCandidate, 8> candidates = {};
    };

private:
    struct DebugModBackgroundShiftPixel {
        uint16_t tileIndex = 0xFFFF;
        uint8_t paletteOffset = 0;
        uint8_t offsetX = 0;
        uint8_t offsetY = 0;
        bool valid = false;
    };

    bool m_debugModRenderCaptureEnabled = false;
    std::vector<DebugModBackgroundPixel> m_debugModBackgroundPixels;
    std::vector<DebugModSpritePixel> m_debugModSpritePixels;
    std::vector<DebugModBackgroundPixel> m_debugModPresentedBackgroundPixels;
    std::vector<DebugModSpritePixel> m_debugModPresentedSpritePixels;
    std::array<int, SCREEN_HEIGHT> m_debugModScanlineScrollX = {};
    std::array<int, SCREEN_HEIGHT> m_debugModScanlineScrollY = {};
    std::array<int, SCREEN_HEIGHT> m_debugModPresentedScanlineScrollX = {};
    std::array<int, SCREEN_HEIGHT> m_debugModPresentedScanlineScrollY = {};
    std::array<DebugModBackgroundShiftPixel, 16> m_debugModBackgroundShift = {};

    int m_lastPPUSTATUSReadCycle; //record the cycle when ppustatus is read

    uint8_t m_openBus;
    uint8_t m_openBusTimer[8]; //1 timer for each bit, decay 1 time per frame

    //settings variables
    int FRAME_NUMBER_OF_LINES;
    int FRAME_VBLANK_START_LINE;
    int FRAME_VBLANK_END_LINE;
    bool m_inOverclockLines;

    bool m_preLine;
    bool m_visibleLine;
    bool m_renderLine;

    bool m_overclockFrame;

    bool m_needUpdateState;
    bool m_needIncVideoRam;

    bool m_prevCycleRenderingEnabled;
    bool m_spriteRenderClockingActiveThisLine;
    bool m_staleBgShiftActive;
    uint16_t m_firstSpriteFetchV;

    int m_update_reg_v_delay;
    uint16_t m_update_reg_v_value;

    uint16_t m_busAddress;
    uint8_t m_busAddressLowLatch;
    int m_updateA12Delay;
    bool m_isSpritePatternFetch;
    bool m_currentReadAffectsBus;

    GameDatabase::PpuModel m_vsPpuModel = GameDatabase::PpuModel::Ppu2C02;


    //Do not serialize variables below
    bool m_cpuDmaReadInProgress;
    uint32_t* m_pFrameBuffer;

    void initOpenBus()
    {
        m_openBus = 0;

        for(int i = 0; i < 8; i++) {
            m_openBusTimer[i] = 0;
        }
    }

    void updateOpenBus(uint8_t value, uint8_t refreshMask)
    {
        m_openBus = value;

        for(int i = 0; i < 8; i++) {
            if( (value&1) && (refreshMask&1) ) m_openBusTimer[i] = 30; //decay after 30 frames
            value >>=1;
            refreshMask >>=1;
        }
    }

    void decayOpenBus()
    {
        for(int i = 0; i < 8; i++) {
           if(m_openBusTimer[i] > 0 && --m_openBusTimer[i] == 0) {
                uint8_t mask = ~(1 << i);
                m_openBus &= mask;
           }
        }
    }

    template<bool writeFlag, bool affectsTheBus, bool updateBusAddress = true>
    GERANES_HOT auto readWritePpuMemory(uint16_t addr, uint8_t data = 0) -> std::conditional_t<writeFlag, void, uint8_t>
    {
        m_currentReadAffectsBus = affectsTheBus;

        if constexpr(!writeFlag && affectsTheBus) {
            m_cartridge.onPpuRead(addr & 0x3FFF);
        }

        if constexpr (affectsTheBus && updateBusAddress)
            setBusAddress(addr);

        addr = normalizePpuAddress(addr);

        if(addr < 0x2000)
        {
            m_cartridge.setPpuFetchSource(m_isSpritePatternFetch);
            if constexpr(writeFlag) {
                m_cartridge.writeChr(addr,data);
                ++m_debugChrGeneration;
            }
            else {
                uint8_t value = m_cartridge.readChr(addr);
                commitPendingDataLatch(value);
                return value;
            }
        }

        else if(addr < 0x3F00)
        {
            const uint16_t nameTableAddr = normalizeNameTableAddress(addr);
            uint8_t addrIndex = (nameTableAddr - 0x2000) >> 10; //0-3 index without mirroring

            if constexpr(writeFlag) {
                writeNameTable(addrIndex, nameTableAddr, data);
                ++m_debugNametableGeneration;
            }
            else {
                uint8_t value = readNameTable(addrIndex, nameTableAddr);
                commitPendingDataLatch(value);
                return value;
            }
        }

        else // addr < 0x4000
        {
            const uint16_t paletteAddr = normalizePaletteAddress(addr);

            if constexpr(writeFlag) {
                m_palette[paletteAddr - 0x3F00] = data;
                ++m_debugPaletteGeneration;
            }
            else {
                uint8_t value = m_palette[paletteAddr - 0x3F00];
                commitPendingDataLatch(value);
                return value;
            }
        }

        if constexpr(!writeFlag)
            return 0;
    }

    GERANES_INLINE uint8_t readPpuMemory(int addr)
    {
        return readWritePpuMemory<false, true>(addr);
    }

    GERANES_INLINE uint8_t completePpuRead(uint16_t /*addr*/)
    {
        const uint16_t latchedAddr = static_cast<uint16_t>((m_busAddress & 0x3F00) | m_busAddressLowLatch);
        return readWritePpuMemory<false, true, false>(latchedAddr);
    }

    GERANES_INLINE void setupPpuReadAddress(uint16_t addr)
    {
        m_busAddressLowLatch = static_cast<uint8_t>(addr & 0x00FF);
        setBusAddress(addr);
        if(m_deferredPpuIo.deferredDataLatchStart) {
            m_deferredPpuIo.deferredDataLatchStart = false;
            m_deferredPpuIo.pendingDataLatchUpdate = true;
            m_deferredPpuIo.pendingDataLatchDelay = 1;
            m_deferredPpuIo.pendingDataLatchAddr = static_cast<uint16_t>(addr & 0x3FFF);
        }
    }

    GERANES_INLINE void writePpuMemory(int addr, uint8_t data)
    {
        readWritePpuMemory<true, true>(addr,data);
    }

    GERANES_INLINE uint8_t fakeReadPpuMemory(int addr)
    {
        return readWritePpuMemory<false, false>(addr);
    }

    GERANES_INLINE void fakeWritePpuMemory(int addr, uint8_t data)
    {
        readWritePpuMemory<true, false>(addr,data);
    }

    GERANES_INLINE uint16_t normalizePpuAddress(uint16_t addr) const
    {
        return static_cast<uint16_t>(addr & 0x3FFF);
    }

    GERANES_INLINE uint16_t normalizeNameTableAddress(uint16_t addr) const
    {
        return static_cast<uint16_t>(normalizePpuAddress(addr) & 0x2FFF);
    }

    GERANES_INLINE uint16_t normalizePaletteAddress(uint16_t addr) const
    {
        uint16_t normalized = static_cast<uint16_t>(normalizePpuAddress(addr) & 0x3F1F);
        if(normalized == 0x3F10) return 0x3F00;
        if(normalized == 0x3F14) return 0x3F04;
        if(normalized == 0x3F18) return 0x3F08;
        if(normalized == 0x3F1C) return 0x3F0C;
        return normalized;
    }

    GERANES_INLINE void commitPendingDataLatch(uint8_t value)
    {
        if(m_deferredPpuIo.pendingDataLatchUpdate && m_deferredPpuIo.pendingDataLatchDelay == 0) {
            m_dataLatch = value;
            m_deferredPpuIo.pendingDataLatchUpdate = false;
        }
    }

    GERANES_INLINE void armDeferredVideoRamIncrement()
    {
        m_deferredPpuIo.deferredVideoRamIncrementArmPending = true;
    }

    GERANES_INLINE void armDeferredDataLatch(uint16_t addr)
    {
        m_deferredPpuIo.deferredDataLatchArmPending = true;
        m_deferredPpuIo.pendingDataLatchAddr = static_cast<uint16_t>(addr & 0x3FFF);
    }

    GERANES_INLINE bool hasPendingPpuDataReadUpdate() const
    {
        return m_needIncVideoRam ||
               m_deferredPpuIo.pendingDataLatchUpdate ||
               m_deferredPpuIo.deferredDataLatchArmPending ||
               m_deferredPpuIo.deferredDataLatchStart ||
               m_deferredPpuIo.deferredDataLatchStartDelay > 0 ||
               m_deferredPpuIo.deferredVideoRamIncrementArmPending ||
               m_deferredPpuIo.deferredVideoRamIncrementDelay > 0;
    }

    GERANES_INLINE void schedulePpuDataReadUpdate(uint16_t addr)
    {
        constexpr uint8_t PPU_DATA_READ_UPDATE_DELAY = 6;
        m_deferredPpuIo.pendingDataLatchUpdate = true;
        m_deferredPpuIo.pendingDataLatchDelay = PPU_DATA_READ_UPDATE_DELAY;
        m_deferredPpuIo.pendingDataLatchAddr = static_cast<uint16_t>(addr & 0x3FFF);
        m_deferredPpuIo.deferredVideoRamIncrementDelay = PPU_DATA_READ_UPDATE_DELAY;
        m_needUpdateState = true;
    }

    GERANES_INLINE void completePendingPpuDataReadUpdate()
    {
        if(!hasPendingPpuDataReadUpdate()) {
            return;
        }

        if(m_deferredPpuIo.pendingDataLatchUpdate) {
            m_dataLatch = fakeReadPpuMemory(m_deferredPpuIo.pendingDataLatchAddr & 0x3FFF);
            m_deferredPpuIo.pendingDataLatchUpdate = false;
        }

        if(m_needIncVideoRam || m_deferredPpuIo.deferredVideoRamIncrementDelay > 0) {
            m_needIncVideoRam = false;
            m_deferredPpuIo.deferredVideoRamIncrementDelay = 0;
            incVideoRamAddr();
        }

        m_deferredPpuIo.deferredDataLatchArmPending = false;
        m_deferredPpuIo.deferredDataLatchStart = false;
        m_deferredPpuIo.deferredDataLatchStartDelay = 0;
        m_deferredPpuIo.deferredVideoRamIncrementArmPending = false;
        m_needUpdateState = true;
    }

    GERANES_INLINE void syncRenderingEnabledFlag()
    {
        const bool targetRenderingEnabled = m_backgroundEnabled || m_spritesEnabled;
        if(m_renderingEnabled != targetRenderingEnabled) {
            m_renderingEnabled = targetRenderingEnabled;
            m_needUpdateState = true;
        }
    }

    GERANES_INLINE void handleRenderingEnabledTransition()
    {
        if(m_prevCycleRenderingEnabled == m_renderingEnabled) {
            return;
        }

        const bool wasRenderingEnabled = m_prevCycleRenderingEnabled;
        const bool renderingEnabledNow = m_renderingEnabled;

        m_prevCycleRenderingEnabled = m_renderingEnabled;

        if(!wasRenderingEnabled && renderingEnabledNow && m_renderLine) {
            m_staleBgShiftActive = true;
        }

        if(m_scanline < 240 && renderingEnabledNow) {
            processOamCorruption();
        }

        if(!m_renderLine || !wasRenderingEnabled || renderingEnabledNow) {
            return;
        }

        setOamCorruptionFlags();

        // When rendering is disabled midscreen, hardware can perturb OAM evaluation state.
        if(m_cycle >= 65 && m_cycle <= 256) {
            // The address increment can be delayed by 1 PPU cycle depending on odd/even timing.
            // In practice the externally visible effect is an increment by 1 after rendering stops.
            m_oamAddr++;

            // Keep N/M in sync to replicate the misalignment bug observed by oam_flicker_test_reenable.
            m_oamAddrN = (m_oamAddr >> 2) & 0x3F;
            m_oamAddrM = m_oamAddr & 0x03;
        }
    }

    GERANES_INLINE void updateDeferredRegV()
    {
        if(m_update_reg_v_delay <= 0) {
            return;
        }

        --m_update_reg_v_delay;
        if(m_update_reg_v_delay != 0) {
            m_needUpdateState = true;
            return;
        }

        m_reg_v = m_update_reg_v_value;

        if(!isActivelyRendering()) {
            // Only update the bus address when not rendering; needed for MMC3 IRQ timing.
            setBusAddress(m_reg_v & 0x3FFF);
        }
    }

    GERANES_INLINE void updateDeferredVideoRamIncrement()
    {
        if(m_deferredPpuIo.deferredVideoRamIncrementDelay <= 0) {
            return;
        }

        --m_deferredPpuIo.deferredVideoRamIncrementDelay;
        if(m_deferredPpuIo.deferredVideoRamIncrementDelay == 0) {
            m_needIncVideoRam = true;
            m_needUpdateState = true;
        }
    }

    GERANES_INLINE void updateDeferredDataLatchArm()
    {
        if(m_deferredPpuIo.deferredDataLatchStartDelay <= 0) {
            return;
        }

        --m_deferredPpuIo.deferredDataLatchStartDelay;
        if(m_deferredPpuIo.deferredDataLatchStartDelay == 0) {
            m_deferredPpuIo.deferredDataLatchStart = true;
        }
        else {
            m_needUpdateState = true;
        }
    }

    GERANES_INLINE void applyPendingVideoRamIncrement(bool wasPending)
    {
        if(!wasPending) {
            return;
        }

        m_needIncVideoRam = false;
        incVideoRamAddr();
    }

    GERANES_INLINE void updatePendingDataLatch()
    {
        if(m_deferredPpuIo.pendingDataLatchUpdate && m_deferredPpuIo.pendingDataLatchDelay > 0) {
            --m_deferredPpuIo.pendingDataLatchDelay;
            m_needUpdateState = true;
        }

        if(!m_deferredPpuIo.pendingDataLatchUpdate || m_deferredPpuIo.pendingDataLatchDelay != 0) {
            return;
        }

        if(!isActivelyRendering()) {
            m_dataLatch = fakeReadPpuMemory(m_deferredPpuIo.pendingDataLatchAddr & 0x3FFF);
            m_deferredPpuIo.pendingDataLatchUpdate = false;
        }
        else {
            m_needUpdateState = true;
        }
    }

    GERANES_INLINE void updateA12Delay()
    {
        if(m_updateA12Delay <= 0) {
            return;
        }

        --m_updateA12Delay;
        if(m_updateA12Delay == 0) {
            m_cartridge.setA12State(m_busAddress & 0x1000);
        }
        else {
            m_needUpdateState = true;
        }
    }

    //index 0-3
    GERANES_INLINE_HOT void writeNameTable(uint8_t addrIndex, uint16_t addr, uint8_t data)
    {
        if(m_cartridge.useCustomNameTable(addrIndex&0x03)) {
            m_cartridge.writeCustomNameTable(addrIndex&0x03, addr&0x3FF, data);
            return;
        }

        int index = m_cartridge.mirroring(addrIndex&0x03);
        m_nameTable[index&3][addr&0x3FF] = data;
    }

    //index 0-3
    GERANES_INLINE_HOT uint8_t readNameTable(uint8_t addrIndex, uint16_t addr)
    {
        uint8_t ret = 0;

        if(m_cartridge.useCustomNameTable(addrIndex&0x03)) {
            ret = m_cartridge.readCustomNameTable(addrIndex&0x03,addr&0x3FF);
        }
        else {
            int index = m_cartridge.mirroring(addrIndex&0x03);
            ret = m_nameTable[index&3][addr&0x3FF];
        }

        if(!m_currentReadAffectsBus) {
            return ret;
        }

        return m_cartridge.transformNameTableRead(addrIndex&0x03, addr&0x3FF, ret);
    }

    GERANES_INLINE void setBusAddress(uint16_t addr) {
        m_busAddress = addr;
        m_updateA12Delay = 2;
        m_needUpdateState = true;        
    }


public:

    SigSlot::Signal<> signalFrameStart;
    SigSlot::Signal<> signalFrameReady; //called when the frame buffer is ready to show
    SigSlot::Signal<> signalScanlineStart;

    PPU(Settings& settings, Cartridge& cartridge) : m_settings(settings), m_cartridge(cartridge)
    {
        std::copy(std::begin(NES_PALETTE), std::end(NES_PALETTE), m_colorPalette.begin());
        refreshOutputColorPalette();
        init();
    }

    void setColorPalette(const std::array<uint32_t, 64>& palette)
    {
        m_colorPalette = palette;
        refreshOutputColorPalette();
    }

    const std::array<uint32_t, 64>& colorPalette() const
    {
        return m_colorPalette;
    }

    void init()
    {
        initOpenBus();

        writePPUCTRL(0);
        writePPUMASK(0);

        memcpy(m_palette, POWER_UP_PALETTE, sizeof(m_palette));

        m_renderingEnabled = m_spritesEnabled || m_backgroundEnabled;
        m_prevCycleRenderingEnabled = m_renderingEnabled;
        m_spriteRenderClockingActiveThisLine = m_prevCycleRenderingEnabled;
        m_staleBgShiftActive = false;
        m_firstSpriteFetchV = 0;

        //PPUSTATUS
        m_VBlankHasStarted = false;
        m_sprite0Hit = false;
        m_spriteOverflow = false;

        m_spritesInThisLine = 0;
        m_testSprite0HitInThisLine = false;
        m_spriteFetchCount = 0;
        memset(m_spriteFetchEntries, 0, sizeof(m_spriteFetchEntries));
        memset(m_spriteRenderEntries, 0, sizeof(m_spriteRenderEntries));
        memset(m_secondaryOam, 0xFF, sizeof(m_secondaryOam));
        memset(m_primaryOam, 0, sizeof(m_primaryOam));
        memset(m_spritesIndexesInThisLine, 0, sizeof(m_spritesIndexesInThisLine));
        memset(m_corruptOamRow, 0, sizeof(m_corruptOamRow));

        m_oamAddr = 0;
        m_oamCopyBuffer = 0xFF;
        m_oamCopyDone = false;
        m_secondaryOamAddr = 0;
        m_spriteInRange = false;
        m_oamAddrN = 0;
        m_oamAddrM = 0;
        m_overflowBugCounter = 0;
        m_sprite0Added = false;
        m_dataLatch = 0;
        m_deferredPpuIo = {};
        m_cpuDmaReadInProgress = false;

        m_currentPixelColorIndex = 0;
        m_bgPatternLowShift = 0;
        m_bgPatternHighShift = 0;
        m_bgAttribLowShift = 0;
        m_bgAttribHighShift = 0;
        m_bgAttribLowLatch = false;
        m_bgAttribHighLatch = false;

        m_scanline = 0;
        m_cycle = 0;

        m_currentY = 0;
        m_currentX = 0;
        m_pFrameBuffer = &m_framebuffer[0];

        m_reg_v = 0;
        m_reg_x = 0;
        m_reg_t = 0;
        m_reg_w = false;
        m_debugChrGeneration = 0;
        m_debugNametableGeneration = 0;
        m_debugPaletteGeneration = 0;

        m_oddFrameFlag = false;

        m_interruptFlag = false;

        m_tileAddr = 0;
        m_paletteOffset = 0;
        m_lowTileByte = 0;
        m_highTileByte = 0;
        m_paletteOffset = 0;
        m_lowTileByte = 0;
        m_highTileByte = 0;
        m_tileData = 0;
        m_debugModRenderCaptureEnabled = false;
        m_debugModBackgroundShift = {};
        m_debugModBackgroundPixels.clear();
        m_debugModSpritePixels.clear();
        m_debugModPresentedBackgroundPixels.clear();
        m_debugModPresentedSpritePixels.clear();
        m_debugModScanlineScrollX.fill(0);
        m_debugModScanlineScrollY.fill(0);
        m_debugModPresentedScanlineScrollX.fill(0);
        m_debugModPresentedScanlineScrollY.fill(0);

        m_lastPPUSTATUSReadCycle = -1;

        clearFramebuffer();
        
        FRAME_NUMBER_OF_LINES = 262;
        FRAME_VBLANK_START_LINE = 241;
        FRAME_VBLANK_END_LINE = 261;

        m_inOverclockLines = false;

        m_preLine = false;
        m_visibleLine = false;
        m_renderLine = false;

        m_overclockFrame = false;

        m_needUpdateState = false;
        m_needIncVideoRam = false;
        memset(m_corruptOamRow, 0, sizeof(m_corruptOamRow));

        m_tileAddr = 0;

        m_update_reg_v_delay = 0;
        m_update_reg_v_value = 0;

        m_busAddress = 0;
        m_busAddressLowLatch = 0;
        m_updateA12Delay = 0;
        m_isSpritePatternFetch = false;
        m_currentReadAffectsBus = true;

        updateSettings();        

        //warm up the PPU by running some cycles
        for(int i = 0; i < 15; i++) {
            ppuCycle();
            ppuCycle();
            ppuCycle();
            if(m_settings.region() == Settings::Region::PAL && (i % 5) == 0) {
                ppuCycle();
            }
        } //need this for read2004.nes
       
        /* read2004.nes output
        FF FF FF FF AA AA 01 01 10 10
        01 01 00 00 00 00 20 20 01 01
        01 01 00 00 30 30 01 01 02 02
        00 00 40 40 02 02 03 03 00 00
        50 50 02 02 04 04 00 00 60 60
        02 02 05 05 00 00 70 70 03 03
        06 06 00 00 80 80 03 03 07 07
        05 01 A0 01 41 01 0B 01 05 01
        E0 01 81 01 0F 01 05 01 F3 01
        00 01 12 01 05 01 F5 01 05 01
        05 01 05 01 05 01 05 01 05 01
        06 01 06 01 06 01 06 01 06 01
        06 01 06 01 06 01 07 01 07 01
        07 01 08 01 09 01 0A 01 0A 01
        0B 01 0C 01 0D 01 0E 01 0F 01
        0F 01 0F 01 0F 01 0F 01 0F 01
        0F 01 0F 01 0F 01 0F 01 0F 01
        0F 01 0F 01 0F 01 0F 01 0F 01
        0F 01 0F 01 10 01 AA 01 01 01
        00 01 00 01 00 01 01 10 01 00
        00 00 00 00 00 20 01 01 01 01
        01 01 00 30 01 02 02 02 02 02
        00 40 02 03 03 03 03 03 00 50
        02 04 04 04 04 04 00 60 02 05
        05 05 05 05 00 70 03 06 06 06
        06 06 00 00 00 00
        */

    }

    bool inOverclockLines()
    {
        return  m_inOverclockLines;
    }

    void updateSettings()
    {        
        switch(m_settings.region())
        {
            case Settings::Region::NTSC:                
                FRAME_VBLANK_START_LINE = 241;
                FRAME_VBLANK_END_LINE = FRAME_VBLANK_START_LINE+20;
                FRAME_NUMBER_OF_LINES = FRAME_VBLANK_END_LINE+1;
                break;
                
            case Settings::Region::PAL:                
                FRAME_VBLANK_START_LINE = 241;
                FRAME_VBLANK_END_LINE = FRAME_VBLANK_START_LINE+70;
                FRAME_NUMBER_OF_LINES = FRAME_VBLANK_END_LINE+1;
                break;

            case Settings::Region::DENDY:
                FRAME_VBLANK_START_LINE = 241;
                FRAME_VBLANK_END_LINE = FRAME_VBLANK_START_LINE+20;
                FRAME_NUMBER_OF_LINES = FRAME_VBLANK_END_LINE+51;
                break;
        }
    }

    Settings::Region region() const
    {
        return m_settings.region();
    }

    //NES adress space
    template<bool writeFlag>
    GERANES_INLINE_HOT uint8_t readWrite(int addr, uint8_t data)
    {
        const int fullAddr = addr;
        uint8_t openBusMask;

        if constexpr(!writeFlag) {
            data = m_openBus;
            openBusMask = 0x00;
        }
        else {
            openBusMask = 0xFF;
        }
        
        addr &= 0x2007;

        switch(addr)
        {
        case 0x2000: //acess: write only
        {
            if constexpr(writeFlag) writePPUCTRL(data, true);
            break;
        }
        case 0x2001: //acess: write only
        {
            if constexpr(writeFlag) writePPUMASK(data, fullAddr == 0x2001);
            break;
        }
        case 0x2002: //acess: read only
        {
            if constexpr(!writeFlag) {
                data = readPPUSTATUS(fullAddr == 0x2002);

                data &= ~0x1F;
                data |= m_openBus&0x1F; //the low 5 bits are open and they should be from decay value
                openBusMask = ~0x1F; //the high 3 bits need refresh
            }
            break;
        }
        case 0x2003: //acess: write only
        {
            if constexpr(writeFlag) writeOAMADDR(data);
            break;
        }
        case 0x2004: //acess: read and write
        {
            if constexpr(writeFlag) writeOAMDATA(data);
            else data = readOAMDATA();
            break;
        }
        case 0x2005: //acess: write twice
        {
            if constexpr(writeFlag) writePPUSCROLL(data);
            break;
        }
        case 0x2006: //acess: write twice
        {
            if constexpr(writeFlag) writePPUADDR(data);
            break;
        }
        case 0x2007: //acess: read and write
        {
            if constexpr(writeFlag) writePPUDATA(data);
            else {
                data = readPPUDATA();

                if(isOnPaletteAddr()) {
                    data &= 0x3F; //the 2 high bits are open and they should be from decay value
                    data |= m_openBus&(~0x3F);
                    if(m_monochromeDisplay) {
                        // Greyscale mode masks the low 4 bits on palette RAM reads.
                        data &= 0xF0;
                    }
                    openBusMask = 0x3F; //the 6 low bits need refresh
                }
                else openBusMask = 0xFF;


            }
            break;
        }
        }

        updateOpenBus(data, openBusMask);

        return data;
    }


    GERANES_INLINE_HOT bool getInterruptFlag()
    {        
        if(m_interruptFlag)
        {
            m_interruptFlag = false;
            return true;
        }

        return false;
    }

    GERANES_INLINE bool isOnPaletteAddr()
    {
        return (m_reg_v >= 0x3F00 && m_reg_v < 0x4000);
    }

    GERANES_INLINE void captureModBackgroundPixel()
    {
        if(!m_debugModRenderCaptureEnabled || m_debugModBackgroundPixels.empty() || m_debugModSpritePixels.empty() ||
           m_currentX < 0 || m_currentX >= SCREEN_WIDTH || m_currentY < 0 || m_currentY >= SCREEN_HEIGHT) {
            return;
        }

        DebugModBackgroundPixel pixel;
        if(m_backgroundEnabled) {
            const DebugModBackgroundShiftPixel& source = m_debugModBackgroundShift[m_reg_x & 0x0F];
            if(source.valid) {
                const uint8_t colorLowBits = static_cast<uint8_t>(m_currentPixelColorIndex & 0x03);
                pixel.tileIndex = source.tileIndex;
                pixel.tileHash = source.tileIndex <= 0x01FF ? debugHashChrTile(source.tileIndex) : 0;
                pixel.palette[0] = static_cast<uint8_t>(m_palette[(source.paletteOffset + 1) & 0x1F] & 0x3F);
                pixel.palette[1] = static_cast<uint8_t>(m_palette[(source.paletteOffset + 2) & 0x1F] & 0x3F);
                pixel.palette[2] = static_cast<uint8_t>(m_palette[(source.paletteOffset + 3) & 0x1F] & 0x3F);
                pixel.paletteIndex = colorLowBits == 0
                    ? static_cast<uint8_t>(m_palette[0] & 0x3F)
                    : static_cast<uint8_t>(m_palette[(source.paletteOffset + colorLowBits) & 0x1F] & 0x3F);
                pixel.colorLowBits = colorLowBits;
                pixel.offsetX = source.offsetX;
                pixel.offsetY = source.offsetY;
                pixel.valid = true;
            }
        }

        const size_t index = static_cast<size_t>(m_currentY) * SCREEN_WIDTH + static_cast<size_t>(m_currentX);
        m_debugModBackgroundPixels[index] = pixel;
        m_debugModSpritePixels[index] = {};
    }

    GERANES_INLINE void captureModScanlineScroll()
    {
        if(!m_debugModRenderCaptureEnabled || m_currentX != 0 || m_currentY < 0 || m_currentY >= SCREEN_HEIGHT) {
            return;
        }

        const uint16_t reg = m_reg_t;
        m_debugModScanlineScrollX[static_cast<size_t>(m_currentY)] =
            (((reg >> 10) & 0x01) << 8) | ((reg & 0x1F) << 3) | (m_reg_x & 0x07);
        m_debugModScanlineScrollY[static_cast<size_t>(m_currentY)] =
            ((((reg >> 5) & 0x1F) << 3) | ((reg >> 12) & 0x07)) + (((reg >> 11) & 0x01) ? 240 : 0);
    }

    GERANES_INLINE_HOT void renderPixel()
    {
        m_currentPixelColorIndex = 0;
        const bool renderingEnabled = m_renderingEnabled;

        if(m_backgroundEnabled) renderBackgroundPixel();
        if(m_debugModRenderCaptureEnabled) {
            captureModScanlineScroll();
            captureModBackgroundPixel();
        }
        if(m_spritesEnabled) renderSpritesPixel();

        uint8_t value;

        //if reg v is pointing to the palette
        if(!renderingEnabled && isOnPaletteAddr()) {
            value  = static_cast<uint8_t>(fakeReadPpuMemory(m_reg_v)&0x3F);
        }
        else {
            if( (m_currentPixelColorIndex&0x03) == 0) m_currentPixelColorIndex = 0;
            value  = static_cast<uint8_t>(m_palette[m_currentPixelColorIndex]&0x3F);
        }

        *m_pFrameBuffer = NESToRGBAColor(value);
        m_pFrameBuffer++;

        if(++m_currentX == SCREEN_WIDTH){
            m_currentX = 0;

            if(++m_currentY == SCREEN_HEIGHT) {
                m_currentY = 0;
                m_currentX = 0;
                m_pFrameBuffer = &m_framebuffer[0];          
            }
        }
    }

    GERANES_INLINE_HOT void renderBackgroundPixel()
    {
        if(!m_showBackgroundLeftmost8Pixels && m_currentX <8) return;

        const uint16_t mask = static_cast<uint16_t>(0x8000 >> m_reg_x);
        uint8_t color = 0;
        if(m_bgPatternLowShift & mask) color |= 0x01;
        if(m_bgPatternHighShift & mask) color |= 0x02;
        if(m_bgAttribLowShift & mask) color |= 0x04;
        if(m_bgAttribHighShift & mask) color |= 0x08;
        m_currentPixelColorIndex = color;
    }

    //for name tables debug x:0-255 y:0-480
    uint32_t getFinalNameTableColor(int vx, int vy)
    {
        uint8_t colorIndex = getColorHighBitsInAttributeTables(vx,vy) | getColorLowBitsInNameTables(vx,vy);

        if( (colorIndex&0x03) != 0) //not transparent
            return NESToRGBAColor(m_palette[colorIndex]);

        return NESToRGBAColor(m_palette[0]);

    }

/*
     (0,0)     (256,0)     (511,0)
       +-----------+-----------+
       |           |           |
       |           |           |
       |   $2000   |   $2400   |
       |           |           |
       |           |           |
(0,240)+-----------+-----------+(511,240)
       |           |           |
       |           |           |
       |   $2800   |   $2C00   |
       |           |           |
       |           |           |
       +-----------+-----------+
     (0,479)   (256,479)   (511,479)


yyy NN YYYYY XXXXX
||| || ||||| +++++-- coarse X scroll
||| || +++++-------- coarse Y scroll
||| ++-------------- nametable select
+++----------------- fine Y scroll

*/

    int getRawScrollX() const
    {
        if(m_renderLine && m_renderingEnabled) {
            int scrollX = ((m_reg_v & 0x1F) << 3) | (m_reg_x & 0x07);
            scrollX = (scrollX - 16) % 256;
            if(scrollX < 0) {
                scrollX += 256;
            }
            return scrollX;
        }

        return ((m_reg_t & 0x1F) << 3) | (m_reg_x & 0x07);
    }

    int getRawScrollY() const
    {
        if(m_renderLine && m_renderingEnabled) {
            return (((m_reg_v >> 5) & 0x1F) << 3) | ((m_reg_v >> 12) & 0x07);
        }

        return (((m_reg_t >> 5) & 0x1F) << 3) | ((m_reg_t >> 12) & 0x07);
    }

    int getVirtualScrollX() const
    {
        const uint16_t reg = (m_renderLine && m_renderingEnabled) ? m_reg_v : m_reg_t;
        int scrollX = (((reg >> 10) & 0x01) << 8) | ((reg & 0x1F) << 3) | (m_reg_x & 0x07);
        if(m_renderLine && m_renderingEnabled) {
            scrollX = (scrollX - 16) % 512;
            if(scrollX < 0) {
                scrollX += 512;
            }
        }
        return scrollX;
    }

    int getVirtualScrollY() const
    {
        int scrollY = getRawScrollY();
        const uint16_t reg = (m_renderLine && m_renderingEnabled) ? m_reg_v : m_reg_t;
        if((reg >> 11) & 0x01) {
            scrollY += 240;
        }
        return scrollY;
    }

    int getCursorX() const
    {
        return getVirtualScrollX();
    }

    int getCursorY() const
    {
        return getVirtualScrollY();
    }

    uint32_t debugChrGeneration() const
    {
        return m_debugChrGeneration;
    }

    uint32_t debugNametableGeneration() const
    {
        return m_debugNametableGeneration;
    }

    uint32_t debugPaletteGeneration() const
    {
        return m_debugPaletteGeneration;
    }

    uint8_t debugPeekPpuMemory(uint16_t addr) const
    {
        addr = normalizePpuAddress(addr);

        if(addr < 0x2000) {
            return const_cast<Cartridge&>(m_cartridge).readChr(addr);
        }

        if(addr < 0x3F00) {
            const uint16_t nameTableAddr = normalizeNameTableAddress(addr);
            const uint8_t addrIndex = static_cast<uint8_t>((nameTableAddr - 0x2000) >> 10);

            uint8_t ret = 0;
            if(m_cartridge.useCustomNameTable(addrIndex & 0x03)) {
                ret = const_cast<Cartridge&>(m_cartridge).readCustomNameTable(addrIndex & 0x03, nameTableAddr & 0x3FF);
            }
            else {
                const int index = const_cast<Cartridge&>(m_cartridge).mirroring(addrIndex & 0x03);
                ret = m_nameTable[index & 3][nameTableAddr & 0x3FF];
            }

            return const_cast<Cartridge&>(m_cartridge).transformNameTableRead(addrIndex & 0x03, nameTableAddr & 0x3FF, ret);
        }

        return m_palette[normalizePaletteAddress(addr) - 0x3F00];
    }

    void debugWritePpuMemory(uint16_t addr, uint8_t data)
    {
        fakeWritePpuMemory(addr, data);
    }

    uint8_t debugPeekPrimaryOam(uint8_t addr) const
    {
        return m_primaryOam[addr];
    }

    void debugWritePrimaryOam(uint8_t addr, uint8_t data)
    {
        m_primaryOam[addr] = data;
    }

    uint8_t debugPeekSecondaryOam(uint8_t addr) const
    {
        return m_secondaryOam[addr & 0x1F];
    }

    void debugWriteSecondaryOam(uint8_t addr, uint8_t data)
    {
        m_secondaryOam[addr & 0x1F] = data;
    }

    int debugBackgroundPatternTableAddress() const
    {
        return static_cast<int>(m_backgroundPatternTableAddress);
    }

    bool debugSpritesEnabled() const
    {
        return m_spritesEnabled;
    }

    uint32_t debugHashChrTile(int tileIndex)
    {
        const int baseAddress = (tileIndex & 0x01FF) * 16;
        uint8_t bytes[16] = {};
        for(int offset = 0; offset < 16; ++offset) {
            bytes[offset] = debugPeekPpuMemory(static_cast<uint16_t>(baseAddress + offset));
        }

        uint32_t hash = 0;
        for(size_t i = 0; i < std::size(bytes); i += sizeof(uint32_t)) {
            uint32_t chunk = 0;
            std::memcpy(&chunk, bytes + i, sizeof(uint32_t));
            hash += chunk;
            hash = (hash << 2) | (hash >> 30);
        }
        return hash;
    }

    void debugSetModRenderCaptureEnabled(bool enabled)
    {
        if(m_debugModRenderCaptureEnabled == enabled) {
            return;
        }
        m_debugModRenderCaptureEnabled = enabled;
        if(enabled) {
            const size_t pixelCount = static_cast<size_t>(SCREEN_WIDTH * SCREEN_HEIGHT);
            if(m_debugModBackgroundPixels.size() != pixelCount) {
                m_debugModBackgroundPixels.assign(pixelCount, {});
            }
            if(m_debugModSpritePixels.size() != pixelCount) {
                m_debugModSpritePixels.assign(pixelCount, {});
            }
            m_debugModScanlineScrollX.fill(0);
            m_debugModScanlineScrollY.fill(0);
            m_debugModBackgroundShift = {};
        }
    }

    void debugPublishModRenderCapture()
    {
        if(!m_debugModRenderCaptureEnabled || m_debugModBackgroundPixels.empty() || m_debugModSpritePixels.empty()) {
            return;
        }
        m_debugModPresentedBackgroundPixels = m_debugModBackgroundPixels;
        m_debugModPresentedSpritePixels = m_debugModSpritePixels;
        m_debugModPresentedScanlineScrollX = m_debugModScanlineScrollX;
        m_debugModPresentedScanlineScrollY = m_debugModScanlineScrollY;
    }

    const DebugModBackgroundPixel& debugModBackgroundPixelAt(int x, int y) const
    {
        static const DebugModBackgroundPixel empty;
        if(x < 0 || x >= SCREEN_WIDTH || y < 0 || y >= SCREEN_HEIGHT) {
            return empty;
        }
        if(!m_debugModPresentedBackgroundPixels.empty()) {
            return m_debugModPresentedBackgroundPixels[static_cast<size_t>(y) * SCREEN_WIDTH + static_cast<size_t>(x)];
        }
        if(m_debugModBackgroundPixels.empty()) {
            return empty;
        }
        return m_debugModBackgroundPixels[static_cast<size_t>(y) * SCREEN_WIDTH + static_cast<size_t>(x)];
    }

    const DebugModSpritePixel& debugModSpritePixelAt(int x, int y) const
    {
        static const DebugModSpritePixel empty;
        if(x < 0 || x >= SCREEN_WIDTH || y < 0 || y >= SCREEN_HEIGHT) {
            return empty;
        }
        if(!m_debugModPresentedSpritePixels.empty()) {
            return m_debugModPresentedSpritePixels[static_cast<size_t>(y) * SCREEN_WIDTH + static_cast<size_t>(x)];
        }
        if(m_debugModSpritePixels.empty()) {
            return empty;
        }
        return m_debugModSpritePixels[static_cast<size_t>(y) * SCREEN_WIDTH + static_cast<size_t>(x)];
    }

    void debugCopyPresentedBackgroundPixels(std::vector<DebugModBackgroundPixel>& out) const
    {
        if(!m_debugModPresentedBackgroundPixels.empty()) {
            out = m_debugModPresentedBackgroundPixels;
            return;
        }
        out = m_debugModBackgroundPixels;
    }

    void debugCopyPresentedSpritePixels(std::vector<DebugModSpritePixel>& out) const
    {
        if(!m_debugModPresentedSpritePixels.empty()) {
            out = m_debugModPresentedSpritePixels;
            return;
        }
        out = m_debugModSpritePixels;
    }

    const DebugModBackgroundPixel* debugPresentedBackgroundPixelsData() const
    {
        if(!m_debugModPresentedBackgroundPixels.empty()) {
            return m_debugModPresentedBackgroundPixels.data();
        }
        return m_debugModBackgroundPixels.empty() ? nullptr : m_debugModBackgroundPixels.data();
    }

    size_t debugPresentedBackgroundPixelsCount() const
    {
        if(!m_debugModPresentedBackgroundPixels.empty()) {
            return m_debugModPresentedBackgroundPixels.size();
        }
        return m_debugModBackgroundPixels.size();
    }

    const DebugModSpritePixel* debugPresentedSpritePixelsData() const
    {
        if(!m_debugModPresentedSpritePixels.empty()) {
            return m_debugModPresentedSpritePixels.data();
        }
        return m_debugModSpritePixels.empty() ? nullptr : m_debugModSpritePixels.data();
    }

    size_t debugPresentedSpritePixelsCount() const
    {
        if(!m_debugModPresentedSpritePixels.empty()) {
            return m_debugModPresentedSpritePixels.size();
        }
        return m_debugModSpritePixels.size();
    }

    int debugPresentedScanlineScrollX(int y) const
    {
        if(y < 0 || y >= SCREEN_HEIGHT) {
            return 0;
        }
        return m_debugModPresentedScanlineScrollX[static_cast<size_t>(y)];
    }

    int debugPresentedScanlineScrollY(int y) const
    {
        if(y < 0 || y >= SCREEN_HEIGHT) {
            return 0;
        }
        return m_debugModPresentedScanlineScrollY[static_cast<size_t>(y)];
    }

    void debugSetModBackgroundPixelForTest(int x, int y, const DebugModBackgroundPixel& pixel)
    {
        if(x < 0 || x >= SCREEN_WIDTH || y < 0 || y >= SCREEN_HEIGHT) {
            return;
        }
        DebugModBackgroundPixel normalized = pixel;
        if(m_debugModBackgroundPixels.empty()) {
            m_debugModBackgroundPixels.assign(static_cast<size_t>(SCREEN_WIDTH * SCREEN_HEIGHT), {});
        }
        m_debugModBackgroundPixels[static_cast<size_t>(y) * SCREEN_WIDTH + static_cast<size_t>(x)] = normalized;
        if(m_debugModPresentedBackgroundPixels.empty()) {
            m_debugModPresentedBackgroundPixels.assign(static_cast<size_t>(SCREEN_WIDTH * SCREEN_HEIGHT), {});
        }
        const size_t index = static_cast<size_t>(y) * SCREEN_WIDTH + static_cast<size_t>(x);
        m_debugModPresentedBackgroundPixels[index] = normalized;
    }

    void debugSetModSpritePixelForTest(int x, int y, const DebugModSpritePixel& pixel)
    {
        if(x < 0 || x >= SCREEN_WIDTH || y < 0 || y >= SCREEN_HEIGHT) {
            return;
        }
        DebugModSpritePixel normalized = pixel;
        if(m_debugModSpritePixels.empty()) {
            m_debugModSpritePixels.assign(static_cast<size_t>(SCREEN_WIDTH * SCREEN_HEIGHT), {});
        }
        m_debugModSpritePixels[static_cast<size_t>(y) * SCREEN_WIDTH + static_cast<size_t>(x)] = normalized;
        if(m_debugModPresentedSpritePixels.empty()) {
            m_debugModPresentedSpritePixels.assign(static_cast<size_t>(SCREEN_WIDTH * SCREEN_HEIGHT), {});
        }
        const size_t index = static_cast<size_t>(y) * SCREEN_WIDTH + static_cast<size_t>(x);
        m_debugModPresentedSpritePixels[index] = normalized;
    }

    GERANES_INLINE_HOT void evaluateSprites()
    {       
        if(m_cycle < 65) {
            m_oamCopyBuffer = 0xFF;
            m_secondaryOam[(m_cycle-1) >> 1] = 0xFF;
        }
        else if(m_cycle == 256) {
            if(m_settings.spriteLimitDisabled()) {
                // Keep a full list of visible sprites for the next scanline so the
                // optional sprite-limit removal can draw sprites beyond secondary OAM.
                cacheSpritesInNextScanline();
            }
            else {
                m_spritesInThisLine = static_cast<int>((m_secondaryOamAddr + 3) >> 2);
                if(m_spritesInThisLine > 8) {
                    m_spritesInThisLine = 8;
                }
            }
            m_testSprite0HitInThisLine = m_sprite0Added;
        }
        else {

            if(m_cycle == 65) { //initialization
                m_oamCopyDone = false;
                m_secondaryOamAddr = 0;
                m_spriteInRange = false;
                m_oamAddrN = (m_oamAddr >> 2) & 0x3F;
                m_oamAddrM = m_oamAddr & 0x03;


                m_sprite0Added = false;
                m_overflowBugCounter = 0;
            }

            if(m_cycle & 1) { //odd cycle
                m_oamCopyBuffer = m_primaryOam[m_oamAddr&0xFF];
            }
            else {                

                if(m_oamCopyDone) {

                    m_oamAddrN = (m_oamAddrN + 1) & 0x3F;

                    if(m_secondaryOamAddr >= sizeof(m_secondaryOam)) {
                        //"As seen above, a side effect of the OAM write disable signal is to turn writes to the secondary OAM into reads from it."
                        m_oamCopyBuffer = m_secondaryOam[m_secondaryOamAddr & 0x1F];
                    }
                }
                else {

                    if(!m_spriteInRange && m_scanline >= m_oamCopyBuffer &&
                       m_scanline < m_oamCopyBuffer + static_cast<int>(m_spriteHeight)) {
                        m_spriteInRange = true;
                    }                    

                    if(m_secondaryOamAddr < sizeof(m_secondaryOam)) {

                        m_secondaryOam[m_secondaryOamAddr] = m_oamCopyBuffer;

                        if(m_spriteInRange) {
                            
                            m_oamAddrM++;
                            m_secondaryOamAddr++;

                            if(m_cycle == 66) {
							    m_sprite0Added = true;
							}

                            if(m_oamAddrM >= 4) {
                                m_oamAddrN = (m_oamAddrN + 1) & 0x3F;
                                m_oamAddrM = 0;

                                if(m_oamAddrN == 0) {
                                    m_oamCopyDone = true;
                                }
                            }

                            //Note: Using "(m_secondaryOamAddr & 0x03) == 0" instead of "m_oamAddrM == 0" is required
							//to replicate a hardware bug noticed in oam_flicker_test_reenable when disabling & re-enabling
							//rendering on a single scanline
							if((m_secondaryOamAddr & 0x03) == 0) {
								//Done copying all 4 bytes
                                m_spriteInRange = false;

                                if(m_oamAddrM != 0) {
                                    bool inRange = m_scanline >= m_oamCopyBuffer &&
                                                   m_scanline < m_oamCopyBuffer + static_cast<int>(m_spriteHeight);
                                    if(!inRange) {
                                        m_oamAddrM = 0;
                                    }
                                }
							}
                        }
                        else {

                            //Nothing to copy, skip to next sprite
							m_oamAddrN = (m_oamAddrN + 1) & 0x3F;
                            m_oamAddrM = 0;
							if(m_oamAddrN == 0) {
								m_oamCopyDone = true;
							}
                        }
                    }
                    else { //secondary is full

                        m_oamCopyBuffer = m_secondaryOam[m_secondaryOamAddr & 0x1F];

                        //8 sprites have been found, check next sprite for overflow + emulate PPU bug
						if(m_spriteInRange) {
							//Sprite is visible, consider this to be an overflow
							m_spriteOverflow= true;
							m_oamAddrM = (m_oamAddrM + 1);
							if(m_oamAddrM == 4) {
								m_oamAddrN = (m_oamAddrN + 1) & 0x3F;
								m_oamAddrM = 0;
							}

							if(m_overflowBugCounter == 0) {
								m_overflowBugCounter = 3;
							} else if(m_overflowBugCounter > 0) {
								m_overflowBugCounter--;
								if(m_overflowBugCounter == 0) {
									//"After it finishes "fetching" this sprite(and setting the overflow flag), it realigns back at the beginning of this line and then continues here on the next sprite"
									m_oamCopyDone = true;
									m_oamAddrM = 0;
								}
							}

						} else {
							//Sprite isn't on this scanline, trigger sprite evaluation bug - increment both H & L at the same time
							m_oamAddrN = (m_oamAddrN + 1) & 0x3F;
							m_oamAddrM = (m_oamAddrM + 1) & 0x03;

							if(m_oamAddrN == 0) {
								m_oamCopyDone = true;
							}
						}

                    }
                }                

                m_oamAddr = (m_oamAddrM & 0x03) | (m_oamAddrN << 2);
            }
        }    

    }    

    GERANES_INLINE void setOamCorruptionFlags()
    {
        if(m_cycle >= 0 && m_cycle < 64) {
            m_corruptOamRow[m_cycle >> 1] = true;
        }
        else if(m_cycle >= 256 && m_cycle < 320) {
            const uint8_t base = static_cast<uint8_t>((m_cycle - 256) >> 3);
            const uint8_t offset = static_cast<uint8_t>(std::min(3, (m_cycle - 256) & 0x07));
            m_corruptOamRow[base * 4 + offset] = true;
        }
    }

    GERANES_INLINE void processOamCorruption()
    {
        for(int i = 0; i < 32; i++) {
            if(m_corruptOamRow[i]) {
                if(i > 0) {
                    memcpy(m_primaryOam + i * 8, m_primaryOam, 8);
                }
                m_corruptOamRow[i] = false;
            }
        }
    }

    GERANES_INLINE uint8_t getSpritePixelFake(const Sprite* sprite, int xOnScreen)
    {
        int spriteY = sprite->y + 1;
        int spriteLineToDraw;
        int spriteXToDraw;

        if((sprite->attrib & 0x80) == 0) spriteLineToDraw = m_currentY - spriteY;
        else spriteLineToDraw = (static_cast<int>(m_spriteHeight) - 1) - (m_currentY - spriteY);

        if((sprite->attrib & 0x40) == 0) spriteXToDraw = xOnScreen - sprite->x;
        else spriteXToDraw = sprite->x - xOnScreen + 7;

        if(m_spriteHeight == SpriteHeight::H8) {
            int index = sprite->indexInPatternTable + (static_cast<int>(m_spritePatternTableAddress) >> 4);
            return (uint8_t)(getColorLowBitsInPatternTable(index, spriteXToDraw, spriteLineToDraw) | ((sprite->attrib & 0x03) << 2));
        }

        int index = sprite->indexInPatternTable;
        if(spriteLineToDraw >= 8) index++;
        if(sprite->indexInPatternTable & 0x01) index += 255;
        return (uint8_t)(getColorLowBitsInPatternTable(index, spriteXToDraw, spriteLineToDraw & 0x07) | ((sprite->attrib & 0x03) << 2));
    }

    GERANES_INLINE const Sprite& primaryOamSprite(uint8_t addr) const
    {
        return *reinterpret_cast<const Sprite*>(&m_primaryOam[addr]);
    }

    GERANES_INLINE const Sprite& secondaryOamSprite(int spriteIndex) const
    {
        return *reinterpret_cast<const Sprite*>(&m_secondaryOam[spriteIndex << 2]);
    }

    GERANES_INLINE int getSpriteRenderLimit(bool& spriteLimitDisabledOut, int& maxSpritesOut) const
    {
        int indexedSpritesCount = (m_spritesInThisLine > 64) ? 64 : m_spritesInThisLine;
        if(m_spriteFetchCount > indexedSpritesCount) {
            indexedSpritesCount = m_spriteFetchCount;
        }

        const bool spriteLimitDisabled = m_settings.spriteLimitDisabled();
        bool spritesAsMask = false;
        if(spriteLimitDisabled && indexedSpritesCount >= 8) {
            const Sprite& first = primaryOamSprite(m_spritesIndexesInThisLine[0]);
            int i = 1;
            for(; i < 8; i++) {
                const Sprite& other = primaryOamSprite(m_spritesIndexesInThisLine[i]);
                if(first.y != other.y && first.indexInPatternTable != other.indexInPatternTable) {
                    break;
                }
            }
            spritesAsMask = i == 8;
        }

        int maxSprites = indexedSpritesCount;
        if(!spriteLimitDisabled || spritesAsMask) {
            if(maxSprites > 8) {
                maxSprites = 8;
            }
        }

        spriteLimitDisabledOut = spriteLimitDisabled;
        maxSpritesOut = maxSprites;
        const int renderedSpriteCount = activeSpriteRenderCount();
        return (maxSprites < renderedSpriteCount) ? maxSprites : renderedSpriteCount;
    }

    GERANES_INLINE void applySpritePixel(int paletteIndex, bool isPixelBehind)
    {
        if((paletteIndex & 0x03) != 0) {
            if((m_currentPixelColorIndex & 0x03) == 0 || !isPixelBehind) {
                m_currentPixelColorIndex = static_cast<uint8_t>(paletteIndex);
            }
        }
    }

    GERANES_INLINE_HOT void renderSpritesPixelFast(int renderedSpriteLimit, int maxSprites)
    {
        int paletteIndex = 0;
        bool isPixelBehind = false;

        for(int i = 0; i < renderedSpriteLimit; i++) {
            SpriteRenderEntry& sprite = m_spriteRenderEntries[i];
            if(!sprite.active || sprite.counting) {
                continue;
            }

            const int color = ((sprite.lowShift & 0x80) ? 0x01 : 0x00) |
                              ((sprite.highShift & 0x80) ? 0x02 : 0x00);
            if(color == 0) {
                continue;
            }

            paletteIndex = color | ((sprite.attr & 0x03) << 2);

            if(sprite.sprite0 && m_backgroundEnabled &&
               (m_currentPixelColorIndex & 0x03) != 0 &&
               m_currentX != 255) {
                m_sprite0Hit = true;
            }

            paletteIndex = 0x10 + paletteIndex;
            isPixelBehind = (sprite.attr & 0x20) != 0;
            break;
        }

        if(paletteIndex == 0 && maxSprites > 8) {
            for(int i = 8; i < maxSprites; i++) {
                const Sprite& sprite = primaryOamSprite(m_spritesIndexesInThisLine[i]);
                if(m_currentX < sprite.x || m_currentX >= sprite.x + 8) continue;

                const uint8_t low = getSpritePixelFake(&sprite, m_currentX);
                if((low & 0x03) == 0) continue;

                paletteIndex = 0x10 + low;
                isPixelBehind = (sprite.attrib & 0x20) != 0;
                break;
            }
        }

        applySpritePixel(paletteIndex, isPixelBehind);
    }

    GERANES_INLINE_HOT void renderSpritesPixelDebug(int renderedSpriteLimit, int maxSprites)
    {
        int paletteIndex = 0;
        bool isPixelBehind = false;
        DebugModSpritePixel* capturedPixel = nullptr;
        if(!m_debugModSpritePixels.empty() &&
           m_currentX >= 0 && m_currentX < SCREEN_WIDTH && m_currentY >= 0 && m_currentY < SCREEN_HEIGHT) {
            capturedPixel = &m_debugModSpritePixels[static_cast<size_t>(m_currentY) * SCREEN_WIDTH + static_cast<size_t>(m_currentX)];
        }

        for(int i = 0; i < renderedSpriteLimit; i++) {
            SpriteRenderEntry& sprite = m_spriteRenderEntries[i];
            if(!sprite.active || sprite.counting) {
                continue;
            }

            const int color = ((sprite.lowShift & 0x80) ? 0x01 : 0x00) |
                              ((sprite.highShift & 0x80) ? 0x02 : 0x00);

            if(capturedPixel != nullptr) {
                const int rawOffsetX = m_currentX - static_cast<int>(sprite.x);
                if(sprite.valid && rawOffsetX >= 0 && rawOffsetX < 8 &&
                   capturedPixel->count < capturedPixel->candidates.size()) {
                    DebugModSpriteCandidate& candidate = capturedPixel->candidates[capturedPixel->count++];
                    candidate.tileIndex = sprite.tileIndex;
                    candidate.tileHash = sprite.tileIndex <= 0x01FF ? debugHashChrTile(sprite.tileIndex) : 0;
                    candidate.palette[0] = static_cast<uint8_t>(m_palette[0x11 + ((sprite.attr & 0x03) << 2)] & 0x3F);
                    candidate.palette[1] = static_cast<uint8_t>(m_palette[0x12 + ((sprite.attr & 0x03) << 2)] & 0x3F);
                    candidate.palette[2] = static_cast<uint8_t>(m_palette[0x13 + ((sprite.attr & 0x03) << 2)] & 0x3F);
                    candidate.paletteSlot = static_cast<uint8_t>(sprite.attr & 0x03);
                    candidate.colorLowBits = static_cast<uint8_t>(color);
                    candidate.offsetX = static_cast<uint8_t>(rawOffsetX);
                    candidate.offsetY = sprite.row;
                    candidate.behindBackground = (sprite.attr & 0x20) != 0;
                    candidate.horizontalMirror = (sprite.attr & 0x40) != 0;
                    candidate.verticalMirror = (sprite.attr & 0x80) != 0;
                    candidate.valid = true;
                    if(!capturedPixel->valid) {
                        capturedPixel->tileIndex = candidate.tileIndex;
                        capturedPixel->tileHash = candidate.tileHash;
                        capturedPixel->palette[0] = candidate.palette[0];
                        capturedPixel->palette[1] = candidate.palette[1];
                        capturedPixel->palette[2] = candidate.palette[2];
                        capturedPixel->paletteSlot = candidate.paletteSlot;
                        capturedPixel->colorLowBits = candidate.colorLowBits;
                        capturedPixel->offsetX = candidate.offsetX;
                        capturedPixel->offsetY = candidate.offsetY;
                        capturedPixel->behindBackground = candidate.behindBackground;
                        capturedPixel->horizontalMirror = candidate.horizontalMirror;
                        capturedPixel->verticalMirror = candidate.verticalMirror;
                        capturedPixel->valid = true;
                    }
                }
            }

            if(color == 0) {
                continue;
            }

            paletteIndex = color | ((sprite.attr & 0x03) << 2);

            if(sprite.sprite0 && m_backgroundEnabled &&
               (m_currentPixelColorIndex & 0x03) != 0 &&
               m_currentX != 255) {
                m_sprite0Hit = true;
            }

            paletteIndex = 0x10 + paletteIndex;
            isPixelBehind = (sprite.attr & 0x20) != 0;
            break;
        }

        if(paletteIndex == 0 && maxSprites > 8) {
            for(int i = 8; i < maxSprites; i++) {
                const Sprite& sprite = primaryOamSprite(m_spritesIndexesInThisLine[i]);
                if(m_currentX < sprite.x || m_currentX >= sprite.x + 8) continue;

                const uint8_t low = getSpritePixelFake(&sprite, m_currentX);
                if((low & 0x03) == 0) continue;

                paletteIndex = 0x10 + low;
                isPixelBehind = (sprite.attrib & 0x20) != 0;
                break;
            }
        }

        applySpritePixel(paletteIndex, isPixelBehind);
    }

    GERANES_INLINE_HOT void renderSpritesPixel()
    {
        if(!m_showSpritesLeftmost8Pixels && m_currentX < 8) return;

        bool spriteLimitDisabled = false;
        int maxSprites = 0;
        const int renderedSpriteLimit = getSpriteRenderLimit(spriteLimitDisabled, maxSprites);
        if(m_debugModRenderCaptureEnabled) {
            renderSpritesPixelDebug(renderedSpriteLimit, maxSprites);
        }
        else {
            renderSpritesPixelFast(renderedSpriteLimit, maxSprites);
        }
    }

    GERANES_INLINE_HOT void clockSpriteRenderers(bool shiftRegisters)
    {
        for(int i = 0; i < 8; i++) {
            SpriteRenderEntry& sprite = m_spriteRenderEntries[i];
            if(!sprite.active) {
                continue;
            }

            if(sprite.counting) {
                if(sprite.xCounter > 0) {
                    sprite.xCounter--;
                }
                if(sprite.xCounter == 0) {
                    sprite.counting = false;
                }
            }
            else if(shiftRegisters) {
                sprite.lowShift <<= 1;
                sprite.highShift <<= 1;
            }
        }
    }

    GERANES_INLINE bool hasActiveSpriteRenderers() const
    {
        for(int i = 0; i < 8; i++) {
            if(m_spriteRenderEntries[i].active) {
                return true;
            }
        }
        return false;
    }

    GERANES_INLINE int activeSpriteRenderCount() const
    {
        int count = 0;
        for(int i = 0; i < 8; i++) {
            if(m_spriteRenderEntries[i].active) {
                ++count;
            }
        }
        return count;
    }

    GERANES_INLINE_HOT void onScanlineStart()
    {
        const bool renderingEnabled = m_renderingEnabled;

        if(m_settings.overclockLines() > 0) {
            if(m_overclockFrame && m_scanline >= 241 && m_scanline < FRAME_VBLANK_START_LINE)
                m_inOverclockLines = true;
            else
                m_inOverclockLines = false;
        }
        else m_inOverclockLines = false;

        m_preLine = (m_scanline == FRAME_VBLANK_END_LINE);
        m_visibleLine = m_scanline < 240;
        m_renderLine = m_preLine || m_visibleLine;
        m_spriteRenderClockingActiveThisLine = m_prevCycleRenderingEnabled || hasActiveSpriteRenderers();
        if(m_preLine && renderingEnabled) {
            processOamCorruption();
        }

        if(m_scanline < 240) {

            if(m_scanline == 0) {

                updateSettings();

                if(m_settings.overclockLines() > 0) {
                    m_overclockFrame = true;
                }

                signalFrameStart();
            }

            if(m_prevCycleRenderingEnabled) {
                if(m_scanline > 0 || (!m_oddFrameFlag || m_settings.region() != Settings::Region::NTSC /*|| GetPpuModel() != PpuModel::Ppu2C02)*/)) {
                    //Set bus address to the tile address calculated from the unused NT fetches at the end of the previous scanline
                    //This doesn't happen on scanline 0 if the last dot of the previous frame was skipped
                    setBusAddress(m_tileAddr);
			    }
            }
            

        }
        else if(m_preLine) {
            // Sprite flags are cleared at the start of the pre-render scanline.
            m_spriteOverflow = false;
            m_sprite0Hit = false;
        }
        else if(m_scanline == 240) {
            //At the start of vblank, the bus address is set back to VideoRamAddr.
            //According to Visual NES, this occurs on scanline 240, cycle 1, but is done here on cycle 0 for performance reasons
            setBusAddress(m_reg_v&0x3FFF);
        }
        else if(m_scanline == 241){
            decayOpenBus();
            debugPublishModRenderCapture();
            signalFrameReady();

            if(m_overclockFrame){
                FRAME_NUMBER_OF_LINES += m_settings.overclockLines();
                FRAME_VBLANK_START_LINE += m_settings.overclockLines();
                FRAME_VBLANK_END_LINE += m_settings.overclockLines();
            }
        }

        signalScanlineStart();
    }

    GERANES_INLINE void updateInterruptState()
    {
        if(!m_interruptFlag) {
            if(m_VBlankHasStarted && m_NMIOnVBlank) {
                m_interruptFlag = m_NMIOnVBlank;
            }
        }
        else if(!m_VBlankHasStarted) {
            m_interruptFlag = false;
        }
    }

    GERANES_INLINE void runBackgroundFetchPipeline()
    {
        shiftTileData();

        switch(m_cycle & 0x07) {
            case 1: setupNameTableByte(); break;
            case 2: fetchNameTableByte(); break;
            case 3: setupAttributeTableByte(); break;
            case 4: fetchAttributeTableByte(); break;
            case 5: setupLowTileByte(); break;
            case 6: fetchLowTileByte(); break;
            case 7: setupHighTileByte(); break;
            case 0: fetchHighTileByte(); storeTileData(); incrementVX(); break;
        }
    }

    GERANES_INLINE void runRenderLineFetches()
    {
        switch(m_cycle) {
            case 256:
                incrementVY();
                break;
            case 257:
                m_firstSpriteFetchV = m_reg_v;
                copyVX();
                break;
            case 337:
                setupNameTableByte();
                break;
            case 338:
                fetchNameTableByte();
                break;
            case 339:
                setupNameTableByte();
                break;
            case 340:
                fetchNameTableByte();
                break;
            case 0:
                setupLowTileByte();
                break;
        }
    }

    GERANES_INLINE void updateSpriteCountersAtEndOfScanline()
    {
        for(int i = 0; i < 8; i++) {
            SpriteRenderEntry& entry = m_spriteRenderEntries[i];
            if(entry.active) {
                entry.counting = entry.xCounter > 0;
            }
        }
    }

    GERANES_INLINE_HOT void ppuCycleVisibleLine(bool renderingEnabled, bool prevCycleRenderingEnabled)
    {
        const unsigned cycle = static_cast<unsigned>(m_cycle);
        const bool visibleCycle = cycle - 1u < 256u;
        const bool spriteFetchCycles = cycle - 257u < 64u;
        const bool bgFetchCycles = visibleCycle || (cycle - 321u < 16u);

        if(visibleCycle) {
            renderPixel();
        }

        if(renderingEnabled) {
            if(cycle == 321u) {
                m_oamCopyBuffer = m_secondaryOam[0];
            }

            if(visibleCycle) {
                evaluateSprites();
            }
        }

        if(prevCycleRenderingEnabled) {
            if(bgFetchCycles) {
                runBackgroundFetchPipeline();
            }

            runRenderLineFetches();

            if(spriteFetchCycles) {
                // OAMADDR is forced to zero during sprite tile loading when rendering.
                m_oamAddr = 0;
                fetchSprites();
            }

            if(cycle == 339u) {
                updateSpriteCountersAtEndOfScanline();
            }
        }

        if(visibleCycle && (prevCycleRenderingEnabled || m_spriteRenderClockingActiveThisLine)) {
            // During forced blanking, sprite X counters keep advancing but the stale pattern
            // shift registers stop shifting until rendering is active again.
            clockSpriteRenderers(prevCycleRenderingEnabled);
        }
    }

    GERANES_INLINE_HOT void ppuCyclePreRenderLine(bool renderingEnabled, bool prevCycleRenderingEnabled)
    {
        const unsigned cycle = static_cast<unsigned>(m_cycle);
        const bool visibleCycle = cycle - 1u < 256u;
        const bool spriteFetchCycles = cycle - 257u < 64u;
        const bool bgFetchCycles = visibleCycle || (cycle - 321u < 16u);

        if(prevCycleRenderingEnabled) {
            if(bgFetchCycles) {
                runBackgroundFetchPipeline();
            }

            runRenderLineFetches();

            if(spriteFetchCycles) {
                // OAMADDR is forced to zero during sprite tile loading when rendering.
                m_oamAddr = 0;
                fetchSprites();
            }

            if(cycle == 339u) {
                updateSpriteCountersAtEndOfScanline();
            }
        }

        if(cycle <= 8u && renderingEnabled && m_oamAddr >= 0x08) {
            // If OAMADDR is not less than eight when rendering starts, the eight bytes
            // starting at OAMADDR & $F8 are copied to the first eight bytes of OAM.
            m_primaryOam[m_cycle - 1] = m_primaryOam[(m_oamAddr & 0xF8) + (m_cycle - 1)];
        }

        if(cycle - 280u < 25u) {
            if(m_prevCycleRenderingEnabled) {
                copyVY();
            }
        }
        else if(m_cycle == VBLANK_CYCLE) {
            m_VBlankHasStarted = false;
        }
        else if(cycle == 339u && m_settings.region() == Settings::Region::NTSC) {
            // Only NTSC has the skipped cycle on odd frames.
            if(m_oddFrameFlag && renderingEnabled) {
                m_cycle++;
            }
            m_oddFrameFlag = !m_oddFrameFlag;
        }
    }

    GERANES_INLINE void ppuCycleVBlankStartLine()
    {
        if(m_cycle == VBLANK_CYCLE && m_lastPPUSTATUSReadCycle != VBLANK_CYCLE) {
            m_VBlankHasStarted = true;
        }
    }

    GERANES_HOT void ppuCycle()
    {        
        if(m_cycle == 0) onScanlineStart();
        const bool renderingEnabled = m_renderingEnabled;
        const bool prevCycleRenderingEnabled = m_prevCycleRenderingEnabled;
        const bool renderLineWithPrevRendering = m_renderLine && prevCycleRenderingEnabled;

        m_cartridge.onPpuCycle(m_scanline, m_cycle, renderLineWithPrevRendering, m_preLine);
        updateInterruptState();

        if(m_visibleLine) {
            ppuCycleVisibleLine(renderingEnabled, prevCycleRenderingEnabled);
        }
        else if(m_preLine) {
            ppuCyclePreRenderLine(renderingEnabled, prevCycleRenderingEnabled);
        }
        else if(m_scanline == FRAME_VBLANK_START_LINE) {
            ppuCycleVBlankStartLine();
        }

        if(++m_cycle == 341) {
            m_lastPPUSTATUSReadCycle = -1;
            m_cycle = 0;

            if(++m_scanline == FRAME_NUMBER_OF_LINES) {
                m_scanline = 0;          
            }
            
        }        

        if(m_needUpdateState) updateState();
    }

    struct SpritePatternInfo {
        uint16_t tileIndex = 0xFFFF;
        uint8_t row = 0;
        uint16_t address = 0;
    };

    GERANES_INLINE SpritePatternInfo getSpritePatternInfo(const Sprite& sprite, bool highPlane)
    {
        const uint8_t spriteScanline = static_cast<uint8_t>(m_preLine ? 6 : (m_scanline + 1));
        uint8_t row = static_cast<uint8_t>(spriteScanline - static_cast<uint8_t>(sprite.y + 1));
        const uint8_t localRow = static_cast<uint8_t>(row & 0x07);

        if(sprite.attrib & 0x80) {
            row = static_cast<uint8_t>((static_cast<uint8_t>(m_spriteHeight) - 1) - (row & (static_cast<uint8_t>(m_spriteHeight) - 1)));
        }
        else {
            row = static_cast<uint8_t>(row & (static_cast<uint8_t>(m_spriteHeight) - 1));
        }

        uint16_t base = 0;
        uint16_t tileIndex = sprite.indexInPatternTable;
        if(m_spriteHeight == SpriteHeight::H16) {
            base = (sprite.indexInPatternTable & 0x01) ? 0x1000 : 0x0000;
            tileIndex = static_cast<uint16_t>(sprite.indexInPatternTable & 0xFE);
            if(row >= 8) {
                tileIndex++;
                row = static_cast<uint8_t>(row - 8);
            }
        }
        else {
            base = static_cast<uint16_t>(m_spritePatternTableAddress);
        }

        SpritePatternInfo info;
        const int32_t absoluteChrAddress = m_cartridge.mapper() != nullptr
            ? m_cartridge.mapper()->debugToAbsoluteChrAddress(static_cast<uint16_t>(base + (tileIndex << 4) + row))
            : -1;
        info.tileIndex = static_cast<uint16_t>(absoluteChrAddress >= 0
            ? (absoluteChrAddress >> 4)
            : ((base >> 4) + tileIndex));
        // Keep the captured row in display-local 0..7 space. The mirror flag remains
        // authoritative for source sampling; the fetched pattern address already points
        // at the mirrored tile row used by the PPU.
        info.row = localRow;
        info.address = static_cast<uint16_t>(base + (tileIndex << 4) + row + (highPlane ? 8 : 0));
        return info;
    }

    GERANES_INLINE uint16_t getSpritePatternAddress(const Sprite& sprite, bool highPlane)
    {
        return getSpritePatternInfo(sprite, highPlane).address;
    }

    GERANES_INLINE bool isSpriteInRangeForScanline(const Sprite& sprite, int scanline) const
    {
        const int spriteY = static_cast<int>(sprite.y) + 1;
        return scanline >= spriteY && scanline < spriteY + static_cast<int>(m_spriteHeight);
    }

    GERANES_INLINE void cacheSpritesInNextScanline()
    {
        m_spritesInThisLine = 0;

        const int nextScanline = (m_scanline + 1) % m_settings.PPULinesPerFrame();
        for(int spriteIndex = 0; spriteIndex < 64; spriteIndex++) {
            const Sprite& sprite = primaryOamSprite(static_cast<uint8_t>(spriteIndex << 2));
            if(!isSpriteInRangeForScanline(sprite, nextScanline)) {
                continue;
            }

            m_spritesIndexesInThisLine[m_spritesInThisLine++] = static_cast<uint8_t>(spriteIndex << 2);
        }
    }

    void fetchSprites() {
        // Mapper hooks must classify sprite-cycle PPU reads as sprite-source.
        setPpuFetchSourceCached(true);

        const int startCycle = 257;
        const int cycleOffset = m_cycle - startCycle;
        const int spriteIndex = cycleOffset >> 3;

        if(m_cycle == startCycle) {
            m_spriteFetchCount = 0;
            memset(m_spriteFetchEntries, 0, sizeof(m_spriteFetchEntries));
        }

        const int fetchCycle = cycleOffset & 0x07;
        if(spriteIndex < 0 || spriteIndex >= 8) {
            return;
        }

        int fetchedSpriteCount = m_preLine ? 8 : static_cast<int>((m_secondaryOamAddr + 3) >> 2);
        if(fetchedSpriteCount > 8) {
            fetchedSpriteCount = 8;
        }
        const bool spriteSlotValid = spriteIndex < fetchedSpriteCount;
        const Sprite& sprite = secondaryOamSprite(spriteIndex);
        bool hasSpriteData = spriteSlotValid;
        if(hasSpriteData && m_preLine) {
            hasSpriteData = isSpriteInRangeForScanline(sprite, 5);
        }
        SpriteFetchEntry& entry = m_spriteFetchEntries[spriteIndex];
        entry.x = hasSpriteData ? sprite.x : 0xFF;
        entry.attr = hasSpriteData ? sprite.attrib : 0;
        entry.sprite0 = hasSpriteData && (spriteIndex == 0) && (m_testSprite0HitInThisLine || m_preLine);
        const uint16_t nameTableAddr = getNameTableAddr();

        switch(fetchCycle) {

            case 0:
                if(spriteIndex == 0) {
                    setupPpuReadAddress(static_cast<uint16_t>(0x2000 | (m_firstSpriteFetchV & 0x0F00) | ((m_reg_v + 2) & 0x00FF)));
                }
                else {
                    setupPpuReadAddress(nameTableAddr);
                }
                break;
            case 1:
                if(spriteIndex == 0) {
                    completePpuRead(static_cast<uint16_t>(0x2000 | (m_firstSpriteFetchV & 0x0F00) | ((m_reg_v + 2) & 0x00FF)));
                }
                else {
                    completePpuRead(nameTableAddr);
                }
                break;
            case 2:
                setupPpuReadAddress(nameTableAddr);
                break;
            case 3:
                completePpuRead(nameTableAddr);
                break;
            case 4:
                {
                    const SpritePatternInfo info = getSpritePatternInfo(sprite, false);
                    if(hasSpriteData && sprite.y != 0xFF) {
                        entry.tileIndex = info.tileIndex;
                        entry.patternAddress = info.address;
                        entry.row = info.row;
                        entry.valid = true;
                    }
                    setupPpuReadAddress(info.address);
                }
                break;
            case 5:
                {
                    const uint8_t value = completePpuRead(getSpritePatternAddress(sprite, false));
                    entry.lowByte = (hasSpriteData && sprite.y != 0xFF) ? value : 0;
                }
                break;
            case 6:
                setupPpuReadAddress(getSpritePatternAddress(sprite, true));
                break;
            case 7:
                {
                    const uint8_t value = completePpuRead(getSpritePatternAddress(sprite, true));
                    entry.highByte = (hasSpriteData && sprite.y != 0xFF) ? value : 0;
                    SpriteRenderEntry& renderEntry = m_spriteRenderEntries[spriteIndex];
                    if(hasSpriteData && sprite.y != 0xFF) {
                        m_spriteFetchCount = static_cast<uint8_t>(spriteIndex + 1);
                        renderEntry.x = entry.x;
                        renderEntry.xCounter = entry.x;
                        renderEntry.attr = entry.attr;
                        renderEntry.lowShift = entry.lowByte;
                        renderEntry.highShift = entry.highByte;
                        renderEntry.tileIndex = entry.tileIndex;
                        renderEntry.patternAddress = entry.patternAddress;
                        renderEntry.row = entry.row;
                        renderEntry.sprite0 = entry.sprite0;
                        renderEntry.counting = false;
                        renderEntry.active = true;
                        renderEntry.valid = entry.valid;

                        if((renderEntry.attr & 0x40) != 0) {
                            renderEntry.lowShift = reverseByte(renderEntry.lowShift);
                            renderEntry.highShift = reverseByte(renderEntry.highShift);
                        }
                    }
                    else {
                        renderEntry.counting = false;
                        renderEntry.active = false;
                        renderEntry.valid = false;
                    }
                }
                break;
        }
    
    }

    //name tables
    //01
    //23
    //(64x60) tiles   (32*30) each table
    GERANES_INLINE uint8_t getTileIndexInNameTables(int x, int y)
    {
        if(y < 30) //table 0 or 1
        {
            //0x2000 + y*32 + x
            if(x < 32) return fakeReadPpuMemory(0x2000 + (y << 5) + x); //name table 0
            //0x2400 + y*32 + (x-32)
            else return fakeReadPpuMemory(0x2400 + (y << 5) + (x-32)); //name table 1
        }
        else //table 2 or 3
        {
            //0x2800 + (y-30)*32 + x
            if(x < 32) return fakeReadPpuMemory(0x2800 + ((y-30) << 5) + x); //name table 2
            //0x2C00 + (y-30)*32 + (x-32)
            else return fakeReadPpuMemory(0x2C00 + ((y-30) << 5) + (x-32)); //name table 3
        }
    }

    //in pixels 512x480
    uint8_t getColorHighBitsInAttributeTables(int x, int y)
    {
        uint16_t baseNameTableAddress;

        if(y < 240) //table 0 or 1
        {
            if(x < 256) baseNameTableAddress = 0x2000; //attribute table 0
            else baseNameTableAddress = 0x2400; //attribute table 1
        }
        else //table 2 or 3
        {
            if(x < 256) baseNameTableAddress = 0x2800; //attribute table 2
            else baseNameTableAddress = 0x2C00; //attribute table 3
        }

        x %= 256;
        y %= 240;

        //uint8_t highBitsColor = readPPUMemory(baseNameTableAddress+0x03C0 + (y/32)*8 + (x/32) );
        uint8_t highBitsColor = fakeReadPpuMemory(baseNameTableAddress+0x03C0 + (y >> 2) + (x >> 5) );

        //highBitsColor >>= 4*((y/16)%2) + 2*((x/16)%2);
        highBitsColor >>= ( ((y>>4)%2) << 2 ) + ( ((x >> 4)%2) << 1 );

        highBitsColor &= 0x03;
        highBitsColor <<= 2;

        return highBitsColor;
    }

    //in pixels 512x480
    uint8_t getColorLowBitsInNameTables(int x, int y)
    {
        m_isSpritePatternFetch = false;
        const int tileX = x >> 3; //x/8
        const int tileY = y >> 3; //y/8

        int tileIndexInPatternTable = getTileIndexInNameTables(tileX,tileY);

        tileIndexInPatternTable += static_cast<int>(m_backgroundPatternTableAddress) >> 4;

        return getColorLowBitsInPatternTable(tileIndexInPatternTable,x%8, y%8);
    }

    //index0-511 x:0-7 y:0-7
    uint8_t getColorLowBitsInPatternTable(int tileIndex, int pixelX, int pixelY)
    {
        uint8_t colorIndex = 0;

        //if(fakeReadPPUMemory(tileIndex*16 + pixelY) & (0x80>>pixelX)) colorIndex = 0x01; //plane 1
        if(fakeReadPpuMemory((tileIndex << 4) + pixelY) & (0x80>>pixelX)) colorIndex = 0x01; //plane 1

        //if(fakeReadPPUMemory(8 + tileIndex*16 + pixelY) & (0x80>>pixelX)) colorIndex |= 0x02; //plane 2
        if(fakeReadPpuMemory(8 + (tileIndex << 4) + pixelY) & (0x80>>pixelX)) colorIndex |= 0x02; //plane 2

        return colorIndex;
    }

    GERANES_INLINE uint32_t getMonochromeColor(uint32_t color)
    {
        uint32_t temp = ( (color&0xFF) + ((color&0xFF00) >> 8) + ((color&0xFF0000) >> 16) ) / 3;
        return (color&0xFF000000) | (temp) | (temp<<8) | (temp<<16);
    }

    GERANES_INLINE uint32_t getEmphasisColor(uint32_t color)
    {
        uint32_t r = color&0xFF;
        uint32_t g = (color&0xFF00) >> 8;
        uint32_t b = (color&0xFF0000) >> 16;

        if(m_settings.region() == Settings::Region::NTSC) {
            if(m_colorEmphasis&0x01) r += 0x30;
            if(m_colorEmphasis&0x02) g += 0x30;
        }
        else {
            //"Note that on the Dendy and PAL NES, the green and red bits swap meaning."
            if(m_colorEmphasis&0x02) r += 0x30;
            if(m_colorEmphasis&0x01) g += 0x30;
        }
        
        if(m_colorEmphasis&0x04) b += 0x30;

        if(r > 0xFF) r = 0xFF;
        if(g > 0xFF) g = 0xFF;
        if(b > 0xFF) b = 0xFF;

        return (color&0xFF000000) | (r) | (g<<8) | (b<<16);
    }

    GERANES_INLINE void refreshOutputColorPalette()
    {
        for(size_t i = 0; i < m_outputColorPalette.size(); ++i) {
            uint8_t index = static_cast<uint8_t>(i);

            switch(m_vsPpuModel) {
            case GameDatabase::PpuModel::Ppu2C04A: index = VS_PALETTE_LUT_2C04_0001[index]; break;
            case GameDatabase::PpuModel::Ppu2C04B: index = VS_PALETTE_LUT_2C04_0002[index]; break;
            case GameDatabase::PpuModel::Ppu2C04C: index = VS_PALETTE_LUT_2C04_0003[index]; break;
            case GameDatabase::PpuModel::Ppu2C04D: index = VS_PALETTE_LUT_2C04_0004[index]; break;
            default:
                break;
            }

            uint32_t color = m_colorPalette[index];

            if(m_colorEmphasis != 0) {
                color = getEmphasisColor(color);
            }
            if(m_monochromeDisplay) {
                color = getMonochromeColor(color);
            }

            m_outputColorPalette[i] = color;
        }
    }

    GERANES_INLINE_HOT uint32_t NESToRGBAColor(uint8_t index)
    {
        return m_outputColorPalette[index & 0x3F];
    }

    void setVsPpuModel(GameDatabase::PpuModel model)
    {
        m_vsPpuModel = model;
        refreshOutputColorPalette();
    }
    
    GERANES_INLINE void writePPUCTRL(uint8_t data, bool notifyMapper = true)
    {
        //put base nametable address in bits 10 and 11 of reg_t
        m_reg_t &= 0xF3FF;
        m_reg_t |= ((static_cast<uint16_t>(data & 0x03)) << 10);
        calculateDebugCursor();

        m_VRAMAddressIncrement = (data&0x04) ? VramAddressIncrement::I32 : VramAddressIncrement::I1;
        m_spritePatternTableAddress = (data&0x08) ? PatternTableAddress::X1000 : PatternTableAddress::X0000;
        m_backgroundPatternTableAddress = (data&0x10) ? PatternTableAddress::X1000 : PatternTableAddress::X0000;
        m_spriteHeight = (data&0x20) ? SpriteHeight::H16 : SpriteHeight::H8;
        if(notifyMapper) {
            m_cartridge.setSpriteSize8x16(m_spriteHeight == SpriteHeight::H16);
        }
        m_PPUSlave = (data&0x40) ? true : false;
        m_NMIOnVBlank = (data&0x80) ? true : false;

        if(m_NMIOnVBlank == false) m_interruptFlag = false;

    }

    GERANES_INLINE void writePPUMASK(uint8_t data, bool notifyMapper = true)
    {
        const bool prevMonochromeDisplay = m_monochromeDisplay;
        const uint8_t prevColorEmphasis = m_colorEmphasis;
        m_monochromeDisplay = (data&0x01) ? true : false;
        m_showBackgroundLeftmost8Pixels = (data&0x02) ? true : false;
        m_showSpritesLeftmost8Pixels = (data&0x04) ? true : false;
        m_backgroundEnabled = (data&0x08) ? true : false;
        m_spritesEnabled = (data&0x10) ? true : false;
        m_colorEmphasis = data >> 5;        
        if(prevMonochromeDisplay != m_monochromeDisplay || prevColorEmphasis != m_colorEmphasis) {
            refreshOutputColorPalette();
        }
        if(notifyMapper) {
            m_cartridge.setPpuMask(data);
        }

        if(m_renderingEnabled != (m_backgroundEnabled || m_spritesEnabled)) {
		    m_needUpdateState = true;
	    }
    }

    GERANES_INLINE uint8_t readPPUSTATUS(bool notifyMapper = true)
    {
        uint8_t ret = 0x00;
        m_lastPPUSTATUSReadCycle = m_cycle;

        if(m_VBlankHasStarted) ret |= 0x80;
        if(notifyMapper) {
            m_cartridge.onPpuStatusRead((ret & 0x80) != 0);
        }

        if(m_sprite0Hit) ret |= 0x40;
        if(m_spriteOverflow) ret |= 0x20;

        m_VBlankHasStarted = false;
        m_interruptFlag = false;
        m_reg_w = false;

        return ret;
    }

    GERANES_INLINE void writeOAMADDR(uint8_t data)
    {
        m_oamAddr = data;
    }    

    GERANES_INLINE uint8_t readOAMDATA()
    {
        uint8_t ret = m_primaryOam[m_oamAddr];

        if (m_renderLine && isRenderingEnabled()) {
            int sampleCycle = m_cycle - 1;
            if(sampleCycle < 0) {
                sampleCycle += 341;
            }

            if(sampleCycle == 0) {
                ret = m_secondaryOam[0];
            }
            else if(sampleCycle >= 1 && sampleCycle <= 64) {
                ret = 0xFF;
            }
            else if(sampleCycle >= 65 && sampleCycle <= 256) {
                if(!m_oamCopyDone) {
                    ret = m_oamCopyBuffer;
                }
                else if(sampleCycle & 0x01) {
                    ret = m_primaryOam[m_oamAddr];
                }
                else {
                    ret = m_secondaryOam[m_secondaryOamAddr & 0x1F];
                }
            }
            else if(sampleCycle >= 257 && sampleCycle <= 320) {
                int spriteIndex = (sampleCycle - 257) / 8;
                if(spriteIndex < m_spritesInThisLine && spriteIndex < 8) {
                    int phase = (sampleCycle - 257) & 0x07;
                    int step = phase > 3 ? 3 : phase;
                    ret = m_secondaryOam[(spriteIndex << 2) + step];
                }
                else if(sampleCycle == 273) {
                    ret = m_secondaryOam[m_secondaryOamAddr & 0x1F];
                }
                else {
                    ret = 0xFF;
                }
            }
            else {
                ret = m_secondaryOam[0];
            }
        }

        return ret;
    }

    GERANES_INLINE bool isRenderingEnabled() {
        //return m_spritesEnabled || m_backgroundEnabled;
        return m_renderingEnabled;
    }

    GERANES_INLINE bool isActivelyRendering() {
        return m_renderLine && m_renderingEnabled;
    }

    GERANES_INLINE int scanline() const {
        return m_scanline;
    }

    GERANES_INLINE int cycle() const {
        return m_cycle;
    }

    GERANES_INLINE void setCpuDmaReadInProgress(bool state)
    {
        m_cpuDmaReadInProgress = state;
    }

    GERANES_INLINE bool sprite0Hit() const {
        return m_sprite0Hit;
    }

    GERANES_INLINE bool nmiLineActive() const {
        return m_VBlankHasStarted && m_NMIOnVBlank;
    }

    GERANES_INLINE void writeOAMDATA(uint8_t data)
    {
        if(!isActivelyRendering()) {
            if((m_oamAddr & 0x03) == 0x02) {
                //"The three unimplemented bits of each sprite's byte 2 do not exist in the PPU and always read back as 0 on PPU revisions that allow reading PPU OAM through OAMDATA ($2004)"
                data &= 0xE3;
            }
            m_primaryOam[m_oamAddr] = data;
            m_oamAddr++;
        } else {
            //"Writes to OAMDATA during rendering (on the pre-render line and the visible lines 0-239, provided either sprite or background rendering is enabled) do not modify values in OAM, 
            //but do perform a glitchy increment of OAMADDR, bumping only the high 6 bits"
            m_oamAddr = static_cast<uint8_t>((m_oamAddr + 4) & 0xFC);
        }
    
    }

    GERANES_INLINE void writePPUSCROLL(uint8_t data)
    {
        if(!m_reg_w)
        {
            m_reg_t &= 0xFFE0;
            m_reg_t |= data >> 3;
            m_reg_x = data&0x07;
        }
        else
        {
            m_reg_t &= 0x8C1F;
            m_reg_t |= (static_cast<uint16_t>(data&0x07)) << 12;
            m_reg_t |= (static_cast<uint16_t>(data&0xF8)) << 2;

            calculateDebugCursor();
        }

        m_reg_w = !m_reg_w;
    }

    GERANES_INLINE void calculateDebugCursor()
    {
        m_debugCursorX = ((m_reg_t & 0x1F) << 3) | (m_reg_x&0x07);
        if((m_reg_t>>10)&0x01) m_debugCursorX += 256;

        m_debugCursorY = (((m_reg_t >> 5)&0x1F)<<3) | ((m_reg_t >> 12)&0x07);
        if((m_reg_t>>10)&0x02) m_debugCursorY += 240;
    }

/*
regs t and v

yyy NN YYYYY XXXXX
||| || ||||| +++++-- coarse X scroll
||| || +++++-------- coarse Y scroll
||| ++-------------- nametable select
+++----------------- fine Y scroll

yyy NNYY YYYX XXXX
*/    

    GERANES_INLINE void writePPUADDR(uint8_t data)
    {
        if(!m_reg_w)
        {
            m_reg_t = (m_reg_t & 0x80FF) | ((static_cast<uint16_t>(data&0x3F))  << 8);
        }
        else
        {
            m_reg_t = (m_reg_t & 0xFF00) | (static_cast<uint16_t>(data));

            m_needUpdateState = true;
            m_update_reg_v_delay = 3;
            m_update_reg_v_value = m_reg_t;
            
            calculateDebugCursor();
        }

        m_reg_w = !m_reg_w;
    }

    GERANES_INLINE uint8_t readPPUDATA()
    {
        uint8_t ret;
        const bool activelyRendering = isActivelyRendering();
        const bool followingDataRead = !activelyRendering && !m_cpuDmaReadInProgress && hasPendingPpuDataReadUpdate();
        
        if(isOnPaletteAddr()) {
            ret = fakeReadPpuMemory(m_reg_v&0x3FFF); //palette
            if(activelyRendering) {
                armDeferredDataLatch(static_cast<uint16_t>(m_reg_v & 0x2FFF));
                armDeferredVideoRamIncrement();
            }
            else if(m_cpuDmaReadInProgress) {
                m_dataLatch = readPpuMemory(m_reg_v&0x2FFF);
                m_needIncVideoRam = true;
            }
            else if(!followingDataRead) {
                schedulePpuDataReadUpdate(static_cast<uint16_t>(m_reg_v & 0x2FFF));
            }
        }     
        else {
            ret = m_dataLatch;
            if(activelyRendering) {
                armDeferredDataLatch(static_cast<uint16_t>(m_reg_v & 0x3FFF));
                armDeferredVideoRamIncrement();
            }
            else if(m_cpuDmaReadInProgress) {
                m_dataLatch = readPpuMemory(m_reg_v&0x3FFF);
                m_needIncVideoRam = true;
            }
            else if(!followingDataRead) {
                schedulePpuDataReadUpdate(static_cast<uint16_t>(m_reg_v & 0x3FFF));
            }
        }     

        m_needUpdateState = true;
        return ret;
    }

    GERANES_INLINE void onCpuBusAccessEnd(uint16_t addr, bool /*write*/)
    {
        if((addr & 0x2007) != 0x2007) {
            return;
        }

        if(m_deferredPpuIo.deferredDataLatchArmPending) {
            m_deferredPpuIo.deferredDataLatchArmPending = false;
            m_deferredPpuIo.deferredDataLatchStartDelay = 1;
        }

        if(m_deferredPpuIo.deferredVideoRamIncrementArmPending) {
            m_deferredPpuIo.deferredVideoRamIncrementArmPending = false;
            m_deferredPpuIo.deferredVideoRamIncrementDelay = 1;
        }

        m_needUpdateState = true;
    }

    void incVideoRamAddr() {
        if(!isActivelyRendering()) {

            m_reg_v += static_cast<uint16_t>(m_VRAMAddressIncrement);
            m_reg_v &= 0x7FFF;

            //Trigger memory read when setting the vram address - needed by MMC3 IRQ counter
            //"Should be clocked when A12 changes to 1 via $2007 read/write"
            setBusAddress(m_reg_v & 0x3FFF);
        
        } else {
            //"During rendering (on the pre-render line and the visible lines 0-239, provided either background or sprite rendering is enabled), "
            //it will update v in an odd way, triggering a coarse X increment and a Y increment simultaneously"
            incrementVX();
            incrementVY();
        }

    }

    GERANES_HOT void updateState() {

        m_needUpdateState = false;
        const bool pendingVideoRamIncrement = m_needIncVideoRam;

        handleRenderingEnabledTransition();
        syncRenderingEnabledFlag();
        updateDeferredRegV();
        updateDeferredVideoRamIncrement();
        updateDeferredDataLatchArm();
        applyPendingVideoRamIncrement(pendingVideoRamIncrement);
        updatePendingDataLatch();
        updateA12Delay();
    }

    GERANES_INLINE void writePPUDATA(uint8_t data)
    {
        if(!isActivelyRendering() && hasPendingPpuDataReadUpdate()) {
            completePendingPpuDataReadUpdate();
        }

        if(isOnPaletteAddr()) {
            fakeWritePpuMemory(m_reg_v, data); //palette
        }
        else {
            writePpuMemory(m_reg_v,data);
        }

        m_needUpdateState = true;
        if(isActivelyRendering()) {
            armDeferredVideoRamIncrement();
        }
        else {
            m_needIncVideoRam = true;
        }
    }

    GERANES_INLINE void fillFramebuffer(uint32_t color)
    {
        uint32_t* start = &m_framebuffer[0];
        uint32_t* end = &m_framebuffer[SCREEN_WIDTH*SCREEN_HEIGHT];
        while(start != end) *start++ = color;

    }

    GERANES_INLINE void clearFramebuffer()
    {
        fillFramebuffer(0xFF000000);
    }

    GERANES_INLINE const uint32_t* getFramebuffer() const
    {
        return m_framebuffer;
    }

    GERANES_INLINE uint32_t getZapperPixel(int x, int y)
    {
        if(m_VBlankHasStarted) return 0;

        if (x < 0 || x >= SCREEN_WIDTH || y < 0 || y >= SCREEN_HEIGHT)
            return 0;

        if(y > m_currentY) return 0;
        if(x > m_currentX) return 0;

        return m_framebuffer[y * SCREEN_WIDTH + x];
    }

    GERANES_INLINE void incrementVX()
    {
        // increment hori(v)
        // if coarse X == 31
        if( (m_reg_v&0x001F) == 31){
            // coarse X = 0
            m_reg_v &= 0xFFE0;
            // switch horizontal nametable
            m_reg_v ^= 0x0400;
        } else {
            // increment coarse X
            m_reg_v++;
        }
 
    }

    GERANES_INLINE void incrementVY()
    {
        // increment vert(v)
        // if fine Y < 7
        if( (m_reg_v&0x7000) != 0x7000) {
            // increment fine Y
            m_reg_v += 0x1000;
        } else {
            // fine Y = 0
            m_reg_v &= 0x8FFF;
            // let y = coarse Y
            int y = (m_reg_v & 0x03E0) >> 5;
            if(y == 29) {
                // coarse Y = 0
                y = 0;
                // switch vertical nametable
                m_reg_v ^= 0x0800;
            } else if(y == 31) {
                // coarse Y = 0, nametable not switched
                y = 0;
            } else {
                // increment coarse Y
                y++;
            }
            // put coarse Y back into v
            m_reg_v = (m_reg_v & 0xFC1F) | (y << 5);
        }
    }

    GERANES_INLINE void copyVX() {
        // hori(v) = hori(t)
        // v: .....F.. ...EDCBA = t: .....F.. ...EDCBA
        m_reg_v = (m_reg_v & 0xFBE0) | (m_reg_t & 0x041F);
    }

    GERANES_INLINE void copyVY() {
        // vert(v) = vert(t)
        // v: .IHGF.ED CBA..... = t: .IHGF.ED CBA.....
        m_reg_v = (m_reg_v & 0x841F) | (m_reg_t & 0x7BE0);
    }

    GERANES_INLINE void setPpuFetchSourceCached(bool spriteFetch)
    {
        if(m_isSpritePatternFetch != spriteFetch) {
            m_isSpritePatternFetch = spriteFetch;
            m_cartridge.setPpuFetchSource(spriteFetch);
        }
    }

    GERANES_INLINE uint16_t getNameTableAddr() {
        uint16_t address = 0x2000 | (m_reg_v & 0x0FFF);
        return address;
    }

    GERANES_INLINE void setupNameTableByte() {
        setPpuFetchSourceCached(false);
        setupPpuReadAddress(getNameTableAddr());
    }

    GERANES_INLINE void fetchNameTableByte() {
        // Background tile fetch source for mapper CHR/NT substitution.
        setPpuFetchSourceCached(false);
        const uint16_t address = getNameTableAddr();
        const uint8_t tileIndex = completePpuRead(address);

        const int fineY = (m_reg_v >> 12) & 7;
        const int table = static_cast<int>(m_backgroundPatternTableAddress);

        m_tileAddr = table + (tileIndex << 4) + fineY;
    }

    GERANES_INLINE uint16_t getAttributeTableAddr() {
        uint16_t address = 0x23C0 | (m_reg_v & 0x0C00) | ((m_reg_v >> 4) & 0x38) | ((m_reg_v >> 2) & 0x07);
        return address;
    }

    GERANES_INLINE void setupAttributeTableByte() {
        setPpuFetchSourceCached(false);
        setupPpuReadAddress(getAttributeTableAddr());
    }

    GERANES_INLINE void fetchAttributeTableByte() {
        // Background tile fetch source for mapper CHR/NT substitution.
        setPpuFetchSourceCached(false);

        const int address = getAttributeTableAddr();
        const int shift = ((m_reg_v >> 4) & 4) | (m_reg_v & 2);
        m_paletteOffset = ((completePpuRead(address) >> shift) & 3) << 2;
    }

    GERANES_INLINE void fetchLowTileByte() {
        setPpuFetchSourceCached(false);
        m_lowTileByte = completePpuRead(m_tileAddr);        
    }

    GERANES_INLINE void setupLowTileByte() {
        setPpuFetchSourceCached(false);
        setupPpuReadAddress(m_tileAddr);
    }

    GERANES_INLINE void fetchHighTileByte() {
        setPpuFetchSourceCached(false);
        m_highTileByte = completePpuRead(m_tileAddr + 8);        
    }

    GERANES_INLINE void setupHighTileByte() {
        setPpuFetchSourceCached(false);
        setupPpuReadAddress(static_cast<uint16_t>(m_tileAddr + 8));
    }

    GERANES_INLINE static uint8_t reverseByte(uint8_t value)
    {
        value = static_cast<uint8_t>(((value & 0xF0) >> 4) | ((value & 0x0F) << 4));
        value = static_cast<uint8_t>(((value & 0xCC) >> 2) | ((value & 0x33) << 2));
        value = static_cast<uint8_t>(((value & 0xAA) >> 1) | ((value & 0x55) << 1));
        return value;
    }

    GERANES_INLINE void storeTileData() {
        m_bgPatternLowShift = static_cast<uint16_t>((m_bgPatternLowShift & 0xFF00) | m_lowTileByte);
        m_bgPatternHighShift = static_cast<uint16_t>((m_bgPatternHighShift & 0xFF00) | m_highTileByte);
        m_bgAttribLowLatch = (m_paletteOffset & 0x04) != 0;
        m_bgAttribHighLatch = (m_paletteOffset & 0x08) != 0;
        m_bgAttribLowShift = static_cast<uint16_t>((m_bgAttribLowShift & 0xFF00) | (m_bgAttribLowLatch ? 0x00FF : 0x0000));
        m_bgAttribHighShift = static_cast<uint16_t>((m_bgAttribHighShift & 0xFF00) | (m_bgAttribHighLatch ? 0x00FF : 0x0000));
        if(m_debugModRenderCaptureEnabled) {
            const uint8_t offsetY = static_cast<uint8_t>(m_tileAddr & 0x07);
            const int32_t absoluteChrAddress = m_cartridge.mapper() != nullptr
                ? m_cartridge.mapper()->debugToAbsoluteChrAddress(m_tileAddr)
                : -1;
            const uint16_t tileIndex = static_cast<uint16_t>(absoluteChrAddress >= 0
                ? (absoluteChrAddress >> 4)
                : ((m_tileAddr >> 4) & 0x01FF));
            for(uint8_t x = 0; x < 8; ++x) {
                DebugModBackgroundShiftPixel& pixel = m_debugModBackgroundShift[8 + x];
                pixel.tileIndex = tileIndex;
                pixel.paletteOffset = m_paletteOffset;
                pixel.offsetX = x;
                pixel.offsetY = offsetY;
                pixel.valid = true;
            }
        }
        m_staleBgShiftActive = false;
    }

    GERANES_INLINE void shiftTileData() {
        m_bgPatternLowShift <<= 1;
        m_bgPatternHighShift <<= 1;
        m_bgPatternHighShift |= 0x01;
        m_bgAttribLowShift <<= 1;
        m_bgAttribHighShift <<= 1;
        if(m_debugModRenderCaptureEnabled) {
            for(size_t i = 0; i + 1 < m_debugModBackgroundShift.size(); ++i) {
                m_debugModBackgroundShift[i] = m_debugModBackgroundShift[i + 1];
            }
            m_debugModBackgroundShift.back() = {};
        }
    }

    GERANES_INLINE uint32_t fetchTileData() {
        return uint32_t(m_tileData >> 32);
    }

    GERANES_INLINE bool isOverclockFrame() {
        return m_overclockFrame;
    }

    GERANES_INLINE void setOverclockFrame(bool state) {
        m_overclockFrame = state;
    }

    void serialization(SerializationBase& s)
    {
        SERIALIZEDATA(s, m_currentPixelColorIndex);
        SERIALIZEDATA(s, m_scanline);
        SERIALIZEDATA(s, m_cycle);

        SERIALIZEDATA(s, m_VRAMAddressIncrement);
        SERIALIZEDATA(s, m_spritePatternTableAddress);
        SERIALIZEDATA(s, m_backgroundPatternTableAddress);
        SERIALIZEDATA(s, m_spriteHeight);
        SERIALIZEDATA(s, m_PPUSlave);
        SERIALIZEDATA(s, m_NMIOnVBlank);

        SERIALIZEDATA(s, m_monochromeDisplay);
        SERIALIZEDATA(s, m_showBackgroundLeftmost8Pixels);
        SERIALIZEDATA(s, m_showSpritesLeftmost8Pixels);
        SERIALIZEDATA(s, m_backgroundEnabled);
        SERIALIZEDATA(s, m_spritesEnabled);
        SERIALIZEDATA(s, m_colorEmphasis);
        SERIALIZEDATA(s, m_renderingEnabled);

        SERIALIZEDATA(s, m_VBlankHasStarted);
        SERIALIZEDATA(s, m_sprite0Hit);
        SERIALIZEDATA(s, m_spriteOverflow);       

        SERIALIZEDATA(s, m_spritesInThisLine);
        SERIALIZEDATA(s, m_testSprite0HitInThisLine);

        SERIALIZEDATA(s, m_oamAddr);
        SERIALIZEDATA(s, m_oamCopyBuffer);
        SERIALIZEDATA(s, m_oamCopyDone);
        SERIALIZEDATA(s, m_secondaryOamAddr);
        SERIALIZEDATA(s, m_spriteInRange);
        SERIALIZEDATA(s, m_oamAddrN);
        SERIALIZEDATA(s, m_oamAddrM);
        SERIALIZEDATA(s, m_overflowBugCounter);
        SERIALIZEDATA(s, m_sprite0Added);

        SERIALIZEDATA(s, m_currentY);
        SERIALIZEDATA(s, m_currentX);

        SERIALIZEDATA(s, m_interruptFlag);

        SERIALIZEDATA(s, m_oddFrameFlag);

        s.array(reinterpret_cast<uint8_t*>(m_nameTable), 1, sizeof(m_nameTable)); //4 pages

        s.array(m_palette, 1, sizeof(m_palette));
        s.array(m_primaryOam, 1, sizeof(m_primaryOam));
        s.array(m_secondaryOam, 1, sizeof(m_secondaryOam));
        s.array(reinterpret_cast<uint8_t*>(m_spritesIndexesInThisLine), 1, sizeof(m_spritesIndexesInThisLine));
        s.array(reinterpret_cast<uint8_t*>(m_corruptOamRow), 1, sizeof(m_corruptOamRow));
        s.array(reinterpret_cast<uint8_t*>(m_spriteFetchEntries), 1, sizeof(m_spriteFetchEntries));
        s.array(reinterpret_cast<uint8_t*>(m_spriteRenderEntries), 1, sizeof(m_spriteRenderEntries));
        SERIALIZEDATA(s, m_spriteFetchCount);

        SERIALIZEDATA(s, m_reg_v);
        SERIALIZEDATA(s, m_reg_x);
        SERIALIZEDATA(s, m_reg_t);
        SERIALIZEDATA(s, m_reg_w);

        SERIALIZEDATA(s, m_dataLatch);
        SERIALIZEDATA(s, m_deferredPpuIo.pendingDataLatchUpdate);
        SERIALIZEDATA(s, m_deferredPpuIo.pendingDataLatchDelay);
        SERIALIZEDATA(s, m_deferredPpuIo.pendingDataLatchAddr);
        SERIALIZEDATA(s, m_deferredPpuIo.deferredDataLatchArmPending);
        SERIALIZEDATA(s, m_deferredPpuIo.deferredDataLatchStart);
        SERIALIZEDATA(s, m_deferredPpuIo.deferredDataLatchStartDelay);
        SERIALIZEDATA(s, m_deferredPpuIo.deferredVideoRamIncrementArmPending);
        SERIALIZEDATA(s, m_deferredPpuIo.deferredVideoRamIncrementDelay);

        SERIALIZEDATA(s, m_tileAddr);
        SERIALIZEDATA(s, m_paletteOffset);
        SERIALIZEDATA(s, m_lowTileByte);
        SERIALIZEDATA(s, m_highTileByte);
        SERIALIZEDATA(s, m_bgPatternLowShift);
        SERIALIZEDATA(s, m_bgPatternHighShift);
        SERIALIZEDATA(s, m_bgAttribLowShift);
        SERIALIZEDATA(s, m_bgAttribHighShift);
        SERIALIZEDATA(s, m_bgAttribLowLatch);
        SERIALIZEDATA(s, m_bgAttribHighLatch);
        SERIALIZEDATA(s, m_tileData);

        SERIALIZEDATA(s, m_lastPPUSTATUSReadCycle);

        SERIALIZEDATA(s, m_openBus);
        s.array(m_openBusTimer, 1, 8);

        SERIALIZEDATA(s, FRAME_NUMBER_OF_LINES);
        SERIALIZEDATA(s, FRAME_VBLANK_START_LINE);
        SERIALIZEDATA(s, FRAME_VBLANK_END_LINE);
        SERIALIZEDATA(s, m_inOverclockLines);
        SERIALIZEDATA(s, m_overclockFrame);
        SERIALIZEDATA(s, m_preLine);
        SERIALIZEDATA(s, m_visibleLine);
        SERIALIZEDATA(s, m_renderLine);

        SERIALIZEDATA(s, m_needUpdateState);
        SERIALIZEDATA(s, m_needIncVideoRam);

        SERIALIZEDATA(s, m_prevCycleRenderingEnabled);
        SERIALIZEDATA(s, m_spriteRenderClockingActiveThisLine);
        SERIALIZEDATA(s, m_staleBgShiftActive);
        SERIALIZEDATA(s, m_firstSpriteFetchV);

        SERIALIZEDATA(s, m_update_reg_v_delay);
        SERIALIZEDATA(s, m_update_reg_v_value);

        SERIALIZEDATA(s, m_busAddress);
        SERIALIZEDATA(s, m_busAddressLowLatch);
        SERIALIZEDATA(s, m_updateA12Delay);
        SERIALIZEDATA(s, m_isSpritePatternFetch);
        SERIALIZEDATA(s, m_currentReadAffectsBus);
        SERIALIZEDATA(s, m_vsPpuModel);

        if(s.isReading()) {
            refreshOutputColorPalette();
        }
        m_pFrameBuffer = &m_framebuffer[m_currentY*SCREEN_WIDTH+m_currentX];
    }

};

} // namespace GeraNES
