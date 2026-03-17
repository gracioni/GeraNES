#pragma once

#include <cassert>
#include <cstdint>
#include <functional>
#include <utility>

class NesAssembler
{
private:
    std::function<void(uint16_t, uint8_t)> m_emit;
    uint16_t m_pc;

    void emitByteInternal(uint8_t value)
    {
        m_emit(m_pc++, value);
    }

    void emitWordInternal(uint16_t value)
    {
        emitByteInternal(static_cast<uint8_t>(value & 0xFF));
        emitByteInternal(static_cast<uint8_t>((value >> 8) & 0xFF));
    }

    void emitOp(uint8_t opcode)
    {
        emitByteInternal(opcode);
    }

    void emitOpImm(uint8_t opcode, uint8_t value)
    {
        emitOp(opcode);
        emitByteInternal(value);
    }

    void emitOpZero(uint8_t opcode, uint8_t addr)
    {
        emitOp(opcode);
        emitByteInternal(addr);
    }

    void emitOpAbs(uint8_t opcode, uint16_t addr)
    {
        emitOp(opcode);
        emitWordInternal(addr);
    }

    void emitOpRel(uint8_t opcode, uint16_t targetAddr)
    {
        emitOp(opcode);
        const int rel = static_cast<int>(targetAddr) - static_cast<int>(m_pc + 1);
        assert(rel >= -128 && rel <= 127);
        emitByteInternal(static_cast<uint8_t>(rel));
    }

public:
    NesAssembler(std::function<void(uint16_t, uint8_t)> emit, uint16_t startAddr)
        : m_emit(std::move(emit))
        , m_pc(startAddr)
    {
    }

    uint16_t position() const
    {
        return m_pc;
    }

    void byte(uint8_t value)
    {
        emitByteInternal(value);
    }

    void word(uint16_t value)
    {
        emitWordInternal(value);
    }

    void opcode(uint8_t value)
    {
        emitOp(value);
    }

#define NES_ASM_IMP(name, opcodeValue) \
    void name() \
    { \
        emitOp(opcodeValue); \
    }

#define NES_ASM_ACC(name, opcodeValue) \
    void name() \
    { \
        emitOp(opcodeValue); \
    }

#define NES_ASM_IMM(name, opcodeValue) \
    void name(uint8_t value) \
    { \
        emitOpImm(opcodeValue, value); \
    }

#define NES_ASM_ZERO(name, opcodeValue) \
    void name(uint8_t addr) \
    { \
        emitOpZero(opcodeValue, addr); \
    }

#define NES_ASM_ABS(name, opcodeValue) \
    void name(uint16_t addr) \
    { \
        emitOpAbs(opcodeValue, addr); \
    }

#define NES_ASM_REL(name, opcodeValue) \
    void name(uint16_t targetAddr) \
    { \
        emitOpRel(opcodeValue, targetAddr); \
    }

    NES_ASM_IMP(brk, 0x00)

    NES_ASM_ZERO(oraIndX, 0x01)
    NES_ASM_IMP(uHlt, 0x02)
    NES_ASM_ZERO(uSloIndX, 0x03)
    NES_ASM_ZERO(uDopZero, 0x04)
    NES_ASM_ZERO(oraZero, 0x05)
    NES_ASM_ZERO(aslZero, 0x06)
    NES_ASM_ZERO(uSloZero, 0x07)
    NES_ASM_IMP(php, 0x08)
    NES_ASM_IMM(oraImm, 0x09)
    NES_ASM_ACC(aslAcc, 0x0A)
    NES_ASM_IMM(aacImm, 0x0B)
    NES_ASM_ABS(uDopAbs, 0x0C)
    NES_ASM_ABS(oraAbs, 0x0D)
    NES_ASM_ABS(aslAbs, 0x0E)
    NES_ASM_ABS(uSloAbs, 0x0F)

    NES_ASM_REL(bpl, 0x10)
    NES_ASM_ZERO(oraIndY, 0x11)
    NES_ASM_ZERO(uSloIndY, 0x13)
    NES_ASM_ZERO(uDopZeroX, 0x14)
    NES_ASM_ZERO(oraZeroX, 0x15)
    NES_ASM_ZERO(aslZeroX, 0x16)
    NES_ASM_ZERO(uSloZeroX, 0x17)
    NES_ASM_IMP(clc, 0x18)
    NES_ASM_ABS(oraAbsY, 0x19)
    NES_ASM_IMP(nop, 0xEA)
    NES_ASM_IMP(nopAlt, 0x1A)
    NES_ASM_ABS(uSloAbsY, 0x1B)
    NES_ASM_ABS(uDopAbsX, 0x1C)
    NES_ASM_ABS(oraAbsX, 0x1D)
    NES_ASM_ABS(aslAbsX, 0x1E)
    NES_ASM_ABS(uSloAbsX, 0x1F)

    NES_ASM_ABS(jsr, 0x20)
    NES_ASM_ZERO(andIndX, 0x21)
    NES_ASM_ZERO(uRlaIndX, 0x23)
    NES_ASM_ZERO(bitZero, 0x24)
    NES_ASM_ZERO(andZero, 0x25)
    NES_ASM_ZERO(rolZero, 0x26)
    NES_ASM_ZERO(uRlaZero, 0x27)
    NES_ASM_IMP(plp, 0x28)
    NES_ASM_IMM(andImm, 0x29)
    NES_ASM_ACC(rolAcc, 0x2A)
    NES_ASM_IMM(aacImmAlt, 0x2B)
    NES_ASM_ABS(bitAbs, 0x2C)
    NES_ASM_ABS(andAbs, 0x2D)
    NES_ASM_ABS(rolAbs, 0x2E)
    NES_ASM_ABS(uRlaAbs, 0x2F)

    NES_ASM_REL(bmi, 0x30)
    NES_ASM_ZERO(andIndY, 0x31)
    NES_ASM_IMP(nopAlt2, 0x32)
    NES_ASM_ZERO(uRlaIndY, 0x33)
    NES_ASM_ZERO(andZeroX, 0x35)
    NES_ASM_ZERO(rolZeroX, 0x36)
    NES_ASM_ZERO(uRlaZeroX, 0x37)
    NES_ASM_IMP(sec, 0x38)
    NES_ASM_ABS(andAbsY, 0x39)
    NES_ASM_ABS(uRlaAbsY, 0x3B)
    NES_ASM_ABS(andAbsX, 0x3D)
    NES_ASM_ABS(rolAbsX, 0x3E)
    NES_ASM_ABS(uRlaAbsX, 0x3F)

    NES_ASM_IMP(rti, 0x40)
    NES_ASM_ZERO(eorIndX, 0x41)
    NES_ASM_ZERO(uSreIndX, 0x43)
    NES_ASM_ZERO(eorZero, 0x45)
    NES_ASM_ZERO(lsrZero, 0x46)
    NES_ASM_ZERO(uSreZero, 0x47)
    NES_ASM_IMP(pha, 0x48)
    NES_ASM_IMM(eorImm, 0x49)
    NES_ASM_ACC(lsrAcc, 0x4A)
    NES_ASM_IMM(uAsrImm, 0x4B)
    NES_ASM_ABS(jmp, 0x4C)
    NES_ASM_ABS(eorAbs, 0x4D)
    NES_ASM_ABS(lsrAbs, 0x4E)
    NES_ASM_ABS(uSreAbs, 0x4F)

    NES_ASM_REL(bvc, 0x50)
    NES_ASM_ZERO(eorIndY, 0x51)
    NES_ASM_ZERO(uSreIndY, 0x53)
    NES_ASM_ZERO(eorZeroX, 0x55)
    NES_ASM_ZERO(lsrZeroX, 0x56)
    NES_ASM_ZERO(uSreZeroX, 0x57)
    NES_ASM_IMP(cli, 0x58)
    NES_ASM_ABS(eorAbsY, 0x59)
    NES_ASM_ABS(uSreAbsY, 0x5B)
    NES_ASM_ABS(eorAbsX, 0x5D)
    NES_ASM_ABS(lsrAbsX, 0x5E)
    NES_ASM_ABS(uSreAbsX, 0x5F)

    NES_ASM_IMP(rts, 0x60)
    NES_ASM_ZERO(adcIndX, 0x61)
    NES_ASM_ZERO(uRraIndX, 0x63)
    NES_ASM_ZERO(adcZero, 0x65)
    NES_ASM_ZERO(rorZero, 0x66)
    NES_ASM_ZERO(uRraZero, 0x67)
    NES_ASM_IMP(pla, 0x68)
    NES_ASM_IMM(adcImm, 0x69)
    NES_ASM_ACC(rorAcc, 0x6A)
    NES_ASM_IMM(uArrImm, 0x6B)
    NES_ASM_ABS(jmpInd, 0x6C)
    NES_ASM_ABS(adcAbs, 0x6D)
    NES_ASM_ABS(rorAbs, 0x6E)
    NES_ASM_ABS(uRraAbs, 0x6F)

    NES_ASM_REL(bvs, 0x70)
    NES_ASM_ZERO(adcIndY, 0x71)
    NES_ASM_ZERO(uRraIndY, 0x73)
    NES_ASM_ZERO(adcZeroX, 0x75)
    NES_ASM_ZERO(rorZeroX, 0x76)
    NES_ASM_ZERO(uRraZeroX, 0x77)
    NES_ASM_IMP(sei, 0x78)
    NES_ASM_ABS(adcAbsY, 0x79)
    NES_ASM_ABS(uRraAbsY, 0x7B)
    NES_ASM_ABS(adcAbsX, 0x7D)
    NES_ASM_ABS(rorAbsX, 0x7E)
    NES_ASM_ABS(uRraAbsX, 0x7F)

    NES_ASM_IMM(uDopImm, 0x80)
    NES_ASM_ZERO(staIndX, 0x81)
    NES_ASM_ZERO(uSaxIndX, 0x83)
    NES_ASM_ZERO(styZero, 0x84)
    NES_ASM_ZERO(staZero, 0x85)
    NES_ASM_ZERO(stxZero, 0x86)
    NES_ASM_ZERO(uSaxZero, 0x87)
    NES_ASM_IMP(dey, 0x88)
    NES_ASM_IMM(uDopImmAlt1, 0x89)
    NES_ASM_IMP(txa, 0x8A)
    NES_ASM_IMM(uUnkImm, 0x8B)
    NES_ASM_ABS(styAbs, 0x8C)
    NES_ASM_ABS(staAbs, 0x8D)
    NES_ASM_ABS(stxAbs, 0x8E)
    NES_ASM_ABS(uSaxAbs, 0x8F)

    NES_ASM_REL(bcc, 0x90)
    NES_ASM_ZERO(staIndY, 0x91)
    NES_ASM_ZERO(uAxaIndY, 0x93)
    NES_ASM_ZERO(styZeroX, 0x94)
    NES_ASM_ZERO(staZeroX, 0x95)
    NES_ASM_ZERO(stxZeroY, 0x96)
    NES_ASM_ZERO(uSaxZeroY, 0x97)
    NES_ASM_IMP(tya, 0x98)
    NES_ASM_ABS(staAbsY, 0x99)
    NES_ASM_IMP(txs, 0x9A)
    NES_ASM_ABS(uTasAbsY, 0x9B)
    NES_ASM_ABS(uSyaAbsX, 0x9C)
    NES_ASM_ABS(staAbsX, 0x9D)
    NES_ASM_ABS(uSxaAbsY, 0x9E)
    NES_ASM_ABS(uAxaAbsY, 0x9F)

    NES_ASM_IMM(ldyImm, 0xA0)
    NES_ASM_ZERO(ldaIndX, 0xA1)
    NES_ASM_IMM(ldxImm, 0xA2)
    NES_ASM_ZERO(uLaxIndX, 0xA3)
    NES_ASM_ZERO(ldyZero, 0xA4)
    NES_ASM_ZERO(ldaZero, 0xA5)
    NES_ASM_ZERO(ldxZero, 0xA6)
    NES_ASM_ZERO(uLaxZero, 0xA7)
    NES_ASM_IMP(tay, 0xA8)
    NES_ASM_IMM(ldaImm, 0xA9)
    NES_ASM_IMP(tax, 0xAA)
    NES_ASM_IMM(uAtxImm, 0xAB)
    NES_ASM_ABS(ldyAbs, 0xAC)
    NES_ASM_ABS(ldaAbs, 0xAD)
    NES_ASM_ABS(ldxAbs, 0xAE)
    NES_ASM_ABS(uLaxAbs, 0xAF)

    NES_ASM_REL(bcs, 0xB0)
    NES_ASM_ZERO(ldaIndY, 0xB1)
    NES_ASM_ZERO(uLaxIndY, 0xB3)
    NES_ASM_ZERO(ldyZeroX, 0xB4)
    NES_ASM_ZERO(ldaZeroX, 0xB5)
    NES_ASM_ZERO(ldxZeroY, 0xB6)
    NES_ASM_ZERO(uLaxZeroY, 0xB7)
    NES_ASM_IMP(clv, 0xB8)
    NES_ASM_ABS(ldaAbsY, 0xB9)
    NES_ASM_IMP(tsx, 0xBA)
    NES_ASM_ABS(uLasAbsY, 0xBB)
    NES_ASM_ABS(ldyAbsX, 0xBC)
    NES_ASM_ABS(ldaAbsX, 0xBD)
    NES_ASM_ABS(ldxAbsY, 0xBE)
    NES_ASM_ABS(uLaxAbsY, 0xBF)

    NES_ASM_IMM(cpyImm, 0xC0)
    NES_ASM_ZERO(cmpIndX, 0xC1)
    NES_ASM_IMM(uDopImmAlt2, 0xC2)
    NES_ASM_ZERO(uDcpIndX, 0xC3)
    NES_ASM_ZERO(cpyZero, 0xC4)
    NES_ASM_ZERO(cmpZero, 0xC5)
    NES_ASM_ZERO(decZero, 0xC6)
    NES_ASM_ZERO(uDcpZero, 0xC7)
    NES_ASM_IMP(iny, 0xC8)
    NES_ASM_IMM(cmpImm, 0xC9)
    NES_ASM_IMP(dex, 0xCA)
    NES_ASM_IMM(uAxsImm, 0xCB)
    NES_ASM_ABS(cpyAbs, 0xCC)
    NES_ASM_ABS(cmpAbs, 0xCD)
    NES_ASM_ABS(decAbs, 0xCE)
    NES_ASM_ABS(uDcpAbs, 0xCF)

    NES_ASM_REL(bne, 0xD0)
    NES_ASM_ZERO(cmpIndY, 0xD1)
    NES_ASM_ZERO(uDcpIndY, 0xD3)
    NES_ASM_ZERO(uDopZeroXAlt1, 0xD4)
    NES_ASM_ZERO(cmpZeroX, 0xD5)
    NES_ASM_ZERO(decZeroX, 0xD6)
    NES_ASM_ZERO(uDcpZeroX, 0xD7)
    NES_ASM_IMP(cld, 0xD8)
    NES_ASM_ABS(cmpAbsY, 0xD9)
    NES_ASM_ABS(uDcpAbsY, 0xDB)
    NES_ASM_ABS(uDopAbsXAlt1, 0xDC)
    NES_ASM_ABS(cmpAbsX, 0xDD)
    NES_ASM_ABS(decAbsX, 0xDE)
    NES_ASM_ABS(uDcpAbsX, 0xDF)

    NES_ASM_IMM(cpxImm, 0xE0)
    NES_ASM_ZERO(sbcIndX, 0xE1)
    NES_ASM_IMM(uDopImmAlt3, 0xE2)
    NES_ASM_ZERO(uIsbIndX, 0xE3)
    NES_ASM_ZERO(cpxZero, 0xE4)
    NES_ASM_ZERO(sbcZero, 0xE5)
    NES_ASM_ZERO(incZero, 0xE6)
    NES_ASM_ZERO(uIsbZero, 0xE7)
    NES_ASM_IMP(inx, 0xE8)
    NES_ASM_IMM(sbcImm, 0xE9)
    NES_ASM_IMM(sbcImmAlt, 0xEB)
    NES_ASM_ABS(cpxAbs, 0xEC)
    NES_ASM_ABS(sbcAbs, 0xED)
    NES_ASM_ABS(incAbs, 0xEE)
    NES_ASM_ABS(uIsbAbs, 0xEF)

    NES_ASM_REL(beq, 0xF0)
    NES_ASM_ZERO(sbcIndY, 0xF1)
    NES_ASM_ZERO(uIsbIndY, 0xF3)
    NES_ASM_ZERO(uDopZeroXAlt2, 0xF4)
    NES_ASM_ZERO(sbcZeroX, 0xF5)
    NES_ASM_ZERO(incZeroX, 0xF6)
    NES_ASM_ZERO(uIsbZeroX, 0xF7)
    NES_ASM_IMP(sed, 0xF8)
    NES_ASM_ABS(sbcAbsY, 0xF9)
    NES_ASM_ABS(uIsbAbsY, 0xFB)
    NES_ASM_ABS(uDopAbsXAlt2, 0xFC)
    NES_ASM_ABS(sbcAbsX, 0xFD)
    NES_ASM_ABS(incAbsX, 0xFE)
    NES_ASM_ABS(uIsbAbsX, 0xFF)

#undef NES_ASM_REL
#undef NES_ASM_ABS
#undef NES_ASM_ZERO
#undef NES_ASM_IMM
#undef NES_ASM_ACC
#undef NES_ASM_IMP
};
