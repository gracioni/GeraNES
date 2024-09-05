#ifndef CPU2A03_H
#define CPU2A03_H

#include "defines.h"
#include "IBus.h"
#include "Serialization.h"

#include "signal/SigSlot.h"

const uint8_t NO_POOL = 0xFF;
const uint8_t BRANCH_TAKEN_PAGECROSS_POLL_INTS_AT_CYCLE = 3;

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
/*0xE0*/ 2,6,3,8,3,3,5,5,2,2,2,2,4,4,6,6,
/*0xF0*/ 2,5,2,8,4,4,6,6,2,4,2,7,4,4,7,7,
};

// BRK (NO_POOL)
// CLI(0x58) SEI(0x78) PLP(0x28) in cycle 0
// some references (https://www.nesdev.org/wiki/CPU_interrupts) says that branch instructions
// 0x90 0xB0 0xF0 0x30 0xD0 0x10 0x50 0x70 are polled in cycle 0
// and all 2 cycles instructions are polled in cycle 0
// but I think this is wrong
static const uint8_t OPCODE_INT_POOL_CYCLE_TABLE[256] =
{
/*0x00*/ NO_POOL,5,1,7,2,2,4,4,2,1,1,1,3,3,5,5,
/*0x10*/ 1,4,1,7,3,3,5,5,1,3,1,6,3,3,6,6,
/*0x20*/ 5,5,1,7,2,2,4,4,0,1,1,1,3,3,5,5,
/*0x30*/ 1,4,1,7,3,3,5,5,1,3,1,6,3,3,6,6,
/*0x40*/ 5,5,1,7,2,2,4,4,2,1,1,1,2,3,5,5,
/*0x50*/ 1,4,1,7,3,3,5,5,0,3,1,6,3,3,6,6,
/*0x60*/ 5,5,1,7,2,2,4,4,3,1,1,1,4,3,5,5,
/*0x70*/ 1,4,1,7,3,3,5,5,0,3,1,6,3,3,6,6,
/*0x80*/ 1,5,1,5,2,2,2,2,1,1,1,1,3,3,3,3,
/*0x90*/ 1,5,1,5,3,3,3,3,1,4,1,4,4,4,4,4,
/*0xA0*/ 1,5,1,5,2,2,2,2,1,1,1,1,3,3,3,3,
/*0xB0*/ 1,4,1,4,3,3,3,3,1,3,1,3,3,3,3,3,
/*0xC0*/ 1,5,1,7,2,2,4,4,1,1,1,1,3,3,5,5,
/*0xD0*/ 1,4,1,7,3,3,5,5,1,3,1,6,3,3,6,6,
/*0xE0*/ 1,5,2,7,2,2,4,4,1,1,1,1,3,3,5,5,
/*0xF0*/ 1,4,1,7,3,3,5,5,1,3,1,6,3,3,6,6,
};

static const uint8_t OPCODE_SIZE_TABLE[256] = {
/*0x00*/	1,2,0,0,0,2,2,0,1,2,1,0,0,3,3,0,
/*0x10*/	2,2,0,0,0,2,2,0,1,3,0,0,0,3,3,0,
/*0x20*/	3,2,0,0,2,2,2,0,1,2,1,0,3,3,3,0,
/*0x30*/	2,2,0,0,0,2,2,0,1,3,0,0,0,3,3,0,
/*0x40*/	1,2,0,0,0,2,2,0,1,2,1,0,3,3,3,0,
/*0x50*/	2,2,0,0,0,2,2,0,1,3,0,0,0,3,3,0,
/*0x60*/	1,2,0,0,0,2,2,0,1,2,1,0,3,3,3,0,
/*0x70*/	2,2,0,0,0,2,2,0,1,3,0,0,0,3,3,0,
/*0x80*/	0,2,0,0,2,2,2,0,1,0,1,0,3,3,3,0,
/*0x90*/	2,2,0,0,2,2,2,0,1,3,1,0,0,3,0,0,
/*0xA0*/	2,2,2,0,2,2,2,0,1,2,1,0,3,3,3,0,
/*0xB0*/	2,2,0,0,2,2,2,0,1,3,1,0,3,3,3,0,
/*0xC0*/	2,2,0,0,2,2,2,0,1,2,1,0,3,3,3,0,
/*0xD0*/	2,2,0,0,0,2,2,0,1,3,0,0,0,3,3,0,
/*0xE0*/	2,2,0,0,2,2,2,0,1,2,1,0,3,3,3,0,
/*0xF0*/	2,2,0,0,0,2,2,0,1,3,0,0,0,3,3,0
};

static const uint8_t OPCODE_WRITE_CYCLES_TABLE[256] =
{
/*0x00*/ 0x1C, 0x00, 0x00, 0xC0, 0x00, 0x00, 0x18, 0x18, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x30,
/*0x10*/ 0x00, 0x00, 0x00, 0xC0, 0x00, 0x00, 0x30, 0x30, 0x00, 0x00, 0x00, 0x60, 0x00, 0x00, 0x60, 0x60,
/*0x20*/ 0x1C, 0x00, 0x00, 0xC0, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x30,
/*0x30*/ 0x00, 0x00, 0x00, 0xC0, 0x00, 0x00, 0x30, 0x30, 0x00, 0x00, 0x00, 0x60, 0x00, 0x00, 0x60, 0x60,
/*0x40*/ 0x00, 0x00, 0x00, 0xC0, 0x00, 0x00, 0x18, 0x18, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x30,
/*0x50*/ 0x00, 0x00, 0x00, 0xC0, 0x00, 0x00, 0x30, 0x30, 0x00, 0x00, 0x00, 0x60, 0x00, 0x00, 0x60, 0x60,
/*0x60*/ 0x00, 0x00, 0x00, 0xC0, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x30,
/*0x70*/ 0x00, 0x00, 0x00, 0xC0, 0x00, 0x00, 0x30, 0x30, 0x00, 0x00, 0x00, 0x60, 0x00, 0x00, 0x60, 0x60,
/*0x80*/ 0x00, 0x20, 0x00, 0x20, 0x04, 0x04, 0x04, 0x04, 0x00, 0x00, 0x00, 0x00, 0x08, 0x08, 0x08, 0x08,
/*0x90*/ 0x00, 0x20, 0x00, 0x00, 0x08, 0x08, 0x08, 0x08, 0x00, 0x10, 0x00, 0x00, 0x10, 0x10, 0x10, 0x00,
/*0xA0*/ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/*0xB0*/ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/*0xC0*/ 0x00, 0x00, 0x00, 0xC0, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x30,
/*0xD0*/ 0x00, 0x00, 0x00, 0xC0, 0x00, 0x00, 0x30, 0x30, 0x00, 0x00, 0x00, 0x60, 0x00, 0x00, 0x60, 0x60,
/*0xE0*/ 0x00, 0x00, 0x00, 0xC0, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x30,
/*0xF0*/ 0x00, 0x00, 0x00, 0xC0, 0x00, 0x00, 0x30, 0x30, 0x00, 0x00, 0x00, 0x60, 0x00, 0x00, 0x60, 0x60
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
    unsigned int m_instructionCycles;
    uint8_t m_opcode;
    uint16_t m_addr;
    bool m_pageCross;

    bool m_nmiSignal;
    bool m_irqSignal;

    int m_nmiStep;
    bool m_irqStep;

    int m_nmiAtInstructionCycle; //the instruction cycle the NMI request flag was set

    enum InterruptCause {NMI = 0, IRQ} m_interruptCause;

    int m_waitCyclesToEmulate;
    bool m_InstructionOrInterruptFlag; //false = instruction cycles, true = interrupt sequence
    int m_sleepCycles;

    int m_realExpectedDiff;

    int m_currentInstructionCycle;

    bool m_nextSequence;
    uint8_t m_poolIntsAtCycle;

    GERANES_INLINE_HOT uint16_t MAKE16(uint8_t low, uint8_t high)
    {
        return ((uint16_t)low) | (((uint16_t)high)<<8);
    }

    GERANES_INLINE_HOT void push(uint8_t data)
    {
        m_bus.write(0x0100 + m_sp, data);
        m_sp--;
    }

    GERANES_INLINE_HOT uint8_t pull(void)
    {
        m_sp++;
        return m_bus.read(0x0100 + m_sp);
    }

    GERANES_INLINE_HOT void push16(uint16_t data)
    {
        push((uint8_t)(data >> 8));
        push((uint8_t)(data & 0xFF));
    }

    GERANES_INLINE_HOT uint16_t pull16(void)
    {
        uint8_t low = pull();
        uint8_t high = pull();
        return MAKE16(low,high);
    }

    GERANES_INLINE_HOT void dummyRead() {
        m_bus.read(m_pc);
    }

    GERANES_INLINE_HOT void getAddrImediate(void)
    {
        m_addr = m_pc+1;
    }

    GERANES_INLINE_HOT void getAddrAbsolute(void)
    {
        m_addr = MAKE16(m_bus.read(m_pc+1),m_bus.read(m_pc+2));
    }

    GERANES_INLINE_HOT void getAddrZeroPage(void)
    {
        m_addr = m_bus.read(m_pc+1);
    }

    GERANES_INLINE_HOT void getAddrAbsoluteX(bool dummyRead = true)
    {
        m_addr = MAKE16(m_bus.read(m_pc+1),m_bus.read(m_pc+2)); 

        m_pageCross = (m_addr ^ (m_addr+m_x)) & 0xFF00;

        //dummy read
        if(m_pageCross || dummyRead) m_bus.read( (m_addr & 0xFF00) | ((m_addr+m_x) & 0xFF) );


        m_addr += m_x;
    }

    GERANES_INLINE_HOT void getAddrAbsoluteY(bool dummyRead = true)
    {
        m_addr = MAKE16(m_bus.read(m_pc+1),m_bus.read(m_pc+2));

        m_pageCross = (m_addr ^ (m_addr+m_y)) & 0xFF00;

        if(m_pageCross || dummyRead) m_bus.read( (m_addr & 0xFF00) | ((m_addr+m_y) & 0xFF) );

        m_addr += m_y;
    }

    GERANES_INLINE_HOT void getAddrZeroPageX(void)
    {
        m_addr = (uint8_t)(m_bus.read(m_pc+1) + m_x);
    }


    GERANES_INLINE_HOT void getAddrZeroPageY(void)
    {
        m_addr = (uint8_t)(m_bus.read(m_pc+1) + m_y);
    }

    GERANES_INLINE_HOT void getAddrIndirect(void)
    {
        uint8_t low = m_bus.read(m_pc+1);
        uint8_t high = m_bus.read(m_pc+2);
        m_addr = MAKE16(m_bus.read(MAKE16(low,high)),m_bus.read(MAKE16(low+1,high)));
    }

    GERANES_INLINE_HOT void getAddrIndirectX(void)
    {
        uint8_t temp = m_bus.read(m_pc+1) + m_x;
        m_addr = MAKE16(m_bus.read(temp),m_bus.read( (uint8_t)(temp+1) ));
    }

    GERANES_INLINE_HOT void getAddrIndirectY(bool dummyRead = true)
    {
        uint8_t temp = m_bus.read(m_pc+1);
        m_addr = MAKE16(m_bus.read(temp),m_bus.read( (uint8_t)(temp+1) ));

        m_pageCross = (m_addr ^ (m_addr+m_y)) & 0xFF00;

        //dummy read
        if(m_pageCross || dummyRead) m_bus.read( (m_addr & 0xFF00) | ((m_addr+m_y) & 0xFF) );


        m_addr += m_y;
    }

    GERANES_INLINE_HOT void getAddrRelative(void)
    {
        m_addr = m_pc+1;
    }

    GERANES_INLINE_HOT void _phi1() {

        if(m_nmiStep==2) {
            m_nmiSignal = true;
            m_nmiStep = 0;
            m_nmiAtInstructionCycle = m_currentInstructionCycle;
        }

        m_irqSignal = m_irqStep;
    }


public:

    SigSlot::Signal<const std::string&> signalError;

    CPU2A03(Ibus& bus) : m_bus(bus)   {
    }

    void init(void)
    {
        m_pc = MAKE16(m_bus.read(0xFFFC),m_bus.read(0xFFFD)); //reset vector

        m_sp = 0xFD;
        m_status = 0x24;

        m_instructionCycles = 0;
        m_cyclesCounter = 0;

        m_addr = 0;
        m_pageCross = false;
        m_opcode = 0;

        m_a = 0;
        m_x = 0;
        m_y = 0;

        m_nmiSignal = false;
        m_irqSignal = false;

        m_nmiStep = 0;
        m_irqStep = false;

        m_nmiAtInstructionCycle = 0;

        m_nextSequence = false;
        m_poolIntsAtCycle = -1;

        m_waitCyclesToEmulate = 0;
        m_InstructionOrInterruptFlag = false;
        m_sleepCycles = 0;
        m_realExpectedDiff = 0;

        m_currentInstructionCycle = 0;
        m_interruptCause = NMI;

    }    

    GERANES_INLINE_HOT void _ADC(void)
    {
        unsigned int temp = (unsigned int)m_a + m_bus.read(m_addr) + (m_carryFlag?1:0);

        m_carryFlag = temp>0xFF;
        m_zeroFlag = ((temp&0xFF) == 0x00);
        m_negativeFlag = (temp&0x80);
        m_overflowFlag = ( !((m_a ^ m_bus.read(m_addr)) & 0x80) && ((m_a ^ temp) & 0x80) );

        m_a = (uint8_t)temp;
    }

    GERANES_INLINE_HOT void OP_69_ADC(void)
    {
        //getAddrImediate();
        _ADC();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0x69];
        m_pc += 2;
    }

    GERANES_INLINE_HOT void OP_65_ADC(void)
    {
        //getAddrZeroPage();
        _ADC();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0x65];
        m_pc += 2;
    }

    GERANES_INLINE_HOT void OP_75_ADC(void)
    {
        //getAddrZeroPageX();
        _ADC();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0x75];
        m_pc += 2;
    }

    GERANES_INLINE_HOT void OP_6D_ADC(void)
    {
        //getAddrAbsolute();
        _ADC();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0x6D];
        m_pc += 3;
    }

    GERANES_INLINE_HOT void OP_7D_ADC(void)
    {
        //getAddrAbsoluteX();
        _ADC();

        m_instructionCycles = OPCODE_CYCLES_TABLE[0x7D];
        if(m_pageCross) ++m_instructionCycles;

        m_pc += 3;
    }

    GERANES_INLINE_HOT void OP_79_ADC(void)
    {
        //getAddrAbsoluteY();
        _ADC();

        m_instructionCycles = OPCODE_CYCLES_TABLE[0x79];
        if(m_pageCross) ++m_instructionCycles;

        m_pc += 3;
    }

    GERANES_INLINE_HOT void OP_61_ADC(void)
    {
        //getAddrIndirectX();
        _ADC();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0x61];;
        m_pc += 2;
    }

    GERANES_INLINE_HOT void OP_71_ADC(void)
    {
        //getAddrIndirectY();
        _ADC();

        m_instructionCycles = OPCODE_CYCLES_TABLE[0x71];
        if(m_pageCross) ++m_instructionCycles;

        m_pc += 2;
    }

    GERANES_INLINE_HOT void _AND(void)
    {
        m_a &= m_bus.read(m_addr);
        m_negativeFlag = (m_a&0x80);
        m_zeroFlag = (m_a == 0x00);
    }

    GERANES_INLINE_HOT void OP_29_AND(void)
    {
        //getAddrImediate();
        _AND();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0x29];
        m_pc += 2;
    }

    GERANES_INLINE_HOT void OP_25_AND(void)
    {
        //getAddrZeroPage();
        _AND();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0x25];
        m_pc += 2;
    }

    GERANES_INLINE_HOT void OP_35_AND(void)
    {
        //getAddrZeroPageX();
        _AND();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0x35];;
        m_pc += 2;
    }

    GERANES_INLINE_HOT void OP_2D_AND(void)
    {
        //getAddrAbsolute();
        _AND();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0x2D];
        m_pc += 3;
    }

    GERANES_INLINE_HOT void OP_3D_AND(void)
    {
        //getAddrAbsoluteX();
        _AND();

        m_instructionCycles = OPCODE_CYCLES_TABLE[0x3D];
        if(m_pageCross) ++m_instructionCycles;

        m_pc += 3;
    }

    GERANES_INLINE_HOT void OP_39_AND(void)
    {
        //getAddrAbsoluteY();
        _AND();

        m_instructionCycles = OPCODE_CYCLES_TABLE[0x39];
        if(m_pageCross) ++m_instructionCycles;

        m_pc += 3;
    }

    GERANES_INLINE_HOT void OP_21_AND(void)
    {
        //getAddrIndirectX();
        _AND();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0x21];
        m_pc += 2;
    }

    GERANES_INLINE_HOT void OP_31_AND(void)
    {
        //getAddrIndirectY();
        _AND();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0x31];
        if(m_pageCross) ++m_instructionCycles;
        m_pc += 2;
    }

    GERANES_INLINE_HOT void _ASL(void)
    {
        uint8_t temp = m_bus.read(m_addr);

        m_carryFlag = (temp&0x80);

        temp <<= 1;

        m_negativeFlag = (temp&0x80);
        m_zeroFlag = (temp == 0x00);

        m_bus.write(m_addr, temp);
    }

    GERANES_INLINE_HOT void OP_0A_ASL(void) //implied
    {
        m_carryFlag = (m_a&0x80);
        m_a <<= 1;
        m_negativeFlag = (m_a&0x80);
        m_zeroFlag = (m_a == 0x00);

        m_instructionCycles = OPCODE_CYCLES_TABLE[0x0A];
        m_pc += 1;
    }

    GERANES_INLINE_HOT void OP_06_ASL(void)
    {
        //getAddrZeroPage();
        _ASL();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0x06];
        m_pc += 2;
    }

    GERANES_INLINE_HOT void OP_16_ASL(void)
    {
        //getAddrZeroPageX();
        _ASL();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0x16];
        m_pc += 2;
    }

    GERANES_INLINE_HOT void OP_0E_ASL(void)
    {
        //getAddrAbsolute();
        _ASL();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0x0E];
        m_pc += 3;
    }

    GERANES_INLINE_HOT void OP_1E_ASL(void)
    {
        //getAddrAbsoluteX();
        _ASL();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0x1E];
        m_pc += 3;
    }

    GERANES_INLINE_HOT void OP_90_BCC(void)
    {
        //getAddrRelative();

        m_pc += 2;

        m_instructionCycles = OPCODE_CYCLES_TABLE[0x90];

        if(!m_carryFlag)
        {
            uint16_t temp = m_pc;
            m_pc += (int8_t)m_bus.read(m_addr);

            if( (temp ^ m_pc) & 0xFF00 ) {
                m_instructionCycles += 2;
                m_poolIntsAtCycle = BRANCH_TAKEN_PAGECROSS_POLL_INTS_AT_CYCLE;
            }
            else {
                m_instructionCycles += 1;
            }
        }     

    }

    GERANES_INLINE_HOT void OP_B0_BCS(void)
    {
        //getAddrRelative();

        m_pc += 2;
        m_instructionCycles = OPCODE_CYCLES_TABLE[0xB0];

        if(m_carryFlag)
        {
            uint16_t temp = m_pc;
            m_pc += (int8_t)m_bus.read(m_addr);

            if( (temp ^ m_pc) & 0xFF00 ) {
                m_instructionCycles += 2;
                m_poolIntsAtCycle = BRANCH_TAKEN_PAGECROSS_POLL_INTS_AT_CYCLE;
            }
            else {
                m_instructionCycles += 1;
            }
        }
    }

    GERANES_INLINE_HOT void OP_F0_BEQ(void)
    {
        //getAddrRelative();

        m_pc += 2;
        m_instructionCycles = OPCODE_CYCLES_TABLE[0xF0];

        if(m_zeroFlag)
        {
            uint16_t temp = m_pc;
            m_pc += (int8_t)m_bus.read(m_addr);

            if( (temp ^ m_pc) & 0xFF00 ) {
                m_instructionCycles += 2;
                m_poolIntsAtCycle = BRANCH_TAKEN_PAGECROSS_POLL_INTS_AT_CYCLE;
            }
            else {
                m_instructionCycles += 1;
            }
        }
    }

    GERANES_INLINE_HOT void _BIT(void)
    {
        uint8_t temp = m_bus.read(m_addr);

        m_zeroFlag = ( (m_a&temp) == 0x00);
        m_negativeFlag = (temp&0x80);
        m_overflowFlag = (temp&0x40);
    }

    GERANES_INLINE_HOT void OP_24_BIT(void)
    {
        //getAddrZeroPage();
        _BIT();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0x24];
        m_pc += 2;
    }

    GERANES_INLINE_HOT void OP_2C_BIT(void)
    {
        //getAddrAbsolute();
        _BIT();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0x2C];
        m_pc += 3;
    }

    GERANES_INLINE_HOT void OP_30_BMI(void)
    {
        //getAddrRelative();

        m_pc += 2;
        m_instructionCycles = OPCODE_CYCLES_TABLE[0x30];

        if(m_negativeFlag)
        {
            uint16_t temp = m_pc;
            m_pc += (int8_t)m_bus.read(m_addr);

            if( (temp ^ m_pc) & 0xFF00 ) {
                m_instructionCycles += 2;
                m_poolIntsAtCycle = BRANCH_TAKEN_PAGECROSS_POLL_INTS_AT_CYCLE;
            }
            else {
                m_instructionCycles += 1;
            }
        }

    }

    GERANES_INLINE_HOT void OP_D0_BNE(void)
    {
        //getAddrRelative();

        m_pc += 2;
        m_instructionCycles = OPCODE_CYCLES_TABLE[0xD0];

        if(!m_zeroFlag)
        {
            uint16_t temp = m_pc;
            m_pc += (int8_t)m_bus.read(m_addr);

            if( (temp ^ m_pc) & 0xFF00 ) {
                m_instructionCycles += 2;
                m_poolIntsAtCycle = BRANCH_TAKEN_PAGECROSS_POLL_INTS_AT_CYCLE;
            }
            else {
                m_instructionCycles += 1;
            }
        }
    }

    GERANES_INLINE_HOT void OP_10_BPL(void)
    {
        //getAddrRelative();

        m_pc += 2;
        m_instructionCycles = OPCODE_CYCLES_TABLE[0x10];

        if(!m_negativeFlag)
        {
            uint16_t temp = m_pc;
            m_pc += (int8_t)m_bus.read(m_addr);

            if( (temp ^ m_pc) & 0xFF00 ) {
                m_instructionCycles += 2;
                m_poolIntsAtCycle = BRANCH_TAKEN_PAGECROSS_POLL_INTS_AT_CYCLE;
            }
            else {
                m_instructionCycles += 1;
            }
        }

    }

    GERANES_INLINE_HOT void OP_00_BRK(void)
    {
        //dummy read the next opcode
        m_bus.read(m_pc+1);

        m_pc += 2;
        push16(m_pc);
        m_brkFlag = true;
        m_unusedFlag = true;
        push(m_status);
        m_intFlag = true;

        if(m_nmiSignal && m_nmiAtInstructionCycle < 5){
            m_pc = MAKE16(m_bus.read(NMI_VECTOR),m_bus.read(NMI_VECTOR+1));
            m_nmiSignal = false;
        }
        else
            m_pc = MAKE16(m_bus.read(BRK_VECTOR), m_bus.read(BRK_VECTOR+1));

        m_irqSignal = false;

        m_instructionCycles = OPCODE_CYCLES_TABLE[0x00];
    }

    GERANES_INLINE_HOT void OP_50_BVC(void)
    {
        //getAddrRelative();

        m_pc += 2;
        m_instructionCycles = OPCODE_CYCLES_TABLE[0x50];

        if(!m_overflowFlag)
        {
            uint16_t temp = m_pc;
            m_pc += (int8_t)m_bus.read(m_addr);

            if( (temp ^ m_pc) & 0xFF00 ) {
                m_instructionCycles += 2;
                m_poolIntsAtCycle = BRANCH_TAKEN_PAGECROSS_POLL_INTS_AT_CYCLE;
            }
            else {
                m_instructionCycles += 1;
            }
        }
    }

    GERANES_INLINE_HOT void OP_70_BVS(void)
    {
        //getAddrRelative();

        m_pc += 2;
        m_instructionCycles = OPCODE_CYCLES_TABLE[0x70];

        if(m_overflowFlag)
        {
            uint16_t temp = m_pc;
            m_pc += (int8_t)m_bus.read(m_addr);

            if( (temp ^ m_pc) & 0xFF00 ) {
                m_instructionCycles += 2;
                m_poolIntsAtCycle = BRANCH_TAKEN_PAGECROSS_POLL_INTS_AT_CYCLE;
            }
            else {
                m_instructionCycles += 1;
            }
        }
    }

    GERANES_INLINE_HOT void OP_18_CLC(void)
    {
        m_carryFlag = false;
        m_pc += 1;
        m_instructionCycles = OPCODE_CYCLES_TABLE[0x18];
    }

    GERANES_INLINE_HOT void OP_D8_CLD(void)
    {
        m_decimalFlag = false;
        m_pc += 1;
        m_instructionCycles = OPCODE_CYCLES_TABLE[0xD8];
    }

    GERANES_INLINE_HOT void OP_58_CLI(void)
    {
        m_intFlag = false;
        m_pc += 1;
        m_instructionCycles = OPCODE_CYCLES_TABLE[0x58];
    }

    GERANES_INLINE_HOT void OP_B8_CLV(void)
    {
        m_overflowFlag = false;
        m_pc += 1;
        m_instructionCycles = OPCODE_CYCLES_TABLE[0xB8];
    }

    GERANES_INLINE_HOT void _CMP(void)
    {
        uint8_t temp = m_bus.read(m_addr);

        m_carryFlag = (m_a>=temp);

        temp = m_a - temp;

        m_zeroFlag = (temp==0);
        m_negativeFlag = (temp&0x80);
    }

    GERANES_INLINE_HOT void OP_C9_CMP(void)
    {
        //getAddrImediate();
        _CMP();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0xC9];
        m_pc += 2;
    }

    GERANES_INLINE_HOT void OP_C5_CMP(void)
    {
        //getAddrZeroPage();
        _CMP();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0xC5];
        m_pc += 2;
    }

    GERANES_INLINE_HOT void OP_D5_CMP(void)
    {
        //getAddrZeroPageX();
        _CMP();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0xD5];
        m_pc += 2;
    }

    GERANES_INLINE_HOT void OP_CD_CMP(void)
    {
        //getAddrAbsolute();
        _CMP();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0xCD];
        m_pc += 3;
    }

    GERANES_INLINE_HOT void OP_DD_CMP(void)
    {
        //getAddrAbsoluteX();
        _CMP();

        m_instructionCycles = OPCODE_CYCLES_TABLE[0xDD];
        if(m_pageCross) ++m_instructionCycles;

        m_pc += 3;
    }

    GERANES_INLINE_HOT void OP_D9_CMP(void)
    {
        //getAddrAbsoluteY();
        _CMP();

        m_instructionCycles = OPCODE_CYCLES_TABLE[0xD9];
        if(m_pageCross) ++m_instructionCycles;

        m_pc += 3;
    }

    GERANES_INLINE_HOT void OP_C1_CMP(void)
    {
        //getAddrIndirectX();
        _CMP();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0xC1];
        m_pc += 2;
    }

    GERANES_INLINE_HOT void OP_D1_CMP(void)
    {
        //getAddrIndirectY();
        _CMP();

        m_instructionCycles = OPCODE_CYCLES_TABLE[0xD1];
        if(m_pageCross) ++m_instructionCycles;

        m_pc += 2;
    }

    GERANES_INLINE_HOT void _CPX(void)
    {
        uint8_t temp = m_bus.read(m_addr);

        m_carryFlag = (m_x>=temp);

        temp = m_x - temp;

        m_zeroFlag = (temp==0);
        m_negativeFlag = (temp&0x80);
    }

    GERANES_INLINE_HOT void OP_E0_CPX(void)
    {
        //getAddrImediate();
        _CPX();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0xE0];
        m_pc += 2;
    }

    GERANES_INLINE_HOT void OP_E4_CPX(void)
    {
        //getAddrZeroPage();
        _CPX();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0xE4];
        m_pc += 2;
    }

    GERANES_INLINE_HOT void OP_EC_CPX(void)
    {
        //getAddrAbsolute();
        _CPX();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0xEC];
        m_pc += 3;
    }

    GERANES_INLINE_HOT void _CPY(void)
    {
        uint8_t temp = m_bus.read(m_addr);

        m_carryFlag = (m_y>=temp);

        temp = m_y - temp;

        m_zeroFlag = (temp==0);
        m_negativeFlag = (temp&0x80);
    }

    GERANES_INLINE_HOT void OP_C0_CPY(void)
    {
        //getAddrImediate();
        _CPY();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0xC0];
        m_pc += 2;
    }

    GERANES_INLINE_HOT void OP_C4_CPY(void)
    {
        //getAddrZeroPage();
        _CPY();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0xC4];
        m_pc += 2;
    }

    GERANES_INLINE_HOT void OP_CC_CPY(void)
    {
        //getAddrAbsolute();
        _CPY();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0xCC];
        m_pc += 3;
    }

    GERANES_INLINE_HOT void _DEC(void)
    {
        uint8_t temp = m_bus.read(m_addr);

        temp--;

        m_negativeFlag = (temp&0x80);
        m_zeroFlag = (temp == 0x00);

        m_bus.write(m_addr,temp);
    }

    GERANES_INLINE_HOT void OP_C6_DEC(void)
    {
        //getAddrZeroPage();
        _DEC();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0xC6];
        m_pc += 2;
    }

    GERANES_INLINE_HOT void OP_D6_DEC(void)
    {
        //getAddrZeroPageX();
        _DEC();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0xD6];
        m_pc += 2;
    }

    GERANES_INLINE_HOT void OP_CE_DEC(void)
    {
        //getAddrAbsolute();
        _DEC();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0xCE];
        m_pc += 3;
    }

    GERANES_INLINE_HOT void OP_DE_DEC(void)
    {
        //getAddrAbsoluteX();
        _DEC();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0xDE];
        m_pc += 3;
    }

    GERANES_INLINE_HOT void OP_CA_DEX(void)
    {
        m_x--;

        m_negativeFlag = (m_x&0x80);
        m_zeroFlag = (m_x == 0x00);

        m_instructionCycles = OPCODE_CYCLES_TABLE[0xCA];
        m_pc += 1;
    }

    GERANES_INLINE_HOT void OP_88_DEY(void)
    {
        m_y--;

        m_negativeFlag = (m_y&0x80);
        m_zeroFlag = (m_y == 0x00);

        m_instructionCycles = OPCODE_CYCLES_TABLE[0x88];
        m_pc += 1;
    }

    GERANES_INLINE_HOT void _EOR(void)
    {
        m_a ^= m_bus.read(m_addr);

        m_negativeFlag = (m_a&0x80);
        m_zeroFlag = (m_a == 0x00);
    }

    GERANES_INLINE_HOT void OP_49_EOR(void)
    {
        //getAddrImediate();
        _EOR();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0x49];
        m_pc += 2;
    }

    GERANES_INLINE_HOT void OP_45_EOR(void)
    {
        //getAddrZeroPage();
        _EOR();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0x45];
        m_pc += 2;
    }

    GERANES_INLINE_HOT void OP_55_EOR(void)
    {
        //getAddrZeroPageX();
        _EOR();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0x55];
        m_pc += 2;
    }

    GERANES_INLINE_HOT void OP_4D_EOR(void)
    {
        //getAddrAbsolute();
        _EOR();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0x4D];
        m_pc += 3;
    }

    GERANES_INLINE_HOT void OP_5D_EOR(void)
    {
        //getAddrAbsoluteX();
        _EOR();

        m_instructionCycles = OPCODE_CYCLES_TABLE[0x5D];
        if(m_pageCross) ++m_instructionCycles;

        m_pc += 3;
    }

    GERANES_INLINE_HOT void OP_59_EOR(void)
    {
        //getAddrAbsoluteY();
        _EOR();

        m_instructionCycles = OPCODE_CYCLES_TABLE[0x59];
        if(m_pageCross) ++m_instructionCycles;

        m_pc += 3;
    }

    GERANES_INLINE_HOT void OP_41_EOR(void)
    {
        //getAddrIndirectX();
        _EOR();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0x41];
        m_pc += 2;
    }

    GERANES_INLINE_HOT void OP_51_EOR(void)
    {
        //getAddrIndirectY();
        _EOR();

        m_instructionCycles = OPCODE_CYCLES_TABLE[0x51];
        if(m_pageCross) ++m_instructionCycles;

        m_pc += 2;
    }

    GERANES_INLINE_HOT  void _INC(void)
    {
        uint8_t temp = m_bus.read(m_addr);

        temp++;

        m_negativeFlag = (temp&0x80);
        m_zeroFlag = (temp == 0x00);

        m_bus.write(m_addr,temp);
    }

    GERANES_INLINE_HOT void OP_E6_INC(void)
    {
        //getAddrZeroPage();
        _INC();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0xE6];
        m_pc += 2;
    }

    GERANES_INLINE_HOT void OP_F6_INC(void)
    {
        //getAddrZeroPageX();
        _INC();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0xF6];
        m_pc += 2;
    }

    GERANES_INLINE_HOT void OP_EE_INC(void)
    {
        //getAddrAbsolute();
        _INC();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0xEE];
        m_pc += 3;
    }

    GERANES_INLINE_HOT void OP_FE_INC(void)
    {
        //getAddrAbsoluteX();
        _INC();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0xFE];
        m_pc += 3;
    }

    GERANES_INLINE_HOT void OP_E8_INX(void)
    {
        m_x++;

        m_negativeFlag = (m_x&0x80);
        m_zeroFlag = (m_x == 0x00);

        m_instructionCycles = OPCODE_CYCLES_TABLE[0xE8];
        m_pc += 1;
    }

    GERANES_INLINE_HOT void OP_C8_INY(void)
    {
        m_y++;

        m_negativeFlag = (m_y&0x80);
        m_zeroFlag = (m_y == 0x00);

        m_instructionCycles = OPCODE_CYCLES_TABLE[0xC8];
        m_pc += 1;
    }

    GERANES_INLINE_HOT void OP_4C_JMP(void)
    {
        //getAddrAbsolute();
        m_pc = m_addr;
        m_instructionCycles = OPCODE_CYCLES_TABLE[0x4C];
    }

    GERANES_INLINE_HOT void OP_6C_JMP(void)
    {
        //getAddrIndirect();
        m_pc = m_addr;
        m_instructionCycles = OPCODE_CYCLES_TABLE[0x6C];
    }

    GERANES_INLINE_HOT void OP_20_JSR(void)
    {
        //getAddrAbsolute();

        m_pc += 2;
        push16(m_pc);

        m_pc = m_addr;

        m_instructionCycles = OPCODE_CYCLES_TABLE[0x20];
    }

    GERANES_INLINE_HOT void _LDA(void)
    {
        m_a = m_bus.read(m_addr);
        m_negativeFlag = (m_a&0x80);
        m_zeroFlag = (m_a == 0);
    }

    GERANES_INLINE_HOT void OP_A9_LDA(void)
    {
        //getAddrImediate();
        _LDA();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0xA9];
        m_pc += 2;
    }

    GERANES_INLINE_HOT void OP_A5_LDA(void)
    {
        //getAddrZeroPage();
        _LDA();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0xA5];
        m_pc += 2;
    }

    GERANES_INLINE_HOT void OP_B5_LDA(void)
    {
        //getAddrZeroPageX();
        _LDA();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0xB5];
        m_pc += 2;
    }

    GERANES_INLINE_HOT void OP_AD_LDA(void)
    {
        //getAddrAbsolute();
        _LDA();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0xAD];
        m_pc += 3;
    }

    GERANES_INLINE_HOT void OP_BD_LDA(void)
    {
        //getAddrAbsoluteX();
        _LDA();

        m_instructionCycles = OPCODE_CYCLES_TABLE[0xBD];
        if(m_pageCross) ++m_instructionCycles;

        m_pc += 3;
    }

    GERANES_INLINE_HOT void OP_B9_LDA(void)
    {
        //getAddrAbsoluteY();
        _LDA();

        m_instructionCycles = OPCODE_CYCLES_TABLE[0xB9];
        if(m_pageCross) ++m_instructionCycles;

        m_pc += 3;
    }

    GERANES_INLINE_HOT void OP_A1_LDA(void)
    {
        //getAddrIndirectX();
        _LDA();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0xA1];
        m_pc += 2;
    }

    GERANES_INLINE_HOT void OP_B1_LDA(void)
    {
        //getAddrIndirectY();
        _LDA();

        m_instructionCycles = OPCODE_CYCLES_TABLE[0xB1];
        if(m_pageCross) ++m_instructionCycles;

        m_pc += 2;
    }

    GERANES_INLINE_HOT void _LDX(void)
    {
        m_x = m_bus.read(m_addr);
        m_negativeFlag = (m_x&0x80);
        m_zeroFlag = (m_x == 0);
    }

    GERANES_INLINE_HOT void OP_A2_LDX(void)
    {
        //getAddrImediate();
        _LDX();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0xA2];
        m_pc += 2;
    }

    GERANES_INLINE_HOT void OP_A6_LDX(void)
    {
        //getAddrZeroPage();
        _LDX();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0xA6];
        m_pc += 2;
    }

    GERANES_INLINE_HOT void OP_B6_LDX(void)
    {
        //getAddrZeroPageY();
        _LDX();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0xB6];
        m_pc += 2;
    }

    GERANES_INLINE_HOT void OP_AE_LDX(void)
    {
        //getAddrAbsolute();
        _LDX();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0xAE];
        m_pc += 3;
    }

    GERANES_INLINE_HOT void OP_BE_LDX(void)
    {
        //getAddrAbsoluteY();
        _LDX();

        m_instructionCycles = OPCODE_CYCLES_TABLE[0xBE];
        if(m_pageCross) ++m_instructionCycles;

        m_pc += 3;
    }

    GERANES_INLINE_HOT void _LDY(void)
    {
        m_y = m_bus.read(m_addr);
        m_negativeFlag = (m_y&0x80);
        m_zeroFlag = (m_y == 0);
    }

    GERANES_INLINE_HOT void OP_A0_LDY(void)
    {
        //getAddrImediate();
        _LDY();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0xA0];
        m_pc += 2;
    }

    GERANES_INLINE_HOT void OP_A4_LDY(void)
    {
        //getAddrZeroPage();
        _LDY();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0xA4];
        m_pc += 2;
    }

    GERANES_INLINE_HOT void OP_B4_LDY(void)
    {
        //getAddrZeroPageX();
        _LDY();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0xB4];
        m_pc += 2;
    }

    GERANES_INLINE_HOT void OP_AC_LDY(void)
    {
        //getAddrAbsolute();
        _LDY();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0xAC];
        m_pc += 3;
    }

    GERANES_INLINE_HOT void OP_BC_LDY(void)
    {
        //getAddrAbsoluteX();
        _LDY();

        m_instructionCycles = OPCODE_CYCLES_TABLE[0xBC];
        if(m_pageCross) ++m_instructionCycles;

        m_pc += 3;
    }

    GERANES_INLINE_HOT void _LSR(void)
    {
        uint8_t temp = m_bus.read(m_addr);
        m_carryFlag = (temp&0x01);
        temp >>= 1;
        m_negativeFlag = false;
        m_zeroFlag = (temp == 0x00);
        m_bus.write(m_addr, temp);
    }

    GERANES_INLINE_HOT void OP_4A_LSR(void) //implied
    {
        m_carryFlag = (m_a&0x01);
        m_a >>= 1;
        m_negativeFlag = false;
        m_zeroFlag = (m_a == 0x00);
        m_instructionCycles = OPCODE_CYCLES_TABLE[0x4A];
        m_pc += 1;
    }

    GERANES_INLINE_HOT void OP_46_LSR(void)
    {
        //getAddrZeroPage();
        _LSR();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0x46];
        m_pc += 2;
    }

    GERANES_INLINE_HOT void OP_56_LSR(void)
    {
        //getAddrZeroPageX();
        _LSR();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0x56];
        m_pc += 2;
    }

    GERANES_INLINE_HOT void OP_4E_LSR(void)
    {
        //getAddrAbsolute();
        _LSR();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0x4E];
        m_pc += 3;
    }

    GERANES_INLINE_HOT void OP_5E_LSR(void)
    {
        //getAddrAbsoluteX();
        _LSR();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0x5E];
        m_pc += 3;
    }

    GERANES_INLINE_HOT void OP_XX_NOP(void)
    {
        m_instructionCycles = 2;
        m_pc += 1;
    }

    GERANES_INLINE_HOT void _ORA(void)
    {
        m_a |= m_bus.read(m_addr);
        m_negativeFlag = (m_a&0x80);
        m_zeroFlag = (m_a == 0x00);
    }

    GERANES_INLINE_HOT void OP_09_ORA(void)
    {
        //getAddrImediate();
        _ORA();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0x09];
        m_pc += 2;
    }

    GERANES_INLINE_HOT void OP_05_ORA(void)
    {
        //getAddrZeroPage();
        _ORA();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0x05];
        m_pc += 2;
    }

    GERANES_INLINE_HOT void OP_15_ORA(void)
    {
        //getAddrZeroPageX();
        _ORA();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0x15];
        m_pc += 2;
    }

    GERANES_INLINE_HOT void OP_0D_ORA(void)
    {
        //getAddrAbsolute();
        _ORA();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0x0D];
        m_pc += 3;
    }

    GERANES_INLINE_HOT void OP_1D_ORA(void)
    {
        //getAddrAbsoluteX();
        _ORA();

        m_instructionCycles = OPCODE_CYCLES_TABLE[0x1D];
        if(m_pageCross) ++m_instructionCycles;

        m_pc += 3;
    }

    GERANES_INLINE_HOT void OP_19_ORA(void)
    {
        //getAddrAbsoluteY();
        _ORA();

        m_instructionCycles = OPCODE_CYCLES_TABLE[0x19];
        if(m_pageCross) ++m_instructionCycles;

        m_pc += 3;
    }

    GERANES_INLINE_HOT void OP_01_ORA(void)
    {
        //getAddrIndirectX();
        _ORA();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0x01];
        m_pc += 2;
    }

    GERANES_INLINE_HOT void OP_11_ORA(void)
    {
        //getAddrIndirectY();
        _ORA();

        m_instructionCycles = OPCODE_CYCLES_TABLE[0x11];
        if(m_pageCross) ++m_instructionCycles;

        m_pc += 2;
    }

    GERANES_INLINE_HOT void OP_48_PHA(void)
    {
        push(m_a);
        m_instructionCycles = OPCODE_CYCLES_TABLE[0x48];
        m_pc += 1;
    }

    GERANES_INLINE_HOT void OP_08_PHP(void)
    {
        m_brkFlag = true;
        m_unusedFlag = true;

        push(m_status);
        m_instructionCycles = OPCODE_CYCLES_TABLE[0x08];
        m_pc += 1;
    }

    GERANES_INLINE_HOT void OP_68_PLA(void)
    {
        m_a = pull();
        m_negativeFlag = (m_a&0x80);
        m_zeroFlag = (m_a == 0x00);
        m_instructionCycles = OPCODE_CYCLES_TABLE[0x68];
        m_pc += 1;
    }

    GERANES_INLINE_HOT void OP_28_PLP(void)
    {
        m_status = pull();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0x28];
        m_pc += 1;
    }

    GERANES_INLINE_HOT void _ROL(void)
    {
        uint8_t temp = m_bus.read(m_addr);

        bool outputCarry = temp&0x80;

        temp <<= 1;
        if(m_carryFlag) temp |= 0x01;

        m_carryFlag = (outputCarry);
        m_negativeFlag = (temp&0x80);
        m_zeroFlag = (temp == 0x00);

        m_bus.write(m_addr,temp);
    }

    GERANES_INLINE_HOT void OP_2A_ROL(void) //implied
    {
        bool outputCarry = m_a&0x80;

        m_a <<= 1;
        if(m_carryFlag) m_a |= 0x01;

        m_carryFlag = (outputCarry);
        m_negativeFlag = (m_a&0x80);
        m_zeroFlag = (m_a == 0x00);

        m_instructionCycles = OPCODE_CYCLES_TABLE[0x2A];
        m_pc += 1;
    }

    GERANES_INLINE_HOT void OP_26_ROL(void)
    {
        //getAddrZeroPage();
        _ROL();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0x26];
        m_pc += 2;
    }

    GERANES_INLINE_HOT void OP_36_ROL(void)
    {
        //getAddrZeroPageX();
        _ROL();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0x36];
        m_pc += 2;
    }

    GERANES_INLINE_HOT void OP_2E_ROL(void)
    {
        //getAddrAbsolute();
        _ROL();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0x2E];
        m_pc += 3;
    }

    GERANES_INLINE_HOT void OP_3E_ROL(void)
    {
        //getAddrAbsoluteX();
        _ROL();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0x3E];
        m_pc += 3;
    }

    GERANES_INLINE_HOT void _ROR(void)
    {
        uint8_t temp = m_bus.read(m_addr);

        bool outputCarry = temp&0x01;

        temp >>= 1;
        if(m_carryFlag) temp |= 0x80;

        m_carryFlag = (outputCarry);
        m_negativeFlag = (temp&0x80);
        m_zeroFlag = (temp == 0x00);

        m_bus.write(m_addr,temp);
    }

    GERANES_INLINE_HOT void OP_6A_ROR(void) //implied
    {
        bool outputCarry = m_a&0x01;

        m_a >>= 1;
        if(m_carryFlag) m_a |= 0x80;

        m_carryFlag = (outputCarry);
        m_negativeFlag = (m_a&0x80);
        m_zeroFlag = (m_a == 0x00);

        m_instructionCycles = OPCODE_CYCLES_TABLE[0x6A];
        m_pc += 1;
    }

    GERANES_INLINE_HOT void OP_66_ROR(void)
    {
        //getAddrZeroPage();
        _ROR();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0x66];
        m_pc += 2;
    }

    GERANES_INLINE_HOT void OP_76_ROR(void)
    {
        //getAddrZeroPageX();
        _ROR();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0x76];
        m_pc += 2;
    }

    GERANES_INLINE_HOT void OP_6E_ROR(void)
    {
        //getAddrAbsolute();
        _ROR();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0x6E];
        m_pc += 3;
    }

    GERANES_INLINE_HOT void OP_7E_ROR(void)
    {
        //getAddrAbsoluteX();
        _ROR();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0x7E];
        m_pc += 3;
    }

    GERANES_INLINE_HOT void OP_40_RTI(void)
    {
        //dummy read the next opcode
        m_bus.read(m_pc+1);

        m_status = pull();
        m_pc = pull16();

        m_instructionCycles = OPCODE_CYCLES_TABLE[0x40];     
    }

    GERANES_INLINE_HOT void OP_60_RTS(void)
    {
        //dummy read the next opcode
        m_bus.read(m_pc+1);

        m_pc = pull16();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0x60];
        m_pc += 1;
    }

    GERANES_INLINE_HOT void _SBC(void)
    {
        unsigned int temp = (unsigned int)m_a - m_bus.read(m_addr) - (m_carryFlag?0:1);

        m_carryFlag = (temp < 0x100);
        m_overflowFlag = (((m_a ^ temp) & 0x80) && ((m_a ^ m_bus.read(m_addr)) & 0x80));

        m_a = temp&0xFF;

        m_negativeFlag = (m_a&0x80);
        m_zeroFlag = (m_a == 0x00);
    }

    GERANES_INLINE_HOT void OP_E9_SBC(void)
    {
        //getAddrImediate();
        _SBC();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0xE9];
        m_pc += 2;
    }

    GERANES_INLINE_HOT void OP_E5_SBC(void)
    {
        //getAddrZeroPage();
        _SBC();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0xE5];
        m_pc += 2;
    }

    GERANES_INLINE_HOT void OP_F5_SBC(void)
    {
        //getAddrZeroPageX();
        _SBC();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0xF5];
        m_pc += 2;
    }

    GERANES_INLINE_HOT void OP_ED_SBC(void)
    {
        //getAddrAbsolute();
        _SBC();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0xED];
        m_pc += 3;
    }

    GERANES_INLINE_HOT void OP_FD_SBC(void)
    {
        //getAddrAbsoluteX();
        _SBC();

        m_instructionCycles = OPCODE_CYCLES_TABLE[0xFD];
        if(m_pageCross) ++m_instructionCycles;

        m_pc += 3;
    }

    GERANES_INLINE_HOT void OP_F9_SBC(void)
    {
        //getAddrAbsoluteY();
        _SBC();

        m_instructionCycles = OPCODE_CYCLES_TABLE[0xF9];
        if(m_pageCross) ++m_instructionCycles;

        m_pc += 3;
    }

    GERANES_INLINE_HOT void OP_E1_SBC(void)
    {
        //getAddrIndirectX();
        _SBC();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0xE1];
        m_pc += 2;
    }

    GERANES_INLINE_HOT void OP_F1_SBC(void)
    {
        //getAddrIndirectY();
        _SBC();

        m_instructionCycles = OPCODE_CYCLES_TABLE[0xF1];
        if(m_pageCross) ++m_instructionCycles;

        m_pc += 2;
    }

    GERANES_INLINE_HOT void OP_38_SEC(void)
    {
        m_carryFlag = true;
        m_instructionCycles = OPCODE_CYCLES_TABLE[0x38];
        m_pc += 1;
    }

    GERANES_INLINE_HOT void OP_F8_SED(void)
    {
        m_decimalFlag = true;
        m_instructionCycles = OPCODE_CYCLES_TABLE[0xF8];
        m_pc += 1;
    }

    GERANES_INLINE_HOT void OP_78_SEI(void)
    {
        m_intFlag = true;
        m_instructionCycles = OPCODE_CYCLES_TABLE[0x78];
        m_pc += 1;
    }

    GERANES_INLINE_HOT void _STA(void)
    {
        m_bus.write(m_addr, m_a);
    }

    GERANES_INLINE_HOT void OP_85_STA(void)
    {
        //getAddrZeroPage();
        _STA();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0x85];
        m_pc += 2;
    }

    GERANES_INLINE_HOT void OP_95_STA(void)
    {
        //getAddrZeroPageX();
        _STA();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0x95];
        m_pc += 2;
    }

    GERANES_INLINE_HOT void OP_8D_STA(void)
    {
        //getAddrAbsolute();
        _STA();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0x8D];
        m_pc += 3;
    }

    GERANES_INLINE_HOT void OP_9D_STA(void)
    {
        //getAddrAbsoluteX();
        _STA();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0x9D];
        m_pc += 3;
    }

    GERANES_INLINE_HOT void OP_99_STA(void)
    {
        //getAddrAbsoluteY();
        _STA();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0x99];
        m_pc += 3;
    }

    GERANES_INLINE_HOT void OP_81_STA(void)
    {
        //getAddrIndirectX();
        _STA();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0x81];
        m_pc += 2;
    }

    GERANES_INLINE_HOT void OP_91_STA(void)
    {
        //getAddrIndirectY();
        _STA();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0x91];
        m_pc += 2;
    }

    GERANES_INLINE_HOT void _STX(void)
    {
        m_bus.write(m_addr, m_x);
    }

    GERANES_INLINE_HOT void OP_86_STX(void)
    {
        //getAddrZeroPage();
        _STX();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0x86];
        m_pc += 2;
    }

    GERANES_INLINE_HOT void OP_96_STX(void)
    {
        //getAddrZeroPageY();
        _STX();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0x96];
        m_pc += 2;
    }

    GERANES_INLINE_HOT void OP_8E_STX(void)
    {
        //getAddrAbsolute();
        _STX();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0x8E];
        m_pc += 3;
    }

    GERANES_INLINE_HOT void _STY(void)
    {
        m_bus.write(m_addr, m_y);
    }

    GERANES_INLINE_HOT void OP_84_STY(void)
    {
        //getAddrZeroPage();
        _STY();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0x84];
        m_pc += 2;
    }

    GERANES_INLINE_HOT void OP_94_STY(void)
    {
        //getAddrZeroPageX();
        _STY();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0x94];
        m_pc += 2;
    }

    GERANES_INLINE_HOT void OP_8C_STY(void)
    {
        //getAddrAbsolute();
        _STY();
        m_instructionCycles = OPCODE_CYCLES_TABLE[0x8C];
        m_pc += 3;
    }

    GERANES_INLINE_HOT void OP_AA_TAX(void)
    {
        m_x = m_a;

        m_negativeFlag = (m_x&0x80);
        m_zeroFlag = (m_x == 0x00);

        m_instructionCycles = OPCODE_CYCLES_TABLE[0xAA];
        m_pc += 1;
    }

    GERANES_INLINE_HOT void OP_A8_TAY(void)
    {
        m_y = m_a;

        m_negativeFlag = (m_y&0x80);
        m_zeroFlag = (m_y == 0x00);

        m_instructionCycles = OPCODE_CYCLES_TABLE[0xA8];
        m_pc += 1;
    }

    GERANES_INLINE_HOT void OP_98_TYA(void)
    {
        m_a = m_y;

        m_negativeFlag = (m_a&0x80);
        m_zeroFlag = (m_a == 0x00);

        m_instructionCycles = OPCODE_CYCLES_TABLE[0x98];
        m_pc += 1;
    }

    GERANES_INLINE_HOT void OP_BA_TSX(void)
    {
        m_x = m_sp;

        m_negativeFlag = (m_x&0x80);
        m_zeroFlag = (m_x == 0x00);

        m_instructionCycles = OPCODE_CYCLES_TABLE[0xBA];
        m_pc += 1;
    }

    GERANES_INLINE_HOT void OP_8A_TXA(void)
    {
        m_a = m_x;

        m_negativeFlag = (m_a&0x80);
        m_zeroFlag = (m_a == 0x00);

        m_instructionCycles = OPCODE_CYCLES_TABLE[0x8A];
        m_pc += 1;
    }

    GERANES_INLINE_HOT void OP_9A_TXS(void)
    {
        m_sp = m_x;
        m_instructionCycles = OPCODE_CYCLES_TABLE[0x9A];
        m_pc += 1;
    }

    GERANES_INLINE_HOT bool checkInterrupts(void)
    {
        if( (m_nmiSignal) || (m_irqSignal && m_intFlag == false))
        {
            if(m_nmiSignal) {
                m_nmiSignal = false;
                m_interruptCause = NMI;

            }
            else {
                m_interruptCause = IRQ;
            }

            m_irqSignal = false;

            return true;
        }

        return false;
    }

    GERANES_INLINE_HOT void emulateInterruptSequence(void)
    {
        push16(m_pc);
        m_brkFlag = false;
        m_unusedFlag = true;
        push(m_status);
        m_intFlag = true;   

        if(m_interruptCause == NMI) {
            m_pc = MAKE16(m_bus.read(NMI_VECTOR),m_bus.read(NMI_VECTOR+1));
        }
        else {

            if(m_nmiSignal && m_nmiAtInstructionCycle < 5) {
                m_pc = MAKE16(m_bus.read(NMI_VECTOR),m_bus.read(NMI_VECTOR+1));

            }
            else {
                m_pc = MAKE16(m_bus.read(IRQ_VECTOR), m_bus.read(IRQ_VECTOR+1));
            }

        }
    }

    int debug = 0;

    void debugState()
    {
        printf("\n");
        printf("%04X ", m_pc);

        int opsize = OPCODE_SIZE_TABLE[m_opcode];

        for(int i = 0; i < opsize; i++) {
            printf("%02X ", m_bus.read(m_pc+i) );
        }

        printf("\t\t");


        printf("a: %02X ", m_a);
        printf("x: %02X ", m_x);
        printf("y: %02X ", m_y);
        printf("p: %02X ", m_status);
        printf("sp: %02X ", m_sp);
    }

    GERANES_INLINE_HOT void fetchOperand()
    {
        switch(addrMode[m_opcode]) {
            case AddrMode::Acc:
            case AddrMode::Imp: dummyRead(); break;
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
        case 0x00: OP_00_BRK(); break;
        case 0x01: OP_01_ORA(); break;
        case 0x05: OP_05_ORA(); break;
        case 0x06: OP_06_ASL(); break;
        case 0x08: OP_08_PHP(); break;
        case 0x09: OP_09_ORA(); break;
        case 0x0a: OP_0A_ASL(); break;
        case 0x0D: OP_0D_ORA(); break;
        case 0x0E: OP_0E_ASL(); break;
        case 0x10: OP_10_BPL(); break;
        case 0x11: OP_11_ORA(); break;
        case 0x15: OP_15_ORA(); break;
        case 0x16: OP_16_ASL(); break;
        case 0x18: OP_18_CLC(); break;
        case 0x19: OP_19_ORA(); break;
        case 0x1D: OP_1D_ORA(); break;
        case 0x1E: OP_1E_ASL(); break;
        case 0x20: OP_20_JSR(); break;
        case 0x21: OP_21_AND(); break;
        case 0x24: OP_24_BIT(); break;
        case 0x25: OP_25_AND(); break;
        case 0x26: OP_26_ROL(); break;
        case 0x28: OP_28_PLP(); break;
        case 0x29: OP_29_AND(); break;
        case 0x2A: OP_2A_ROL(); break;
        case 0x2C: OP_2C_BIT(); break;
        case 0x2D: OP_2D_AND(); break;
        case 0x2E: OP_2E_ROL(); break;
        case 0x30: OP_30_BMI(); break;
        case 0x31: OP_31_AND(); break;
        case 0x32: OP_XX_NOP(); break;
        case 0x33: OP_XX_NOP(); break;
        case 0x34: OP_XX_NOP(); break;
        case 0x35: OP_35_AND(); break;
        case 0x36: OP_36_ROL(); break;
        case 0x38: OP_38_SEC(); break;
        case 0x39: OP_39_AND(); break;
        case 0x3D: OP_3D_AND(); break;
        case 0x3E: OP_3E_ROL(); break;
        case 0x40: OP_40_RTI(); break;
        case 0x41: OP_41_EOR(); break;
        case 0x45: OP_45_EOR(); break;
        case 0x46: OP_46_LSR(); break;
        case 0x48: OP_48_PHA(); break;
        case 0x49: OP_49_EOR(); break;
        case 0x4A: OP_4A_LSR(); break;
        case 0x4C: OP_4C_JMP(); break;
        case 0x4D: OP_4D_EOR(); break;
        case 0x4E: OP_4E_LSR(); break;
        case 0x50: OP_50_BVC(); break;
        case 0x51: OP_51_EOR(); break;
        case 0x55: OP_55_EOR(); break;
        case 0x56: OP_56_LSR(); break;
        case 0x58: OP_58_CLI(); break;
        case 0x59: OP_59_EOR(); break;
        case 0x5D: OP_5D_EOR(); break;
        case 0x5E: OP_5E_LSR(); break;
        case 0x60: OP_60_RTS(); break;
        case 0x61: OP_61_ADC(); break;
        case 0x65: OP_65_ADC(); break;
        case 0x66: OP_66_ROR(); break;
        case 0x68: OP_68_PLA(); break;
        case 0x69: OP_69_ADC(); break;
        case 0x6A: OP_6A_ROR(); break;
        case 0x6C: OP_6C_JMP(); break;
        case 0x6D: OP_6D_ADC(); break;
        case 0x6E: OP_6E_ROR(); break;
        case 0x70: OP_70_BVS(); break;
        case 0x71: OP_71_ADC(); break;
        case 0x75: OP_75_ADC(); break;
        case 0x76: OP_76_ROR(); break;
        case 0x78: OP_78_SEI(); break;
        case 0x79: OP_79_ADC(); break;
        case 0x7D: OP_7D_ADC(); break;
        case 0x7E: OP_7E_ROR(); break;
        case 0x81: OP_81_STA(); break;
        case 0x84: OP_84_STY(); break;
        case 0x85: OP_85_STA(); break;
        case 0x86: OP_86_STX(); break;
        case 0x88: OP_88_DEY(); break;
        case 0x8A: OP_8A_TXA(); break;
        case 0x8C: OP_8C_STY(); break;
        case 0x8D: OP_8D_STA(); break;
        case 0x8E: OP_8E_STX(); break;
        case 0x90: OP_90_BCC(); break;
        case 0x91: OP_91_STA(); break;
        case 0x94: OP_94_STY(); break;
        case 0x95: OP_95_STA(); break;
        case 0x96: OP_96_STX(); break;
        case 0x98: OP_98_TYA(); break;
        case 0x99: OP_99_STA(); break;
        case 0x9A: OP_9A_TXS(); break;
        case 0x9D: OP_9D_STA(); break;
        case 0xA0: OP_A0_LDY(); break;
        case 0xA1: OP_A1_LDA(); break;
        case 0xA2: OP_A2_LDX(); break;
        case 0xA4: OP_A4_LDY(); break;
        case 0xA5: OP_A5_LDA(); break;
        case 0xA6: OP_A6_LDX(); break;
        case 0xA8: OP_A8_TAY(); break;
        case 0xA9: OP_A9_LDA(); break;
        case 0xAA: OP_AA_TAX(); break;
        case 0xAC: OP_AC_LDY(); break;
        case 0xAD: OP_AD_LDA(); break;
        case 0xAE: OP_AE_LDX(); break;
        case 0xB0: OP_B0_BCS(); break;
        case 0xB1: OP_B1_LDA(); break;
        case 0xB4: OP_B4_LDY(); break;
        case 0xB5: OP_B5_LDA(); break;
        case 0xB6: OP_B6_LDX(); break;
        case 0xB8: OP_B8_CLV(); break;
        case 0xB9: OP_B9_LDA(); break;
        case 0xBA: OP_BA_TSX(); break;
        case 0xBC: OP_BC_LDY(); break;
        case 0xBD: OP_BD_LDA(); break;
        case 0xBE: OP_BE_LDX(); break;
        case 0xC0: OP_C0_CPY(); break;
        case 0xC1: OP_C1_CMP(); break;
        case 0xC4: OP_C4_CPY(); break;
        case 0xC5: OP_C5_CMP(); break;
        case 0xC6: OP_C6_DEC(); break;
        case 0xC8: OP_C8_INY(); break;
        case 0xC9: OP_C9_CMP(); break;
        case 0xCA: OP_CA_DEX(); break;
        case 0xCC: OP_CC_CPY(); break;
        case 0xCD: OP_CD_CMP(); break;
        case 0xCE: OP_CE_DEC(); break;
        case 0xD0: OP_D0_BNE(); break;
        case 0xD1: OP_D1_CMP(); break;
        case 0xD5: OP_D5_CMP(); break;
        case 0xD6: OP_D6_DEC(); break;
        case 0xD8: OP_D8_CLD(); break;
        case 0xD9: OP_D9_CMP(); break;
        case 0xDD: OP_DD_CMP(); break;
        case 0xDE: OP_DE_DEC(); break;
        case 0xE0: OP_E0_CPX(); break;
        case 0xE1: OP_E1_SBC(); break;
        case 0xE4: OP_E4_CPX(); break;
        case 0xE5: OP_E5_SBC(); break;
        case 0xE6: OP_E6_INC(); break;
        case 0xE8: OP_E8_INX(); break;
        case 0xE9: OP_E9_SBC(); break;
        case 0xEA: OP_XX_NOP(); break;
        case 0xEC: OP_EC_CPX(); break;
        case 0xED: OP_ED_SBC(); break;
        case 0xEE: OP_EE_INC(); break;
        case 0xF0: OP_F0_BEQ(); break;
        case 0xF1: OP_F1_SBC(); break;
        case 0xF5: OP_F5_SBC(); break;
        case 0xF6: OP_F6_INC(); break;
        case 0xF8: OP_F8_SED(); break;
        case 0xF9: OP_F9_SBC(); break;
        case 0xFD: OP_FD_SBC(); break;
        case 0xFE: OP_FE_INC(); break;
        default:
        {
            OP_XX_NOP();
            char aux[64];
            sprintf(aux, "Invalid opcode: 0x%02X", m_opcode);
            signalError(aux);
            break;
        }
        };
    }

    GERANES_INLINE int getOpcodeCyclesHint()
    {
        return OPCODE_CYCLES_TABLE[m_opcode];
    }

    GERANES_INLINE_HOT void begin() {

        if(m_waitCyclesToEmulate == 0 && m_realExpectedDiff == 0) {

            m_currentInstructionCycle = 0;
            m_nmiAtInstructionCycle = 0;

            m_InstructionOrInterruptFlag  = m_nextSequence;

            if(m_InstructionOrInterruptFlag) {
                m_opcode = 0x00; //BRK
                m_waitCyclesToEmulate = 7; //interrupt sequence
                m_nextSequence = false;
                m_poolIntsAtCycle = NO_POOL;
            }
            else {
                m_opcode = m_bus.read(m_pc);
                m_waitCyclesToEmulate = getOpcodeCyclesHint();
                m_poolIntsAtCycle = OPCODE_INT_POOL_CYCLE_TABLE[m_opcode];
                fetchOperand();
            }
        }

    }

    GERANES_INLINE_HOT void phi1()
    {
        if(!isHalted()) {

            _phi1();

            if(m_realExpectedDiff > 0) {
                m_realExpectedDiff--;
            }
            else {

                if(--m_waitCyclesToEmulate == 0) {

                    if(m_InstructionOrInterruptFlag){
                        emulateInterruptSequence();
                    }
                    else {
                        int expected = getOpcodeCyclesHint();                        
                        emulateOpcode();
                        m_realExpectedDiff = m_instructionCycles - expected;
                    }

                }

            }

        }        
    }

    GERANES_INLINE void phi2(bool nmiState, bool irqState) {

        if(!isHalted()) {

            if(m_poolIntsAtCycle == m_currentInstructionCycle) {
                m_nextSequence = checkInterrupts();
            }

            switch(m_nmiStep) {
            case 0:
                if(!nmiState) m_nmiStep++;
                 break;

            case 1:
                if(nmiState) m_nmiStep++;
                break;
            }

            m_irqStep = irqState;

            m_currentInstructionCycle++;
        }

        m_cyclesCounter++;

        if(m_sleepCycles > 0) --m_sleepCycles;
    }


    GERANES_INLINE void sleep(int cycles)
    {
        m_sleepCycles += cycles;
    }

    GERANES_INLINE bool isOpcodeWriteCycle(int offset = 0) {
        return ((OPCODE_WRITE_CYCLES_TABLE[m_opcode] >> (m_currentInstructionCycle+offset)) & 0x01) != 0;
    }

    GERANES_INLINE bool isOddCycle() {
        return m_cyclesCounter%2 != 0;
    }

    GERANES_INLINE size_t cycleNumber() {
        return m_cyclesCounter;
    }


    GERANES_INLINE bool isHalted() {
        return m_sleepCycles > 0;
    }

    void serialization(SerializationBase& s)
    {
        SERIALIZEDATA(s, m_pc);
        SERIALIZEDATA(s, m_sp);
        SERIALIZEDATA(s, m_status);
        SERIALIZEDATA(s, m_a);
        SERIALIZEDATA(s, m_x);
        SERIALIZEDATA(s, m_y);
        SERIALIZEDATA(s, m_instructionCycles);
        SERIALIZEDATA(s, m_cyclesCounter);
        SERIALIZEDATA(s, m_opcode);
        SERIALIZEDATA(s, m_addr);
        SERIALIZEDATA(s, m_pageCross);
        SERIALIZEDATA(s, m_nmiSignal);
        SERIALIZEDATA(s, m_nmiStep);
        SERIALIZEDATA(s, m_irqSignal);
        SERIALIZEDATA(s, m_irqStep);
        SERIALIZEDATA(s, m_waitCyclesToEmulate);
        SERIALIZEDATA(s, m_InstructionOrInterruptFlag);
        SERIALIZEDATA(s, m_sleepCycles);
        SERIALIZEDATA(s, m_realExpectedDiff);
        SERIALIZEDATA(s, m_currentInstructionCycle);
        SERIALIZEDATA(s, m_interruptCause);

        SERIALIZEDATA(s, m_nmiAtInstructionCycle);
        SERIALIZEDATA(s, m_poolIntsAtCycle);
        SERIALIZEDATA(s, m_nextSequence);
    }

    uint16_t addr() {
        return m_addr;
    }


};



#endif
