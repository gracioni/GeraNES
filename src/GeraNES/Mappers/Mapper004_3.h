#pragma once

#include "Mapper004.h"

//McAcc
//game: Mickey Letterland
//Based on krikzz's research: https://forums.nesdev.com/viewtopic.php?p=242427#p242427

class Mapper004_3 : public Mapper004 {

private:
	uint32_t m_counter = 0;
	bool m_lastState = false;

public:

    Mapper004_3(ICartridgeData& cd) : Mapper004(cd)
    {     
    }

	void serialization(SerializationBase& s) override
	{
		Mapper004::serialization(s);
		SERIALIZEDATA(s, m_counter);
		SERIALIZEDATA(s, m_lastState);
	}

	GERANES_HOT void writePrg(int addr, uint8_t data) override
	{
        addr &= 0xF001;

		if(addr== 0x4001) {
			//"Writing to $C001 resets pulse counter."
			m_counter = 0;
		}
		Mapper004::writePrg(addr, data);
	}

	void setA12State(bool state) override
	{
		if(!state && (m_lastState)) {
			m_counter++;

			if(m_counter == 1) {
				//"Counter clocking happens once per 8 A12 cycles at first cycle"
				if(m_irqCounter == 0 || m_irqClearFlag) {
					m_irqCounter = m_reloadValue;
				} else {
					m_irqCounter--;
				}

				if(m_irqCounter == 0 && m_enableInterrupt) {
					m_interruptFlag = true;
				}

				m_irqClearFlag = false;
			} else if(m_counter == 8) {
				m_counter = 0;
			}
		}
		m_lastState = state;
	}

};
