#ifndef MAPPER119_H
#define MAPPER119_H

#include "Mapper004.h"

class Mapper119 : public Mapper004 {

private:

    const uint8_t CHRRAM_BIT_MASK = 0x40; //bit 6

public:

    Mapper119(ICartridgeData& cd) : Mapper004(cd)
    {
    }

    template<int WindowSize>
    GERANES_INLINE uint8_t readChrRam(int bank, int addr)
    {
        addr = bank*WindowSize + (addr&(WindowSize-1));
        addr = addr&(W8K-1);

        return getChrRam()[addr];
    }

    template<int WindowSize>
    GERANES_INLINE void writeChrRam(int bank, int addr, uint8_t data)
    {
        addr = bank*WindowSize + (addr&(WindowSize-1));
        addr = addr&(W8K-1);

        getChrRam()[addr] = data;
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {

        if(!m_chrMode)
        {
            switch(addr>>10) {
            case 0:
            case 1: if(m_chrReg[0]&CHRRAM_BIT_MASK) return readChrRam<W2K>((m_chrReg[0]&m_chrMask)>>1,addr); break;
            case 2:
            case 3: if(m_chrReg[1]&CHRRAM_BIT_MASK) return readChrRam<W2K>((m_chrReg[1]&m_chrMask)>>1,addr); break;
            case 4: if(m_chrReg[2]&CHRRAM_BIT_MASK) return readChrRam<W1K>(m_chrReg[2]&m_chrMask,addr); break;
            case 5: if(m_chrReg[3]&CHRRAM_BIT_MASK) return readChrRam<W1K>(m_chrReg[3]&m_chrMask,addr); break;
            case 6: if(m_chrReg[4]&CHRRAM_BIT_MASK) return readChrRam<W1K>(m_chrReg[4]&m_chrMask,addr); break;
            case 7: if(m_chrReg[5]&CHRRAM_BIT_MASK) return readChrRam<W1K>(m_chrReg[5]&m_chrMask,addr); break;
            }

        }
        else
        {
            switch(addr>>10) {
            case 0: if(m_chrReg[2]&CHRRAM_BIT_MASK) return readChrRam<W1K>(m_chrReg[2]&m_chrMask,addr); break;
            case 1: if(m_chrReg[3]&CHRRAM_BIT_MASK) return readChrRam<W1K>(m_chrReg[3]&m_chrMask,addr); break;
            case 2: if(m_chrReg[4]&CHRRAM_BIT_MASK) return readChrRam<W1K>(m_chrReg[4]&m_chrMask,addr); break;
            case 3: if(m_chrReg[5]&CHRRAM_BIT_MASK) return readChrRam<W1K>(m_chrReg[5]&m_chrMask,addr); break;
            case 4:
            case 5: if(m_chrReg[0]&CHRRAM_BIT_MASK) return readChrRam<W2K>((m_chrReg[0]&m_chrMask)>>1,addr); break;
            case 6:
            case 7: if(m_chrReg[1]&CHRRAM_BIT_MASK) return readChrRam<W2K>((m_chrReg[1]&m_chrMask)>>1,addr); break;
            }
        }

        if(!m_chrMode)
        {
            switch(addr >> 10) { // addr/1k
            case 0:
            case 1: return m_cd.readChr<W2K>((m_chrReg[0]&m_chrMask)>>1,addr);
            case 2:
            case 3: return m_cd.readChr<W2K>((m_chrReg[1]&m_chrMask)>>1,addr);
            case 4: return m_cd.readChr<W1K>(m_chrReg[2]&m_chrMask,addr);
            case 5: return m_cd.readChr<W1K>(m_chrReg[3]&m_chrMask,addr);
            case 6: return m_cd.readChr<W1K>(m_chrReg[4]&m_chrMask,addr);
            case 7: return m_cd.readChr<W1K>(m_chrReg[5]&m_chrMask,addr);
            }
        }
        else
        {
            switch(addr>>10) {
            case 0: return m_cd.readChr<W1K>(m_chrReg[2]&m_chrMask,addr);
            case 1: return m_cd.readChr<W1K>(m_chrReg[3]&m_chrMask,addr);
            case 2: return m_cd.readChr<W1K>(m_chrReg[4]&m_chrMask,addr);
            case 3: return m_cd.readChr<W1K>(m_chrReg[5]&m_chrMask,addr);
            case 4:
            case 5: return m_cd.readChr<W2K>((m_chrReg[0]&m_chrMask)>>1,addr);
            case 6:
            case 7: return m_cd.readChr<W2K>((m_chrReg[1]&m_chrMask)>>1,addr);
            }

        }

        return 0;
    }

    GERANES_HOT void writeChr(int addr, uint8_t data) override
    {
        if(!m_chrMode)
        {
            switch(addr>>10) {
            case 0:
            case 1: if(m_chrReg[0]&CHRRAM_BIT_MASK) writeChrRam<W2K>((m_chrReg[0]&m_chrMask)>>1,addr,data); break;
            case 2:
            case 3: if(m_chrReg[1]&CHRRAM_BIT_MASK) writeChrRam<W2K>((m_chrReg[1]&m_chrMask)>>1,addr,data); break;
            case 4: if(m_chrReg[2]&CHRRAM_BIT_MASK) writeChrRam<W1K>(m_chrReg[2]&m_chrMask,addr,data); break;
            case 5: if(m_chrReg[3]&CHRRAM_BIT_MASK) writeChrRam<W1K>(m_chrReg[3]&m_chrMask,addr,data); break;
            case 6: if(m_chrReg[4]&CHRRAM_BIT_MASK) writeChrRam<W1K>(m_chrReg[4]&m_chrMask,addr,data); break;
            case 7: if(m_chrReg[5]&CHRRAM_BIT_MASK) writeChrRam<W1K>(m_chrReg[5]&m_chrMask,addr,data); break;
            }
        }
        else
        {
            switch(addr>>10) {
            case 0: if(m_chrReg[2]&CHRRAM_BIT_MASK) writeChrRam<W1K>(m_chrReg[2]&m_chrMask,addr,data); break;
            case 1: if(m_chrReg[3]&CHRRAM_BIT_MASK) writeChrRam<W1K>(m_chrReg[3]&m_chrMask,addr,data); break;
            case 2: if(m_chrReg[4]&CHRRAM_BIT_MASK) writeChrRam<W1K>(m_chrReg[4]&m_chrMask,addr,data); break;
            case 3: if(m_chrReg[5]&CHRRAM_BIT_MASK) writeChrRam<W1K>(m_chrReg[5]&m_chrMask,addr,data); break;
            case 4:
            case 5: if(m_chrReg[0]&CHRRAM_BIT_MASK) writeChrRam<W2K>((m_chrReg[0]&m_chrMask)>>1,addr,data); break;
            case 6:
            case 7: if(m_chrReg[1]&CHRRAM_BIT_MASK) writeChrRam<W2K>((m_chrReg[1]&m_chrMask)>>1,addr,data); break;
            }
        }
    }

    void serialization(SerializationBase& s) override
    {
        Mapper004::serialization(s);
    }

};

#endif // MAPPER119_H
