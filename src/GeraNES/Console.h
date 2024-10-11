#ifndef CONSOLE_H
#define CONSOLE_H

class CPU2A03;
class PPU;
class DMA;
class APU;
class Cartridge;

class Console {

    private:

        CPU2A03& m_cpu;
        PPU& m_ppu;
        DMA& m_dma;
        APU& m_apu;
        Cartridge& m_cartridge;
    
    public:

        Console(CPU2A03& cpu, PPU& ppu, DMA& dma, APU& apu, Cartridge& cartridge) :
        m_cpu(cpu), m_ppu(ppu), m_dma(dma), m_apu(apu), m_cartridge(cartridge)
        {
        }

        CPU2A03& cpu() { return m_cpu; }
        PPU& ppu() { return m_ppu; }
        DMA& dma() { return m_dma; }
        APU& apu() { return m_apu; }
        Cartridge& cartridge() { return m_cartridge; }

};

#endif