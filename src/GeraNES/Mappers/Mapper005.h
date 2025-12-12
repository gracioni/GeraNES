#pragma once

#include "BaseMapper.h"
#include <cstring>
#include <cstdint>
#include <cassert>
#include <stdio.h>

//#define MMC5_DEBUG

class Mapper005 : public BaseMapper
{
protected:
    // Configuration registers
    uint8_t m_prgMode = 3;    // $5100 bits 1..0 (default mode 3 = 8k banks)
    uint8_t m_chrMode = 3;    // $5101 bits 1..0 (default 1KB)
    bool m_prgRamProtect1 = 0; // $5102
    bool m_prgRamProtect2 = 0; // $5103

    // ExRAM / nametable / fill
    uint8_t m_exRamMode = 0;  // $5104
    uint8_t m_ntMap = 0;     // $5105
    uint8_t m_fillTile = 0;  // $5106
    uint8_t m_fillColor = 0; // $5107

    // PRG registers ($5113-$5117)
    uint8_t m_prgReg[5] = {0};

    // CHR registers ($5120-$512B + $5130)
    uint16_t m_chrReg[12] = {0};
    uint8_t m_chrUpper = 0; // $5130 low 2 bits

    // Sound regs (basic storage) $5000-$5015
    uint8_t m_soundRegs[0x16] = {0};

    // ExRAM 1KB
    uint8_t m_exRam[0x400];

    // Scanline IRQ / detection ($5203/$5204 etc)
    bool m_inFrame = false;
    uint8_t m_scanlineCounter = 0;
    uint8_t m_scanlineCompare = 0; // $5203
    bool m_scanlinePending = false; // pending flag (5204)
    bool m_scanlineEnable = false;  // enable flag (5204 write bit7)
    int m_ppuMatchCount = 0;
    uint16_t m_lastPpuAddr = 0xFFFF;
    int m_idleCounter = 0;
    bool m_irqLine = false; // actual IRQ output line for console

    // PPU rendering state (for ExRAM write-only-during-rendering behavior)
    bool m_ppuRendering = false;

    // Masks derived from cartridge
    uint8_t m_prgMask = 0;   // for 8KB banks
    uint32_t m_chrMask = 0;  // for 1KB banks

    // A12 fallback state (for compatibility)
    bool m_a12LastState = false;

    // helpers
    template<BankSize bs>
    GERANES_INLINE uint8_t readChrBank(int bank, int addr) {
        if(hasChrRam()) return readChrRam<bs>(bank, addr);
        uint32_t b = static_cast<uint32_t>(bank) & m_chrMask;
        return cd().readChr<bs>((int)b, addr);
    }

    template<BankSize bs>
    GERANES_INLINE void writeChrBank(int bank, int addr, uint8_t data) {
        writeChrRam<bs>(bank, addr, data);
    }

public:
    Mapper005(ICartridgeData& cd) : BaseMapper(cd)
    {
        m_prgMask = calculateMask(cd.numberOfPRGBanks<BankSize::B8K>());
        m_chrMask = calculateMask(static_cast<uint32_t>(cd.numberOfCHRBanks<BankSize::B1K>()));

        // sensible power-on defaults: mode 3 (8k prg), CHR 1k
        m_prgMode = 3;
        m_chrMode = 3;

        const uint32_t nb = cd.numberOfPRGBanks<BankSize::B8K>();
        if (nb >= 4) {
            m_prgReg[0] = 0 & m_prgMask;
            m_prgReg[1] = 1 & m_prgMask;
            m_prgReg[2] = (nb - 2) & m_prgMask;
            m_prgReg[3] = (nb - 1) & m_prgMask;
            m_prgReg[4] = (nb - 1) & m_prgMask;
        } else {
            for (int i=0;i<5;i++) m_prgReg[i] = i & m_prgMask;
        }

        std::memset(m_exRam, 0, sizeof(m_exRam));
        std::memset(m_soundRegs, 0, sizeof(m_soundRegs));
    }

    virtual ~Mapper005() {}

    // -----------------
    // HANDLERS CALLED BY CORE
    // -----------------
    // Your core calls:
    //  - write0x4000(off, data) / read0x4000(off, openBus) for CPU $4000-$5FFF (off = addr&0x1FFF)
    //  - writeSaveRam(addr&0x1FFF, data) / readSaveRam(addr&0x1FFF) for CPU $6000-$7FFF
    //  - writePrg(addr&0x7FFF, data) / readPrg(addr&0x7FFF) for CPU $8000-$FFFF (masked)
    //
    // Implement these accordingly.

    // ---------- CPU $4000-$5FFF (off = addr & 0x1FFF)
    virtual void writeMapperRegister(int off, uint8_t data) override
    {
        const uint16_t cpuAddr = 0x4000 + (off & 0x1FFF);

        #ifdef MMC5_DEBUG
        printf("[MMC5] write0x4000 cpu=0x%04X data=0x%02X\n", cpuAddr, data);
        #endif

        // Sound regs: $5000-$5015 are inside this range
        if (cpuAddr >= 0x5000 && cpuAddr <= 0x5015) {
            uint16_t idx = cpuAddr - 0x5000;
            m_soundRegs[idx] = data;
            // TODO: hook to extra APU emulation if you want to hear MMC5 audio.
            return;
        }

        // Mapper registers are in $5100-$53FF etc (we implement the main ones)
        if (cpuAddr >= 0x5100 && cpuAddr <= 0x51FF) {
            uint16_t off5100 = cpuAddr - 0x5100;
            switch (off5100) {
                case 0x00: // $5100 PRG mode
                    m_prgMode = data & 0x03;
                    #ifdef MMC5_DEBUG
                    printf("[MMC5] $5100 PRG mode = %d\n", m_prgMode);
                    #endif
                    return;
                case 0x01: // $5101 CHR mode
                    m_chrMode = data & 0x03;
                    #ifdef MMC5_DEBUG
                    printf("[MMC5] $5101 CHR mode = %d\n", m_chrMode);
                    #endif
                    return;
                case 0x02:
                    m_prgRamProtect1 = (data & 0x03) == 0x02;
                    return; // $5102
                case 0x03: // $5103 PRG-RAM protect
                    m_prgRamProtect2 = (data & 0x03) == 0x01;              
                    return;
                case 0x04: // $5104 ExRAM mode
                    m_exRamMode = data & 0x03;
                    return;
                case 0x05: // $5105 Nametable config
                    m_ntMap = data;
                    return;
                case 0x06: // $5106 fill tile
                    m_fillTile = data;
                    return;
                case 0x07: // $5107 fill attribute
                    m_fillColor = data & 0x03;
                    return;
                default:
                    break;
            }
        }

        // PRG bank registers $5113-$5117 are mapped in the same $5100 window => detect them
        if (cpuAddr >= 0x5113 && cpuAddr <= 0x5117) {
            int r = cpuAddr - 0x5113;
            if (r >=0 && r < 5) {
                m_prgReg[r] = data;
                #ifdef MMC5_DEBUG
                printf("[MMC5] $511%d = 0x%02X\n", 3 + r, data);
                #endif
            }
            return;
        }

        // CHR regs $5120 - $512B
        if (cpuAddr >= 0x5120 && cpuAddr <= 0x512B) {
            int idx = cpuAddr - 0x5120;
            if (idx >= 0 && idx < 12) {
                m_chrReg[idx] = (uint16_t)data | ((uint16_t)m_chrUpper << 8);
                #ifdef MMC5_DEBUG
                printf("[MMC5] $512%X = 0x%02X (chr idx %d => %03X)\n", idx, data, idx, m_chrReg[idx]);
                #endif
            }
            return;
        }

        if (cpuAddr == 0x5130) {
            m_chrUpper = data & 0x03;
            #ifdef MMC5_DEBUG
            printf("[MMC5] $5130 = 0x%02X (upper bits)\n", m_chrUpper);
            #endif
            return;
        }

        // Split screen & scanline $5200-$5206
        if (cpuAddr >= 0x5200 && cpuAddr <= 0x5206) {
            switch (cpuAddr) {
                case 0x5200: /* split control */ /* TODO: store if needed */ return;
                case 0x5201: /* split vscroll */ /* TODO */ return;
                case 0x5202: /* split bank */ /* TODO */ return;
                case 0x5203: // IRQ trigger
                    m_scanlineCompare = data;
                    return;
                case 0x5204: // IRQ enable (write)
                    m_scanlineEnable = (data & 0x80) != 0;
                    if (!m_scanlineEnable) {
                        m_irqLine = false;
                    } else {
                        if (m_scanlinePending) m_irqLine = true;
                    }
                    return;
                case 0x5205: // multiplier low (write)
                    m_soundRegs[0x05] = data; // reuse soundRegs storage for product if you like
                    return;
                case 0x5206: // multiplier high (write)
                    m_soundRegs[0x06] = data;
                    return;
                default: return;
            }
        }

        // ExRAM CPU window $5C00-$5FFF
        if (cpuAddr >= 0x5C00 && cpuAddr <= 0x5FFF) {
            uint16_t exOff = cpuAddr - 0x5C00;
            // Behavior depends on m_exRamMode:
            // Ex0/Ex1: writes are allowed *only during rendering* (we require mapper to be notified of rendering)
            // Ex2: cpu read/write allowed
            // Ex3: cpu read-only
            if (m_exRamMode == 0 || m_exRamMode == 1) {
                if (m_ppuRendering) {
                    m_exRam[exOff & 0x03FF] = data;
                } else {
                    // write ignored (or writes as zero per doc). We'll ignore.
                }
            } else if (m_exRamMode == 2) {
                m_exRam[exOff & 0x03FF] = data;
            } else if (m_exRamMode == 3) {
                // write ignored
            }
            return;
        }

        // anything else in $4000-$5FFF we ignore here (APU regs at $4000-$4017 handled elsewhere by core)
    }

    virtual uint8_t readMapperRegister(int off, uint8_t openBusData) override
    {
        const uint16_t cpuAddr = 0x4000 + (off & 0x1FFF);

        #ifdef MMC5_DEBUG
        printf("[MMC5] read0x4000 cpu=0x%04X\n", cpuAddr);
        #endif

        // sound status $5015
        if (cpuAddr == 0x5015) {
            // return length status for extra pulses in low bits (approx)
            uint8_t v = m_soundRegs[0x15];
            return v;
        }

        // IRQ status $5204 read
        if (cpuAddr == 0x5204) {
            uint8_t v = 0;
            if (m_scanlinePending) v |= 0x01;
            if (m_inFrame) v |= 0x02;
            // reading clears pending flag (ack)
            m_scanlinePending = false;
            m_irqLine = false;
            return v;
        }

        // multiplier product $5205/$5206 (read)
        if (cpuAddr == 0x5205) {
            uint16_t product = (uint16_t)m_soundRegs[0x05] * (uint16_t)m_soundRegs[0x06];
            return (uint8_t)(product & 0xFF);
        }
        if (cpuAddr == 0x5206) {
            uint16_t product = (uint16_t)m_soundRegs[0x05] * (uint16_t)m_soundRegs[0x06];
            return (uint8_t)((product >> 8) & 0xFF);
        }

        // ExRAM CPU window read $5C00-$5FFF
        if (cpuAddr >= 0x5C00 && cpuAddr <= 0x5FFF) {
            uint16_t exOff = cpuAddr - 0x5C00;
            if (m_exRamMode == 2 || m_exRamMode == 3) {
                return m_exRam[exOff & 0x03FF];
            } else {
                // Ex0/Ex1 read is "not allowed" -> open bus
                return openBusData;
            }
        }

        return openBusData;
    }

    // ---------- PRG-RAM window $6000-$7FFF (addr is addr&0x1FFF)
    virtual void writeSaveRam(int addr, uint8_t data) override
    {
        if(!m_prgRamProtect1 || !m_prgRamProtect2) return;

        // Map the 8KB SaveRam window into the actual save RAM using $5113 (m_prgReg[0]).
        // Use cartridge-provided saveRamSize to compute number of 8k pages. If saveRamSize==0 fallback to default BaseMapper.
        size_t saveSize = (size_t)cd().saveRamSize();
        if (saveSize == 0) {
            // fallback to base behavior
            BaseMapper::writeSaveRam(addr, data);
            return;
        }

        const uint16_t pageSize = 0x2000; // 8KB pages
        size_t nPages = (saveSize + pageSize - 1) / pageSize;
        if (nPages == 0) nPages = 1;

        uint32_t bank = (uint32_t)m_prgReg[0] & (uint32_t)(nPages - 1); // wrap around available pages
        uint32_t mapped = (bank * pageSize) + (addr & 0x1FFF);
        // forward to BaseMapper storage (BaseMapper will mask by saveRamSize if needed)
        BaseMapper::writeSaveRam((int)mapped, data);
    }

    virtual uint8_t readSaveRam(int addr) override
    {
        size_t saveSize = (size_t)cd().saveRamSize();
        if (saveSize == 0) {
            return BaseMapper::readSaveRam(addr);
        }

        const uint16_t pageSize = 0x2000;
        size_t nPages = (saveSize + pageSize - 1) / pageSize;
        if (nPages == 0) nPages = 1;

        uint32_t bank = (uint32_t)m_prgReg[0] & (uint32_t)(nPages - 1);
        uint32_t mapped = (bank * pageSize) + (addr & 0x1FFF);
        return BaseMapper::readSaveRam((int)mapped);
    }

    // ---------- PRG area $8000-$FFFF
    // core calls readPrg/writePrg with addr&0x7FFF for addresses >= 0x8000
    GERANES_HOT uint8_t readPrg(int maskedAddr) override
    {
        const uint32_t cpuAddr = (maskedAddr & 0x7FFF) + 0x8000; // reconstruct original cpu addr

        #ifdef MMC5_DEBUG
        printf("[MMC5] readPrg masked=0x%04X cpu=0x%04X\n", maskedAddr & 0x7FFF, cpuAddr);
        #endif

        // mapping logic depends on m_prgMode
        if (cpuAddr >= 0x8000 && cpuAddr <= 0xFFFF) {
            const int slot = (cpuAddr >> 13) & 0x3; // which 8K window 0..3
            uint8_t bankIndex = 0;
            switch (m_prgMode) {
                case 0: {
                    // 32KB switching via $5117 -> align to 4*8KB
                    uint8_t base8k = (m_prgReg[4] & m_prgMask) & ~0x3;
                    bankIndex = (uint8_t)((base8k + slot) & m_prgMask);
                    break;
                }
                case 1: {
                    // $8000-$BFFF: $5115 (16KB). $C000-$FFFF: $5117 (16KB)
                    if (cpuAddr <= 0xBFFF) {
                        uint8_t base16k = (m_prgReg[1] & m_prgMask) & ~0x1;
                        bankIndex = (uint8_t)((base16k + slot) & m_prgMask);
                    } else {
                        uint8_t base16k = (m_prgReg[4] & m_prgMask) & ~0x1;
                        int slotC = ((cpuAddr - 0xC000) >> 13) & 0x1;
                        bankIndex = (uint8_t)((base16k + slotC + 2) & m_prgMask);
                    }
                    break;
                }
                case 2: {
                    // $8000-$BFFF: $5115(16KB), $C000-$DFFF: $5116(8K), $E000-$FFFF: $5117(8K)
                    if (cpuAddr <= 0xBFFF) {
                        uint8_t base16k = (m_prgReg[1] & m_prgMask) & ~0x1;
                        bankIndex = (uint8_t)((base16k + slot) & m_prgMask);
                    } else if (cpuAddr <= 0xDFFF) {
                        bankIndex = (uint8_t)(m_prgReg[2] & m_prgMask);
                    } else {
                        bankIndex = (uint8_t)(m_prgReg[4] & m_prgMask);
                    }
                    break;
                }
                case 3:
                default: {
                    // mode 3: four 8KB banks ($5114-$5117)
                    bankIndex = m_prgReg[slot] & m_prgMask;
                    break;
                }
            }

            return cd().readPrg<BankSize::B8K>(bankIndex, cpuAddr);
        }

        return 0xFF;
    }

    GERANES_HOT void writePrg(int maskedAddr, uint8_t data) override
    {
        const uint32_t cpuAddr = (maskedAddr & 0x7FFF) + 0x8000;

        #ifdef MMC5_DEBUG
        printf("[MMC5] writePrg masked=0x%04X cpu=0x%04X data=0x%02X\n", maskedAddr & 0x7FFF, cpuAddr, data);
        #endif

        // Some games write to ROM area to trigger special behavior (e.g., Bandit Kings writes PRG-RAM via ROM window).
        // MMC5 supports PRG-RAM overlay via registers. For now we only support standard behavior:
        // If write falls into area intended for PRG-RAM (some boards let ROM address space target RAM via register bits),
        // then the cartridge may expect these writes to update PRG-RAM. We don't implement those exotic behaviors here
        // beyond writeSaveRam override which handles the normal $6000-$7FFF window.
        // Normally writes to $8000+ are writes to mapper registers on some mappers (MMC3), but MMC5 uses $5000-$5FFF for regs.
        (void)cpuAddr; (void)data;
    }

    // ---------- CHR handlers
    GERANES_HOT virtual uint8_t readChr(int addr) override
    {
        switch (m_chrMode) {
            case 0: {
                // 8KB mode: use m_chrReg[0] as base
                uint32_t full = (static_cast<uint32_t>(m_chrUpper) << 8) | (uint32_t)m_chrReg[0];
                full &= (~0x7u) & m_chrMask;
                int bank8k = (int)(full & m_chrMask);
                return readChrBank<BankSize::B8K>(bank8k, addr);
            }
            case 1: {
                // 4KB mode
                if (addr < 0x1000) {
                    uint32_t idx = (static_cast<uint32_t>(m_chrUpper) << 8) | m_chrReg[8];
                    idx &= (~0x3u) & m_chrMask;
                    return readChrBank<BankSize::B4K>((int)idx, addr);
                } else {
                    uint32_t idx = (static_cast<uint32_t>(m_chrUpper) << 8) | m_chrReg[9];
                    idx &= (~0x3u) & m_chrMask;
                    return readChrBank<BankSize::B4K>((int)idx, addr);
                }
            }
            case 2: {
                // 2KB mode
                int slot = (addr >> 11) & 0x3;
                int regIndex = slot * 2;
                if (regIndex > 11) regIndex = 11;
                uint32_t idx = (static_cast<uint32_t>(m_chrUpper) << 8) | m_chrReg[regIndex];
                idx &= (~0x1u) & m_chrMask;
                return readChrBank<BankSize::B2K>((int)idx, addr);
            }
            case 3:
            default: {
                // 1KB mode
                int slot = (addr >> 10) & 0x7;
                int regIndex = slot;
                if (regIndex > 11) regIndex = 11;
                uint32_t idx = (static_cast<uint32_t>(m_chrUpper) << 8) | m_chrReg[regIndex];
                idx &= m_chrMask;
                return readChrBank<BankSize::B1K>((int)idx, addr);
            }
        }
    }

    GERANES_HOT virtual void writeChr(int addr, uint8_t data) override
    {
        if (!hasChrRam()) return;

        switch (m_chrMode) {
            case 0: {
                uint32_t full = (static_cast<uint32_t>(m_chrUpper) << 8) | (uint32_t)m_chrReg[0];
                full &= (~0x7u) & m_chrMask;
                writeChrBank<BankSize::B8K>((int)full, addr, data);
                break;
            }
            case 1: {
                if (addr < 0x1000) {
                    uint32_t idx = (static_cast<uint32_t>(m_chrUpper) << 8) | m_chrReg[8];
                    idx &= (~0x3u) & m_chrMask;
                    writeChrBank<BankSize::B4K>((int)idx, addr, data);
                } else {
                    uint32_t idx = (static_cast<uint32_t>(m_chrUpper) << 8) | m_chrReg[9];
                    idx &= (~0x3u) & m_chrMask;
                    writeChrBank<BankSize::B4K>((int)idx, addr, data);
                }
                break;
            }
            case 2: {
                int slot = (addr >> 11) & 0x3;
                int regIndex = slot * 2;
                if (regIndex > 11) regIndex = 11;
                uint32_t idx = (static_cast<uint32_t>(m_chrUpper) << 8) | m_chrReg[regIndex];
                idx &= (~0x1u) & m_chrMask;
                writeChrBank<BankSize::B2K>((int)idx, addr, data);
                break;
            }
            case 3:
            default: {
                int slot = (addr >> 10) & 0x7;
                int regIndex = slot;
                if (regIndex > 11) regIndex = 11;
                uint32_t idx = (static_cast<uint32_t>(m_chrUpper) << 8) | m_chrReg[regIndex];
                idx &= m_chrMask;
                writeChrBank<BankSize::B1K>((int)idx, addr, data);
                break;
            }
        }
    }

    // ---------- Mirroring / nametable behavior
    GERANES_HOT MirroringType mirroringType() override
    {
        // If any nametable maps to ExRAM (value 2) make PPU use ExRAM via four-screen mode hook.
        uint8_t n = m_ntMap;
        for (int i = 0; i < 4; ++i) {
            uint8_t v = (n >> (i*2)) & 0x03;
            if (v == 2) return MirroringType::FOUR_SCREEN;
        }
        if (cd().useFourScreenMirroring()) return MirroringType::FOUR_SCREEN;
        // fallback to base mapper mirroring
        return BaseMapper::mirroringType();
    }

    // ---------- PPU integration hooks for scanline detection & ExRAM render-only writes
    // Call mapper.ppuRead(ppuAddr) when PPU reads $2000-$2FFF (nametable/attribute accesses).
    void ppuRead(uint16_t ppuAddr)
    {
        if (ppuAddr < 0x2000 || ppuAddr > 0x2FFF) {
            m_ppuMatchCount = 0;
            m_lastPpuAddr = 0xFFFF;
            return;
        }

        if (ppuAddr == m_lastPpuAddr) {
            m_ppuMatchCount++;
        } else {
            m_ppuMatchCount = 1;
            m_lastPpuAddr = ppuAddr;
        }

        if (m_ppuMatchCount >= 3) {
            if (!m_inFrame) {
                m_inFrame = true;
                m_scanlineCounter = 0;
                m_scanlinePending = false;
            } else {
                m_scanlineCounter++;
                if (m_scanlineCounter == m_scanlineCompare) {
                    m_scanlinePending = true;
                    if (m_scanlineEnable) m_irqLine = true;
                }
            }
            m_ppuMatchCount = 0;
            m_lastPpuAddr = 0xFFFF;
        }
    }

    // Call this each M2 rising edge from CPU/PPU: ppuWasReading = true if PPU read happened during last M2.
    void ppuIdleTick(bool ppuWasReading)
    {
        if (ppuWasReading) {
            m_idleCounter = 0;
        } else {
            m_idleCounter++;
            if (m_idleCounter >= 3) {
                // PPU inactive -> clear inFrame and counters
                m_inFrame = false;
                m_ppuMatchCount = 0;
                m_lastPpuAddr = 0xFFFF;
                m_scanlineCounter = 0;
                m_scanlinePending = false;
                m_irqLine = false;
            }
        }
    }

    // A12 fallback (called by system when PPU A12 toggles)
    void setA12State(bool state) override
    {
        if (!m_a12LastState && state) {
            // rising edge
            m_scanlineCounter++;
            if (m_scanlineCounter == m_scanlineCompare) {
                m_scanlinePending = true;
                if (m_scanlineEnable) m_irqLine = true;
            }
        }
        m_a12LastState = state;
    }

    void setPpuRendering(bool rendering) {
        m_ppuRendering = rendering;
    }

    // Read mapper-specific register directly (some cores route CPU reads $5xxx through this)
    uint8_t readRegister(uint16_t addr, uint8_t openBusData = 0)
    {
        uint16_t off = addr & 0x0FFF;
        if (off == 0x204) { // $5204
            uint8_t v = 0;
            if (m_scanlinePending) v |= 0x01;
            if (m_inFrame) v |= 0x02;
            m_scanlinePending = false;
            m_irqLine = false;
            return v;
        }
        if (off == 0x205) { // $5205 low product
            uint16_t product = (uint16_t)m_soundRegs[0x05] * (uint16_t)m_soundRegs[0x06];
            return (uint8_t)(product & 0xFF);
        }
        if (off == 0x206) { // $5206 high product
            uint16_t product = (uint16_t)m_soundRegs[0x05] * (uint16_t)m_soundRegs[0x06];
            return (uint8_t)((product >> 8) & 0xFF);
        }
        return openBusData;
    }

    // Console polls this to decide /IRQ line
    bool getInterruptFlag() override
    {
        return m_irqLine;
    }

    // Expose ExRAM read/write for PPU when nametable mapped to ExRAM
    uint8_t exRamRead(uint16_t offset)
    {
        offset &= 0x03FF;
        return m_exRam[offset];
    }

    void exRamWrite(uint16_t offset, uint8_t v)
    {
        offset &= 0x03FF;
        // obey mode: Ex0/Ex1 only during rendering
        if (m_exRamMode == 0 || m_exRamMode == 1) {
            if (m_ppuRendering) m_exRam[offset] = v;
        } else if (m_exRamMode == 2) {
            m_exRam[offset] = v;
        } else { /* Ex3 write ignored */ }
    }

    // cycle hook
    void cycle() override { /* nothing required here for now */ }

    // Serialization
    virtual void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);

        SERIALIZEDATA(s, m_prgMode);
        SERIALIZEDATA(s, m_chrMode);
        SERIALIZEDATA(s, m_prgRamProtect1);
        SERIALIZEDATA(s, m_prgRamProtect2);

        SERIALIZEDATA(s, m_exRamMode);
        SERIALIZEDATA(s, m_ntMap);
        SERIALIZEDATA(s, m_fillTile);
        SERIALIZEDATA(s, m_fillColor);

        for (int i=0;i<5;i++) SERIALIZEDATA(s, m_prgReg[i]);

        for (int i=0;i<12;i++) SERIALIZEDATA(s, m_chrReg[i]);
        SERIALIZEDATA(s, m_chrUpper);

        s.array(m_exRam, 1, 0x400);

        SERIALIZEDATA(s, m_inFrame);
        SERIALIZEDATA(s, m_scanlineCounter);
        SERIALIZEDATA(s, m_scanlineCompare);
        SERIALIZEDATA(s, m_scanlinePending);
        SERIALIZEDATA(s, m_scanlineEnable);
        SERIALIZEDATA(s, m_ppuMatchCount);
        SERIALIZEDATA(s, m_lastPpuAddr);
        SERIALIZEDATA(s, m_idleCounter);
        SERIALIZEDATA(s, m_irqLine);

        SERIALIZEDATA(s, m_prgMask);
        SERIALIZEDATA(s, m_chrMask);

        SERIALIZEDATA(s, m_a12LastState);
    }
};
