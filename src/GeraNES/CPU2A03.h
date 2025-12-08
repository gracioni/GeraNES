#pragma once

#include <array>

#include "defines.h"
#include "IBus.h"
#include "Serialization.h"

#include "Console.h"
#include "PPU.h"
#include "APU/APU.h"
#include "Cartridge.h"

class DMA;

#include "signal/signal.h"

const uint8_t DO_NOT_POOL_INTS = 0xFF;

// BRK (NO_POOL)
// CLI(0x58) SEI(0x78) PLP(0x28) in cycle 1
// some references (https://www.nesdev.org/wiki/CPU_interrupts) says that branch instructions
// 0x90 0xB0 0xF0 0x30 0xD0 0x10 0x50 0x70 are polled in cycle 1
// and all 2 cycles instructions are polled in cycle 1
inline constexpr std::array<uint8_t, 256> OPCODE_INT_POOL_CYCLE_TABLE =
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

inline constexpr std::array<AddrMode, 256> addrMode = {
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
            bool carry : 1;
            bool zero : 1;
            bool irq : 1;
            bool decimal : 1;
            bool brk : 1;
            bool unused : 1;
            bool overflow : 1;
            bool negative : 1;
        } m_flags;
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
        m_flags.negative = value&0x80;
        m_flags.zero = value==0x00;
    }

    GERANES_INLINE_HOT void dummyRead() {
        readMemory(m_pc);
    }

    GERANES_INLINE_HOT void getAddrImmediate()
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

        m_pc = MAKE16(low, high); // reset vector

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

        m_poolIntsAtCycle = DO_NOT_POOL_INTS;

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

        unsigned int result = (unsigned int)m_a + value + (m_flags.carry ? 1 : 0);

        m_flags.carry = result > 0xFF;
        m_flags.overflow = (!((m_a ^ value) & 0x80) && ((m_a ^ result) & 0x80));

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
        writeMemory(m_addr, value); // dummy write

        m_flags.carry = value & 0x80;

        value <<= 1;

        updateZeroAndNegativeFlags(value);

        writeMemory(m_addr, value);
    }

    GERANES_INLINE_HOT void ASL_implied()
    {
        dummyRead();
        m_flags.carry = (m_a & 0x80);
        m_a <<= 1;
        updateZeroAndNegativeFlags(m_a);
    }

    GERANES_INLINE_HOT void AAC()
    {
        const uint8_t value = readMemory(m_addr);
        m_a &= value;

        updateZeroAndNegativeFlags(m_a);

        m_flags.carry = m_flags.negative;
    }

    GERANES_INLINE_HOT void U_SLO()
    {
        // ASL & ORA

        uint8_t value = readMemory(m_addr);
        writeMemory(m_addr, value); // dummy write

        m_flags.carry = value & 0x80;

        value <<= 1;

        m_a = m_a | value;

        updateZeroAndNegativeFlags(m_a);

        writeMemory(m_addr, value);
    }

    GERANES_INLINE_HOT void branch(bool condition)
    {

        int8_t offset = readMemory(m_addr);

        if (condition)
        {
            const uint16_t value = m_pc;
            m_pc += offset;

            dummyRead();

            if ((value ^ m_pc) & 0xFF00)
            {
                m_poolIntsAtCycle = 3;
                dummyRead();
            }
        }
    }

    GERANES_INLINE_HOT void BCC()
    {
        branch(!m_flags.carry);
    }

    GERANES_INLINE_HOT void BCS()
    {
        branch(m_flags.carry);
    }

    GERANES_INLINE_HOT void BEQ()
    {
        branch(m_flags.zero);
    }

    GERANES_INLINE_HOT void BIT()
    {
        const uint8_t value = readMemory(m_addr);

        m_flags.zero = (m_a & value) == 0x00;
        m_flags.negative = value & 0x80;
        m_flags.overflow = value & 0x40;
    }

    GERANES_INLINE_HOT void BMI()
    {
        branch(m_flags.negative);
    }

    GERANES_INLINE_HOT void BNE()
    {
        branch(!m_flags.zero);
    }

    GERANES_INLINE_HOT void BPL()
    {
        branch(!m_flags.negative);
    }

    GERANES_INLINE_HOT void BRK()
    {
        emulateInterruptSequence(true);
    }

    GERANES_INLINE_HOT void BVC()
    {
        branch(!m_flags.overflow);
    }

    GERANES_INLINE_HOT void BVS()
    {
        branch(m_flags.overflow);
    }

    GERANES_INLINE_HOT void CLC()
    {
        dummyRead();
        m_flags.carry = false;
    }

    GERANES_INLINE_HOT void CLD()
    {
        dummyRead();
        m_flags.decimal = false;
    }

    GERANES_INLINE_HOT void CLI()
    {
        dummyRead();
        m_flags.irq = false;
    }

    GERANES_INLINE_HOT void CLV()
    {
        dummyRead();
        m_flags.overflow = false;
    }

    GERANES_INLINE_HOT void CMP()
    {
        uint8_t value = readMemory(m_addr);

        m_flags.carry = m_a >= value;

        value = m_a - value;

        updateZeroAndNegativeFlags(value);
    }

    GERANES_INLINE_HOT void CPX()
    {
        uint8_t value = readMemory(m_addr);

        m_flags.carry = m_x >= value;

        value = m_x - value;

        updateZeroAndNegativeFlags(value);
    }

    GERANES_INLINE_HOT void CPY()
    {
        uint8_t value = readMemory(m_addr);

        m_flags.carry = m_y >= value;

        value = m_y - value;

        updateZeroAndNegativeFlags(value);
    }

    GERANES_INLINE_HOT void DEC()
    {
        uint8_t value = readMemory(m_addr);
        writeMemory(m_addr, value); // dummy write

        value--;

        updateZeroAndNegativeFlags(value);

        writeMemory(m_addr, value);
    }

    GERANES_INLINE_HOT void U_DCP()
    {
        // DEC & CMP

        // DEC
        uint8_t value = readMemory(m_addr);
        writeMemory(m_addr, value); // dummy write
        value--;

        writeMemory(m_addr, value);

        // CMP
        m_flags.carry = m_a >= value;
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
        // CMP & DEX
        const uint8_t value = readMemory(m_addr);
        uint8_t result = (m_a & m_x) - value;

        m_flags.carry = (m_a & m_x) >= value;

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

    GERANES_INLINE_HOT void INC()
    {
        uint8_t value = readMemory(m_addr);
        writeMemory(m_addr, value); // dummy write

        value++;

        updateZeroAndNegativeFlags(value);

        writeMemory(m_addr, value);
    }

    GERANES_INLINE_HOT void U_ISB()
    {
        // INC & SBC
        uint8_t value = readMemory(m_addr);
        writeMemory(m_addr, value); // dummy write
        value++;

        // ADC
        unsigned int result = (unsigned int)m_a + (value ^ 0xFF) + (m_flags.carry ? 1 : 0);
        m_flags.carry = result > 0xFF;
        m_flags.overflow = (!((m_a ^ (value ^ 0xFF)) & 0x80) && ((m_a ^ result) & 0x80));

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
        push16(m_pc - 1);
        m_pc = m_addr;
    }

    GERANES_INLINE_HOT void loadReg(uint8_t &reg)
    {
        reg = readMemory(m_addr);
        updateZeroAndNegativeFlags(reg);
    }

    GERANES_INLINE_HOT void LDA() { loadReg(m_a); }

    GERANES_INLINE_HOT void U_LAX()
    {
        // LDA & LDX
        LDA();
        m_x = m_a;
    }

    GERANES_INLINE_HOT void LDX() { loadReg(m_x); }

    GERANES_INLINE_HOT void LDY() { loadReg(m_y); }

    GERANES_INLINE_HOT void LSR()
    {
        uint8_t value = readMemory(m_addr);
        writeMemory(m_addr, value); // dummy write
        m_flags.carry = value & 0x01;
        value >>= 1;
        updateZeroAndNegativeFlags(value);
        writeMemory(m_addr, value);
    }

    GERANES_INLINE_HOT void LSR_implied()
    {
        dummyRead();
        m_flags.carry = m_a & 0x01;
        m_a >>= 1;
        updateZeroAndNegativeFlags(m_a);
    }

    GERANES_INLINE_HOT void U_ASR()
    {
        const uint8_t value = readMemory(m_addr);

        m_a &= value;

        m_flags.carry = m_a & 0x01;

        m_a >>= 1;

        updateZeroAndNegativeFlags(m_a);
    }

    GERANES_INLINE_HOT void U_SRE()
    {
        // ROL & AND
        uint8_t value = readMemory(m_addr);
        writeMemory(m_addr, value); // dummy write

        m_flags.carry = value & 0x01;
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
        m_flags.brk = true;
        m_flags.unused = true;
        push(m_status);
    }

    GERANES_INLINE_HOT void PLA()
    {
        dummyRead();
        dummyRead(); // GERA REMOVER timing erro
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
        writeMemory(m_addr, value); // dummy write

        bool outputCarry = value & 0x80;

        value <<= 1;
        if (m_flags.carry)
            value |= 0x01;

        m_flags.carry = outputCarry;
        updateZeroAndNegativeFlags(value);

        writeMemory(m_addr, value);
    }

    GERANES_INLINE_HOT void ROL_implied()
    {
        dummyRead();

        bool outputCarry = m_a & 0x80;

        m_a <<= 1;
        if (m_flags.carry)
            m_a |= 0x01;

        m_flags.carry = outputCarry;
        updateZeroAndNegativeFlags(m_a);
    }

    GERANES_INLINE_HOT void U_RLA()
    {
        // LSR & EOR

        uint8_t value = readMemory(m_addr);
        writeMemory(m_addr, value); // dummy write

        // ROL
        bool outputCarry = value & 0x80;

        value <<= 1;
        if (m_flags.carry)
            value |= 0x01;

        m_flags.carry = outputCarry;

        m_a &= value;

        updateZeroAndNegativeFlags(m_a);

        writeMemory(m_addr, value);
    }

    GERANES_INLINE_HOT void ROR()
    {
        uint8_t value = readMemory(m_addr);
        writeMemory(m_addr, value); // dummy write

        bool outputCarry = value & 0x01;

        value >>= 1;
        if (m_flags.carry)
            value |= 0x80;

        m_flags.carry = outputCarry;
        updateZeroAndNegativeFlags(value);

        writeMemory(m_addr, value);
    }

    GERANES_INLINE_HOT void ROR_implied()
    {
        dummyRead();

        bool outputCarry = m_a & 0x01;

        m_a >>= 1;
        if (m_flags.carry)
            m_a |= 0x80;

        m_flags.carry = outputCarry;
        updateZeroAndNegativeFlags(m_a);
    }

    void U_ARR()
    {
        const uint8_t value = readMemory(m_addr);

        m_a = ((m_a & value) >> 1) | (m_flags.carry ? 0x80 : 0x00);
        updateZeroAndNegativeFlags(m_a);
        m_flags.carry = m_a & 0x40;
        m_flags.overflow = (m_flags.carry ? 0x01 : 0x00) ^ ((m_a >> 5) & 0x01);
    }

    GERANES_INLINE_HOT void U_RRA()
    {
        // ROR & ADC

        // ROR
        uint8_t value = readMemory(m_addr);
        writeMemory(m_addr, value); // dummy write

        bool outputCarry = value & 0x01;

        value >>= 1;
        if (m_flags.carry)
            value |= 0x80;

        m_flags.carry = outputCarry;

        // ADC
        unsigned int result = (unsigned int)m_a + value + (m_flags.carry ? 1 : 0);
        m_flags.carry = result > 0xFF;
        m_flags.overflow = (!((m_a ^ value) & 0x80) && ((m_a ^ result) & 0x80));

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
        m_pc = pull16() + 1;
    }

    GERANES_INLINE_HOT void SBC()
    {
        const uint8_t value = readMemory(m_addr);

        const unsigned int result = (unsigned int)m_a - value - (m_flags.carry ? 0 : 1);

        m_flags.carry = (result < 0x100);
        m_flags.overflow = (((m_a ^ result) & 0x80) && ((m_a ^ value) & 0x80));

        m_a = result & 0xFF;

        updateZeroAndNegativeFlags(m_a);
    }

    GERANES_INLINE_HOT void U_HLT()
    {
    }

    GERANES_INLINE_HOT void SEC()
    {
        dummyRead();
        m_flags.carry = true;
    }

    GERANES_INLINE_HOT void SED()
    {
        dummyRead();
        m_flags.decimal = true;
    }

    GERANES_INLINE_HOT void SEI()
    {
        dummyRead();
        m_flags.irq = true;
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
        // STA & STX
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
        // LDA & TAX
        const uint8_t value = readMemory(m_addr);
        m_a = value; // LDA
        m_x = m_a;   // TAX

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
        // This may not be the actual behavior, but the read/write operations are needed for proper cycle counting
        writeMemory(m_addr, ((m_addr >> 8) + 1) & m_a & m_x);
    }

    GERANES_INLINE_HOT void U_TAS()
    {
        //"AND X register with accumulator and store result in stack
        // pointer, then AND stack pointer with the high byte of the
        // target address of the argument + 1. Store result in memory."
        m_sp = m_x & m_a;
        writeMemory(m_addr, m_sp & ((m_addr >> 8) + 1));
    }

    GERANES_INLINE_HOT void U_SYA()
    {

        const uint8_t addrHigh = m_addr >> 8;
        const uint8_t addrLow = m_addr & 0xFF;
        const uint8_t value = m_y & (addrHigh + 1);

        // From here: http://forums.nesdev.com/viewtopic.php?f=3&t=3831&start=30
        // Unsure if this is accurate or not
        //"the target address for e.g. SYA becomes ((y & (addr_high + 1)) << 8) | addr_low instead of the normal ((addr_high + 1) << 8) | addr_low"
        writeMemory(((m_y & (addrHigh + 1)) << 8) | addrLow, value);
    }

    GERANES_INLINE_HOT void U_SXA()
    {
        const uint8_t addrHigh = m_addr >> 8;
        const uint8_t addrLow = m_addr & 0xFF;
        const uint8_t value = m_x & (addrHigh + 1);
        writeMemory(((m_x & (addrHigh + 1)) << 8) | addrLow, value);
    }

    GERANES_INLINE_HOT void U_LAS()
    {
        //"AND memory with stack pointer, transfer result to accumulator, X register and stack pointer."
        const uint8_t value = readMemory(m_addr);
        m_a = value & m_sp;
        m_x = m_a;
        m_sp = m_a;
        updateZeroAndNegativeFlags(m_a);
    }

    GERANES_INLINE_HOT void U_UNK()
    {
        dummyRead();
    }

    GERANES_INLINE_HOT void checkInterrupts()
    {
        if( (m_nmiSignal) || (m_irqSignal && m_flags.irq == false))
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
        m_flags.brk = isBrk;
        m_flags.unused = true;
        push(m_status);
        m_flags.irq = true;   

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
            case AddrMode::Imm: getAddrImmediate(); break;
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

    void emulateOpcode();

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

GERANES_INLINE_HOT void CPU2A03::beginCycle() {

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

using OpFunc = void (CPU2A03::*)();

inline constexpr std::array<OpFunc, 256> OPCODE_TABLE = {
//          0                1                2                3                4                5                6                7                8                9                A                B                C                D                E                F
/*0x00*/ &CPU2A03::BRK,     &CPU2A03::ORA,     &CPU2A03::U_HLT,  &CPU2A03::U_SLO,  &CPU2A03::U_DOP,  &CPU2A03::ORA,     &CPU2A03::ASL,     &CPU2A03::U_SLO,  &CPU2A03::PHP,     &CPU2A03::ORA,     &CPU2A03::ASL_implied, &CPU2A03::AAC,  &CPU2A03::U_DOP,  &CPU2A03::ORA,     &CPU2A03::ASL,     &CPU2A03::U_SLO,
/*0x10*/ &CPU2A03::BPL,     &CPU2A03::ORA,     &CPU2A03::U_HLT,  &CPU2A03::U_SLO,  &CPU2A03::U_DOP,  &CPU2A03::ORA,     &CPU2A03::ASL,     &CPU2A03::U_SLO,  &CPU2A03::CLC,     &CPU2A03::ORA,     &CPU2A03::NOP,         &CPU2A03::U_SLO,  &CPU2A03::U_DOP,  &CPU2A03::ORA,     &CPU2A03::ASL,     &CPU2A03::U_SLO,
/*0x20*/ &CPU2A03::JSR,     &CPU2A03::AND,     &CPU2A03::U_HLT,  &CPU2A03::U_RLA,  &CPU2A03::BIT,    &CPU2A03::AND,     &CPU2A03::ROL,     &CPU2A03::U_RLA,  &CPU2A03::PLP,     &CPU2A03::AND,     &CPU2A03::ROL_implied, &CPU2A03::AAC,  &CPU2A03::BIT,    &CPU2A03::AND,     &CPU2A03::ROL,     &CPU2A03::U_RLA,
/*0x30*/ &CPU2A03::BMI,     &CPU2A03::AND,     &CPU2A03::NOP,    &CPU2A03::U_RLA,  &CPU2A03::U_DOP,  &CPU2A03::AND,     &CPU2A03::ROL,     &CPU2A03::U_RLA,  &CPU2A03::SEC,     &CPU2A03::AND,     &CPU2A03::NOP,         &CPU2A03::U_RLA,  &CPU2A03::U_DOP,  &CPU2A03::AND,     &CPU2A03::ROL,     &CPU2A03::U_RLA,
/*0x40*/ &CPU2A03::RTI,     &CPU2A03::EOR,     &CPU2A03::U_HLT,  &CPU2A03::U_SRE,  &CPU2A03::U_DOP,  &CPU2A03::EOR,     &CPU2A03::LSR,     &CPU2A03::U_SRE,  &CPU2A03::PHA,     &CPU2A03::EOR,     &CPU2A03::LSR_implied, &CPU2A03::U_ASR, &CPU2A03::JMP,    &CPU2A03::EOR,     &CPU2A03::LSR,     &CPU2A03::U_SRE,
/*0x50*/ &CPU2A03::BVC,     &CPU2A03::EOR,     &CPU2A03::U_HLT,  &CPU2A03::U_SRE,  &CPU2A03::U_DOP,  &CPU2A03::EOR,     &CPU2A03::LSR,     &CPU2A03::U_SRE,  &CPU2A03::CLI,     &CPU2A03::EOR,     &CPU2A03::NOP,         &CPU2A03::U_SRE,  &CPU2A03::U_DOP,  &CPU2A03::EOR,     &CPU2A03::LSR,     &CPU2A03::U_SRE,
/*0x60*/ &CPU2A03::RTS,     &CPU2A03::ADC,     &CPU2A03::U_HLT,  &CPU2A03::U_RRA,  &CPU2A03::U_DOP,  &CPU2A03::ADC,     &CPU2A03::ROR,     &CPU2A03::U_RRA,  &CPU2A03::PLA,     &CPU2A03::ADC,     &CPU2A03::ROR_implied, &CPU2A03::U_ARR, &CPU2A03::JMP,    &CPU2A03::ADC,     &CPU2A03::ROR,     &CPU2A03::U_RRA,
/*0x70*/ &CPU2A03::BVS,     &CPU2A03::ADC,     &CPU2A03::U_HLT,  &CPU2A03::U_RRA,  &CPU2A03::U_DOP,  &CPU2A03::ADC,     &CPU2A03::ROR,     &CPU2A03::U_RRA,  &CPU2A03::SEI,     &CPU2A03::ADC,     &CPU2A03::NOP,         &CPU2A03::U_RRA,  &CPU2A03::U_DOP,  &CPU2A03::ADC,     &CPU2A03::ROR,     &CPU2A03::U_RRA,
/*0x80*/ &CPU2A03::U_DOP,    &CPU2A03::STA,     &CPU2A03::U_DOP,  &CPU2A03::U_SAX,  &CPU2A03::STY,    &CPU2A03::STA,     &CPU2A03::STX,     &CPU2A03::U_SAX,  &CPU2A03::DEY,     &CPU2A03::U_DOP,    &CPU2A03::TXA,         &CPU2A03::U_UNK, &CPU2A03::STY,    &CPU2A03::STA,     &CPU2A03::STX,     &CPU2A03::U_SAX,
/*0x90*/ &CPU2A03::BCC,     &CPU2A03::STA,     &CPU2A03::U_HLT,  &CPU2A03::U_AXA,  &CPU2A03::STY,    &CPU2A03::STA,     &CPU2A03::STX,     &CPU2A03::U_SAX,  &CPU2A03::TYA,     &CPU2A03::STA,     &CPU2A03::TXS,         &CPU2A03::U_TAS, &CPU2A03::U_SYA,   &CPU2A03::STA,     &CPU2A03::U_SXA,   &CPU2A03::U_AXA,
/*0xA0*/ &CPU2A03::LDY,     &CPU2A03::LDA,     &CPU2A03::LDX,    &CPU2A03::U_LAX,  &CPU2A03::LDY,    &CPU2A03::LDA,     &CPU2A03::LDX,     &CPU2A03::U_LAX,  &CPU2A03::TAY,     &CPU2A03::LDA,     &CPU2A03::TAX,         &CPU2A03::U_ATX, &CPU2A03::LDY,    &CPU2A03::LDA,     &CPU2A03::LDX,     &CPU2A03::U_LAX,
/*0xB0*/ &CPU2A03::BCS,     &CPU2A03::LDA,     &CPU2A03::U_HLT,  &CPU2A03::U_LAX,  &CPU2A03::LDY,    &CPU2A03::LDA,     &CPU2A03::LDX,     &CPU2A03::U_LAX,  &CPU2A03::CLV,     &CPU2A03::LDA,     &CPU2A03::TSX,         &CPU2A03::U_LAS, &CPU2A03::LDY,    &CPU2A03::LDA,     &CPU2A03::LDX,     &CPU2A03::U_LAX,
/*0xC0*/ &CPU2A03::CPY,     &CPU2A03::CMP,     &CPU2A03::U_DOP,  &CPU2A03::U_DCP,  &CPU2A03::CPY,    &CPU2A03::CMP,     &CPU2A03::DEC,     &CPU2A03::U_DCP,  &CPU2A03::INY,     &CPU2A03::CMP,     &CPU2A03::DEX,         &CPU2A03::U_AXS, &CPU2A03::CPY,    &CPU2A03::CMP,     &CPU2A03::DEC,     &CPU2A03::U_DCP,
/*0xD0*/ &CPU2A03::BNE,     &CPU2A03::CMP,     &CPU2A03::U_HLT,  &CPU2A03::U_DCP,  &CPU2A03::U_DOP,  &CPU2A03::CMP,     &CPU2A03::DEC,     &CPU2A03::U_DCP,  &CPU2A03::CLD,     &CPU2A03::CMP,     &CPU2A03::NOP,         &CPU2A03::U_DCP,  &CPU2A03::U_DOP,  &CPU2A03::CMP,     &CPU2A03::DEC,     &CPU2A03::U_DCP,
/*0xE0*/ &CPU2A03::CPX,     &CPU2A03::SBC,     &CPU2A03::U_DOP,  &CPU2A03::U_ISB,  &CPU2A03::CPX,    &CPU2A03::SBC,     &CPU2A03::INC,     &CPU2A03::U_ISB,  &CPU2A03::INX,     &CPU2A03::SBC,     &CPU2A03::NOP,         &CPU2A03::SBC,  &CPU2A03::CPX,    &CPU2A03::SBC,     &CPU2A03::INC,     &CPU2A03::U_ISB,
/*0xF0*/ &CPU2A03::BEQ,     &CPU2A03::SBC,     &CPU2A03::U_HLT,  &CPU2A03::U_ISB,  &CPU2A03::U_DOP,  &CPU2A03::SBC,     &CPU2A03::INC,     &CPU2A03::U_ISB,  &CPU2A03::SED,     &CPU2A03::SBC,     &CPU2A03::NOP,         &CPU2A03::U_ISB,  &CPU2A03::U_DOP,  &CPU2A03::SBC,     &CPU2A03::INC,     &CPU2A03::U_ISB,
};

GERANES_INLINE_HOT void CPU2A03::emulateOpcode() {
    (this->*OPCODE_TABLE[m_opcode])();
}
