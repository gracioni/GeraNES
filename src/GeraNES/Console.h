#pragma once

namespace GeraNES {

class CPU2A03;
class PPU;
class APU;
class Cartridge;

class Console {

    private:

        CPU2A03& m_cpu;
        PPU& m_ppu;
        APU& m_apu;
        Cartridge& m_cartridge;
    
    public:

        Console(CPU2A03& cpu, PPU& ppu, APU& apu, Cartridge& cartridge) :
        m_cpu(cpu), m_ppu(ppu), m_apu(apu), m_cartridge(cartridge)
        {
        }

        CPU2A03& cpu() { return m_cpu; }
        PPU& ppu() { return m_ppu; }
        APU& apu() { return m_apu; }
        Cartridge& cartridge() { return m_cartridge; }
        const CPU2A03& cpu() const { return m_cpu; }
        const PPU& ppu() const { return m_ppu; }
        const APU& apu() const { return m_apu; }
        const Cartridge& cartridge() const { return m_cartridge; }

};

} // namespace GeraNES
