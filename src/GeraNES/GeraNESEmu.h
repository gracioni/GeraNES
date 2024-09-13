#ifndef GERANES_EMU_H
#define GERANES_EMU_H

#include "defines.h"
#include "IBus.h"
#include "Cartridge.h"
#include "CPU2A03.h"
#include "PPU.h"
#include "APU/APU.h"
#include "Controller.h"
#include "Settings.h"
#include "IAudioOutput.h"
#include "DMA.h"

#include "Serialization.h"

#include "signal/SigSlot.h"
#include "Logger.h"

#include "Rewind.h"

class GeraNESEmu : public Ibus, public SigSlot::SigSlotBase, public IRewindable
{
private:

    const uint32_t MAX_4011_WRITES_TO_DISABLE_OVERCLOCK = 2;

    Settings m_settings;
    IAudioOutput& m_audioOutput;
    Cartridge m_cartridge;
    CPU2A03 m_cpu;
    PPU m_ppu;
    APU m_apu;
    DMA m_dma;
    uint8_t m_ram[0x800]; //2K
    Controller m_controller1;
    Controller m_controller2;

    uint32_t m_update_cycles;

    uint32_t m_4011WriteCounter;
    bool m_newFrame;

    bool m_halt;

    uint32_t m_cyclesPerSecond;

    uint32_t renderAudioCyclesAcc;

    uint8_t m_openBus;

    uint32_t m_frameCount;

    bool m_runningLoop;

    //do not serialize bellow atributtes
    bool m_saveStateFlag;
    bool m_loadStateFlag;    

    Rewind m_rewind;

    template<bool writeFlag>
    auto busReadWrite(int addr, uint8_t data = 0) -> std::conditional_t<writeFlag, void, uint8_t>
    {
        if constexpr(!writeFlag) data = m_openBus;

        switch(addr>>12)
        {
        case 0:
        case 1:
            if constexpr(writeFlag) m_ram[addr&0x7FF] = data;
            else data = m_ram[addr&0x7FF];
            break;
        case 2:
        case 3:
            data = m_ppu.readWrite<writeFlag>(addr, data);
            break;
        case 4:
        case 5:
            if(addr < 0x4014) { //APU registers

                if constexpr(writeFlag) {

                    m_apu.write(addr&0x3FFF, data);

                    //disable overclock when the game generate PCM audio
                    if(addr == 0x4011){
                        if(++m_4011WriteCounter == MAX_4011_WRITES_TO_DISABLE_OVERCLOCK && m_ppu.isOverclockFrame()) {
                            m_ppu.setOverclockFrame(false);
                            updateInternalTimingStuff();                            
                         }
                    }
                }

            }
            else if(addr < 0x4018) {

                switch(addr) {
                //DMA transfer
                case 0x4014: //acess: write only
                {
                    if constexpr(writeFlag)
                    {
                        uint16_t addr = static_cast<uint16_t>(data) << 8;
                        m_dma.OAMRequest(addr);
                    }
                    break;
                }

                case 0x4015: //APU
                {
                    if constexpr(writeFlag) m_apu.write(addr&0x3FFF, data);
                    else {
                        data = m_apu.read(addr&0x3FFF);
                    }
                    break;
                 }

                case 0x4016: //controller 1
                {
                    if constexpr(writeFlag)
                    {
                        m_controller1.write(data);
                        m_controller2.write(data);

                    }
                    else {
                        data = m_controller1.read(!m_cpu.isHalted());
                        data = (data&0x1F) | (m_openBus&(~0x1F));
                    }
                    break;
                }

                case 0x4017: //controller 2
                {
                    if constexpr(writeFlag) m_apu.write(addr&0x3FFF, data);
                    else {
                        data = m_controller2.read(!m_cpu.isHalted());
                        data = (data&0x1F) | (m_openBus&(~0x1F));
                    }
                    break;
                 }                

                }

                m_openBus = 0x40; //I dont know if this is right XD
            }
            else if( addr < 0x4020) { //unallocated IO space
                m_openBus = data;
            }
            else {

                if constexpr(writeFlag) m_cartridge.write0x4000(addr&0x1FFF, data);
                data = m_cartridge.read0x4000(addr&0x1FFF,data);

                m_openBus = data;
            }

            break;

        case 6:
        case 7:
            if constexpr(writeFlag) m_cartridge.writeSRAM8k(addr&0x1FFF, data);
            else data = m_cartridge.readSRAM8k(addr&0x1FFF);
            break;

        default: // >= 8
            if constexpr(writeFlag) m_cartridge.writePRG32k(addr&0x7FFF, data);
            else data = m_cartridge.readPRG32k(addr&0x7FFF);
            break;

        }

        if constexpr(!writeFlag)
            return data;
    }

    void onError(const std::string& e)
    {
        signalError(e);
        Logger::instance().log(e, Logger::Type::ERROR);
        m_halt = true;
    }

    void onFrameStart()
    {
        m_4011WriteCounter = 0;
        updateInternalTimingStuff();
        signalFrameStart();
    }

    void onFrameReady()
    {
        m_newFrame = true;
        ++m_frameCount;
        
        signalFrameReady();
    }

    void onDMCRequest(uint16_t addr, bool reload) {
        m_dma.dmcRequest(addr, reload);
    }

    void updateInternalTimingStuff()
    {
        if(m_ppu.isOverclockFrame())
            m_cyclesPerSecond = m_settings.CPUClockHz() * (1 + m_settings.overclockLines()/m_settings.PPULinesPerFrame());
        else
            m_cyclesPerSecond = m_settings.CPUClockHz();      
        }

    const std::string saveStateFileName() {
        return std::string(STATES_FOLDER) + m_cartridge.romFile().hash() + ".ss";
    }

public:

    SigSlot::Signal<const std::string&> signalError;
    SigSlot::Signal<> signalFrameStart;
    SigSlot::Signal<> signalFrameReady;

    GeraNESEmu(IAudioOutput& audioOutput = DummyAudioOutput::instance()) :
        m_settings(),
        m_audioOutput(audioOutput),
        m_cartridge(),
        m_cpu(*this),
        m_ppu(m_settings, m_cartridge),
        m_apu(m_audioOutput,m_settings),
        m_dma(*this, m_apu.getSampleChannel(), m_cpu),
        m_controller1(),
        m_controller2(),
        m_rewind(*this)
    {
        m_halt = false;        

        m_update_cycles = 0;

        renderAudioCyclesAcc = 0;

        m_frameCount = 0;

        m_saveStateFlag = false;
        m_loadStateFlag = false;
        m_runningLoop = false;

        m_cpu.signalError.bind(&GeraNESEmu::onError, this);

        m_ppu.signalFrameStart.bind(&GeraNESEmu::onFrameStart, this);
        m_ppu.signalFrameReady.bind(&GeraNESEmu::onFrameReady, this);
        m_apu.getSampleChannel().dmcRequest.bind(&GeraNESEmu::onDMCRequest, this);
    }

    ~GeraNESEmu()
    {
    }

    //maxTime - seconds
    void setupRewindSystem(bool enabled, double maxTime, int FPSDivider = 1)
    {
        m_rewind.setup(enabled, maxTime, FPSDivider);
    }

    void setRewind(bool state)
    {
        m_rewind.setRewind(state);
    }

    void resetRewindSystem()
    {
        m_rewind.reset();
    }

    void close()
    {
        m_cartridge.clear();
        m_ppu.clearFramebuffer();
        m_rewind.destroy();
    }

    const std::string open(const std::string& filename)
    {
        std::string result;

        m_ppu.clearFramebuffer();
      

        result = m_cartridge.open(filename);

        if(result.empty()) { //no errors

            m_halt = false;
            m_4011WriteCounter = 0;
            m_newFrame = false;
            m_frameCount = 0;

            memset(m_ram, 0, sizeof(m_ram));

            m_openBus = 0;

            m_settings.setRegion(Settings::Region::NTSC);

            if(filename.find("(E)") != std::string::npos)
                m_settings.setRegion(Settings::Region::PAL);

            updateInternalTimingStuff();

            m_ppu.init();
            m_cpu.init();
            m_apu.init();
            m_dma.init();         

            Logger::instance().log(m_cartridge.debug());

            resetRewindSystem();
        }

        return result;
    }

    GERANES_INLINE uint8_t read(int addr) override
    {
        return busReadWrite<false>(addr);
    }

    GERANES_INLINE void write(int addr, uint8_t data) override
    {
        busReadWrite<true>(addr,data);
    } 

    GERANES_INLINE_HOT void cycle() {

        //PPU   X---X---X---X---X---X---X---X---X---X---X-...
        //CPU   --X-----------X-----------X-----------X---...
        //CPU   --1-------2---1-------2---1-------2---1---...

        m_ppu.ppuCycle();

        m_cpu.begin();

        if(!m_ppu.inOverclockLines()){
            m_dma.cycle();
            m_apu.cycle();
            m_cartridge.cycle();
        }

        m_cpu.phi1();    
    
        m_ppu.ppuCycle();      
    
        m_ppu.ppuCycle();          

        m_ppu.ppuCyclePAL();                    

        if(!m_ppu.inOverclockLines()) m_cpu.phi2(m_ppu.getInterruptFlag(), m_apu.getInterruptFlag() || m_cartridge.getInterruptFlag());

    }

    /**
     * Return true on new frame
     */
    template<bool waitForNewFrame>
    GERANES_INLINE bool _update(uint32_t dt) //miliseconds
    {
        if(!m_cartridge.isValid()) return false;

        dt = std::min(dt, (uint32_t)1000/10);  //0.1s

        if constexpr(!waitForNewFrame)
            m_update_cycles += m_cyclesPerSecond  * dt;    

        const uint32_t renderAudioCycles = m_cyclesPerSecond * 1;

        bool ret = false;

        bool loop = false;

        if constexpr(waitForNewFrame)
            loop = true;
        else
            loop = m_update_cycles >= 1000;

        m_runningLoop = true;

        while(loop)
        {            
            cycle();            

            if constexpr(!waitForNewFrame) {
                
                m_update_cycles -= 1000;
                renderAudioCyclesAcc += 1000;  

                while(renderAudioCyclesAcc >= renderAudioCycles) {
                    renderAudioCyclesAcc -= renderAudioCycles;

                    bool enableAudio = m_rewind.rewindLimit();

                    m_audioOutput.render(1, !enableAudio);                       
                }
            }                 

            if(m_newFrame) {
                m_rewind.newFrame();
                ret = true;
                m_newFrame = false;
            }

            if(m_halt) {
                close();
                break;
            }

            if constexpr(waitForNewFrame)
                loop = !ret;
            else
                loop = m_update_cycles >= 1000;

        }        

        if constexpr(waitForNewFrame) {

            bool enableAudio = m_rewind.rewindLimit();

            m_audioOutput.render(dt, !enableAudio); 
        }

        m_runningLoop = false;

        if(m_saveStateFlag) {
            _saveState();
            m_saveStateFlag = false;
        }

        if(m_loadStateFlag) {
            _loadState();
            m_loadStateFlag = false;
        }

        return ret;
    }

    GERANES_INLINE bool update(uint32_t dt) {
        return _update<false>(dt);
    }

    GERANES_INLINE bool updateUntilFrame(uint32_t dt) {
        return _update<true>(dt);
    }  

    GERANES_INLINE const uint32_t* getFramebuffer()
    {
        return m_ppu.getFramebuffer();
    }

    void _saveState()
    {
        if(!m_cartridge.isValid()) return;

        Serialize s;
        serialization(s);
        s.saveToFile(saveStateFileName());
    }    

    void saveState() {
        if(!m_runningLoop) _saveState();
        else m_saveStateFlag = true;
    }

    void _loadState()
    {
        if(!m_cartridge.isValid()) return;

        Deserialize d;

        if(d.loadFromFile(saveStateFileName())) {
            serialization(d);
        }

        resetRewindSystem();
    }

    void loadState() {
        if(!m_runningLoop) _loadState();
        else m_loadStateFlag = true;
    }    

    void loadStateFromMemory(const std::vector<uint8_t>& data) override
    {
        auto old = m_update_cycles; //preserve this

        Deserialize d;
        d.setData(data);
        serialization(d);

        m_update_cycles = old;
    }

    /*
    void calculateSerializationSize()
    {
        GetSerializationSize s;
        serialization(s);
        m_serializationSize = s.size();

        Logger::instance().log("Serialization size: " + std::to_string(m_serializationSize) + "\n");
    }
    */

    bool overclocked()
    {
        return m_settings.overclockLines() > 0;
    }

    void enableOverclock(bool state)
    {
        m_settings.setOverclockLines(state ? m_settings.PPULinesPerFrame() : 0);
        updateInternalTimingStuff();
    }

    void disableSpriteLimit(bool state)
    {
        m_settings.disableSpriteLimit(state);
    }

    bool spriteLimitDisabled()
    {
        return m_settings.spriteLimitDisabled();
    }

    void setRegion(Settings::Region value)
    {
        if(value != m_settings.region()) {
            m_settings.setRegion(value);
            m_rewind.reset();
            updateInternalTimingStuff();
        }
    }

    Settings::Region region()
    {
        return m_settings.region();
    }

    void serialization(SerializationBase& s) override
    {
        m_cpu.serialization(s);
        m_cartridge.serialization(s);
        m_ppu.serialization(s);
        m_apu.serialization(s);
        s.array(m_ram, 1, 0x800);
        m_controller1.serialization(s);
        m_controller2.serialization(s);
        m_settings.serialization(s);
        m_dma.serialization(s);

        SERIALIZEDATA(s, m_cyclesPerSecond);
  
        SERIALIZEDATA(s, renderAudioCyclesAcc);

        SERIALIZEDATA(s, m_openBus);

        SERIALIZEDATA(s, m_halt);

        SERIALIZEDATA(s, m_4011WriteCounter);
        SERIALIZEDATA(s, m_newFrame);

        SERIALIZEDATA(s, m_frameCount);

        SERIALIZEDATA(s, m_runningLoop);
    }

    void setController1Buttons(bool bA, bool bB, bool bSelect, bool bStart, bool bUp, bool bDown, bool bLeft, bool bRight)
    {
        m_controller1.setButtonsStatus(bA,bB,bSelect,bStart,bUp,bDown,bLeft,bRight);
    }

    void setController2Buttons(bool bA, bool bB, bool bSelect, bool bStart, bool bUp, bool bDown, bool bLeft, bool bRight)
    {
        m_controller2.setButtonsStatus(bA,bB,bSelect,bStart,bUp,bDown,bLeft,bRight);
    }

    PPU& getPPU(void)
    {
        return m_ppu;
    }

    bool valid() //ready to run
    {
        return m_cartridge.isValid();
    }

    bool isRewinding()
    {
        return m_rewind.isRewinding();
    }

    uint32_t getRegionFPS() {
        if(m_settings.region() == Settings::Region::PAL) return 50;
        return 60;
    }

    int getFPS() override {
        return getRegionFPS();
    }

    uint32_t frameCount() {
        return m_frameCount;
    }

};

#endif
