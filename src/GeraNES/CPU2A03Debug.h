#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

#include "CPU2A03.h"

struct CPU2A03DebugLine
{
    uint16_t address = 0;
    uint8_t size = 1;
    std::string bytes;
    std::string mnemonic;
    bool isCurrent = false;
};

class CPU2A03Debug
{
public:
    using ReadFn = std::function<uint8_t(uint16_t)>;

    static std::string formatStatus(uint8_t status)
    {
        std::string text;
        text.reserve(8);
        text.push_back((status & 0x80) ? 'N' : 'n');
        text.push_back((status & 0x40) ? 'V' : 'v');
        text.push_back((status & 0x20) ? 'U' : 'u');
        text.push_back((status & 0x10) ? 'B' : 'b');
        text.push_back((status & 0x08) ? 'D' : 'd');
        text.push_back((status & 0x04) ? 'I' : 'i');
        text.push_back((status & 0x02) ? 'Z' : 'z');
        text.push_back((status & 0x01) ? 'C' : 'c');
        return text;
    }

    static std::vector<CPU2A03DebugLine> disassembleAround(
        uint16_t pc,
        int beforeCount,
        int afterCount,
        const ReadFn& read)
    {
        std::vector<CPU2A03DebugLine> lines;
        if(!read) return lines;

        const std::vector<uint16_t> precedingStarts = findPrecedingInstructionStarts(
            pc,
            std::max(beforeCount, 0),
            read
        );

        for(uint16_t start : precedingStarts) {
            lines.push_back(decodeLine(start, pc, read));
        }

        uint16_t addr = pc;
        for(int i = 0; i <= std::max(afterCount, 0); ++i) {
            lines.push_back(decodeLine(addr, pc, read));
            addr = static_cast<uint16_t>(addr + lines.back().size);
        }

        return lines;
    }

private:
    static constexpr std::array<const char*, 256> MNEMONICS = {{
        "BRK","ORA","KIL","SLO","NOP","ORA","ASL","SLO","PHP","ORA","ASL","ANC","NOP","ORA","ASL","SLO",
        "BPL","ORA","KIL","SLO","NOP","ORA","ASL","SLO","CLC","ORA","NOP","SLO","NOP","ORA","ASL","SLO",
        "JSR","AND","KIL","RLA","BIT","AND","ROL","RLA","PLP","AND","ROL","ANC","BIT","AND","ROL","RLA",
        "BMI","AND","NOP","RLA","NOP","AND","ROL","RLA","SEC","AND","NOP","RLA","NOP","AND","ROL","RLA",
        "RTI","EOR","KIL","SRE","NOP","EOR","LSR","SRE","PHA","EOR","LSR","ALR","JMP","EOR","LSR","SRE",
        "BVC","EOR","KIL","SRE","NOP","EOR","LSR","SRE","CLI","EOR","NOP","SRE","NOP","EOR","LSR","SRE",
        "RTS","ADC","KIL","RRA","NOP","ADC","ROR","RRA","PLA","ADC","ROR","ARR","JMP","ADC","ROR","RRA",
        "BVS","ADC","KIL","RRA","NOP","ADC","ROR","RRA","SEI","ADC","NOP","RRA","NOP","ADC","ROR","RRA",
        "NOP","STA","NOP","SAX","STY","STA","STX","SAX","DEY","NOP","TXA","XAA","STY","STA","STX","SAX",
        "BCC","STA","KIL","AHX","STY","STA","STX","SAX","TYA","STA","TXS","TAS","SHY","STA","SHX","AHX",
        "LDY","LDA","LDX","LAX","LDY","LDA","LDX","LAX","TAY","LDA","TAX","LXA","LDY","LDA","LDX","LAX",
        "BCS","LDA","KIL","LAX","LDY","LDA","LDX","LAX","CLV","LDA","TSX","LAS","LDY","LDA","LDX","LAX",
        "CPY","CMP","NOP","DCP","CPY","CMP","DEC","DCP","INY","CMP","DEX","AXS","CPY","CMP","DEC","DCP",
        "BNE","CMP","KIL","DCP","NOP","CMP","DEC","DCP","CLD","CMP","NOP","DCP","NOP","CMP","DEC","DCP",
        "CPX","SBC","NOP","ISB","CPX","SBC","INC","ISB","INX","SBC","NOP","SBC","CPX","SBC","INC","ISB",
        "BEQ","SBC","KIL","ISB","NOP","SBC","INC","ISB","SED","SBC","NOP","ISB","NOP","SBC","INC","ISB"
    }};

    static std::string hex8(uint8_t value)
    {
        std::ostringstream out;
        out << '$' << std::uppercase << std::hex;
        out.width(2);
        out.fill('0');
        out << static_cast<unsigned int>(value);
        return out.str();
    }

    static std::string hex16(uint16_t value)
    {
        std::ostringstream out;
        out << '$' << std::uppercase << std::hex;
        out.width(4);
        out.fill('0');
        out << static_cast<unsigned int>(value);
        return out.str();
    }

    static uint8_t instructionSize(uint8_t opcode)
    {
        switch(addrMode[opcode]) {
            case AddrMode::Abs:
            case AddrMode::AbsX:
            case AddrMode::AbsXW:
            case AddrMode::AbsY:
            case AddrMode::AbsYW:
            case AddrMode::Ind:
                return 3;

            case AddrMode::Imm:
            case AddrMode::Rel:
            case AddrMode::Zero:
            case AddrMode::ZeroX:
            case AddrMode::ZeroY:
            case AddrMode::IndX:
            case AddrMode::IndY:
            case AddrMode::IndYW:
                return 2;

            default:
                return 1;
        }
    }

    static std::string formatOperand(uint16_t address, uint8_t opcode, const ReadFn& read)
    {
        const uint8_t lo = read(static_cast<uint16_t>(address + 1));
        const uint8_t hi = read(static_cast<uint16_t>(address + 2));
        const uint16_t absAddr = static_cast<uint16_t>(lo | (static_cast<uint16_t>(hi) << 8));

        switch(addrMode[opcode]) {
            case AddrMode::Acc: return "A";
            case AddrMode::Imm: return "#" + hex8(lo);
            case AddrMode::Rel:
            {
                const int8_t offset = static_cast<int8_t>(lo);
                const uint16_t target = static_cast<uint16_t>(address + 2 + offset);
                return hex16(target);
            }
            case AddrMode::Zero: return hex8(lo);
            case AddrMode::ZeroX: return hex8(lo) + ",X";
            case AddrMode::ZeroY: return hex8(lo) + ",Y";
            case AddrMode::Abs: return hex16(absAddr);
            case AddrMode::AbsX:
            case AddrMode::AbsXW: return hex16(absAddr) + ",X";
            case AddrMode::AbsY:
            case AddrMode::AbsYW: return hex16(absAddr) + ",Y";
            case AddrMode::Ind: return "(" + hex16(absAddr) + ")";
            case AddrMode::IndX: return "(" + hex8(lo) + ",X)";
            case AddrMode::IndY:
            case AddrMode::IndYW: return "(" + hex8(lo) + "),Y";
            default: return "";
        }
    }

    static CPU2A03DebugLine decodeLine(uint16_t address, uint16_t currentPc, const ReadFn& read)
    {
        CPU2A03DebugLine line;
        line.address = address;
        line.isCurrent = address == currentPc;

        const uint8_t opcode = read(address);
        line.size = instructionSize(opcode);

        std::ostringstream byteStream;
        byteStream << std::uppercase << std::hex;
        for(uint8_t i = 0; i < line.size; ++i) {
            if(i > 0) byteStream << ' ';
            byteStream.width(2);
            byteStream.fill('0');
            byteStream << static_cast<unsigned int>(read(static_cast<uint16_t>(address + i)));
        }
        line.bytes = byteStream.str();

        line.mnemonic = MNEMONICS[opcode];
        const std::string operand = formatOperand(address, opcode, read);
        if(!operand.empty()) {
            line.mnemonic += " " + operand;
        }

        return line;
    }

    static std::vector<uint16_t> findPrecedingInstructionStarts(
        uint16_t pc,
        int beforeCount,
        const ReadFn& read)
    {
        std::vector<uint16_t> starts;
        if(beforeCount <= 0) return starts;

        const uint16_t searchStart = static_cast<uint16_t>(std::max(0, static_cast<int>(pc) - beforeCount * 3 - 8));
        const int targetOffset = static_cast<int>(pc - searchStart);
        std::vector<int> previous(targetOffset + 1, -1);
        previous[0] = 0;

        for(int offset = 0; offset < targetOffset; ++offset) {
            if(previous[offset] < 0) continue;
            const uint16_t address = static_cast<uint16_t>(searchStart + offset);
            const int next = offset + instructionSize(read(address));
            if(next > targetOffset) continue;
            if(previous[next] < 0) {
                previous[next] = offset;
            }
        }

        if(previous[targetOffset] < 0) {
            return starts;
        }

        std::vector<uint16_t> path;
        int offset = targetOffset;
        while(offset > 0) {
            const int startOffset = previous[offset];
            if(startOffset < 0 || startOffset == offset) break;
            path.push_back(static_cast<uint16_t>(searchStart + startOffset));
            offset = startOffset;
        }

        std::reverse(path.begin(), path.end());
        if(static_cast<int>(path.size()) > beforeCount) {
            path.erase(path.begin(), path.end() - beforeCount);
        }
        return path;
    }
};
