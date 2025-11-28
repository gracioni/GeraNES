#ifndef CPU2A03_H
#define CPU2A03_H

#include "defines.h"
#include "IBus.h"
#include "Serialization.h"

#include "Console.h"
#include "PPU.h"
#include "APU/APU.h"
#include "Cartridge.h"

class DMA;

#include "signal/SigSlot.h"

const uint8_t DO_NOT_POOL_INTS = 0xFF;

#ifdef UNUSED_TABLES

static const uint8_t OPCODE_CYCLES_TABLE[256] =
{
/*0x00*/ 7,6,2,8,3,3,5,5,3,2,2,2,4,4,6,6,
/*0x10*/ 2,5,2,8,4,4,6,6,2,4,2,7,4,4,7,7,
/*0x20*/ 6,6,2,8,3,3,5,5,4,2,2,2,4,4,6,6,
/*0x30*/ 2,5,2,8,4,4,6,6,2,4,2,7,4,4,7,7,
/*0x40*/ 6,6,2,8,3,3,5,5,3,2,2,2,3,4,6,6,
/*0x50*/ 2,5,2,8,4,4,6,6,2,4,2,7,4,4,7,7,
/*0x60*/ 6,6,2,8,3,3,5,5,4,2,2,2,5,4,6,6,
/*0x70*/ 2,5,2,8,4,4,6,6,2,4,2,7,4,4,7,7,
/*0x80*/ 2,6,2,6,3,3,3,3,2,2,2,2,4,4,4,4,
/*0x90*/ 2,6,2,6,4,4,4,4,2,5,2,5,5,5,5,5,
/*0xA0*/ 2,6,2,6,3,3,3,3,2,2,2,2,4,4,4,4,
/*0xB0*/ 2,5,2,5,4,4,4,4,2,4,2,4,4,4,4,4,
/*0xC0*/ 2,6,2,8,3,3,5,5,2,2,2,2,4,4,6,6,
/*0xD0*/ 2,5,2,8,4,4,6,6,2,4,2,7,4,4,7,7,
/*0xE0*/ 2,6,2,8,3,3,5,5,2,2,2,2,4,4,6,6,
/*0xF0*/ 2,5,2,8,4,4,6,6,2,4,2,7,4,4,7,7,
};

static const uint8_t OPCODE_EXTRA_CYCLE_ON_PAGE_CROSS[256] =
{
//       0 1 2 3 4 5 6 7 8 9 A B C D E F
/*0x00*/ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
/*0x10*/ 0,1,0,0,0,0,0,0,0,1,0,0,1,1,0,0,
/*0x20*/ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
/*0x30*/ 0,1,0,0,0,0,0,0,0,1,0,0,1,1,0,0,
/*0x40*/ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
/*0x50*/ 0,1,0,0,0,0,0,0,0,1,0,0,1,1,0,0,
/*0x60*/ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
/*0x70*/ 0,1,0,0,0,0,0,0,0,1,0,0,1,1,0,0,
/*0x80*/ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
/*0x90*/ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
/*0xA0*/ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
/*0xB0*/ 0,1,0,1,0,0,0,0,0,1,0,1,1,1,1,1,
/*0xC0*/ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
/*0xD0*/ 0,1,0,0,0,0,0,0,0,1,0,0,1,1,0,0,
/*0xE0*/ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
/*0xF0*/ 0,1,0,0,0,0,0,0,0,1,0,0,1,1,0,0,
};

static const uint8_t OPCODE_WRITE_CYCLES_TABLE[256] =
{
/*0x00*/ 0x3C, 0x00, 0x00, 0xC0, 0x00, 0x00, 0x18, 0x18, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x30,
/*0x10*/ 0x00, 0x00, 0x00, 0xC0, 0x00, 0x00, 0x30, 0x30, 0x00, 0x00, 0x00, 0x60, 0x00, 0x00, 0x60, 0x60,
/*0x20*/ 0x30, 0x00, 0x00, 0xC0, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x30,
/*0x30*/ 0x00, 0x00, 0x00, 0xC0, 0x00, 0x00, 0x30, 0x30, 0x00, 0x00, 0x00, 0x60, 0x00, 0x00, 0x60, 0x60,
/*0x40*/ 0x00, 0x00, 0x00, 0xC0, 0x00, 0x00, 0x18, 0x18, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x30,
/*0x50*/ 0x00, 0x00, 0x00, 0xC0, 0x00, 0x00, 0x30, 0x30, 0x00, 0x00, 0x00, 0x60, 0x00, 0x00, 0x60, 0x60,
/*0x60*/ 0x00, 0x00, 0x00, 0xC0, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x30,
/*0x70*/ 0x00, 0x00, 0x00, 0xC0, 0x00, 0x00, 0x30, 0x30, 0x00, 0x00, 0x00, 0x60, 0x00, 0x00, 0x60, 0x60,
/*0x80*/ 0x00, 0x20, 0x00, 0x20, 0x04, 0x04, 0x04, 0x04, 0x00, 0x00, 0x00, 0x00, 0x08, 0x08, 0x08, 0x08,
/*0x90*/ 0x00, 0x20, 0x00, 0x20, 0x08, 0x08, 0x08, 0x08, 0x00, 0x10, 0x00, 0x10, 0x10, 0x10, 0x10, 0x10,
/*0xA0*/ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/*0xB0*/ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/*0xC0*/ 0x00, 0x00, 0x00, 0xC0, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x30,
/*0xD0*/ 0x00, 0x00, 0x00, 0xC0, 0x00, 0x00, 0x30, 0x30, 0x00, 0x00, 0x00, 0x60, 0x00, 0x00, 0x60, 0x60,
/*0xE0*/ 0x00, 0x00, 0x00, 0xC0, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x30,
/*0xF0*/ 0x00, 0x00, 0x00, 0xC0, 0x00, 0x00, 0x30, 0x30, 0x00, 0x00, 0x00, 0x60, 0x00, 0x00, 0x60, 0x60
};

#endif

// BRK (NO_POOL)
// CLI(0x58) SEI(0x78) PLP(0x28) in cycle 1
// some references (https://www.nesdev.org/wiki/CPU_interrupts) says that branch instructions
// 0x90 0xB0 0xF0 0x30 0xD0 0x10 0x50 0x70 are polled in cycle 1
// and all 2 cycles instructions are polled in cycle 1
static const uint8_t OPCODE_INT_POOL_CYCLE_TABLE[256] =
{
/*0x00*/ DO_NOT_POOL_INTS,5,1,7,2,2,4,4,2,1,1,1,3,3,5,5,
/*0x10*/ 1,4,1,7,3,3,5,5,1,3,1,6,3,3,6,6,
/*0x20*/ 5,5,1,7,2,2,4,4,1,1,1,1,3,3,5,5,
/*0x30*/ 1,4,1,7,3,3,5,5,1,3,1,6,3,3,6,6,
/*0x40*/ 5,5,1,7,2,2,4,4,2,1,1,1,2,3,5,5,
/*0x50*/ 1,4,1,7,3,3,5,5,1,3,1,6,3,3,6,6,
/*0x60*/ 5,5,1,7,2,2,4,4,3,1,1,1,4,3,5,5,
/*0x70*/ 1,4,1,7,3,3,5,5,1,3,1,6,3,3,6,6,
/*0x80*/ 1,5,1,5,2,2,2,2,1,1,1,1,3,3,3,3,
/*0x90*/ 1,5,1,5,3,3,3,3,1,4,1,4,4,4,4,4,
/*0xA0*/ 1,5,1,5,2,2,2,2,1,1,1,1,3,3,3,3,
/*0xB0*/ 1,4,1,4,3,3,3,3,1,3,1,3,3,3,3,3,
/*0xC0*/ 1,5,1,7,2,2,4,4,1,1,1,1,3,3,5,5,
/*0xD0*/ 1,4,1,7,3,3,5,5,1,3,1,6,3,3,6,6,
/*0xE0*/ 1,5,2,7,2,2,4,4,1,1,1,1,3,3,5,5,
/*0xF0*/ 1,4,1,7,3,3,5,5,1,3,1,6,3,3,6,6,
};

enum class AddrMode
{
    None, Acc, Imp, Imm, Rel,
    Zero, Abs, ZeroX, ZeroY,
    Ind, IndX, IndY, IndYW,
    AbsX, AbsXW, AbsY, AbsYW
};

typedef AddrMode M;

static AddrMode addrMode[] = {
//          0           1               2           3               4               5               6               7               8           9           A           B           C           D           E           F
/*0x00*/    M::Imp,     M::IndX,        M::None,    M::IndX,        M::Zero,        M::Zero,        M::Zero,        M::Zero,        M::Imp,     M::Imm,     M::Acc,     M::Imm,     M::Abs,     M::Abs,     M::Abs,     M::Abs,
/*0x10*/    M::Rel,     M::IndY,        M::None,    M::IndYW,       M::ZeroX,       M::ZeroX,       M::ZeroX,       M::ZeroX,       M::Imp,     M::AbsY,    M::Imp,     M::AbsYW,   M::AbsX,    M::AbsX,    M::AbsXW,   M::AbsXW,
/*0x20*/    M::Abs,     M::IndX,        M::None,    M::IndX,        M::Zero,        M::Zero,        M::Zero,        M::Zero,        M::Imp,     M::Imm,     M::Acc,     M::Imm,     M::Abs,     M::Abs,     M::Abs,     M::Abs,
/*0x30*/    M::Rel,     M::IndY,        M::None,    M::IndYW,       M::ZeroX,       M::ZeroX,       M::ZeroX,       M::ZeroX,       M::Imp,     M::AbsY,    M::Imp,     M::AbsYW,   M::AbsX,    M::AbsX,    M::AbsXW,   M::AbsXW,
/*0x40*/    M::Imp,     M::IndX,        M::None,    M::IndX,        M::Zero,        M::Zero,        M::Zero,        M::Zero,        M::Imp,     M::Imm,     M::Acc,     M::Imm,     M::Abs,     M::Abs,     M::Abs,     M::Abs,
/*0x50*/    M::Rel,     M::IndY,        M::None,    M::IndYW,       M::ZeroX,       M::ZeroX,       M::ZeroX,       M::ZeroX,       M::Imp,     M::AbsY,    M::Imp,     M::AbsYW,   M::AbsX,    M::AbsX,    M::AbsXW,   M::AbsXW,
/*0x60*/    M::Imp,     M::IndX,        M::None,    M::IndX,        M::Zero,        M::Zero,        M::Zero,        M::Zero,        M::Imp,     M::Imm,     M::Acc,     M::Imm,     M::Ind,     M::Abs,     M::Abs,     M::Abs,
/*0x70*/    M::Rel,     M::IndY,        M::None,    M::IndYW,       M::ZeroX,       M::ZeroX,       M::ZeroX,       M::ZeroX,       M::Imp,     M::AbsY,    M::Imp,     M::AbsYW,   M::AbsX,    M::AbsX,    M::AbsXW,   M::AbsXW,
/*0x80*/    M::Imm,     M::IndX,        M::Imm,     M::IndX,        M::Zero,        M::Zero,        M::Zero,        M::Zero,        M::Imp,     M::Imm,     M::Imp,     M::Imm,     M::Abs,     M::Abs,     M::Abs,     M::Abs,
/*0x90*/    M::Rel,     M::IndYW,       M::None,    M::IndYW,       M::ZeroX,       M::ZeroX,       M::ZeroY,       M::ZeroY,       M::Imp,     M::AbsYW,   M::Imp,     M::AbsYW,   M::AbsXW,   M::AbsXW,   M::AbsYW,   M::AbsYW,
/*0xA0*/    M::Imm,     M::IndX,        M::Imm,     M::IndX,        M::Zero,        M::Zero,        M::Zero,        M::Zero,        M::Imp,     M::Imm,     M::Imp,     M::Imm,     M::Abs,     M::Abs,     M::Abs,     M::Abs,
/*0xB0*/    M::Rel,     M::IndY,        M::None,    M::IndY,        M::ZeroX,       M::ZeroX,       M::ZeroY,       M::ZeroY,       M::Imp,     M::AbsY,    M::Imp,     M::AbsY,    M::AbsX,    M::AbsX,    M::AbsY,    M::AbsY,
/*0xC0*/    M::Imm,     M::IndX,        M::Imm,     M::IndX,        M::Zero,        M::Zero,        M::Zero,        M::Zero,        M::Imp,     M::Imm,     M::Imp,     M::Imm,     M::Abs,     M::Abs,     M::Abs,     M::Abs,
/*0xD0*/    M::Rel,     M::IndY,        M::None,    M::IndYW,       M::ZeroX,       M::ZeroX,       M::ZeroX,       M::ZeroX,       M::Imp,     M::AbsY,    M::Imp,     M::AbsYW,   M::AbsX,    M::AbsX,    M::AbsXW,   M::AbsXW,
/*0xE0*/    M::Imm,     M::IndX,        M::Imm,     M::IndX,        M::Zero,        M::Zero,        M::Zero,        M::Zero,        M::Imp,     M::Imm,     M::Imp,     M::Imm,     M::Abs,     M::Abs,     M::Abs,     M::Abs,
/*0xF0*/    M::Rel,     M::IndY,        M::None,    M::IndYW,       M::ZeroX,       M::ZeroX,       M::ZeroX,       M::ZeroX,       M::Imp,     M::AbsY,    M::Imp,     M::AbsYW,   M::AbsX,    M::AbsX,    M::AbsXW,   M::AbsXW,
};



class CPU2A03
{
private:

    const uint16_t NMI_VECTOR = 0xFFFA;
    const uint16_t IRQ_VECTOR = 0xFFFE;
    const uint16_t BRK_VECTOR = 0xFFFE;

    Ibus& m_bus;
    Console& m_console;

    uint16_t m_pc;
    uint8_t m_sp;
    uint8_t m_a;
    uint8_t m_x;
    uint8_t m_y;

    union {
        uint8_t m_status;
        struct {
            bool m_carryFlag : 1;
            bool m_zeroFlag : 1;
            bool m_intFlag : 1;
            bool m_decimalFlag : 1;
            bool m_brkFlag : 1;
            bool m_unusedFlag : 1;
            bool m_overflowFlag : 1;
            bool m_negativeFlag : 1;
        };
    };

    unsigned int m_cyclesCounter;
    uint8_t m_opcode;
    uint16_t m_addr;

    bool m_nmiSignal;
    bool m_irqSignal;

    enum class NmiStep {WAITING_HIGH, WAITING_LOW, OK};

    NmiStep m_nmiStep;
    bool m_irqStep;

    enum class Interrupt {NONE, NMI, IRQ} m_interrupt;

    int m_haltCycles;

    int m_extraCycles;

    int m_currentInstructionCycle;

    uint8_t m_poolIntsAtCycle;

    //Do not serialize variables below

    int m_runCount;

    bool m_writeCycle;

    GERANES_INLINE_HOT uint16_t MAKE16(uint8_t low, uint8_t high)
    {
        return ((uint16_t)low) | (((uint16_t)high)<<8);
    }

    GERANES_INLINE_HOT void push(uint8_t data)
    {
        writeMemory(0x0100 + m_sp, data);
        m_sp--;
    }

    GERANES_INLINE_HOT uint8_t pull()
    {
        m_sp++;
        return readMemory(0x0100 + m_sp);
    }

    GERANES_INLINE_HOT void push16(uint16_t data)
    {
        push((uint8_t)(data >> 8));
        push((uint8_t)(data & 0xFF));
    }

    GERANES_INLINE_HOT uint16_t pull16()
    {
        const uint8_t low = pull();
        const uint8_t high = pull();

        return MAKE16(low,high);
    }

    GERANES_INLINE_HOT void updateZeroAndNegativeFlags(uint8_t value) {
        m_negativeFlag = value&0x80;
        m_zeroFlag = value==0x00;
    }

    GERANES_INLINE_HOT void dummyRead() {
        readMemory(m_pc);
    }

    GERANES_INLINE_HOT void getAddrImediate()
    {
        m_addr = m_pc++;
    }

    GERANES_INLINE_HOT void getAddrAbsolute()
    {
        const uint8_t low = readMemory(m_pc++);
        const uint8_t high = readMemory(m_pc++);

        m_addr = MAKE16(low, high);
    }

    GERANES_INLINE_HOT void getAddrZeroPage()
    {
        m_addr = readMemory(m_pc++);
    }

    GERANES_INLINE_HOT void getAddrAbsoluteX(bool dummyRead = true)
    {
        const uint8_t low = readMemory(m_pc++);
        const uint8_t high = readMemory(m_pc++);

        m_addr = MAKE16(low, high); 

        const bool pageCross = (m_addr ^ (m_addr+m_x)) & 0xFF00;

        //dummy read
        if(pageCross || dummyRead) readMemory( (m_addr & 0xFF00) | ((m_addr+m_x) & 0xFF) );

        m_addr += m_x;
    }

    GERANES_INLINE_HOT void getAddrAbsoluteY(bool dummyRead = true)
    {
        const uint8_t low = readMemory(m_pc++);
        const uint8_t high = readMemory(m_pc++);

        m_addr = MAKE16(low, high);

        const bool pageCross = (m_addr ^ (m_addr+m_y)) & 0xFF00;

        if(pageCross || dummyRead) readMemory( (m_addr & 0xFF00) | ((m_addr+m_y) & 0xFF) );
        
        m_addr += m_y;
    }

    GERANES_INLINE_HOT void getAddrZeroPageX()
    {
        m_addr = readMemory(m_pc++);
        readMemory(m_addr); //dummy read
        m_addr = (uint8_t)(m_addr+m_x);
    }


    GERANES_INLINE_HOT void getAddrZeroPageY()
    {
        m_addr = readMemory(m_pc++);
        readMemory(m_addr); //dummy read
        m_addr = (uint8_t)(m_addr+m_y);
    }

    GERANES_INLINE_HOT void getAddrIndirect()
    {
        const uint8_t low = readMemory(m_pc++);
        const uint8_t high = readMemory(m_pc++);

        const uint8_t iLow = readMemory(MAKE16(low,high));
        const uint8_t iHigh = readMemory(MAKE16(low+1,high));

        m_addr = MAKE16(iLow, iHigh);
    }

    GERANES_INLINE_HOT void getAddrIndirectX()
    {
        uint8_t value = readMemory(m_pc++);
        readMemory(value); //dummy read
        value += m_x;

        const uint8_t iLow = readMemory(value);
        const uint8_t iHigh = readMemory((uint8_t)(value+1));

        m_addr = MAKE16(iLow, iHigh); 
    }

    GERANES_INLINE_HOT void getAddrIndirectY(bool dummyRead = true)
    {
        const uint8_t value = readMemory(m_pc++);

        const uint8_t low = readMemory(value);
        const uint8_t high = readMemory((uint8_t)(value+1));

        m_addr = MAKE16(low, high);

        const bool pageCross = (m_addr ^ (m_addr+m_y)) & 0xFF00;

        //dummy read
        if(pageCross || dummyRead) readMemory( (m_addr & 0xFF00) | ((m_addr+m_y) & 0xFF) );

        m_addr += m_y;
    }

    GERANES_INLINE_HOT void getAddrRelative()
    {
        m_addr = m_pc++;
    }

    GERANES_INLINE_HOT void phi1() {

        if(m_nmiStep == NmiStep::OK) {
            m_nmiSignal = true;
            m_nmiStep = NmiStep::WAITING_LOW;
        }

        m_irqSignal = m_irqStep;
    }


public:

    SigSlot::Signal<const std::string&> signalError;

    CPU2A03(Ibus& bus, Console& console) : m_bus(bus), m_console(console)   {
    }

    void init()
    {
        const uint8_t low = readMemory(0xFFFC);
        const uint8_t high = readMemory(0xFFFD);

        m_pc = MAKE16(low, high); //reset vector

        m_sp = 0xFD;
        m_status = 0x24;

        m_cyclesCounter = 0;

        m_addr = 0;
        m_opcode = 0;

        m_a = 0;
        m_x = 0;
        m_y = 0;

        m_nmiSignal = false;
        m_irqSignal = false;

        m_nmiStep = NmiStep::WAITING_LOW;
        m_irqStep = false;

        m_poolIntsAtCycle = -1;

        m_haltCycles = 0;
        m_extraCycles = 0;

        m_runCount = 0;
        m_writeCycle = false;

        m_currentInstructionCycle = 0;
        m_interrupt = Interrupt::NONE;

    }    

    GERANES_INLINE_HOT void ADC()
    {
        const uint8_t value = readMemory(m_addr);

        unsigned int result = (unsigned int)m_a + value + (m_carryFlag?1:0);

        m_carryFlag = result>0xFF;        
        m_overflowFlag = ( !((m_a ^ value) & 0x80) && ((m_a ^ result) & 0x80) );

        m_a = (uint8_t)result;

        updateZeroAndNegativeFlags(m_a);
    }

    GERANES_INLINE_HOT void AND()
    {
        m_a &= readMemory(m_addr);
        updateZeroAndNegativeFlags(m_a);
    }

    GERANES_INLINE_HOT void ASL()
    {
        uint8_t value = readMemory(m_addr);
        writeMemory(m_addr, value); //dummy write

        m_carryFlag = value & 0x80;

        value <<= 1;

        updateZeroAndNegativeFlags(value);

        writeMemory(m_addr, value);
    }

    GERANES_INLINE_HOT void ASL_implied()
    {
        dummyRead();
        m_carryFlag = (m_a&0x80);
        m_a <<= 1;
        updateZeroAndNegativeFlags(m_a);       
    }

    GERANES_INLINE_HOT void AAC()
	{
        const uint8_t value = readMemory(m_addr);
        m_a &= value;

        updateZeroAndNegativeFlags(m_a);

        m_carryFlag = m_negativeFlag;        
	}

    GERANES_INLINE_HOT void U_SLO()
    {
        //ASL & ORA

        uint8_t value = readMemory(m_addr);
        writeMemory(m_addr, value); //dummy write

        m_carryFlag = value & 0x80;

        value <<= 1;        
        
        m_a = m_a | value;

        updateZeroAndNegativeFlags(m_a);

        writeMemory(m_addr, value);
    }

    GERANES_INLINE_HOT void branch(bool condition) {

        int8_t offset = readMemory(m_addr);

        if(condition)
        {
            const uint16_t value = m_pc;
            m_pc += offset;

            dummyRead();

            if( (value ^ m_pc) & 0xFF00 ) {                
                m_poolIntsAtCycle = 3;
                dummyRead();
            }
            
        }   

    }

    GERANES_INLINE_HOT void BCC()
    {
        branch(!m_carryFlag);   
    }

    GERANES_INLINE_HOT void BCS()
    {
        branch(m_carryFlag); 
    }

    GERANES_INLINE_HOT void BEQ()
    {
        branch(m_zeroFlag);
    }

    GERANES_INLINE_HOT void BIT()
    {
        const uint8_t value = readMemory(m_addr);

        m_zeroFlag = (m_a & value) == 0x00;
        m_negativeFlag = value & 0x80;
        m_overflowFlag = value & 0x40;
    }

    GERANES_INLINE_HOT void BMI()
    {
        branch(m_negativeFlag);
    }

    GERANES_INLINE_HOT void BNE()
    {
        branch(!m_zeroFlag);
    }

    GERANES_INLINE_HOT void BPL()
    {
        branch(!m_negativeFlag);
    }

    GERANES_INLINE_HOT void BRK()
    {
        emulateInterruptSequence(true);      
    }

    GERANES_INLINE_HOT void BVC()
    {
        branch(!m_overflowFlag);
    }

    GERANES_INLINE_HOT void BVS()
    {
        branch(m_overflowFlag);
    }

    GERANES_INLINE_HOT void CLC()
    {
        dummyRead();
        m_carryFlag = false;       
    }

    GERANES_INLINE_HOT void CLD()
    {
        dummyRead();
        m_decimalFlag = false;        
    }

    GERANES_INLINE_HOT void CLI()
    {
        dummyRead();
        m_intFlag = false; 
    }

    GERANES_INLINE_HOT void CLV()
    {
        dummyRead();
        m_overflowFlag = false;
    }

    GERANES_INLINE_HOT void CMP()
    {
        uint8_t value = readMemory(m_addr);

        m_carryFlag = m_a >= value;

        value = m_a - value;

        updateZeroAndNegativeFlags(value);
    }

    GERANES_INLINE_HOT void CPX()
    {
        uint8_t value = readMemory(m_addr);

        m_carryFlag = m_x >= value;

        value = m_x - value;

        updateZeroAndNegativeFlags(value);
    }

    GERANES_INLINE_HOT void CPY()
    {
        uint8_t value = readMemory(m_addr);

        m_carryFlag = m_y >= value;

        value = m_y - value;

        updateZeroAndNegativeFlags(value);
    }

    GERANES_INLINE_HOT void DEC()
    {
        uint8_t value = readMemory(m_addr);
        writeMemory(m_addr, value); //dummy write

        value--;

        updateZeroAndNegativeFlags(value);

        writeMemory(m_addr, value);
    }

    GERANES_INLINE_HOT void U_DCP()
	{
		//DEC & CMP

        //DEC
		uint8_t value = readMemory(m_addr);
        writeMemory(m_addr, value); //dummy write
		value--;

        writeMemory(m_addr, value);
		
        //CMP
        m_carryFlag = m_a >= value;
        value = m_a - value;
        updateZeroAndNegativeFlags(value);		
	}

    GERANES_INLINE_HOT void DEX()
    {
        dummyRead();
        m_x--;
        updateZeroAndNegativeFlags(m_x);        
    }

    void U_AXS()
	{
		//CMP & DEX
		const uint8_t value = readMemory(m_addr);
		uint8_t result = (m_a & m_x) - value;
		
		m_carryFlag = (m_a & m_x) >= value;

        m_x = result;
        updateZeroAndNegativeFlags(m_x);
	}

    GERANES_INLINE_HOT void DEY()
    {
        dummyRead();
        m_y--;
        updateZeroAndNegativeFlags(m_y);    
    }

    GERANES_INLINE_HOT void EOR()
    {
        m_a ^= readMemory(m_addr);
        updateZeroAndNegativeFlags(m_a);
    }

    GERANES_INLINE_HOT  void INC()
    {
        uint8_t value = readMemory(m_addr);
        writeMemory(m_addr, value); //dummy write

        value++;

        updateZeroAndNegativeFlags(value);

        writeMemory(m_addr,value);
    } 

    GERANES_INLINE_HOT void U_ISB()
    {
        //INC & SBC
        uint8_t value = readMemory(m_addr);
        writeMemory(m_addr, value); //dummy write        
        value++;

        //ADC
        unsigned int result = (unsigned int)m_a + (value ^ 0xFF) + (m_carryFlag?1:0);        
        m_carryFlag = result>0xFF;        
        m_overflowFlag = ( !((m_a ^ (value ^ 0xFF)) & 0x80) && ((m_a ^ result) & 0x80) );

        m_a = (uint8_t)result;

        updateZeroAndNegativeFlags(m_a);

        writeMemory(m_addr, value);      
    }

    GERANES_INLINE_HOT void INX()
    {
        dummyRead();
        m_x++;
        updateZeroAndNegativeFlags(m_x);        
    }

    GERANES_INLINE_HOT void INY()
    {
        dummyRead();
        m_y++;
        updateZeroAndNegativeFlags(m_y);                
    }

    GERANES_INLINE_HOT void JMP()
    {
        m_pc = m_addr;        
    }

    GERANES_INLINE_HOT void JSR()
    {
        dummyRead();
        push16(m_pc-1);
        m_pc = m_addr;
    }

    GERANES_INLINE_HOT void LDA()
    {
        m_a = readMemory(m_addr);
        updateZeroAndNegativeFlags(m_a);
    }

    GERANES_INLINE_HOT void U_LAX()
	{
		//LDA & LDX
		m_a = readMemory(m_addr);
        updateZeroAndNegativeFlags(m_a);
		m_x = m_a;
	}

    GERANES_INLINE_HOT void LDX()
    {
        m_x = readMemory(m_addr);
        updateZeroAndNegativeFlags(m_x);
    }

    GERANES_INLINE_HOT void LDY()
    {
        m_y = readMemory(m_addr);
        updateZeroAndNegativeFlags(m_y);
    }

    GERANES_INLINE_HOT void LSR()
    {
        uint8_t value = readMemory(m_addr);
        writeMemory(m_addr, value); //dummy write        
        m_carryFlag = value & 0x01;
        value >>= 1;
        updateZeroAndNegativeFlags(value);
        writeMemory(m_addr, value);
    }

    GERANES_INLINE_HOT void LSR_implied()
    {
        dummyRead();
        m_carryFlag = m_a & 0x01;
        m_a >>= 1;
        updateZeroAndNegativeFlags(m_a);       
    }

    GERANES_INLINE_HOT void U_ASR()
	{
        const uint8_t value = readMemory(m_addr);

        m_a &= value;

		m_carryFlag = m_a & 0x01;

        m_a >>= 1;

        updateZeroAndNegativeFlags(m_a);		
	}

    GERANES_INLINE_HOT void U_SRE()
	{
		//ROL & AND
		uint8_t value = readMemory(m_addr);
		writeMemory(m_addr, value); //dummy write

        m_carryFlag = value & 0x01;
        value >>= 1;              

        m_a ^= value;
        updateZeroAndNegativeFlags(m_a); 

		writeMemory(m_addr, value);
	}

    GERANES_INLINE_HOT void NOP()
    {
        readMemory(m_addr);
    }

    GERANES_INLINE_HOT void U_DOP()
    {
        readMemory(m_addr);     
    }

    GERANES_INLINE_HOT void ORA()
    {
        m_a |= readMemory(m_addr);
        updateZeroAndNegativeFlags(m_a);
    }

    GERANES_INLINE_HOT void PHA()
    {
        dummyRead();
        push(m_a);       
    }

    GERANES_INLINE_HOT void PHP()
    {
        dummyRead();
        m_brkFlag = true;
        m_unusedFlag = true;
        push(m_status);        
    }

    GERANES_INLINE_HOT void PLA()
    {
        dummyRead();
        dummyRead(); //GERA REMOVER timing erro
        m_a = pull();
        updateZeroAndNegativeFlags(m_a);       
    }

    GERANES_INLINE_HOT void PLP()
    {
        dummyRead();
        dummyRead();
        m_status = pull();        
    }

    GERANES_INLINE_HOT void ROL()
    {
        uint8_t value = readMemory(m_addr);
        writeMemory(m_addr, value); //dummy write

        bool outputCarry = value & 0x80;

        value <<= 1;
        if(m_carryFlag) value |= 0x01;

        m_carryFlag = outputCarry;
        updateZeroAndNegativeFlags(value);

        writeMemory(m_addr, value);
    }

    GERANES_INLINE_HOT void ROL_implied()
    {
        dummyRead();

        bool outputCarry = m_a&0x80;

        m_a <<= 1;
        if(m_carryFlag) m_a |= 0x01;

        m_carryFlag = outputCarry;
        updateZeroAndNegativeFlags(m_a);        
    }

    GERANES_INLINE_HOT void U_RLA()
	{
		//LSR & EOR

		uint8_t value = readMemory(m_addr);
        writeMemory(m_addr, value); //dummy write

        //ROL
		bool outputCarry = value & 0x80;

        value <<= 1;
        if(m_carryFlag) value |= 0x01;

        m_carryFlag = outputCarry;        

		m_a &= value;

        updateZeroAndNegativeFlags(m_a);

		writeMemory(m_addr, value);
	}

    GERANES_INLINE_HOT void ROR()
    {
        uint8_t value = readMemory(m_addr);
        writeMemory(m_addr, value); //dummy write

        bool outputCarry = value & 0x01;

        value >>= 1;
        if(m_carryFlag) value |= 0x80;

        m_carryFlag = outputCarry;
        updateZeroAndNegativeFlags(value);

        writeMemory(m_addr,value);
    }

    GERANES_INLINE_HOT void ROR_implied()
    {
        dummyRead();

        bool outputCarry = m_a&0x01;

        m_a >>= 1;
        if(m_carryFlag) m_a |= 0x80;

        m_carryFlag = outputCarry;
        updateZeroAndNegativeFlags(m_a);       
    }

    void U_ARR()
	{
        const uint8_t value = readMemory(m_addr);

        m_a = ((m_a & value) >> 1) | (m_carryFlag ? 0x80 : 0x00);
        updateZeroAndNegativeFlags(m_a);
        m_carryFlag = m_a & 0x40;
        m_overflowFlag = (m_carryFlag ? 0x01 : 0x00) ^ ((m_a >> 5) & 0x01);
	}

    GERANES_INLINE_HOT void U_RRA()
    {
        //ROR & ADC

        //ROR
        uint8_t value = readMemory(m_addr);
        writeMemory(m_addr, value); //dummy write

        bool outputCarry = value & 0x01;

        value >>= 1;
        if(m_carryFlag) value |= 0x80;

        m_carryFlag = outputCarry;

        //ADC
        unsigned int result = (unsigned int)m_a + value + (m_carryFlag?1:0);
        m_carryFlag = result>0xFF;        
        m_overflowFlag = ( !((m_a ^ value) & 0x80) && ((m_a ^ result) & 0x80) );
        
        m_a = (uint8_t)result;

        updateZeroAndNegativeFlags(m_a);     

        writeMemory(m_addr, value);
    }

    GERANES_INLINE_HOT void RTI()
    {        
        dummyRead();
        dummyRead();
        m_status = pull();
        m_pc = pull16();
    }

    GERANES_INLINE_HOT void RTS()
    {       
        dummyRead(); 
        dummyRead();
        dummyRead();
        m_pc = pull16()+1;        
    }

    GERANES_INLINE_HOT void SBC()
    {
        const uint8_t value = readMemory(m_addr);

        const unsigned int result = (unsigned int)m_a - value - (m_carryFlag?0:1);

        m_carryFlag = (result < 0x100);
        m_overflowFlag = (((m_a ^ result) & 0x80) && ((m_a ^ value) & 0x80));

        m_a = result & 0xFF;

        updateZeroAndNegativeFlags(m_a);
    }

    GERANES_INLINE_HOT void U_HLT()
    {
    }

    GERANES_INLINE_HOT void SEC()
    {
        dummyRead();
        m_carryFlag = true;       
    }

    GERANES_INLINE_HOT void SED()
    {
        dummyRead();
        m_decimalFlag = true;        
    }

    GERANES_INLINE_HOT void SEI()
    {
        dummyRead();
        m_intFlag = true;        
    }

    GERANES_INLINE_HOT void STA()
    {
        writeMemory(m_addr, m_a);
    }

    GERANES_INLINE_HOT void STX()
    {
        writeMemory(m_addr, m_x);
    }

    GERANES_INLINE_HOT void U_SAX()
	{
		//STA & STX
        writeMemory(m_addr, m_a & m_x);
	}

    GERANES_INLINE_HOT void STY()
    {
        writeMemory(m_addr, m_y);
    }

    GERANES_INLINE_HOT void TAX()
    {
        dummyRead();
        m_x = m_a;
        updateZeroAndNegativeFlags(m_x);        
    }

    GERANES_INLINE_HOT void U_ATX()
	{
		//LDA & TAX
		const uint8_t value = readMemory(m_addr);
		m_a = value; //LDA
		m_x = m_a; //TAX		

        updateZeroAndNegativeFlags(m_a);
	}

    GERANES_INLINE_HOT void TAY()
    {
        dummyRead();
        m_y = m_a;
        updateZeroAndNegativeFlags(m_y);       
    }

    GERANES_INLINE_HOT void TYA()
    {
        dummyRead();
        m_a = m_y;
        updateZeroAndNegativeFlags(m_a);
    }

    GERANES_INLINE_HOT void TSX()
    {
        dummyRead();
        m_x = m_sp;
        updateZeroAndNegativeFlags(m_x);        
    }

    GERANES_INLINE_HOT void TXA()
    {
        dummyRead();
        m_a = m_x;
        updateZeroAndNegativeFlags(m_a);
    }

    GERANES_INLINE_HOT void TXS()
    {
        dummyRead();
        m_sp = m_x;       
    }

    GERANES_INLINE_HOT void U_AXA()
    {		
		//"This opcode stores the result of A AND X AND the high byte of the target address of the operand +1 in memory."	
		//This may not be the actual behavior, but the read/write operations are needed for proper cycle counting
		writeMemory(m_addr, ((m_addr >> 8) + 1) & m_a & m_x);       
    }

    GERANES_INLINE_HOT void U_TAS()
	{
		//"AND X register with accumulator and store result in stack
		//pointer, then AND stack pointer with the high byte of the
		//target address of the argument + 1. Store result in memory."
		m_sp = m_x & m_a;
        writeMemory(m_addr, m_sp & ((m_addr >> 8) + 1));
	}

    GERANES_INLINE_HOT void U_SYA() {

        const uint8_t addrHigh = m_addr >> 8;
		const uint8_t addrLow = m_addr & 0xFF;
		const uint8_t value = m_y & (addrHigh + 1);
		
		//From here: http://forums.nesdev.com/viewtopic.php?f=3&t=3831&start=30
		//Unsure if this is accurate or not
		//"the target address for e.g. SYA becomes ((y & (addr_high + 1)) << 8) | addr_low instead of the normal ((addr_high + 1) << 8) | addr_low"
        writeMemory(((m_y & (addrHigh + 1)) << 8) | addrLow, value);
    }

    GERANES_INLINE_HOT void U_SXA() {
        const uint8_t addrHigh = m_addr >> 8;
		const uint8_t addrLow = m_addr & 0xFF;
		const uint8_t value = m_x & (addrHigh + 1);
        writeMemory(((m_x & (addrHigh + 1)) << 8) | addrLow, value);
    }

    GERANES_INLINE_HOT void U_LAS() {
		//"AND memory with stack pointer, transfer result to accumulator, X register and stack pointer."
		const uint8_t value = readMemory(m_addr);
		m_a = value & m_sp;
		m_x = m_a;
		m_sp = m_a;
        updateZeroAndNegativeFlags(m_a);
    }

    GERANES_INLINE_HOT void U_UNK() {
        dummyRead();
    }

    GERANES_INLINE_HOT void checkInterrupts()
    {
        if( (m_nmiSignal) || (m_irqSignal && m_intFlag == false))
        {
            if(m_nmiSignal) {
                m_nmiSignal = false;
                m_interrupt = Interrupt::NMI;
            }
            else {
                m_interrupt = Interrupt::IRQ;
            }

            m_irqSignal = false;   
        }    
    }

    GERANES_INLINE_HOT void emulateInterruptSequence(bool isBrk = false)
    {
        dummyRead();
        push16(isBrk ? m_pc+1 : m_pc);
        m_brkFlag = isBrk;
        m_unusedFlag = true;
        push(m_status);
        m_intFlag = true;   

        if(m_interrupt == Interrupt::NMI || m_nmiSignal) {
            const uint8_t low = readMemory(NMI_VECTOR);
            const uint8_t high = readMemory(NMI_VECTOR+1);
            m_pc = MAKE16(low, high);
        }
        else {
            const uint8_t low = readMemory(IRQ_VECTOR);
            const uint8_t high = readMemory(IRQ_VECTOR+1);
            m_pc = MAKE16(low, high);
        }        
    }

    GERANES_INLINE_HOT void fetchOperand()
    {
        switch(addrMode[m_opcode]) {
            case AddrMode::Acc:
            case AddrMode::Imp: m_addr = 0; break;
            case AddrMode::Imm: getAddrImediate(); break;
            case AddrMode::Rel: getAddrRelative(); break;
            case AddrMode::Zero: getAddrZeroPage(); break;
            case AddrMode::ZeroX: getAddrZeroPageX(); break;
            case AddrMode::ZeroY: getAddrZeroPageY(); break;
            case AddrMode::Ind: getAddrIndirect(); break;
            case AddrMode::IndX: getAddrIndirectX(); break;
            case AddrMode::IndY: getAddrIndirectY(false); break;
            case AddrMode::IndYW: getAddrIndirectY(true); break;
            case AddrMode::Abs: getAddrAbsolute(); break;
            case AddrMode::AbsX: getAddrAbsoluteX(false); break;
            case AddrMode::AbsXW: getAddrAbsoluteX(true); break;
            case AddrMode::AbsY: getAddrAbsoluteY(false); break;
            case AddrMode::AbsYW: getAddrAbsoluteY(true); break;
            default: break;
        }
    }

    GERANES_INLINE_HOT void emulateOpcode()
    {
        switch(m_opcode)
        {
            case 0x00: BRK(); break;
            case 0x01: ORA(); break;
            case 0x02: U_HLT(); break;
            case 0x03: U_SLO(); break;
            case 0x04: U_DOP(); break;
            case 0x05: ORA(); break;
            case 0x06: ASL(); break;
            case 0x07: U_SLO(); break;
            case 0x08: PHP(); break;
            case 0x09: ORA(); break;
            case 0x0A: ASL_implied(); break;
            case 0x0B: AAC(); break;
            case 0x0C: U_DOP(); break;
            case 0x0D: ORA(); break;
            case 0x0E: ASL(); break;
            case 0x0F: U_SLO(); break;
            case 0x10: BPL(); break;
            case 0x11: ORA(); break;
            case 0x12: U_HLT(); break;
            case 0x13: U_SLO(); break;
            case 0x14: U_DOP(); break;
            case 0x15: ORA(); break;
            case 0x16: ASL(); break;
            case 0x17: U_SLO(); break;
            case 0x18: CLC(); break;
            case 0x19: ORA(); break;
            case 0x1A: NOP(); break;
            case 0x1B: U_SLO(); break;
            case 0x1C: U_DOP(); break;
            case 0x1D: ORA(); break;
            case 0x1E: ASL(); break;
            case 0x1F: U_SLO(); break;
            case 0x20: JSR(); break;
            case 0x21: AND(); break;
            case 0x22: U_HLT(); break;
            case 0x23: U_RLA(); break;
            case 0x24: BIT(); break;
            case 0x25: AND(); break;
            case 0x26: ROL(); break;
            case 0x27: U_RLA(); break;
            case 0x28: PLP(); break;
            case 0x29: AND(); break;
            case 0x2A: ROL_implied(); break;
            case 0x2B: AAC(); break;
            case 0x2C: BIT(); break;
            case 0x2D: AND(); break;
            case 0x2E: ROL(); break;
            case 0x2F: U_RLA(); break;
            case 0x30: BMI(); break;
            case 0x31: AND(); break;
            case 0x32: NOP(); break;
            case 0x33: U_RLA(); break;
            case 0x34: U_DOP(); break;
            case 0x35: AND(); break;
            case 0x36: ROL(); break;
            case 0x37: U_RLA(); break;
            case 0x38: SEC(); break;
            case 0x39: AND(); break;
            case 0x3A: NOP(); break;
            case 0x3B: U_RLA(); break;
            case 0x3C: U_DOP(); break;
            case 0x3D: AND(); break;
            case 0x3E: ROL(); break;
            case 0x3F: U_RLA(); break;
            case 0x40: RTI(); break;
            case 0x41: EOR(); break;
            case 0x42: U_HLT(); break;
            case 0x43: U_SRE(); break;
            case 0x44: U_DOP();  break;
            case 0x45: EOR(); break;
            case 0x46: LSR(); break;
            case 0x47: U_SRE();  break;
            case 0x48: PHA(); break;
            case 0x49: EOR(); break;
            case 0x4A: LSR_implied(); break;
            case 0x4B: U_ASR();  break;
            case 0x4C: JMP(); break;
            case 0x4D: EOR(); break;
            case 0x4E: LSR(); break;
            case 0x4F: U_SRE();  break;        
            case 0x50: BVC(); break;
            case 0x51: EOR(); break;
            case 0x52: U_HLT(); break;
            case 0x53: U_SRE(); break;
            case 0x54: U_DOP();  break;
            case 0x55: EOR(); break;
            case 0x56: LSR(); break;
            case 0x57: U_SRE();  break;
            case 0x58: CLI(); break;
            case 0x59: EOR(); break;
            case 0x5A: NOP(); break;
            case 0x5B: U_SRE(); break;
            case 0x5C: U_DOP(); break;
            case 0x5D: EOR(); break;
            case 0x5E: LSR(); break;
            case 0x5F: U_SRE(); break;
            case 0x60: RTS(); break;
            case 0x61: ADC(); break;
            case 0x62: U_HLT(); break;
            case 0x63: U_RRA(); break;
            case 0x64: U_DOP(); break;
            case 0x65: ADC(); break;
            case 0x66: ROR(); break;
            case 0x67: U_RRA(); break;
            case 0x68: PLA(); break;
            case 0x69: ADC(); break;
            case 0x6A: ROR_implied(); break;
            case 0x6B: U_ARR(); break;
            case 0x6C: JMP(); break;
            case 0x6D: ADC(); break;
            case 0x6E: ROR(); break;
            case 0x6F: U_RRA(); break;
            case 0x70: BVS(); break;
            case 0x71: ADC(); break;
            case 0x72: U_HLT(); break;
            case 0x73: U_RRA(); break;
            case 0x74: U_DOP(); break;
            case 0x75: ADC(); break;
            case 0x76: ROR(); break;
            case 0x77: U_RRA(); break;
            case 0x78: SEI(); break;
            case 0x79: ADC(); break;
            case 0x7A: NOP(); break;
            case 0x7B: U_RRA(); break;
            case 0x7C: U_DOP(); break;
            case 0x7D: ADC(); break;
            case 0x7E: ROR(); break;
            case 0x7F: U_RRA(); break;
            case 0x80: U_DOP(); break;
            case 0x81: STA(); break;
            case 0x82: U_DOP(); break;
            case 0x83: U_SAX(); break;
            case 0x84: STY(); break;
            case 0x85: STA(); break;
            case 0x86: STX(); break;
            case 0x87: U_SAX(); break;
            case 0x88: DEY(); break;
            case 0x89: U_DOP(); break;
            case 0x8A: TXA(); break;
            case 0x8B: U_UNK(); break;
            case 0x8C: STY(); break;
            case 0x8D: STA(); break;
            case 0x8E: STX(); break;
            case 0x8F: U_SAX(); break;
            case 0x90: BCC(); break;
            case 0x91: STA(); break;
            case 0x92: U_HLT(); break;
            case 0x93: U_AXA(); break;
            case 0x94: STY(); break;
            case 0x95: STA(); break;
            case 0x96: STX(); break;
            case 0x97: U_SAX(); break;
            case 0x98: TYA(); break;
            case 0x99: STA(); break;
            case 0x9A: TXS(); break;
            case 0x9B: U_TAS(); break;
            case 0x9C: U_SYA(); break;
            case 0x9E: U_SXA(); break;
            case 0x9D: STA(); break;
            case 0x9F: U_AXA(); break;
            case 0xA0: LDY(); break;
            case 0xA1: LDA(); break;
            case 0xA2: LDX(); break;
            case 0xA3: U_LAX(); break;
            case 0xA4: LDY(); break;
            case 0xA5: LDA(); break;
            case 0xA6: LDX(); break;
            case 0xA7: U_LAX(); break;
            case 0xA8: TAY(); break;
            case 0xA9: LDA(); break;
            case 0xAA: TAX(); break;
            case 0xAB: U_ATX(); break;
            case 0xAC: LDY(); break;
            case 0xAD: LDA(); break;
            case 0xAE: LDX(); break;
            case 0xAF: U_LAX();  break;
            case 0xB0: BCS(); break;
            case 0xB1: LDA(); break;
            case 0xB2: U_HLT(); break;
            case 0xB3: U_LAX(); break;
            case 0xB4: LDY(); break;
            case 0xB6: LDX(); break;
            case 0xB5: LDA(); break;
            case 0xB7: U_LAX(); break;
            case 0xB8: CLV(); break;
            case 0xB9: LDA(); break;
            case 0xBA: TSX(); break;
            case 0xBB: U_LAS(); break;
            case 0xBC: LDY(); break;
            case 0xBD: LDA(); break;
            case 0xBE: LDX(); break;
            case 0xBF: U_LAX(); break;
            case 0xC0: CPY(); break;
            case 0xC1: CMP(); break;
            case 0xC2: U_DOP(); break;
            case 0xC3: U_DCP(); break;
            case 0xC4: CPY(); break;
            case 0xC5: CMP(); break;
            case 0xC6: DEC(); break;
            case 0xC7: U_DCP(); break;
            case 0xC8: INY(); break;
            case 0xC9: CMP(); break;
            case 0xCA: DEX(); break;
            case 0xCB: U_AXS(); break;
            case 0xCC: CPY(); break;
            case 0xCD: CMP(); break;
            case 0xCE: DEC(); break;
            case 0xCF: U_DCP(); break;
            case 0xD0: BNE(); break;
            case 0xD1: CMP(); break;
            case 0xD2: U_HLT(); break;
            case 0xD3: U_DCP(); break;
            case 0xD4: U_DOP(); break;
            case 0xD5: CMP(); break;
            case 0xD6: DEC(); break;
            case 0xD7: U_DCP(); break;
            case 0xD8: CLD(); break;
            case 0xD9: CMP(); break;
            case 0xDA: NOP(); break;
            case 0xDB: U_DCP(); break;
            case 0xDC: U_DOP(); break;
            case 0xDD: CMP(); break;
            case 0xDE: DEC(); break;
            case 0xDF: U_DCP(); break;
            case 0xE0: CPX(); break;
            case 0xE1: SBC(); break;
            case 0xE2: U_DOP(); break;
            case 0xE3: U_ISB(); break;
            case 0xE4: CPX(); break;
            case 0xE5: SBC(); break;
            case 0xE6: INC(); break;
            case 0xE7: U_ISB(); break;
            case 0xE8: INX(); break;
            case 0xE9: SBC(); break;
            case 0xEA: NOP(); break;
            case 0xEB: SBC(); break;
            case 0xEC: CPX(); break;
            case 0xED: SBC(); break;
            case 0xEF: U_ISB(); break;
            case 0xEE: INC(); break;
            case 0xF0: BEQ(); break;
            case 0xF1: SBC(); break;
            case 0xF2: U_HLT(); break;
            case 0xF3: U_ISB(); break;
            case 0xF4: U_DOP(); break;
            case 0xF5: SBC(); break;
            case 0xF6: INC(); break;
            case 0xF7: U_ISB(); break;
            case 0xF8: SED(); break;
            case 0xF9: SBC(); break;
            case 0xFA: NOP(); break;
            case 0xFB: U_ISB(); break;
            case 0xFC: U_DOP(); break;
            case 0xFD: SBC(); break;
            case 0xFE: INC(); break;
            case 0xFF: U_ISB(); break;
        };
    }

    GERANES_INLINE void phi2(bool nmiState, bool irqState) {
        
        if(m_poolIntsAtCycle == m_currentInstructionCycle) {
            checkInterrupts();
        }

        switch(m_nmiStep) {

            case NmiStep::WAITING_LOW:
                if(!nmiState) m_nmiStep = NmiStep::WAITING_HIGH;
                break;

            case NmiStep::WAITING_HIGH:
                if(nmiState) m_nmiStep = NmiStep::OK;
                break;

            case NmiStep::OK:
                assert(false); //should never occur
                break;
        }

        m_irqStep = irqState;

        m_currentInstructionCycle++; 
    }


    GERANES_INLINE void halt(int cycles)
    {
        m_haltCycles += cycles;
    }

    GERANES_INLINE bool isOpcodeWriteCycle() {
        //return ((OPCODE_WRITE_CYCLES_TABLE[m_opcode] >> m_currentInstructionCycle) & 0x01) != 0;
        return m_writeCycle;
    }

    GERANES_INLINE bool isOddCycle() {
        return m_cyclesCounter%2 != 0;
    }

    GERANES_INLINE bool isHalted() {
        return m_haltCycles > 0;
    }   
    
    GERANES_INLINE_HOT int run() {

        m_runCount = 0;
        m_currentInstructionCycle = 0;        
        m_addr = 0;

        if(m_interrupt != Interrupt::NONE) {
            m_poolIntsAtCycle = DO_NOT_POOL_INTS;
            m_opcode = 0x00; //BRK
            dummyRead();            
            emulateInterruptSequence();
            m_interrupt = Interrupt::NONE;
        }
        else {
            m_opcode = readMemory(m_pc++);                       
            m_poolIntsAtCycle = OPCODE_INT_POOL_CYCLE_TABLE[m_opcode];
            fetchOperand();
            emulateOpcode();           
        }

        return m_runCount;
    }

    GERANES_INLINE_HOT uint8_t readMemory(uint16_t addr) {

        beginCycle();

        while(m_haltCycles > 0) {     
            endCycle();
            m_haltCycles--;
            beginCycle();
        }

        uint8_t ret =  m_bus.read(addr);
        
        endCycle();        
        
        return ret;
    }

    GERANES_INLINE_HOT void writeMemory(uint16_t addr, uint8_t value) {

        m_writeCycle = true;

        beginCycle();

        assert(!isHalted());

        assert(isOpcodeWriteCycle());

        m_bus.write(addr, value);   
        
        endCycle();

        m_writeCycle = false;
    }

    void beginCycle();

    void endCycle();  

    void serialization(SerializationBase& s)
    {
        SERIALIZEDATA(s, m_pc);
        SERIALIZEDATA(s, m_sp);
        SERIALIZEDATA(s, m_status);
        SERIALIZEDATA(s, m_a);
        SERIALIZEDATA(s, m_x);
        SERIALIZEDATA(s, m_y);
        SERIALIZEDATA(s, m_cyclesCounter);
        SERIALIZEDATA(s, m_opcode);
        SERIALIZEDATA(s, m_addr);
        SERIALIZEDATA(s, m_nmiSignal);
        SERIALIZEDATA(s, m_nmiStep);
        SERIALIZEDATA(s, m_irqSignal);
        SERIALIZEDATA(s, m_irqStep);
        SERIALIZEDATA(s, m_haltCycles);
        SERIALIZEDATA(s, m_extraCycles);
        SERIALIZEDATA(s, m_currentInstructionCycle);
        SERIALIZEDATA(s, m_interrupt);
        SERIALIZEDATA(s, m_poolIntsAtCycle);
    }

    uint16_t busAddr() {  
        return m_addr;
    }


};

#include "DMA.h"

inline void CPU2A03::beginCycle() {

    //PPU   X---X---X---X---X---X---X---X---X---X---X-...
    //CPU   --X-----------X-----------X-----------X---...
    //CPU   --1-------2---1-------2---1-------2---1---...

    if(!m_console.ppu().inOverclockLines()){              
        m_console.dma().cycle();
        m_console.apu().cycle();                           
    }
   
}

inline void CPU2A03::endCycle() {


    if(!isHalted()) phi1();

    m_console.ppu().ppuCycle();      

    m_console.ppu().ppuCycle();

    if(!isHalted()) {
        phi2(
            m_console.ppu().getInterruptFlag(),
            m_console.apu().getInterruptFlag() || m_console.cartridge().getInterruptFlag()
        );
    }        

    if(!m_console.ppu().inOverclockLines()) {       
        m_console.cartridge().cycle();
    }       

    m_console.ppu().ppuCyclePAL(); 

    m_console.ppu().ppuCycle();    

    m_runCount++;

    if(!m_console.ppu().inOverclockLines()) {
        m_cyclesCounter++;
    }
}



#endif
