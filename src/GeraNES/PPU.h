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

    Settings& m_settings;
    Cartridge& m_cartridge;

    uint32_t m_framebuffer[SCREEN_WIDTH*SCREEN_HEIGHT];
    uint32_t m_framebufferFriendly[SCREEN_WIDTH*SCREEN_HEIGHT];

    uint8_t m_currentPixelColorIndex;

    int m_currentScanline;
    int m_currentScanlineCycle;

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

    //PPUSTATUS
    bool m_VBlankHasStarted;
    bool m_sprite0Hit;
    bool m_spriteOverflow;

    bool m_sprite0HitDelayed;

    //sprites evaluate variables
    uint8_t m_spritesInThisLine;
    bool m_testSprite0HitInThisLine;

    //OAM read/write address
    uint8_t m_OAMAddr;

    //Rendering position
    int m_currentY;
    int m_currentX;

    bool m_interruptFlag;

    bool m_oddFrameFlag;

    /*
    NES has nametables 0 and 1 in the console, 2 and 3 are in
    cartridge when four screen mirroring is used
    they are here for simplification
    */
    uint8_t m_nameTable[4][0x400]; //4x 1KB

    uint8_t m_palette[0x20]; //32 Bytes

    uint8_t m_primaryOAM[0x100]; //256 bytes

    uint8_t m_secondaryOAM[0x100]; //256 bytes / 32 bytes on real hardware

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
    uint8_t m_nameTableByte;
    uint8_t m_attributeTableByte;
    uint8_t m_lowTileByte;
    uint8_t m_highTileByte;
    uint64_t m_tileData;

    int m_lastPPUSTATUSReadCycle; //record the cycle when ppustatus is read

    uint8_t m_openBus;
    uint8_t m_openBusTimer[8]; //1 timer for each bit, decay 1 time per frame

    uint32_t m_powerUpCounter;

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

    template<bool writeFlag>
    GERANES_HOT auto readWritePPUMemory(uint16_t addr, uint8_t data = 0) -> std::conditional_t<writeFlag, void, uint8_t>
    {
        addr &= 0x3FFF; //mirror 0x0000-0x3FFF when addr >= 0x4000

        if(addr < 0x2000)
        {
            if constexpr(writeFlag) m_cartridge.writeCHR8k(addr,data);
            else return m_cartridge.readCHR8k(addr);
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

    GERANES_INLINE uint8_t readPPUMemory(int addr)
    {
        return readWritePPUMemory<false>(addr);
    }

    GERANES_INLINE void writePPUMemory(int addr, uint8_t data)
    {
        readWritePPUMemory<true>(addr,data);
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

        //PPUSTATUS
        m_VBlankHasStarted = false;
        m_sprite0HitDelayed = m_sprite0Hit = false;
        m_spriteOverflow = false;

        m_spritesInThisLine = 0;
        m_testSprite0HitInThisLine = false;

        m_OAMAddr = 0;
        m_dataLatch = 0;
        m_lastWrite = 0;

        m_currentPixelColorIndex = 0;

        m_currentScanline = 0;
        m_currentScanlineCycle = 0;

        m_currentY = 0;
        m_currentX = 0;

        m_reg_v = 0;
        m_reg_x = 0;
        m_reg_t = 0;
        m_reg_w = false;

        m_oddFrameFlag = false;

        m_interruptFlag = false;

        m_nameTableByte = 0;
        m_attributeTableByte = 0;
        m_lowTileByte = 0;
        m_highTileByte = 0;
        m_tileData = 0;

        m_lastPPUSTATUSReadCycle = -1;

        clearFramebuffer();

        m_powerUpCounter = 0;
        
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

        updateSettings();

        //test if cobra triangle hangs
        for(int i = 0; i < 341 * 260; i++) cycle();
        //m_interruptFlag = false;
        //m_NMIDelay1Instruction = false;
        //m_powerUpCounter = 29658 * 3;
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
            FRAME_NUMBER_OF_LINES = 262;
            FRAME_VBLANK_START_LINE = 241;
            FRAME_VBLANK_END_LINE = 261;
            m_PALFlag = false;
            m_PALCounter = 0;
            break;
        case Settings::Region::PAL:
            FRAME_NUMBER_OF_LINES = 312;
            FRAME_VBLANK_START_LINE = 241;
            FRAME_VBLANK_END_LINE = 311;
            m_PALFlag = true;
            m_PALCounter = 5;
            break;
        }
    }

    //NES adress space
    template<bool writeFlag>
    GERANES_INLINE_HOT uint8_t readWrite(int addr, uint8_t data)
    {
        if(!writeFlag) data = m_openBus;
        uint8_t openBusMask = writeFlag ? 0xFF : 0x00;

        addr &= 0x2007;

        switch(addr)
        {
        case 0x2000: //acess: write only
        {
            if(writeFlag) writePPUCTRL(data);
            break;
        }
        case 0x2001: //acess: write only
        {
            if(writeFlag) writePPUMASK(data);
            break;
        }
        case 0x2002: //acess: read only
        {
            if(!writeFlag) {
                data = readPPUSTATUS();

                data &= ~0x1F;
                data |= m_openBus&0x1F; //the low 5 bits are open and they should be from decay value
                openBusMask = ~0x1F; //the high 3 bits need refresh
            }
            break;
        }
        case 0x2003: //acess: write only
        {
            if(writeFlag) writeOAMADDR(data);
            break;
        }
        case 0x2004: //acess: read and write
        {
            if(writeFlag) writeOAMDATA(data);
            else data = readOAMDATA();
            break;
        }
        case 0x2005: //acess: write twice
        {
            if(writeFlag) writePPUSCROLL(data);
            break;
        }
        case 0x2006: //acess: write twice
        {
            if(writeFlag) writePPUADDR(data);
            break;
        }
        case 0x2007: //acess: read and write
        {
            if(writeFlag) writePPUDATA(data);
            else {
                data = readPPUDATA();

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
        if(!m_backgroundEnabled && !m_spritesEnabled && isOnPaletteAddr()) {
            value  = readPPUMemory(m_reg_v)&0x3F;
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

        //int data = fetchTileData() >> ((7 - m_reg_x) * 4);
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

    GERANES_INLINE void evaluateSprites()
    {
        m_spritesInThisLine = 0;
        m_testSprite0HitInThisLine = false;

        for(int i = 0; i < 64; i++){

            //const int& spriteY = (int)m_primaryOAM[4*i] + 1;
            const int& spriteY = (int)m_primaryOAM[i << 2] + 1;

            //239+1 :)
            if(spriteY >= 240){ //hidden sprite
                if(spriteY == 240) m_spriteOverflow = true;
                continue;
            }

            if( (m_currentY >= spriteY) && (m_currentY < (spriteY+(m_spriteSize8x16?16:8)))  ){

                std::memcpy(&m_secondaryOAM[4*m_spritesInThisLine], &m_primaryOAM[4*i], 4);

                if(i == 0) m_testSprite0HitInThisLine = true;

                m_spritesInThisLine++;

                if(m_spritesInThisLine >= 9 && !m_settings.spriteLimitDisabled()) break; //sprite limit
            }
        }

        if((m_backgroundEnabled || m_spritesEnabled) && m_spritesInThisLine > 8) m_spriteOverflow = true;

    }


    GERANES_INLINE_HOT void renderSpritesPixel()
    {
        if(!m_showSpritesLeftmost8Pixels && m_currentX < 8) return;
        if(m_currentY == 0) return;

        int spriteLineToDraw = 0;
        int spriteXToDraw = 0;

        int paletteIndex = 0;
        bool isPixelBehind = false;

        for(int i = 0; i < m_spritesInThisLine; i++)
        {
            const uint8_t* currentSprite = &m_secondaryOAM[i << 2];

            const int& spriteY = (int)(*currentSprite++)+1; //0
            const uint8_t& spriteIndexInPatternTable = *currentSprite++; //1
            const uint8_t& spriteAttrib = *currentSprite++; //2
            const int& spriteX = *currentSprite; //3

            if(i >= 9 && !m_settings.spriteLimitDisabled()) break; //sprite limit

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

                int index = 0;

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
            if(     i == 0 && m_testSprite0HitInThisLine && m_backgroundEnabled &&
                    (m_currentPixelColorIndex&0x03) != 0 &&
                    (paletteIndex&0x03) != 0 && m_currentX != 255
               )
            {
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
            if(--m_PALCounter == 0){
                m_PALCounter = 5;
                ppuCycle();
            }
        }
    }

    GERANES_INLINE_HOT void onScanlineChanged()
    {
        if(m_settings.overclockLines() > 0) {
            if(m_overclockFrame && m_currentScanline >= 241 && m_currentScanline < FRAME_VBLANK_START_LINE)
                m_inOverclockLines = true;
            else
                m_inOverclockLines = false;
        }
        else m_inOverclockLines = false;

        m_preLine = (m_currentScanline == FRAME_VBLANK_END_LINE);
        m_visibleLine = m_currentScanline < 240;
        m_renderLine = m_preLine || m_visibleLine;

        if(m_currentScanline == 0) {
            updateSettings();

            if(m_settings.overclockLines() > 0) {
                m_overclockFrame = true;
            }

            signalFrameStart();
        }
        else if(m_currentScanline == 241){
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
        if(m_currentScanlineCycle == 0) onScanlineChanged();

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


        //if(m_powerUpCounter > 0) m_powerUpCounter--;

        if(m_sprite0Hit) m_sprite0HitDelayed = true;

        if(m_renderLine) {

            bool renderingEnabled = m_backgroundEnabled || m_spritesEnabled;
            bool preFetchCycle = m_currentScanlineCycle >= 321 && m_currentScanlineCycle <= 336;
            bool visibleCycle = m_currentScanlineCycle >= 1 && m_currentScanlineCycle <= 256;
            bool fetchCycle = preFetchCycle || visibleCycle;

            if(m_visibleLine && visibleCycle) renderPixel();

            if(renderingEnabled) {

                if(fetchCycle) {
                    m_tileData <<= 4;
                    switch(m_currentScanlineCycle%8){
                    case 1: fetchNameTableByte(); break;
                    case 3: fetchAttributeTableByte(); break;
                    case 5: fetchLowTileByte(); break;
                    case 7: fetchHighTileByte(); break;
                    case 0:
                        storeTileData();
                        incrementVX();
                        break;
                    }
                }

                switch(m_currentScanlineCycle) {

                // by the docs, it should be at 256, but this seems to be the correct time to incVY,
                // it fix the freezes of battletoads at lvl 2
                case 251: incrementVY(); break;

                case 256: evaluateSprites(); break;
                case 257: copyVX(); break;
                //A12 rises on the 5th cycle of every 8 fetch cycles (4, 12, 20, 28, etc for BG fetches
                //if BG uses $1xxx.... 260,268, etc for Sprite fetches when they use $1xxx)
                case 268: m_cartridge.tickMapper(); break;
                }
            }

            if(m_preLine) {

                if(m_currentScanlineCycle == 304) { //280 to 304 copy each tick
                    if(renderingEnabled) copyVY();
                }
                else if(m_currentScanlineCycle == VBLANK_CYCLE){
                    m_spriteOverflow = false;
                    m_sprite0HitDelayed = m_sprite0Hit = false;
                    m_VBlankHasStarted = false;
                }

                else if(m_currentScanlineCycle == 337+VBLANK_CYCLE)
                {
                    if(m_oddFrameFlag && m_backgroundEnabled) m_currentScanlineCycle++;
                    m_oddFrameFlag = !m_oddFrameFlag;
                }

            }
        }

        else if(m_currentScanline == FRAME_VBLANK_START_LINE)
        {
            //Reading one PPU clock before reads it as clear and never sets the flag or generates NMI for that frame.
            //Reading on the same PPU clock or one later reads it as set, clears it, and suppresses the NMI for that frame

            //1 cycle before -> m_lastPPUSTATUSReadCycle == VBLANK_CYCLE

            if(m_currentScanlineCycle == VBLANK_CYCLE) {
                if( m_lastPPUSTATUSReadCycle != VBLANK_CYCLE) m_VBlankHasStarted = true; //suppression?

            }
        }

        if(++m_currentScanlineCycle == 341) {

            m_lastPPUSTATUSReadCycle = -1;

            if(++m_currentScanline == FRAME_NUMBER_OF_LINES) {
                m_currentScanline = 0;      
            }

            m_currentScanlineCycle = 0;
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
            if(x < 32) return readPPUMemory(0x2000 + (y << 5) + x); //name table 0
            //0x2400 + y*32 + (x-32)
            else return readPPUMemory(0x2400 + (y << 5) + (x-32)); //name table 1
        }
        else //table 2 or 3
        {
            //0x2800 + (y-30)*32 + x
            if(x < 32) return readPPUMemory(0x2800 + ((y-30) << 5) + x); //name table 2
            //0x2C00 + (y-30)*32 + (x-32)
            else return readPPUMemory(0x2C00 + ((y-30) << 5) + (x-32)); //name table 3
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
        uint8_t highBitsColor = readPPUMemory(baseNameTableAddress+0x03C0 + (y >> 2) + (x >> 5) );

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

        //if(readPPUMemory(tileIndex*16 + pixelY) & (0x80>>pixelX)) colorIndex = 0x01; //plane 1
        if(readPPUMemory((tileIndex << 4) + pixelY) & (0x80>>pixelX)) colorIndex = 0x01; //plane 1

        //if(readPPUMemory(8 + tileIndex*16 + pixelY) & (0x80>>pixelX)) colorIndex |= 0x02; //plane 2
        if(readPPUMemory(8 + (tileIndex << 4) + pixelY) & (0x80>>pixelX)) colorIndex |= 0x02; //plane 2

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

        if(m_colorEmphasis&0x01) r += 0x30;
        if(m_colorEmphasis&0x02) g += 0x30;
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
        //if(m_powerUpCounter != 0) return;

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
        //if(m_powerUpCounter != 0) return;

        m_monochromeDisplay = (data&0x01) ? true : false;
        m_showBackgroundLeftmost8Pixels = (data&0x02) ? true : false;
        m_showSpritesLeftmost8Pixels = (data&0x04) ? true : false;
        m_backgroundEnabled = (data&0x08) ? true : false;
        m_spritesEnabled = (data&0x10) ? true : false;
        m_colorEmphasis = data >> 5;
    }

    GERANES_INLINE uint8_t readPPUSTATUS()
    {
        uint8_t ret = 0x00;

        m_lastPPUSTATUSReadCycle = m_currentScanlineCycle;

        if(m_VBlankHasStarted) ret |= 0x80;

        if(m_sprite0HitDelayed) ret |= 0x40;
        if(m_spriteOverflow) ret |= 0x20;

        m_VBlankHasStarted = false;
        m_reg_w = false;

        ret |= m_lastWrite & 0x1F; //5 least significant bits

        return ret;
    }

    GERANES_INLINE void writeOAMADDR(uint8_t data)
    {
        m_OAMAddr = data;
    }

    GERANES_INLINE uint8_t readOAMDATA()
    {
        const int test = 0;

        uint8_t ret = m_primaryOAM[m_OAMAddr];

        if (m_backgroundEnabled || m_spritesEnabled)
        {
            if (m_currentScanlineCycle < 64+test)
                ret = 0xFF;
            else if (m_currentScanlineCycle < 256+test)
                ret = 0xFF;
            else if (m_currentScanlineCycle < 320+test) { //Micro Machines relies on this
                //ret = 0xFF;
                int s = m_currentScanlineCycle - (256+test);
                int index = s/8;
                int offset = s%8;                

                if(index < m_spritesInThisLine) {

                    const uint8_t* currentSprite = &m_secondaryOAM[index << 2];

                    if(offset < 4) {
                        ret = *(currentSprite+offset);
                        //if(offset == 0) ret++; //sprite Y
                    }
                    else
                        ret = *(currentSprite+3); //sprite X
                }
                else if(index == m_spritesInThisLine) {

                    const uint8_t* currentSprite = &m_primaryOAM[63 << 2];

                    if(offset == 0) {
                        ret = *(currentSprite);
                        //ret++; //sprite Y
                    }
                    else
                        ret = 0xFF;
                    
                }
                else {
                    ret = 0xFF;
                }

            }
            else //and this:
                ret = m_secondaryOAM[0]; //first byte of secondary oam
                
        }




        //Third sprite bytes should be masked with $e3 on read
        if( (m_OAMAddr&0x03) == 2) ret &= 0xE3;

        //Address should not increment on $2004 read
        return ret;
    }

    GERANES_INLINE void writeOAMDATA(uint8_t data)
    {
        m_primaryOAM[m_OAMAddr] = data;
        m_OAMAddr++;
    }

    GERANES_INLINE void writePPUSCROLL(uint8_t data)
    {
        //if(m_powerUpCounter != 0) return;

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
        //if(m_powerUpCounter != 0) return;

        if(!m_reg_w)
        {
            m_reg_t = (m_reg_t & 0x80FF) | ((static_cast<uint16_t>(data&0x3F))  << 8);
        }
        else
        {
            m_reg_t = (m_reg_t & 0xFF00) | (static_cast<uint16_t>(data));

            //m_reg_t will be m_reg_v
            if(m_reg_t < 0x2000 && (m_reg_t&0x1000) && !(m_reg_v&0x1000)  ) //A12 rise
                m_cartridge.tickMapper();

            m_reg_v = m_reg_t;

            calculateDebugCursor();
        }

        m_reg_w = !m_reg_w;
    }

    GERANES_INLINE uint8_t readPPUDATA()
    {
        uint8_t ret;

        uint16_t oldV = m_reg_v;

        if(m_reg_v >= 0x3F00)
        {
            ret = readPPUMemory(m_reg_v);
            m_dataLatch = readPPUMemory(m_reg_v&0x2FFF);            
        }
        else
        {
            ret = m_dataLatch;
            m_dataLatch = readPPUMemory(m_reg_v);            
        }

        if( (m_spritesEnabled || m_backgroundEnabled) && m_currentScanline < 240) //rendering
        {
            if(m_VRAMAddressIncrement == 1) {
                incrementVY();
            }
            else incrementVX();

        }
        else m_reg_v+=m_VRAMAddressIncrement;

        m_reg_v &= 0x7FFF; //wrap 0x3FFF? burai fighter test = 0x7FFF

        if(m_reg_v < 0x2000 && (m_reg_v&0x1000) && !(oldV&0x1000) ) //A12 rise
            m_cartridge.tickMapper();

        return ret;
    }

    GERANES_INLINE void writePPUDATA(uint8_t data)
    {
        uint16_t oldV = m_reg_v;

        m_lastWrite = data;
        writePPUMemory(m_reg_v,data);

        if( (m_spritesEnabled || m_backgroundEnabled) && m_currentScanline < 240) //rendering
        {
            if(m_VRAMAddressIncrement == 1) {
                incrementVY();
            }
            else incrementVX();

        }
        else m_reg_v+=m_VRAMAddressIncrement;

        m_reg_v &= 0x7FFF; //wrap

        if(m_reg_v < 0x2000 && (m_reg_v&0x1000) && !(oldV&0x1000) ) //A12 rise
            m_cartridge.tickMapper();
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

    GERANES_INLINE void fetchNameTableByte() {
        m_nameTableByte = readPPUMemory(0x2000 | (m_reg_v & 0x0FFF));
    }

    GERANES_INLINE void fetchAttributeTableByte() {

        int address = 0x23C0 | (m_reg_v & 0x0C00) | ((m_reg_v >> 4) & 0x38) | ((m_reg_v >> 2) & 0x07);
        int shift = ((m_reg_v >> 4) & 4) | (m_reg_v & 2);
        m_attributeTableByte = ((readPPUMemory(address) >> shift) & 3) << 2;
    }

    GERANES_INLINE void fetchLowTileByte() {
        int fineY = (m_reg_v >> 12) & 7;
        int table = m_backgroundPatternTableAddress ? 0x1000 : 0x0000;
        int tile = m_nameTableByte;
        int address = table + (tile << 4) + fineY;
        m_lowTileByte = readPPUMemory(address);
    }

    GERANES_INLINE void fetchHighTileByte() {
        int fineY = (m_reg_v >> 12) & 7;
        int table = m_backgroundPatternTableAddress ? 0x1000 : 0x0000;
        int tile = m_nameTableByte;
        int address = table + (tile << 4) + fineY;
        m_highTileByte = readPPUMemory(address + 8);
    }

    GERANES_INLINE void storeTileData() {
        uint64_t data = 0;
        for(int i= 0; i < 8; i++) {
            int p1 = (m_lowTileByte & 0x80) >> 7;
            int p2 = (m_highTileByte & 0x80) >> 6;
            m_lowTileByte <<= 1;
            m_highTileByte <<= 1;
            data <<= 4;
            data |= uint32_t(m_attributeTableByte | p1 | p2);
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
        SERIALIZEDATA(s, m_currentScanline);
        SERIALIZEDATA(s, m_currentScanlineCycle);

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
        SERIALIZEDATA(s, m_sprite0HitDelayed);
        SERIALIZEDATA(s, m_spriteOverflow);       

        SERIALIZEDATA(s, m_spritesInThisLine);
        SERIALIZEDATA(s, m_testSprite0HitInThisLine);

        SERIALIZEDATA(s, m_OAMAddr);
        SERIALIZEDATA(s, m_currentY);
        SERIALIZEDATA(s, m_currentX);

        SERIALIZEDATA(s, m_interruptFlag);

        SERIALIZEDATA(s, m_oddFrameFlag);

        s.array(reinterpret_cast<uint8_t*>(m_nameTable), 1, sizeof(m_nameTable)); //4 pages

        s.array(m_palette, 1, sizeof(m_palette));
        s.array(m_primaryOAM, 1, sizeof(m_primaryOAM));
        s.array(m_secondaryOAM, 1, sizeof(m_secondaryOAM));

        SERIALIZEDATA(s, m_reg_v);
        SERIALIZEDATA(s, m_reg_x);
        SERIALIZEDATA(s, m_reg_t);
        SERIALIZEDATA(s, m_reg_w);

        SERIALIZEDATA(s, m_dataLatch);
        SERIALIZEDATA(s, m_lastWrite);        

        SERIALIZEDATA(s, m_nameTableByte);
        SERIALIZEDATA(s, m_attributeTableByte);
        SERIALIZEDATA(s, m_lowTileByte);
        SERIALIZEDATA(s, m_highTileByte);
        SERIALIZEDATA(s, m_tileData);

        SERIALIZEDATA(s, m_lastPPUSTATUSReadCycle);

        SERIALIZEDATA(s, m_openBus);
        s.array(m_openBusTimer, 1, 8);

        SERIALIZEDATA(s, m_powerUpCounter);

        SERIALIZEDATA(s, FRAME_NUMBER_OF_LINES);
        SERIALIZEDATA(s, FRAME_VBLANK_START_LINE);
        SERIALIZEDATA(s, FRAME_VBLANK_END_LINE);
        SERIALIZEDATA(s, m_PALFlag);
        SERIALIZEDATA(s, m_PALCounter);
        SERIALIZEDATA(s, m_inOverclockLines);
        SERIALIZEDATA(s,m_preLine);
        SERIALIZEDATA(s,m_visibleLine);
        SERIALIZEDATA(s,m_renderLine);

        SERIALIZEDATA(s,m_overclockFrame);
    }

};

#endif
