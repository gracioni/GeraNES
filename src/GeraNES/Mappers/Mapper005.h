#pragma once

#include "BaseMapper.h"
#include <cstring>
#include <cstdint>
#include <cassert>
#include <stdio.h>

class Mapper005 : public BaseMapper
{
private:

    enum class ChrType {A, B};

    ChrType m_lastChrWritten = ChrType::A;
    ChrType m_currentChrSet = ChrType::A;

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
    bool m_irqEnable = false;  // enable flag (5204 write bit7)
    int m_ppuMatchCount = 0;
    uint16_t m_lastPpuAddr = 0xFFFF;
    int m_idleCounter = 0;
    bool m_irqLine = false; // actual IRQ output line for console

    // Masks derived from cartridge
    uint8_t m_prgMask = 0;   // for 8KB banks
    uint8_t m_saveRamMask = 0;

    uint16_t m_currentBgTileIndex = 0;
    uint8_t  m_currentBgPalette = 0;
    uint8_t m_currentBgChrLow = 0;

    bool m_spriteSize8x16 = false;

public:
    Mapper005(ICartridgeData& cd) : BaseMapper(cd)
    {
        m_prgMask = calculateMask(cd.numberOfPRGBanks<BankSize::B8K>());
        m_saveRamMask = calculateMask(cd.numberOfSRamBanks<BankSize::B8K>());

        // sensible power-on defaults: mode 3 (8k prg), CHR 1k
        m_prgMode = 3;
        m_chrMode = 3;

        const uint32_t nb = cd.numberOfPRGBanks<BankSize::B8K>();
        if (nb >= 4) {
            m_prgReg[0] = 0 & m_saveRamMask;
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
                    return;
                case 0x01: // $5101 CHR mode
                    m_chrMode = data & 0x03;
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
            if(r == 0) m_prgReg[r] = data & m_saveRamMask;
            else m_prgReg[r] = data & m_prgMask;
            return;
        }

        // CHR regs $5120 - $512B
        if (cpuAddr >= 0x5120 && cpuAddr <= 0x512B) {
            int idx = cpuAddr - 0x5120;
            m_chrReg[idx] = (uint16_t)data | ((uint16_t)m_chrUpper << 8);
            m_lastChrWritten = idx < 8 ? ChrType::A : ChrType::B;
            return;
        }

        if (cpuAddr == 0x5130) {
            m_chrUpper = data & 0x03;
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
                    m_irqEnable = (data & 0x80) != 0;
                    if (!m_irqEnable) {
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
                if (m_inFrame) { //ppu rendering
                    m_exRam[exOff & 0x03FF] = data;
                } else {
                    // write ignored (or writes as zero per doc). We'll ignore.
                    //m_exRam[exOff & 0x03FF] = 0;
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

        // sound status $5015
        if (cpuAddr == 0x5015) {
            // return length status for extra pulses in low bits (approx)
            uint8_t v = m_soundRegs[0x15];
            return v;
        }

        // IRQ status $5204 read
        if (cpuAddr == 0x5204) {
            uint8_t v = 0;
            if (m_scanlinePending) v |= 0x80;
            if (m_inFrame) v |= 0x40;
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

        if(cd().saveRamSize() > 0) {
            writeSRamBank<BankSize::B8K>(m_prgReg[0], addr, data);
        }
    }

    virtual uint8_t readSaveRam(int addr) override
    {
        if(cd().saveRamSize() > 0) {
            return readSRamBank<BankSize::B8K>(m_prgReg[0], addr);
        }

        return 0;
    }

    // ---------- PRG area $8000-$FFFF
    // core calls readPrg/writePrg with addr&0x7FFF for addresses >= 0x8000
    GERANES_HOT uint8_t readPrg(int addr) override
    {
        switch (m_prgMode) {
            case 0: {
                int bank = (m_prgReg[4] >> 2);
                return cd().readPrg<BankSize::B32K>(bank, addr);
            }
            case 1: {
                switch(addr >> 12) { //addr/16k
                    case 0: {
                        int bank = (m_prgReg[2] >> 1);
                        return cd().readPrg<BankSize::B16K>(bank, addr);
                    }
                    case 1: {
                        int bank = (m_prgReg[4] >> 1);
                        return cd().readPrg<BankSize::B16K>(bank, addr);
                    }
                }                 
            }
            case 2: {
                switch(addr >> 12) { //addr/16k
                    case 0: {
                        int bank = (m_prgReg[2] >> 1);
                        return cd().readPrg<BankSize::B16K>(bank, addr);
                    }
                    case 1: {
                        int div = addr >> 13; //addr/8k
                        int bank = m_prgReg[div+1];
                        return cd().readPrg<BankSize::B8K>(bank, addr);
                    }
                }                 
            }
            case 3:
            default: {
                int div = addr >> 13; //addr/8k
                int bank = m_prgReg[div+1];
                return cd().readPrg<BankSize::B8K>(bank, addr);
            }
        }        

        return 0xFF;
    }

    // GERANES_HOT void writePrg(int maskedAddr, uint8_t data) override    
    // Some games write to ROM area to trigger special behavior (e.g., Bandit Kings writes PRG-RAM via ROM window).
    // MMC5 supports PRG-RAM overlay via registers. For now we only support standard behavior:
    // If write falls into area intended for PRG-RAM (some boards let ROM address space target RAM via register bits),
    // then the cartridge may expect these writes to update PRG-RAM. We don't implement those exotic behaviors here
    // beyond writeSaveRam override which handles the normal $6000-$7FFF window.
    // Normally writes to $8000+ are writes to mapper registers on some mappers (MMC3), but MMC5 uses $5000-$5FFF for regs.

    // ---------- CHR handlers
    GERANES_HOT virtual uint8_t readChr(int addr) override
    {
        addr &= 0x1FFF;

        if (!m_spriteSize8x16            // 8x8 sprites
            && m_currentChrSet == ChrType::B)
        {
            addr &= 0x0FFF;              // <<< mirroring
        }

        if (m_exRamMode == 1 && m_isBg) {
            // BG em Ex1
            uint8_t bank4k = (m_chrUpper << 6) | m_currentBgChrLow;
            return readChrBank<BankSize::B4K>(bank4k, addr);
        }

        if(m_currentChrSet == ChrType::A) {
            switch (m_chrMode) {
                case 0: return readChrBank<BankSize::B8K>(m_chrReg[7], addr);
                case 1:
                    switch(addr >> 12) { // addr/0x1000
                        case 0: return readChrBank<BankSize::B4K>(m_chrReg[3], addr);
                        case 1: return readChrBank<BankSize::B4K>(m_chrReg[7], addr);
                    }
                case 2:
                    switch(addr >> 11) { // addr/0x800
                        case 0: return readChrBank<BankSize::B2K>(m_chrReg[1], addr);
                        case 1: return readChrBank<BankSize::B2K>(m_chrReg[3], addr);
                        case 2: return readChrBank<BankSize::B2K>(m_chrReg[5], addr);
                        case 3: return readChrBank<BankSize::B2K>(m_chrReg[7], addr);
                    }
                case 3: {
                    int index = addr >> 10; // addr/0x400
                    return readChrBank<BankSize::B1K>(m_chrReg[index], addr);
                }                    
            }
        }
        else { //ChrType::B
            switch (m_chrMode) {
                case 0: return readChrBank<BankSize::B8K>(m_chrReg[11], addr);
                case 1:
                    switch(addr >> 12) { // addr/0x1000
                        case 0: return readChrBank<BankSize::B4K>(m_chrReg[11], addr);
                        case 1: return readChrBank<BankSize::B4K>(m_chrReg[11], addr);
                    }
                case 2:
                    switch(addr >> 11) { // addr/0x800
                        case 0: return readChrBank<BankSize::B2K>(m_chrReg[9], addr);
                        case 1: return readChrBank<BankSize::B2K>(m_chrReg[11], addr);
                        case 2: return readChrBank<BankSize::B2K>(m_chrReg[9], addr);
                        case 3: return readChrBank<BankSize::B2K>(m_chrReg[11], addr);
                    }
                case 3: {
                    int index = addr >> 10; // addr/0x400
                    return readChrBank<BankSize::B1K>(m_chrReg[8+(index%4)], addr);
                }
            }
        }
        
        return 0;
    }

    GERANES_HOT virtual void writeChr(int addr, uint8_t data) override
    {
        if (!hasChrRam()) return;

        if(m_currentChrSet == ChrType::A) {
            switch (m_chrMode) {
                case 0: writeChrBank<BankSize::B8K>(m_chrReg[7], addr, data); break;
                case 1:
                    switch(addr >> 12) { // addr/0x1000
                        case 0: writeChrBank<BankSize::B4K>(m_chrReg[3], addr, data); break;
                        case 1: writeChrBank<BankSize::B4K>(m_chrReg[7], addr, data); break;
                    }
                    break;
                case 2:
                    switch(addr >> 11) { // addr/0x800
                        case 0: writeChrBank<BankSize::B2K>(m_chrReg[1], addr, data); break;
                        case 1: writeChrBank<BankSize::B2K>(m_chrReg[3], addr, data); break;
                        case 2: writeChrBank<BankSize::B2K>(m_chrReg[5], addr, data); break;
                        case 3: writeChrBank<BankSize::B2K>(m_chrReg[7], addr, data); break;
                    }
                    break;
                case 3: {
                    int index = addr >> 10; // addr/0x400
                    writeChrBank<BankSize::B1K>(m_chrReg[index], addr, data); break;
                }                    
            }
        }
        else { //ChrType::B
            switch (m_chrMode) {
                case 0: writeChrBank<BankSize::B8K>(m_chrReg[11], addr, data); break;
                case 1:
                    switch(addr >> 12) { // addr/0x1000
                        case 0: writeChrBank<BankSize::B4K>(m_chrReg[11], addr, data); break;
                        case 1: writeChrBank<BankSize::B4K>(m_chrReg[11], addr, data); break;
                    }
                    break;
                case 2:
                    switch(addr >> 11) { // addr/0x800
                        case 0: writeChrBank<BankSize::B2K>(m_chrReg[9], addr, data); break;
                        case 1: writeChrBank<BankSize::B2K>(m_chrReg[11], addr, data); break;
                        case 2: writeChrBank<BankSize::B2K>(m_chrReg[9], addr, data); break;
                        case 3: writeChrBank<BankSize::B2K>(m_chrReg[11], addr, data); break;
                    }
                    break;
                case 3: {
                    int index = addr >> 10; // addr/0x400
                    writeChrBank<BankSize::B1K>(m_chrReg[8+(index%4)], addr, data); break;
                }                    
            }
        } 
    }

    GERANES_HOT MirroringType mirroringType() override
    {
        return MirroringType::CUSTOM;
    }

    virtual uint8_t customMirroring(uint8_t addrIndex) override
    {
        uint8_t x = (m_ntMap >> (2*addrIndex)) & 0x03;

        switch(x) {
            case 0: return 0;
            case 1: return 1;
        }

        assert(false); //should never occur
        return 0;
    }

    GERANES_HOT bool useCustomNameTable(uint8_t index) override
    {
        uint8_t x = (m_ntMap >> (2*index)) & 0x03;

        switch(x) {
            case 0: return false;
            case 1: return false;
            case 2: return true;
            case 3: return true;
        }

        assert(false); //should never occur
        return false;
    }

    GERANES_HOT uint8_t readFillNameTable(uint16_t addr) {
        if (addr < 0x3C0)
            return m_fillTile;
        else {
            uint8_t a = m_fillColor & 0x03;
            return (a << 6) | (a << 4) | (a << 2) | a;
        }
    }

    GERANES_HOT uint8_t readExRamAsNametable(uint16_t addr) {

        if(m_exRamMode == 0 || m_exRamMode == 1)
            return m_exRam[addr];

        return 0;
    }

    GERANES_HOT uint8_t readCustomNameTable(uint8_t addrIndex, uint16_t addr) override
    {        
        uint8_t x = (m_ntMap >> (2*addrIndex)) & 0x03;

        switch(x) {
            case 2: return readExRamAsNametable(addr);
            case 3: return readFillNameTable(addr);
        }

        assert(false); //should never occur
        return 0;
    }

    bool m_isBg = false;

    void configMMC5(bool is8x16, bool isBg) override {
        
        m_isBg = isBg;
        m_spriteSize8x16 = is8x16;
        
        if(is8x16) {
            m_currentChrSet = isBg ? ChrType::B : ChrType::A;            
        }
        else {
            m_currentChrSet = m_lastChrWritten;
        }
    }

    void onMMC5Scanline(bool inFrame) override
    {        
        if(!m_inFrame && inFrame) {
            m_inFrame = true;
            m_scanlineCounter = 0;
            m_scanlinePending = false;
            return;
        }
        
        if(!inFrame) {
            m_inFrame = false;
            return;
        }       

        m_scanlineCounter++;
        if (m_scanlineCounter == m_scanlineCompare) {
            m_scanlinePending = true;
            if (m_irqEnable) m_irqLine = true;
        }  
    }
    
    virtual void onMMC5BgTileIndex(uint16_t tileIndex) override {
        if (m_exRamMode == 1) {
            uint8_t v = m_exRam[tileIndex & 0x03FF];

            uint8_t chrLow = v & 0x3F;
            uint8_t pal    = (v >> 6) << 2;

            m_currentBgPalette = pal;
            m_currentBgChrLow = chrLow;
        }
    }

    virtual std::optional<uint8_t> getMMC5BgPalette() const override {
        if(m_exRamMode == 1) {
            return m_currentBgPalette;
        }
        return std::nullopt;
    }

    // Console polls this to decide /IRQ line
    bool getInterruptFlag() override
    {
        return m_irqLine;
    }    

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
        SERIALIZEDATA(s, m_irqEnable);
        SERIALIZEDATA(s, m_ppuMatchCount);
        SERIALIZEDATA(s, m_lastPpuAddr);
        SERIALIZEDATA(s, m_idleCounter);
        SERIALIZEDATA(s, m_irqLine);

        SERIALIZEDATA(s, m_prgMask);
    }
};
