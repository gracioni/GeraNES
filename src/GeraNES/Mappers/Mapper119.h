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
    GERANES_INLINE uint8_t readCHRRAM(int bank, int addr)
    {
        addr = bank*WindowSize + (addr&(WindowSize-1));
        addr = addr&(W8K-1);

        return getVRAM()[addr];
    }

    template<int WindowSize>
    GERANES_INLINE void writeCHRRAM(int bank, int addr, uint8_t data)
    {
        addr = bank*WindowSize + (addr&(WindowSize-1));
        addr = addr&(W8K-1);

        getVRAM()[addr] = data;
    }

    GERANES_HOT uint8_t readCHR8k(int addr) override
    {

        if(!m_CHRMode)
        {
            switch(addr>>10) {
            case 0:
            case 1: if(m_CHRReg[0]&CHRRAM_BIT_MASK) return readCHRRAM<W2K>((m_CHRReg[0]&m_CHRMask)>>1,addr); break;
            case 2:
            case 3: if(m_CHRReg[1]&CHRRAM_BIT_MASK) return readCHRRAM<W2K>((m_CHRReg[1]&m_CHRMask)>>1,addr); break;
            case 4: if(m_CHRReg[2]&CHRRAM_BIT_MASK) return readCHRRAM<W1K>(m_CHRReg[2]&m_CHRMask,addr); break;
            case 5: if(m_CHRReg[3]&CHRRAM_BIT_MASK) return readCHRRAM<W1K>(m_CHRReg[3]&m_CHRMask,addr); break;
            case 6: if(m_CHRReg[4]&CHRRAM_BIT_MASK) return readCHRRAM<W1K>(m_CHRReg[4]&m_CHRMask,addr); break;
            case 7: if(m_CHRReg[5]&CHRRAM_BIT_MASK) return readCHRRAM<W1K>(m_CHRReg[5]&m_CHRMask,addr); break;
            }

        }
        else
        {
            switch(addr>>10) {
            case 0: if(m_CHRReg[2]&CHRRAM_BIT_MASK) return readCHRRAM<W1K>(m_CHRReg[2]&m_CHRMask,addr); break;
            case 1: if(m_CHRReg[3]&CHRRAM_BIT_MASK) return readCHRRAM<W1K>(m_CHRReg[3]&m_CHRMask,addr); break;
            case 2: if(m_CHRReg[4]&CHRRAM_BIT_MASK) return readCHRRAM<W1K>(m_CHRReg[4]&m_CHRMask,addr); break;
            case 3: if(m_CHRReg[5]&CHRRAM_BIT_MASK) return readCHRRAM<W1K>(m_CHRReg[5]&m_CHRMask,addr); break;
            case 4:
            case 5: if(m_CHRReg[0]&CHRRAM_BIT_MASK) return readCHRRAM<W2K>((m_CHRReg[0]&m_CHRMask)>>1,addr); break;
            case 6:
            case 7: if(m_CHRReg[1]&CHRRAM_BIT_MASK) return readCHRRAM<W2K>((m_CHRReg[1]&m_CHRMask)>>1,addr); break;
            }
        }

        if(!m_CHRMode)
        {
            switch(addr >> 10) { // addr/1k
            case 0:
            case 1: return m_cd.readChr<W2K>((m_CHRReg[0]&m_CHRMask)>>1,addr);
            case 2:
            case 3: return m_cd.readChr<W2K>((m_CHRReg[1]&m_CHRMask)>>1,addr);
            case 4: return m_cd.readChr<W1K>(m_CHRReg[2]&m_CHRMask,addr);
            case 5: return m_cd.readChr<W1K>(m_CHRReg[3]&m_CHRMask,addr);
            case 6: return m_cd.readChr<W1K>(m_CHRReg[4]&m_CHRMask,addr);
            case 7: return m_cd.readChr<W1K>(m_CHRReg[5]&m_CHRMask,addr);
            }
        }
        else
        {
            switch(addr>>10) {
            case 0: return m_cd.readChr<W1K>(m_CHRReg[2]&m_CHRMask,addr);
            case 1: return m_cd.readChr<W1K>(m_CHRReg[3]&m_CHRMask,addr);
            case 2: return m_cd.readChr<W1K>(m_CHRReg[4]&m_CHRMask,addr);
            case 3: return m_cd.readChr<W1K>(m_CHRReg[5]&m_CHRMask,addr);
            case 4:
            case 5: return m_cd.readChr<W2K>((m_CHRReg[0]&m_CHRMask)>>1,addr);
            case 6:
            case 7: return m_cd.readChr<W2K>((m_CHRReg[1]&m_CHRMask)>>1,addr);
            }

        }

        return 0;
    }

    GERANES_HOT void writeCHR8k(int addr, uint8_t data) override
    {
        if(!m_CHRMode)
        {
            switch(addr>>10) {
            case 0:
            case 1: if(m_CHRReg[0]&CHRRAM_BIT_MASK) writeCHRRAM<W2K>((m_CHRReg[0]&m_CHRMask)>>1,addr,data); break;
            case 2:
            case 3: if(m_CHRReg[1]&CHRRAM_BIT_MASK) writeCHRRAM<W2K>((m_CHRReg[1]&m_CHRMask)>>1,addr,data); break;
            case 4: if(m_CHRReg[2]&CHRRAM_BIT_MASK) writeCHRRAM<W1K>(m_CHRReg[2]&m_CHRMask,addr,data); break;
            case 5: if(m_CHRReg[3]&CHRRAM_BIT_MASK) writeCHRRAM<W1K>(m_CHRReg[3]&m_CHRMask,addr,data); break;
            case 6: if(m_CHRReg[4]&CHRRAM_BIT_MASK) writeCHRRAM<W1K>(m_CHRReg[4]&m_CHRMask,addr,data); break;
            case 7: if(m_CHRReg[5]&CHRRAM_BIT_MASK) writeCHRRAM<W1K>(m_CHRReg[5]&m_CHRMask,addr,data); break;
            }
        }
        else
        {
            switch(addr>>10) {
            case 0: if(m_CHRReg[2]&CHRRAM_BIT_MASK) writeCHRRAM<W1K>(m_CHRReg[2]&m_CHRMask,addr,data); break;
            case 1: if(m_CHRReg[3]&CHRRAM_BIT_MASK) writeCHRRAM<W1K>(m_CHRReg[3]&m_CHRMask,addr,data); break;
            case 2: if(m_CHRReg[4]&CHRRAM_BIT_MASK) writeCHRRAM<W1K>(m_CHRReg[4]&m_CHRMask,addr,data); break;
            case 3: if(m_CHRReg[5]&CHRRAM_BIT_MASK) writeCHRRAM<W1K>(m_CHRReg[5]&m_CHRMask,addr,data); break;
            case 4:
            case 5: if(m_CHRReg[0]&CHRRAM_BIT_MASK) writeCHRRAM<W2K>((m_CHRReg[0]&m_CHRMask)>>1,addr,data); break;
            case 6:
            case 7: if(m_CHRReg[1]&CHRRAM_BIT_MASK) writeCHRRAM<W2K>((m_CHRReg[1]&m_CHRMask)>>1,addr,data); break;
            }
        }
    }

    void serialization(SerializationBase& s) override
    {
        Mapper004::serialization(s);
    }

};

#endif // MAPPER119_H
