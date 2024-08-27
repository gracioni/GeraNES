#ifndef INCLUDE_GERANESEMU
#define INCLUDE_GERANESEMU

#include "defines.h"
#include "IBus.h"
#include "Cartridge.h"
#include "CPU2A03.h"
#include "PPU.h"
#include "APU.h"
#include "Controller.h"
#include "Settings.h"
#include "IAudioOutput.h"
#include "DMA.h"

#include "Serialization.h"

#include "util/CircularBuffer.h"
#include "signal/SigSlot.h"
#include "Logger.h"


class GeraNesEmu : public Ibus, public SigSlot::SigSlotBase
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

    uint8_t m_saveStatePoint;

    uint32_t m_frameCount;

    struct Rewind
    {
        bool activeFlag = false;
        double timer = 0.0;
        int FPSDivider = 1;
        int FPSAuxCounter = 0;
        CircularBuffer<std::vector<uint8_t>>* buffer = nullptr;

        void setup(bool enabled, double maxTime, int _FPSDivider)
        {   
            FPSDivider = _FPSDivider;

            if(buffer != nullptr) {
                delete buffer;
                buffer = nullptr;
            }

            if(enabled) buffer = new CircularBuffer<std::vector<uint8_t>>(static_cast<size_t>(60/FPSDivider * maxTime),CircularBuffer<std::vector<uint8_t>>::REPLACE);

            activeFlag = false;
            timer = 0.0;
        }

        void reset()
        {
            activeFlag = false;
            timer = 0.0;
            FPSAuxCounter = FPSDivider;
            if(buffer != nullptr) buffer->clear();
        }

        //return true when sample a frame
        bool update()
        {
            bool ret = false;

            if( --FPSAuxCounter == 0 ) {
                FPSAuxCounter = FPSDivider;
                ret = true;
            }

            return ret;
        }

        void addState(const std::vector<uint8_t> data)
        {
            if(buffer != nullptr) buffer->write(data);
        }

        void shutdown()
        {
            FPSAuxCounter = FPSDivider;

            if(buffer != nullptr) {
                delete buffer;
                buffer = nullptr;
            }

            activeFlag = false;
            timer = 0.0;
        }

    } m_rewind;

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
                        data = m_controller1.read();
                        data = (data&0x1F) | (m_openBus&(~0x1F));
                    }
                    break;
                }

                case 0x4017: //controller 2
                {
                    if constexpr(writeFlag) m_apu.write(addr&0x3FFF, data);
                    else {
                        data = m_controller2.read();
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
        Logger::instance().log(e, Logger::ERROR2);
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

        if(m_rewind.buffer != nullptr && (!m_rewind.activeFlag || m_rewind.buffer->size() == 0) && m_rewind.update() ) {
            Serialize s;
            serialization(s);
            m_rewind.addState(s.getData());
        }

        if(m_rewind.buffer != nullptr && m_rewind.activeFlag) {

            if(m_rewind.buffer->size() > 1 && m_rewind.update()) {
                //load state from memory
                loadStateFromMemory(m_rewind.buffer->readBack());
            }
            else {
                loadStateFromMemory(m_rewind.buffer->peakBack());
            }

        }

        signalFrameReady();
    }

    void onDMCRequest(uint16_t addr, bool reload) {
        //qDebug() << m_cpu.cycleNumber() << ": " << "DMC Request" << (reload ? " reload" : "write");
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

    GeraNesEmu(IAudioOutput& audioOutput = DummyAudioOutput::instance()) :
        m_settings(),
        m_audioOutput(audioOutput),
        m_cartridge(),
        m_cpu(*this),
        m_ppu(m_settings, m_cartridge),
        m_apu(m_audioOutput,m_settings, *this),
        m_dma(*this, m_apu.getSampleChannel(), m_cpu),
        m_controller1(),
        m_controller2()
    {
        m_halt = false;        

        m_update_cycles = 0;

        renderAudioCyclesAcc = 0;

        m_frameCount = 0;

        m_cpu.signalError.bind(&GeraNesEmu::onError, this);

        m_ppu.signalFrameStart.bind(&GeraNesEmu::onFrameStart, this);
        m_ppu.signalFrameReady.bind(&GeraNesEmu::onFrameReady, this);
        m_apu.getSampleChannel().dmcRequest2.bind(&GeraNesEmu::onDMCRequest, this);
    }

    ~GeraNesEmu()
    {
        m_rewind.shutdown();
    }

    //maxTime - seconds
    void setupRewindSystem(bool enabled, double maxTime, int FPSDivider = 1)
    {
        m_rewind.setup(enabled, maxTime, FPSDivider);
    }

    void setRewind(bool state)
    {
        m_rewind.activeFlag = state;
    }

    void resetRewindSystem()
    {
        m_rewind.reset();
    }

    void close()
    {
        m_cartridge.clear();
        m_ppu.clearFramebuffer();
        m_rewind.shutdown();
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

            m_settings.setRegion(Settings::NTSC);

            if(filename.find("(E)") != std::string::npos)
                m_settings.setRegion(Settings::PAL);

            updateInternalTimingStuff();

            m_ppu.init();
            m_cpu.init();
            m_apu.init();
            m_dma.init();


            m_saveStatePoint = 0;

            Logger::instance().log(m_cartridge.debug());

            resetRewindSystem();
        }

        return result;
    }

    GERANES_INLINE uint8_t read(int addr)
    {
        return busReadWrite<false>(addr);
    }

    GERANES_INLINE void write(int addr, uint8_t data)
    {
        busReadWrite<true>(addr,data);
    }

    GERANES_INLINE_HOT bool update(uint32_t dt) //miliseconds
    {
        if(!m_cartridge.isValid()) return false;

        dt = std::min(dt, (uint32_t)1000/10);  //0.1s      

        m_update_cycles += m_cyclesPerSecond  * dt;

        const uint32_t renderAudioCycles = m_cyclesPerSecond * 1;

        while(m_update_cycles >= 1000)
        {
            //PPU   X---X---X---X---X---X---X---X---X---X---X-...
            //CPU   --X-----------X-----------X-----------X---...
            //CPU   --1-------2---1-------2---1-------2---1---...

            // in the rewind system, a new save state is queued in each frameReady signal.
            // this can happen in any ppuCycle() call, so we need this for loop to track
            // where continue the simulation after load a save state
            // without this the save state system is not deterministic

            for(;m_saveStatePoint < 4; ++m_saveStatePoint) {
                switch(m_saveStatePoint) {
                case 0:

                    m_ppu.ppuCycle();



                    m_cpu.begin();

                    if(!m_ppu.inOverclockLines()){
                        m_dma.cycle();
                        m_apu.cycle();
                        m_cartridge.cycle();
                    }

                    m_cpu.phi1();

                    break;

                case 1:
                    m_ppu.ppuCycle();
                    break;

                case 2:
                    m_ppu.ppuCycle();
                    break;

                case 3:
                    m_ppu.ppuCyclePAL();

                    if(!m_ppu.inOverclockLines()) m_cpu.phi2(m_ppu.getInterruptFlag(), m_apu.getInterruptFlag() || m_cartridge.getInterruptFlag());
                  
                    m_update_cycles -= 1000;             

                    renderAudioCyclesAcc += 1000;  

                    while(renderAudioCyclesAcc >= renderAudioCycles) {
                        renderAudioCyclesAcc -= renderAudioCycles;

                        bool enableAudio = false;

                        //dont render when holding 1 frame in rewind mode
                        if(m_rewind.buffer == nullptr) enableAudio = true;
                        else {
                            if(!m_rewind.activeFlag) enableAudio = true;
                            else if(m_rewind.buffer->size() > 1) enableAudio = true;
                        }

                        m_audioOutput.render(1, enableAudio ? 1.0f : 0.0f);                       
                    }

                    break;
                }
            }
            m_saveStatePoint = 0;

            if(m_halt) {
                close();
                break;
            }     
        }

        if(m_newFrame){
            m_newFrame = false;
            return true;
        }

        return false;
    }

    GERANES_INLINE_HOT bool updateUntilFrame(uint32_t dt) //miliseconds
    {
        if(!m_cartridge.isValid()) return false;

        dt = std::min(dt, (uint32_t)1000/10);  //0.1s    

        while(!m_newFrame)
        {
            //PPU   X---X---X---X---X---X---X---X---X---X---X-...
            //CPU   --X-----------X-----------X-----------X---...
            //CPU   --1-------2---1-------2---1-------2---1---...

            // in the rewind system, a new save state is queued in each frameReady signal.
            // this can happen in any ppuCycle() call, so we need this for loop to track
            // where continue the simulation after load a save state
            // without this the save state system is not deterministic

            for(;m_saveStatePoint < 4; ++m_saveStatePoint) {
                switch(m_saveStatePoint) {
                case 0:

                    m_ppu.ppuCycle();



                    m_cpu.begin();

                    if(!m_ppu.inOverclockLines()){
                        m_dma.cycle();
                        m_apu.cycle();
                        m_cartridge.cycle();
                    }

                    m_cpu.phi1();

                    break;

                case 1:
                    m_ppu.ppuCycle();
                    break;

                case 2:
                    m_ppu.ppuCycle();
                    break;

                case 3:
                    m_ppu.ppuCyclePAL();

                    if(!m_ppu.inOverclockLines()) m_cpu.phi2(m_ppu.getInterruptFlag(), m_apu.getInterruptFlag() || m_cartridge.getInterruptFlag());

                    break;
                }
            }
            m_saveStatePoint = 0;

            if(m_halt) {
                close();
                break;
            }     
        }

        {
            bool enableAudio = false;

            //dont render when holding 1 frame in rewind mode
            if(m_rewind.buffer == nullptr) enableAudio = true;
            else {
                if(!m_rewind.activeFlag) enableAudio = true;
                else if(m_rewind.buffer->size() > 1) enableAudio = true;
            }

            m_audioOutput.render(dt, enableAudio ? 1.0f : 0.0f);                       
        }

        m_newFrame = false;        

        return true;
    }   

    GERANES_INLINE const uint32_t* getFramebuffer()
    {
        return m_ppu.getFramebuffer();
    }

    void saveState()
    {
        if(!m_cartridge.isValid()) return;

        Serialize s;
        serialization(s);
        s.saveToFile(saveStateFileName());
    }

    void loadState()
    {
        if(!m_cartridge.isValid()) return;

        Deserialize d;

        if(d.loadFromFile(saveStateFileName()))
            serialization(d);

        resetRewindSystem();
    }

    void loadStateFromMemory(const std::vector<uint8_t>& data)
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
        m_settings.setRegion(value);

        updateInternalTimingStuff();
    }

    Settings::Region region()
    {
        return m_settings.region();
    }

    void serialization(SerializationBase& s)
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

        SERIALIZEDATA(s, m_saveStatePoint);

        SERIALIZEDATA(s, m_frameCount);
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
        return m_rewind.buffer != nullptr && m_rewind.activeFlag;
    }

    uint32_t getRegionFPS() {
        if(m_settings.region() == Settings::NTSC) return 50;
        return 60;
    }

    uint32_t frameCount() {
        return m_frameCount;
    }

};

#endif
