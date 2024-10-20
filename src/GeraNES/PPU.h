#ifndef PPU_H
#define PPU_H

#include "signal/SigSlot.h"

#include "Settings.h"
#include "Cartridge.h"

#include "Serialization.h"

const int SCREEN_WIDTH = 256;
const int SCREEN_HEIGHT = 240;

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

const uint8_t POWER_UP_PALETTE[] = {
    0x09, 0x01, 0x00, 0x01, 0x00, 0x02, 0x02, 0x0D, 0x08, 0x10, 0x08, 0x24, 0x00, 0x00, 0x04, 0x2C,
    0x09, 0x01, 0x34, 0x03, 0x00, 0x04, 0x00, 0x14, 0x08, 0x3A, 0x00, 0x02, 0x00, 0x20, 0x2C, 0x08
};

class PPU
{

private:

    struct Sprite {
        uint8_t y;
        uint8_t indexInPatternTable;
        uint8_t attrib;
        uint8_t x;
    };

    Settings& m_settings;
    Cartridge& m_cartridge;

    uint32_t m_framebuffer[SCREEN_WIDTH*SCREEN_HEIGHT];
    uint32_t m_framebufferFriendly[SCREEN_WIDTH*SCREEN_HEIGHT];

    uint8_t m_currentPixelColorIndex;

    int m_scanline;
    int m_cycle;

    //PPUCTRL
    int m_VRAMAddressIncrement; // 1 or 32
    bool m_sprite8x8PatternTableAddress; // 0x0000 or 0x1000, ignored in 8x16 mode
    bool m_backgroundPatternTableAddress; // 0x0000 or 0x1000
    bool m_spriteSize8x16; //false = 8x8 true = 8x16
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

    uint8_t m_primaryOam[0x100]; //256 bytes

    uint8_t m_secondaryOam[0x20]; //32 bytes

    uint8_t m_spritesIndexesInThisLine[64];

    //write/read internal regs
    uint16_t m_reg_v;
    uint8_t m_reg_x;
    uint16_t m_reg_t;
    bool m_reg_w;

    uint8_t m_dataLatch;
    uint8_t m_lastWrite;

    int m_debugCursorX = 0;
    int m_debugCursorY = 0;

    // background temporary variables
    uint16_t m_tileAddr;
    uint8_t m_paletteOffset;
    uint8_t m_lowTileByte;
    uint8_t m_highTileByte;
    uint64_t m_tileData;

    int m_lastPPUSTATUSReadCycle; //record the cycle when ppustatus is read

    uint8_t m_openBus;
    uint8_t m_openBusTimer[8]; //1 timer for each bit, decay 1 time per frame

    //settings variables
    int FRAME_NUMBER_OF_LINES;
    int FRAME_VBLANK_START_LINE;
    int FRAME_VBLANK_END_LINE;
    bool m_PALFlag;
    int m_PALCounter;

    bool m_inOverclockLines;

    bool m_preLine;
    bool m_visibleLine;
    bool m_renderLine;

    bool m_overclockFrame;

    int m_ignoreVideoRamReadCycles;
    bool m_needUpdateState;
    bool m_needIncVideoRam;

    bool m_prevCycleRenderingEnabled;

    int m_update_reg_v_delay;
    uint16_t m_update_reg_v_value;

    uint16_t m_busAddress;
    int m_updateA12Delay;

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

    template<bool writeFlag, bool affectsTheBus>
    GERANES_HOT auto readWritePpuMemory(uint16_t addr, uint8_t data = 0) -> std::conditional_t<writeFlag, void, uint8_t>
    {
        if constexpr (affectsTheBus)
            setBusAddress(addr);

        addr &= 0x3FFF; //mirror 0x0000-0x3FFF when addr >= 0x4000        

        if(addr < 0x2000)
        {
            if constexpr(writeFlag) m_cartridge.writeChr(addr,data);
            else return m_cartridge.readChr(addr);
        }

        else if(addr < 0x3F00)
        {
            addr &= 0x2FFF;

            uint8_t addrIndex = (addr-0x2000) >> 10; //0-3 index without mirroring

            if constexpr(writeFlag) writeNameTable(addrIndex,addr,data);
            else return readNameTable(addrIndex,addr);
        }

        else // addr < 0x4000
        {
            addr &= 0x3F1F; //mirror till 0x4000-1

            if(addr == 0x3F10) addr = 0x3F00;
            else if(addr == 0x3F14) addr = 0x3F04;
            else if(addr == 0x3F18) addr = 0x3F08;
            else if(addr == 0x3F1C) addr = 0x3F0C;

            if constexpr(writeFlag) m_palette[addr - 0x3F00] = data;
            else return m_palette[addr - 0x3F00];
        }

        if constexpr(!writeFlag)
            return 0;
    }

    GERANES_INLINE uint8_t readPpuMemory(int addr)
    {
        return readWritePpuMemory<false, true>(addr);
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

    //index 0-3
    GERANES_INLINE_HOT void writeNameTable(uint8_t addrIndex, uint16_t addr, uint8_t data)
    {
        int index = m_cartridge.mirroring(addrIndex&0x03);
        m_nameTable[index&3][addr&0x3FF] = data;
    }

    //index 0-3
    GERANES_INLINE_HOT uint8_t readNameTable(uint8_t addrIndex, uint16_t addr)
    {
        if(m_cartridge.useCustomNameTable(addrIndex&0x03)) {
            return m_cartridge.readCustomNameTable(addrIndex&0x03,addr&0x3FF);
        }

        int index = m_cartridge.mirroring(addrIndex&0x03);
        return m_nameTable[index&3][addr&0x3FF];
    }

    GERANES_INLINE void setBusAddress(uint16_t addr) {
        m_busAddress = addr;
        m_updateA12Delay = 2;
        m_needUpdateState = true;        
    }


public:

    SigSlot::Signal<> signalFrameStart;
    SigSlot::Signal<> signalFrameReady; //called when the frame buffer is ready to show

    PPU(Settings& settings, Cartridge& cartridge) : m_settings(settings), m_cartridge(cartridge)
    {
        init();
    }

    void init()
    {
        initOpenBus();

        writePPUCTRL(0);
        writePPUMASK(0);

        memcpy(m_palette, POWER_UP_PALETTE, sizeof(m_palette));

        m_renderingEnabled = m_spritesEnabled || m_backgroundEnabled;
        m_prevCycleRenderingEnabled = m_renderingEnabled;

        //PPUSTATUS
        m_VBlankHasStarted = false;
        m_sprite0Hit = false;
        m_spriteOverflow = false;

        m_spritesInThisLine = 0;
        m_testSprite0HitInThisLine = false;

        m_oamAddr = 0;
        m_dataLatch = 0;
        m_lastWrite = 0;

        m_currentPixelColorIndex = 0;

        m_scanline = 0;
        m_cycle = 0;

        m_currentY = 0;
        m_currentX = 0;

        m_reg_v = 0;
        m_reg_x = 0;
        m_reg_t = 0;
        m_reg_w = false;

        m_oddFrameFlag = false;

        m_interruptFlag = false;

        m_tileAddr = 0;
        m_paletteOffset = 0;
        m_lowTileByte = 0;
        m_highTileByte = 0;
        m_tileData = 0;

        m_lastPPUSTATUSReadCycle = -1;

        clearFramebuffer();
        
        FRAME_NUMBER_OF_LINES = 262;
        FRAME_VBLANK_START_LINE = 241;
        FRAME_VBLANK_END_LINE = 261;

        m_PALFlag = false;
        m_PALCounter = 5;

        m_inOverclockLines = false;

        m_preLine = false;
        m_visibleLine = false;
        m_renderLine = false;

        m_overclockFrame = false;

        m_ignoreVideoRamReadCycles = 0;
        m_needUpdateState = false;
        m_needIncVideoRam = false;

        m_tileAddr = 0;

        m_update_reg_v_delay = 0;
        m_update_reg_v_value = 0;

        m_busAddress = 0;
        m_updateA12Delay = 0;

        updateSettings();        

        for(int i = 0; i < 15; i++) cycle(); //need this for read2004.nes
       
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
                m_PALFlag = false;
                m_PALCounter = 0;              
                break;
                
            case Settings::Region::PAL:                
                FRAME_VBLANK_START_LINE = 241;
                FRAME_VBLANK_END_LINE = FRAME_VBLANK_START_LINE+70;
                FRAME_NUMBER_OF_LINES = FRAME_VBLANK_END_LINE+1;
                m_PALFlag = true;
                m_PALCounter = 0;
                break;

            case Settings::Region::DENDY:
                FRAME_VBLANK_START_LINE = 241;
                FRAME_VBLANK_END_LINE = FRAME_VBLANK_START_LINE+20;
                FRAME_NUMBER_OF_LINES = FRAME_VBLANK_END_LINE+51;
                m_PALFlag = false;
                m_PALCounter = 0; 
                break;
        }
    }

    //NES adress space
    template<bool writeFlag>
    GERANES_INLINE_HOT uint8_t readWrite(int addr, uint8_t data)
    {
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
            if constexpr(writeFlag) writePPUCTRL(data);
            break;
        }
        case 0x2001: //acess: write only
        {
            if constexpr(writeFlag) writePPUMASK(data);
            break;
        }
        case 0x2002: //acess: read only
        {
            if constexpr(!writeFlag) {
                data = readPPUSTATUS();

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

                if(m_ignoreVideoRamReadCycles > 0) {
                    openBusMask = 0xFF;
                }
                else {
                    data = readPPUDATA();
                }

                if(isOnPaletteAddr()) {
                    data &= 0x3F; //the 2 high bits are open and they should be from decay value
                    data |= m_openBus&(~0x3F);
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

    GERANES_INLINE_HOT void renderPixel()
    {
        m_currentPixelColorIndex = 0;

        if(m_backgroundEnabled) renderBackgroundPixel();
        if(m_spritesEnabled) renderSpritesPixel();

        uint8_t value;

        //if reg v is pointing to the palette
        if(!isRenderingEnabled() && isOnPaletteAddr()) {
            value  = fakeReadPpuMemory(m_reg_v)&0x3F;
        }
        else {
            if( (m_currentPixelColorIndex&0x03) == 0) m_currentPixelColorIndex = 0;
            value  = m_palette[m_currentPixelColorIndex]&0x3F;
        }

        m_framebuffer[m_currentY*SCREEN_WIDTH+m_currentX] = NESToRGBAColor(value);

        if(++m_currentX == SCREEN_WIDTH){
            m_currentX = 0;

            if(++m_currentY == SCREEN_HEIGHT) {
                m_currentY = 0;
                m_currentX = 0;                
            }
        }
    }

    GERANES_INLINE_HOT void renderBackgroundPixel()
    {
        if(!m_showBackgroundLeftmost8Pixels && m_currentX <8) return;

        int data = fetchTileData() >> ((7 - m_reg_x) << 2);

        m_currentPixelColorIndex = uint8_t(data & 0x0F);
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

    int getCursorX()
    {
        return m_debugCursorX;
    }

    int getCursorY()
    {        
        return m_debugCursorY;
    }

    GERANES_INLINE_HOT void evaluateSprites()
    {       
        if(m_cycle < 65) {
            m_oamCopyBuffer = 0xFF;
            m_secondaryOam[(m_cycle-1) >> 1] = 0xFF;
        }
        else if(m_cycle == 256) {

            m_spritesInThisLine = 0; //emulator way of do this, not the real ppu behavior

            for(int i = 0; i < 64; i++) {
  
                const int& spriteY = (int)m_primaryOam[i << 2] + 1;

                if( (m_currentY >= spriteY) && (m_currentY < (spriteY+(m_spriteSize8x16?16:8))) ) {  
                    m_spritesIndexesInThisLine[m_spritesInThisLine++] = i << 2;
                }
            }

            //-----------------------------------------------------------------------------
            
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

                    if(!m_spriteInRange && m_scanline >= m_oamCopyBuffer && m_scanline < m_oamCopyBuffer + (m_spriteSize8x16 ? 16 : 8)) {
                        m_spriteInRange = true;
                    }                    

                    if(m_secondaryOamAddr < sizeof(m_secondaryOam)) {

                        m_secondaryOam[m_secondaryOamAddr] = m_oamCopyBuffer;

                        if(m_spriteInRange) {
                            
                            m_oamAddrM++;
                            m_secondaryOamAddr++;

                            if(m_oamAddrN == 0) {
							    m_sprite0Added = true;
							}

                            //Note: Using "(m_secondaryOamAddr & 0x03) == 0" instead of "m_oamAddrM == 0" is required
							//to replicate a hardware bug noticed in oam_flicker_test_reenable when disabling & re-enabling
							//rendering on a single scanline
							if((m_secondaryOamAddr & 0x03) == 0) {
								//Done copying all 4 bytes
								m_spriteInRange = false;
								m_oamAddrM = 0;

								m_oamAddrN = (m_oamAddrN + 1) & 0x3F;
								if(m_oamAddrN == 0) {
									m_oamCopyDone = true;
								}
							}
                        }
                        else {

                            //Nothing to copy, skip to next sprite
							m_oamAddrN = (m_oamAddrN + 1) & 0x3F;
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

    GERANES_INLINE_HOT void renderSpritesPixel()
    {
        if(!m_showSpritesLeftmost8Pixels && m_currentX < 8) return;
        if(m_currentY == 0) return;

        int spriteLineToDraw = 0;
        int spriteXToDraw = 0;

        int paletteIndex = 0;
        bool isPixelBehind = false;

        bool spritesAsMask = false;

        if(m_settings.spriteLimitDisabled() && m_spritesInThisLine >= 8) {
            
            // Detecting masking effects
            // Games will place 8 consecutive sprites with the same Y coordinate and same tile number.
            // If you see this, then that is a sign that the game is using a masking effect,
            // and the 8-sprite limit should be enforced for that area.

            const Sprite* first = (Sprite*)&m_primaryOam[m_spritesIndexesInThisLine[0]];
 

            int i = 1;

            //test if all sprites have the same y and x
            for(; i < 8; i++) {
                const Sprite* other = (Sprite*)&m_primaryOam[m_spritesIndexesInThisLine[i]];


                if(first->y != other->y && first->indexInPatternTable != other->indexInPatternTable) break;
            }

            spritesAsMask = i == 8;
        }

        for(int i = 0; i < m_spritesInThisLine; i++)
        {
            const Sprite* sprite = (Sprite*)&m_primaryOam[m_spritesIndexesInThisLine[i]];

            const int& spriteY = sprite->y+1;
            const uint8_t& spriteIndexInPatternTable = sprite->indexInPatternTable;
            const uint8_t& spriteAttrib = sprite->attrib;
            const int& spriteX = sprite->x;

            if(i >= 8 && (!m_settings.spriteLimitDisabled() || spritesAsMask)) break;

            //if( !(m_currentX >= spriteX && m_currentX < spriteX+8) ) continue;
            if( m_currentX < spriteX || m_currentX >= spriteX+8) continue;

            if(m_spriteSize8x16 == false) //sprite 8x8
            {

                if( (spriteAttrib&0x80) == false) //vertical flip = false?
                    spriteLineToDraw = m_currentY - spriteY;
                else
                    spriteLineToDraw = spriteY - m_currentY + 7;

                if( (spriteAttrib&0x40) == false) //horizontal flip = false?
                    spriteXToDraw = m_currentX - spriteX;
                else
                    spriteXToDraw = spriteX - m_currentX + 7;

                int index = spriteIndexInPatternTable + (m_sprite8x8PatternTableAddress?256:0); //add 512 if sprite is in second page (0x1000);

                paletteIndex =  getColorLowBitsInPatternTable(index,spriteXToDraw,spriteLineToDraw);
                paletteIndex |= (spriteAttrib&0x03) << 2; //get 2 high bits
            }
            else //sprite 8x16
            {
                if( (spriteAttrib&0x80) == false) //vertical flip check
                    spriteLineToDraw = m_currentY - spriteY;
                else
                    spriteLineToDraw = spriteY - m_currentY + 15;

                if( (spriteAttrib&0x40) == false) //horizontal flip = false?
                    spriteXToDraw = m_currentX - spriteX;
                else
                    spriteXToDraw = spriteX - m_currentX + 7;

                int index;

                if(spriteLineToDraw < 8) //top sprite
                {
                    index = spriteIndexInPatternTable;
                }
                else //bottom sprite
                {
                    index = spriteIndexInPatternTable;
                    index += 1;
                }

                if(spriteIndexInPatternTable&0x01) index += 255;

                paletteIndex = getColorLowBitsInPatternTable(index,spriteXToDraw,spriteLineToDraw%8);
                paletteIndex |= (spriteAttrib&0x03) << 2; //get 2 high bits
            }

            //sprite0hit test
            if (i == 0 && m_testSprite0HitInThisLine && m_backgroundEnabled &&
            (m_currentPixelColorIndex&0x03) != 0 &&
            (paletteIndex&0x03) != 0 && m_currentX != 255) {
                m_sprite0Hit = true;
            }

            if( (paletteIndex&0x03) != 0)
            {
                paletteIndex = 0x10+paletteIndex;
                isPixelBehind = spriteAttrib&0x20;
                break;
            }

        }

        if( (paletteIndex&0x03) != 0 )
        {
            if( (m_currentPixelColorIndex&0x03) == 0) m_currentPixelColorIndex = paletteIndex;
            else
            {
                if(!isPixelBehind) m_currentPixelColorIndex = paletteIndex;
            }
        }
    }

    //called every cpu cycle
    GERANES_INLINE_HOT void cycle()
    {
        ppuCycle();
        ppuCycle();
        ppuCycle();

        ppuCyclePAL();
    }

    GERANES_INLINE void ppuCyclePAL()
    {
        if(m_PALFlag) { //3.2 ppu cycles / cpu cycle

            if(m_PALCounter > 0) --m_PALCounter;

            if(m_PALCounter == 0){
                m_PALCounter = 5;
                ppuCycle();
            }
        }
    }

    GERANES_INLINE_HOT void onScanlineChanged()
    {
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
        else if(m_scanline == 240) {
            //At the start of vblank, the bus address is set back to VideoRamAddr.
            //According to Visual NES, this occurs on scanline 240, cycle 1, but is done here on cycle 0 for performance reasons
            setBusAddress(m_reg_v&0x3FFF);
        }
        else if(m_scanline == 241){
            decayOpenBus();
            memcpy(m_framebufferFriendly,m_framebuffer,SCREEN_WIDTH*SCREEN_HEIGHT*sizeof(uint32_t));
            signalFrameReady();

            if(m_overclockFrame){
                FRAME_NUMBER_OF_LINES += m_settings.overclockLines();
                FRAME_VBLANK_START_LINE += m_settings.overclockLines();
                FRAME_VBLANK_END_LINE += m_settings.overclockLines();
            }
        }
    }

    GERANES_HOT void ppuCycle()
    {        
        if(m_cycle == 0) onScanlineChanged();

        if(!m_interruptFlag) {
            if(m_VBlankHasStarted && m_NMIOnVBlank) {
                m_interruptFlag =  m_NMIOnVBlank;
            }
        }
        else {
            if(!m_VBlankHasStarted) {
                m_interruptFlag = false;
            }
        }

        if(m_renderLine) {

            bool preFetchCycle = m_cycle >= 321 && m_cycle <= 336;
            bool spriteFetchCycles = m_cycle >= 257 && m_cycle <= 320;
            bool visibleCycle = m_cycle >= 1 && m_cycle <= 256;
            bool bgFetchCycles = preFetchCycle || visibleCycle;

            if(m_visibleLine && visibleCycle) {
                renderPixel();
            }            

            if(m_visibleLine && isRenderingEnabled()) {

                if(m_cycle == 321) {
                    m_oamCopyBuffer = m_secondaryOam[0];
                }

                if(visibleCycle) evaluateSprites();
            }            
            
            if(m_prevCycleRenderingEnabled) {
         
                if(bgFetchCycles) {

                    m_tileData <<= 4;

                    switch(m_cycle%8) {
                        case 1: fetchNameTableByte(); break;                        
                        case 3: fetchAttributeTableByte(); break;
                        case 5: fetchLowTileByte(); break;
                        case 7: fetchHighTileByte(); break;
                        case 0: storeTileData(); incrementVX(); break; 
                    }
                }
                
                switch(m_cycle) {
                    case 256: incrementVY(); break;
                    case 257: copyVX(); break;
                }
            }

            if(isRenderingEnabled()) {
                //"OAMADDR is set to 0 during each of ticks 257-320 (the sprite tile loading interval) of the pre-render and visible scanlines." (When rendering)
			    if(spriteFetchCycles) {
                    m_oamAddr = 0;
                    fetchSprites();
                }
            }

            if(m_preLine) {

                if(m_cycle >= 280 && m_cycle <= 304) { //280 to 304 copy each tick
                    if(m_prevCycleRenderingEnabled) copyVY();
                }
                else if(m_cycle == VBLANK_CYCLE){
                    m_spriteOverflow = false;
                    m_sprite0Hit = false;
                    m_VBlankHasStarted = false;
                }

                else if(m_cycle == 339 && m_settings.region() == Settings::Region::NTSC /*&& GetPpuModel() == PpuModel::Ppu2C02*/) //only NTSC has skip cycle in odd frames
                {
                    if(m_oddFrameFlag && isRenderingEnabled()) m_cycle++;
                    m_oddFrameFlag = !m_oddFrameFlag;
                }

                if(m_cycle == 337 || m_cycle == 339) {
                    fetchNameTableByte();  //unused NT fetch
                }

            }
        }

        else if(m_scanline == FRAME_VBLANK_START_LINE)
        {
            //Reading one PPU clock before reads it as clear and never sets the flag or generates NMI for that frame.
            //Reading on the same PPU clock or one later reads it as set, clears it, and suppresses the NMI for that frame

            //1 cycle before -> m_lastPPUSTATUSReadCycle == VBLANK_CYCLE

            if(m_cycle == VBLANK_CYCLE) {
                if( m_lastPPUSTATUSReadCycle != VBLANK_CYCLE) m_VBlankHasStarted = true; //suppression?

            }
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

    void fetchSprites() {

        const int startCycle = 257;

        int fetchCycle = (m_cycle - startCycle) % 8;   

        switch(fetchCycle) {

            case 0: readPpuMemory(getNameTableAddr()); break;
            case 2: readPpuMemory(getAttributeTableAddr()); break;
            case 4: {         

                bool state;

                int spriteIndex = (m_cycle - startCycle) / 8;
                Sprite* sprite = (Sprite*)&m_secondaryOam[spriteIndex << 2];

                uint8_t tile = sprite->indexInPatternTable;
               
                if(m_spriteSize8x16) {
                    state = tile & 1;
                }
                else state = m_sprite8x8PatternTableAddress;

                setBusAddress(m_busAddress | (state ? 0x1000 : 0)); // approximate relevant behavior         

                break;
            }
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
        const int tileX = x >> 3; //x/8
        const int tileY = y >> 3; //y/8

        int tileIndexInPatternTable = getTileIndexInNameTables(tileX,tileY);

        if(m_backgroundPatternTableAddress) tileIndexInPatternTable += 256; //0x1000

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

    GERANES_INLINE_HOT uint32_t NESToRGBAColor(uint8_t index)
    {
        uint32_t color = NES_PALETTE[index];

        if(m_colorEmphasis != 0) color =  getEmphasisColor(color);
        if(m_monochromeDisplay) color = getMonochromeColor(color);

        return color;
    }
    
    GERANES_INLINE void writePPUCTRL(uint8_t data)
    {
        //put base nametable address in bits 10 and 11 of reg_t
        m_reg_t &= 0xF3FF;
        m_reg_t |= ((static_cast<uint16_t>(data & 0x03)) << 10);

        m_VRAMAddressIncrement = (data&0x04) ? 32 : 1;
        m_sprite8x8PatternTableAddress = (data&0x08) ? true : false;
        m_backgroundPatternTableAddress = (data&0x10) ? true : false;
        m_spriteSize8x16 = (data&0x20) ? true : false;
        m_PPUSlave = (data&0x40) ? true : false;
        m_NMIOnVBlank = (data&0x80) ? true : false;

        if(m_NMIOnVBlank == false) m_interruptFlag = false;

    }

    GERANES_INLINE void writePPUMASK(uint8_t data)
    {
        m_monochromeDisplay = (data&0x01) ? true : false;
        m_showBackgroundLeftmost8Pixels = (data&0x02) ? true : false;
        m_showSpritesLeftmost8Pixels = (data&0x04) ? true : false;
        m_backgroundEnabled = (data&0x08) ? true : false;
        m_spritesEnabled = (data&0x10) ? true : false;
        m_colorEmphasis = data >> 5;        

        if(m_renderingEnabled != (m_backgroundEnabled || m_spritesEnabled)) {
		    m_needUpdateState = true;
	    }
    }

    GERANES_INLINE uint8_t readPPUSTATUS()
    {
        uint8_t ret = 0x00;

        m_lastPPUSTATUSReadCycle = m_cycle;

        if(m_VBlankHasStarted) ret |= 0x80;

        if(m_sprite0Hit) ret |= 0x40;
        if(m_spriteOverflow) ret |= 0x20;

        m_VBlankHasStarted = false;
        m_reg_w = false;

        ret |= m_lastWrite & 0x1F; //5 least significant bits

        return ret;
    }

    GERANES_INLINE void writeOAMADDR(uint8_t data)
    {
        m_oamAddr = data;
    }    

    GERANES_INLINE uint8_t readOAMDATA()
    {
        const int delay = 1; //read2004.nes

        uint8_t ret = m_primaryOam[m_oamAddr];

        if (m_renderLine && isRenderingEnabled()) {
            if(m_cycle >= (257+delay) && m_cycle <= (320+delay)) {
                uint8_t step = ((m_cycle - (257+delay)) % 8) > 3 ? 3 : ((m_cycle - (257+delay)) % 8);
                uint8_t addr = (m_cycle - (257+delay)) / 8 * 4 + step;
                ret = m_secondaryOam[addr];
            } else {
                ret = m_oamCopyBuffer;
            }
        }

        return ret;
    }

    GERANES_INLINE bool isRenderingEnabled() {
        //return m_spritesEnabled || m_backgroundEnabled;
        return m_renderingEnabled;
    }    

    GERANES_INLINE void writeOAMDATA(uint8_t data)
    {
        if((!m_renderLine && (m_settings.region() != Settings::Region::PAL || m_renderLine)) || !isRenderingEnabled()) {
            if((m_oamAddr & 0x03) == 0x02) {
                //"The three unimplemented bits of each sprite's byte 2 do not exist in the PPU and always read back as 0 on PPU revisions that allow reading PPU OAM through OAMDATA ($2004)"
                data &= 0xE3;
            }
            m_primaryOam[m_oamAddr] = data;
            m_oamAddr++;
        } else {
            //"Writes to OAMDATA during rendering (on the pre-render line and the visible lines 0-239, provided either sprite or background rendering is enabled) do not modify values in OAM, 
            //but do perform a glitchy increment of OAMADDR, bumping only the high 6 bits"
            m_oamAddr = ((m_oamAddr + 1) & 0xFC) | (m_oamAddr & 0x3);
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

        m_ignoreVideoRamReadCycles = 6;          
        
        if(isOnPaletteAddr()) {
            ret = fakeReadPpuMemory(m_reg_v&0x3FFF); //palette
            m_dataLatch = readPpuMemory(m_reg_v&0x2FFF);
        }     
        else {
            ret = m_dataLatch;
            m_dataLatch = readPpuMemory(m_reg_v&0x3FFF);
        }     

        m_needUpdateState = true;
        m_needIncVideoRam = true;        
        

        return ret;
    }

    void incVideoRamAddr() {

        if( !m_renderLine || !isRenderingEnabled()) {

            m_reg_v+=m_VRAMAddressIncrement;
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

        //Rendering enabled flag is apparently set with a 1 cycle delay (i.e setting it at cycle 5 will render cycle 6 like cycle 5 and then take the new settings for cycle 7)
        if(m_prevCycleRenderingEnabled != m_renderingEnabled) {

            m_prevCycleRenderingEnabled = m_renderingEnabled;

            if(m_renderLine) {

                 if(!m_prevCycleRenderingEnabled) {    

                    //When rendering is disabled midscreen, set the vram bus back to the value of 'v'
                    //setBusAddress(m_reg_v & 0x3FFF); //break hard drivin
                    
                    if(m_cycle >= 65 && m_cycle <= 256) {
                        //Disabling rendering during OAM evaluation will trigger a glitch causing the current address to be incremented by 1
                        //The increment can be "delayed" by 1 PPU cycle depending on whether or not rendering is disabled on an even/odd cycle
                        //e.g, if rendering is disabled on an even cycle, the following PPU cycle will increment the address by 5 (instead of 4)
                        //     if rendering is disabled on an odd cycle, the increment will wait until the next odd cycle (at which point it will be incremented by 1)
                        //In practice, there is no way to see the difference, so we just increment by 1 at the end of the next cycle after rendering was disabled
                        m_oamAddr++;

                        //Also corrupt H/L to replicate a bug found in oam_flicker_test_reenable when rendering is disabled around scanlines 128-136
                        //Reenabling the causes the OAM evaluation to restart misaligned, and ends up generating a single sprite that's offset by 1
                        //such that it's Y=tile index, index = attributes, attributes = x, and X = the next sprite's Y value
                        m_oamAddrN = (m_oamAddr >> 2) & 0x3F;
                        m_oamAddrM = m_oamAddr & 0x03;
                        
                    }
                }
            }
        }

        if(m_renderingEnabled != (m_backgroundEnabled || m_spritesEnabled)) {
            m_renderingEnabled = m_backgroundEnabled || m_spritesEnabled;
            m_needUpdateState = true;
        }

        if(m_update_reg_v_delay > 0) {
            m_update_reg_v_delay--;
            if(m_update_reg_v_delay == 0) {
   
                m_reg_v = m_update_reg_v_value;

                if( !m_renderLine || !isRenderingEnabled()) {
                    //Only set the VRAM address on the bus if the PPU is not rendering
                    //More info here: https://forums.nesdev.com/viewtopic.php?p=132145#p132145
                    //Trigger bus address change when setting the vram address - needed by MMC3 IRQ counter
                    //"4) Should be clocked when A12 changes to 1 via $2006 write"
                    setBusAddress(m_reg_v & 0x3FFF);
                }

            } else {
                m_needUpdateState = true;
            }
        }

        //Delay vram address increment by 1 ppu cycle after a read/write to 2007
		//This allows the full_palette tests to properly display single-pixel glitches 
		//that display the "wrong" color on the screen until the increment occurs (matches hardware)
        if(m_needIncVideoRam) {
            m_needIncVideoRam = false;
            incVideoRamAddr();
        }

        if(m_ignoreVideoRamReadCycles > 0) {
            --m_ignoreVideoRamReadCycles;

            if(m_ignoreVideoRamReadCycles > 0) {
                m_needUpdateState = true;
            }
        }

        if(m_updateA12Delay > 0) {
            --m_updateA12Delay;

            if(m_updateA12Delay == 0)
                m_cartridge.setA12State(m_busAddress&0x1000);
            else
                m_needUpdateState = true;
        }    

    }

    GERANES_INLINE void writePPUDATA(uint8_t data)
    {
        m_lastWrite = data;

        if(isOnPaletteAddr()) {
            fakeWritePpuMemory(m_reg_v, data); //palette
        }
        else {
            writePpuMemory(m_reg_v,data);
        }

        m_needUpdateState = true;
        m_needIncVideoRam = true;
    }

    GERANES_INLINE void fillFramebuffer(uint32_t color)
    {
        uint32_t* start = &m_framebuffer[0];
        uint32_t* end = &m_framebuffer[SCREEN_WIDTH*SCREEN_HEIGHT];
        while(start != end) *start++ = color;

        memcpy(m_framebufferFriendly,m_framebuffer,SCREEN_WIDTH*SCREEN_HEIGHT*sizeof(uint32_t));
    }

    GERANES_INLINE void clearFramebuffer()
    {
        fillFramebuffer(0xFF000000);
    }

    GERANES_INLINE const uint32_t* getFramebuffer()
    {
        return m_framebufferFriendly;
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

    GERANES_INLINE uint16_t getNameTableAddr() {
        uint16_t address = 0x2000 | (m_reg_v & 0x0FFF);
        return address;
    }

    GERANES_INLINE void fetchNameTableByte() {
        uint16_t address = getNameTableAddr();
        uint8_t tileIndex = readPpuMemory(address);

        int fineY = (m_reg_v >> 12) & 7;
        int table = m_backgroundPatternTableAddress ? 0x1000 : 0x0000;

        m_tileAddr = table + (tileIndex << 4) + fineY;
    }

    GERANES_INLINE uint16_t getAttributeTableAddr() {
        uint16_t address = 0x23C0 | (m_reg_v & 0x0C00) | ((m_reg_v >> 4) & 0x38) | ((m_reg_v >> 2) & 0x07);
        return address;
    }

    GERANES_INLINE void fetchAttributeTableByte() {

        int address = getAttributeTableAddr();
        int shift = ((m_reg_v >> 4) & 4) | (m_reg_v & 2);
        m_paletteOffset = ((readPpuMemory(address) >> shift) & 3) << 2;
    }

    GERANES_INLINE void fetchLowTileByte() {
        m_lowTileByte = readPpuMemory(m_tileAddr);        
    }

    GERANES_INLINE void fetchHighTileByte() {
        m_highTileByte = readPpuMemory(m_tileAddr + 8);        
    }

    GERANES_INLINE void storeTileData() {
        uint64_t data = 0;
        for(int i= 0; i < 8; i++) {
            int p1 = (m_lowTileByte & 0x80) >> 7;
            int p2 = (m_highTileByte & 0x80) >> 6;
            m_lowTileByte <<= 1;
            m_highTileByte <<= 1;
            data <<= 4;
            data |= uint32_t(m_paletteOffset | p1 | p2);
        }
        m_tileData |= data;
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
        SERIALIZEDATA(s, m_sprite8x8PatternTableAddress);
        SERIALIZEDATA(s, m_backgroundPatternTableAddress);
        SERIALIZEDATA(s, m_spriteSize8x16);
        SERIALIZEDATA(s, m_PPUSlave);
        SERIALIZEDATA(s, m_NMIOnVBlank);

        SERIALIZEDATA(s, m_monochromeDisplay);
        SERIALIZEDATA(s, m_showBackgroundLeftmost8Pixels);
        SERIALIZEDATA(s, m_showSpritesLeftmost8Pixels);
        SERIALIZEDATA(s, m_backgroundEnabled);
        SERIALIZEDATA(s, m_spritesEnabled);
        SERIALIZEDATA(s, m_colorEmphasis);

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

        SERIALIZEDATA(s, m_reg_v);
        SERIALIZEDATA(s, m_reg_x);
        SERIALIZEDATA(s, m_reg_t);
        SERIALIZEDATA(s, m_reg_w);

        SERIALIZEDATA(s, m_dataLatch);
        SERIALIZEDATA(s, m_lastWrite);        

        SERIALIZEDATA(s, m_tileAddr);
        SERIALIZEDATA(s, m_paletteOffset);
        SERIALIZEDATA(s, m_lowTileByte);
        SERIALIZEDATA(s, m_highTileByte);
        SERIALIZEDATA(s, m_tileData);

        SERIALIZEDATA(s, m_lastPPUSTATUSReadCycle);

        SERIALIZEDATA(s, m_openBus);
        s.array(m_openBusTimer, 1, 8);

        SERIALIZEDATA(s, FRAME_NUMBER_OF_LINES);
        SERIALIZEDATA(s, FRAME_VBLANK_START_LINE);
        SERIALIZEDATA(s, FRAME_VBLANK_END_LINE);
        SERIALIZEDATA(s, m_PALFlag);
        SERIALIZEDATA(s, m_PALCounter);
        SERIALIZEDATA(s, m_inOverclockLines);
        SERIALIZEDATA(s, m_preLine);
        SERIALIZEDATA(s, m_visibleLine);
        SERIALIZEDATA(s, m_renderLine);

        SERIALIZEDATA(s, m_ignoreVideoRamReadCycles);
        SERIALIZEDATA(s, m_needUpdateState);
        SERIALIZEDATA(s, m_needIncVideoRam);

        SERIALIZEDATA(s, m_prevCycleRenderingEnabled);

        SERIALIZEDATA(s, m_update_reg_v_delay);
        SERIALIZEDATA(s, m_update_reg_v_value);

        SERIALIZEDATA(s, m_busAddress);
        SERIALIZEDATA(s, m_updateA12Delay);
    }

};

#endif
