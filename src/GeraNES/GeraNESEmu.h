#pragma once

#include "defines.h"
#include "IBus.h"
#include "Cartridge.h"
#include "CPU2A03.h"
#include "PPU.h"
#include "APU/APU.h"
#include "Controller.h"
#include "Zapper.h"
#include "BandaiHyperShot.h"
#include "Settings.h"
#include "IAudioOutput.h"
#include "DMA.h"
#include "Console.h"
#include "HardwareActions.h"

#include "Serialization.h"

#include "signal/signal.h"
#include "logger/logger.h"

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
    Zapper m_zapper1;
    Zapper m_zapper2;
    BandaiHyperShot m_bandaiHyperShot;
    Console m_console;

    uint32_t m_updateCyclesAcc;

    uint32_t m_4011WriteCounter;
    bool m_newFrame;

    bool m_halt;

    int m_cpuCyclesAcc;

    uint32_t m_cyclesPerSecond;

    uint32_t m_audioRenderCyclesAcc;
    uint32_t m_lastAudioRenderedMs;
    double m_vsyncAudioCompMsAcc;
    int m_vsyncAudioSkipMsDebt;

    uint8_t m_openBus;

    uint32_t m_frameCount;
    HardwareActions m_hardwareActions;

    bool m_runningLoop;
    bool m_speedBoost;
    bool m_paused;
    static constexpr int SPEED_BOOST_MULTIPLIER = 3;

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
                            updateCyclesPerSecond();                            
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
                        m_bandaiHyperShot.write4016(data);
                    }
                    else {

                        bool useZapper = m_settings.getPortDevice(Settings::Port::P_1) == std::optional<Settings::Device>(Settings::Device::ZAPPER);

                        if(useZapper) data = m_zapper1.read();
                        else data = m_controller1.read(!m_cpu.isHalted());

                        bool useBandaiHyperShot =
                            m_settings.getExpansionDevice() == Settings::ExpansionDevice::BANDAI_HYPERSHOT;
                        if(useBandaiHyperShot) {
                            data = static_cast<uint8_t>((data & ~0x02) | m_bandaiHyperShot.read4016(!m_cpu.isHalted()));
                        }

                        data = m_cartridge.readMapperRegister(addr & 0x1FFF, data);
                        data = m_hardwareActions.applyVsSystemRead4016(
                            data,
                            m_cartridge.isValid() && m_cartridge.system() == GameDatabase::System::VsSystem
                        );
                        data = (data&0x7F) | (m_openBus&(~0x7F));                        
                    }
                    break;
                }

                case 0x4017: //controller 2
                {
                    if constexpr(writeFlag) m_apu.write(addr&0x3FFF, data);
                    else {

                        bool useZapper = m_settings.getPortDevice(Settings::Port::P_2) == std::optional<Settings::Device>(Settings::Device::ZAPPER);

                        if(useZapper) data = m_zapper2.read();
                        else data = m_controller2.read(!m_cpu.isHalted());                        

                        bool useBandaiHyperShot =
                            m_settings.getExpansionDevice() == Settings::ExpansionDevice::BANDAI_HYPERSHOT;
                        if(useBandaiHyperShot) {
                            const uint8_t expData = m_bandaiHyperShot.read4017();
                            data = static_cast<uint8_t>((data & ~0x18) | (expData & 0x18));
                        }

                        data = m_cartridge.readMapperRegister(addr & 0x1FFF, data);
                        data = m_hardwareActions.applyVsSystemRead4017(
                            data,
                            m_cartridge.isValid() && m_cartridge.system() == GameDatabase::System::VsSystem
                        );
                        data = (data&0x7F) | (m_openBus&(~0x7F));
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

                if constexpr(writeFlag) {
                    m_cartridge.writeMapperRegister(addr&0x1FFF, data);
                }
                else {
                    data = m_cartridge.readMapperRegister(addr&0x1FFF,data);
                }

                m_openBus = data;
            }

            break;

        case 6:
        case 7:
            if constexpr(writeFlag) m_cartridge.writeSaveRam(addr&0x1FFF, data);
            else data = m_cartridge.readSaveRam(addr&0x1FFF);
            break;

        default: // >= 8
            if constexpr(writeFlag) m_cartridge.writePrg(addr&0x7FFF, data);
            else {
                m_cartridge.onCpuRead(static_cast<uint16_t>(addr));
                data = m_cartridge.readPrg(addr&0x7FFF);
            }
            break;

        }

        if constexpr(writeFlag) {
            // Mapper CPU-write hooks must not see DMA writes.
            if(!m_cpu.isHalted()) {
                m_cartridge.onCpuWrite(static_cast<uint16_t>(addr), data);
            }
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
        m_hardwareActions.onFrameStart();

        updateCyclesPerSecond();
        signalFrameStart();
    }

    void onFrameReady()
    {
        m_newFrame = true;
        ++m_frameCount;
        
        signalFrameReady();
    }

    void onScanlineStart()
    {
        m_zapper1.onScanlineChanged();
        m_zapper2.onScanlineChanged();
        m_bandaiHyperShot.onScanlineChanged();
        m_cartridge.onScanlineStart(m_ppu.isActivelyRendering(), m_ppu.scanline());
    }

    void onDMCRequest(uint16_t addr, bool reload) {
        m_dma.dmcRequest(addr, reload);
    }

    void resyncAudioAfterStateLoad()
    {
        // Audio output internals (wave generators/FIFOs) are not part of save states.
        // Re-sync them to the restored APU/settings to avoid stale buffered audio.
        m_audioOutput.clearAudioBuffers();
        m_audioOutput.setExpansionSourceRateHz(m_settings.CPUClockHz());
        m_audioOutput.setExpansionAudioVolume(1.0f);
        m_lastAudioRenderedMs = 0;
        m_vsyncAudioCompMsAcc = 0.0;
        m_vsyncAudioSkipMsDebt = 0;
        m_apu.updateAudioOutput();
    }

    GERANES_INLINE void renderAudioMs(uint32_t ms)
    {
        if(ms == 0) return;

        m_audioOutput.setExpansionAudioVolume(m_rewind.isRewinding() ? 0.0f : 1.0f);
        bool enableAudio = m_rewind.rewindLimit() && !m_speedBoost;
        m_audioOutput.render(ms, !enableAudio);
    }

    GERANES_INLINE void compensateVsyncAudioDrift(uint32_t dt)
    {
        if(dt == 0) return;

        // Ignore large hitches; keep normal underflow/prebuffer behavior in the backend.
        if(dt > 34) {
            m_vsyncAudioCompMsAcc = 0.0;
            m_vsyncAudioSkipMsDebt = 0;
            return;
        }

        m_vsyncAudioCompMsAcc += static_cast<double>(dt) - static_cast<double>(m_lastAudioRenderedMs);

        if(m_vsyncAudioCompMsAcc < -10.0) m_vsyncAudioCompMsAcc = -10.0;
        if(m_vsyncAudioCompMsAcc > 10.0) m_vsyncAudioCompMsAcc = 10.0;

        while(m_vsyncAudioCompMsAcc >= 1.0) {
            renderAudioMs(1);
            m_vsyncAudioCompMsAcc -= 1.0;
        }

        // Symmetric path, but conservative: only build skip-debt after sustained negative drift.
        // This avoids harming the common NES-NTSC-on-60Hz case where positive compensation is critical.
        constexpr double NEGATIVE_DRIFT_DEADBAND_MS = 60.0;
        constexpr int MAX_SKIP_DEBT_MS = 3;
        while(m_vsyncAudioCompMsAcc <= -NEGATIVE_DRIFT_DEADBAND_MS && m_vsyncAudioSkipMsDebt < MAX_SKIP_DEBT_MS) {
            ++m_vsyncAudioSkipMsDebt;
            m_vsyncAudioCompMsAcc += 1.0;
        }
    }

    void updateCyclesPerSecond()
    {
        if(m_ppu.isOverclockFrame()) {
            const uint64_t base = static_cast<uint64_t>(m_settings.CPUClockHz());
            const uint64_t lines = static_cast<uint64_t>(m_settings.PPULinesPerFrame());
            const uint64_t overclock = static_cast<uint64_t>(m_settings.overclockLines());
            m_cyclesPerSecond = static_cast<uint32_t>((base * (lines + overclock)) / lines);
        } else
            m_cyclesPerSecond = m_settings.CPUClockHz();      
        }

    const std::string saveStateFileName() {
        return std::string(STATES_FOLDER) + basename(m_cartridge.romFile().fileName()) + ".s";
    }

public:

    SigSlot::Signal<const std::string&> signalError;
    SigSlot::Signal<> signalFrameStart;
    SigSlot::Signal<> signalFrameReady;

    GeraNESEmu(IAudioOutput& audioOutput = DummyAudioOutput::instance()) :
    m_settings(),
    m_audioOutput(audioOutput),
    m_cartridge(),
    m_cpu(*this, m_console),
    m_ppu(m_settings, m_cartridge),
    m_apu(m_audioOutput,m_settings),
    m_dma(*this, m_apu.getSampleChannel(), m_cpu),
    m_controller1(),
    m_controller2(),
    m_zapper1(),
    m_zapper2(),
    m_bandaiHyperShot(),
    m_rewind(*this),
    m_console(m_cpu, m_ppu, m_dma, m_apu, m_cartridge)
    {
        init();

        m_cpu.signalError.bind(&GeraNESEmu::onError, this);

        m_ppu.signalFrameStart.bind(&GeraNESEmu::onFrameStart, this);
        m_ppu.signalFrameReady.bind(&GeraNESEmu::onFrameReady, this);
        m_ppu.signalScanlineStart.bind(&GeraNESEmu::onScanlineStart, this);
        m_apu.getSampleChannel().dmcRequest.bind(&GeraNESEmu::onDMCRequest, this);

        auto f = [&](int x, int y){

            uint32_t pixel = m_ppu.getZapperPixel(x, y);

            int r =  pixel        & 0xFF;
            int g = (pixel >> 8)  & 0xFF;
            int b = (pixel >> 16) & 0xFF;

            float luma = 0.2126f*r + 0.7152f*g + 0.0722f*b;

            return luma;
        };

        m_zapper1.setPixelChecker(f);
        m_zapper2.setPixelChecker(f);
        m_bandaiHyperShot.setPixelChecker(f);
    }

    ~GeraNESEmu()
    {
    }

    void init() {

        m_halt = false;        

        m_updateCyclesAcc = 0;

        m_cpuCyclesAcc = 1;
        m_audioRenderCyclesAcc = 0;
        m_lastAudioRenderedMs = 0;
        m_vsyncAudioCompMsAcc = 0.0;
        m_vsyncAudioSkipMsDebt = 0;

        m_frameCount = 0;

        m_saveStateFlag = false;
        m_loadStateFlag = false;
        m_runningLoop = false;
        m_speedBoost = false;
        m_paused = false;

        m_openBus = 0;

        memset(m_ram, 0, sizeof(m_ram));            
        m_hardwareActions.reset();
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

    void setSpeedBoost(bool state)
    {
        if(m_speedBoost && !state) {
            m_audioOutput.init();
        }
        m_speedBoost = state;
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

    bool open(const std::string& filename)
    {
        m_audioOutput.clearAudioBuffers();
        m_ppu.clearFramebuffer();      

        bool result = m_cartridge.open(filename);

        if(result) { //no errors

            init();            

            switch(m_cartridge.system()) {

                case GameDatabase::System::NesPal:
                    m_settings.setRegion(Settings::Region::PAL);
                    break;

                case GameDatabase::System::NesNtsc:
                    m_settings.setRegion(Settings::Region::NTSC);
                    break;

                case GameDatabase::System::Dendy:
                    m_settings.setRegion(Settings::Region::DENDY);
                    break;

                default:
                    m_settings.setRegion(Settings::Region::NTSC);

            }  
            m_audioOutput.setExpansionSourceRateHz(m_settings.CPUClockHz());

            switch(m_cartridge.inputType()) {

                case GameDatabase::InputType::VsZapper:
                case GameDatabase::InputType::Zapper:
                    setPortDevice(Settings::Port::P_1, Settings::Device::CONTROLLER);
                    setPortDevice(Settings::Port::P_2, Settings::Device::ZAPPER);
                    setExpansionDevice(Settings::ExpansionDevice::NONE);
                    break;

                case GameDatabase::InputType::TwoZappers:
                    setPortDevice(Settings::Port::P_1, Settings::Device::ZAPPER);
                    setPortDevice(Settings::Port::P_2, Settings::Device::ZAPPER);
                    setExpansionDevice(Settings::ExpansionDevice::NONE);
                    break;

                case GameDatabase::InputType::BandaiHypershot:
                    setPortDevice(Settings::Port::P_1, Settings::Device::CONTROLLER);
                    setPortDevice(Settings::Port::P_2, Settings::Device::CONTROLLER);
                    setExpansionDevice(Settings::ExpansionDevice::BANDAI_HYPERSHOT);
                    break;

                default:
                    setPortDevice(Settings::Port::P_1, Settings::Device::CONTROLLER);
                    setPortDevice(Settings::Port::P_2, Settings::Device::CONTROLLER);
                    setExpansionDevice(Settings::ExpansionDevice::NONE);
            }

            updateCyclesPerSecond();

            m_ppu.setVsPpuModel(m_cartridge.vsPpuModel());
            m_cartridge.reset();
            m_ppu.init();
            m_cpu.init();
            m_apu.init();
            m_dma.init();         

            resetRewindSystem();
        }

        return result;
    }

    GERANES_HOT uint8_t read(int addr) override
    {
        return busReadWrite<false>(addr);
    }

    GERANES_HOT void write(int addr, uint8_t data) override
    {
        busReadWrite<true>(addr,data);
    }        

    /**
     * Return true on new frame
     */
    template<bool waitForNewFrame>
    GERANES_INLINE bool _update(uint32_t dt) //miliseconds
    {
        const int AUDIO_RENDER_TIME_STEP = 1; //ms

        if(!m_cartridge.isValid()) return false;

        dt = std::min(dt, (uint32_t)1000/10);  //0.1s

        if constexpr(!waitForNewFrame)
            m_updateCyclesAcc += (m_cyclesPerSecond * (m_speedBoost ? SPEED_BOOST_MULTIPLIER : 1)) * dt;

        const uint32_t audioRenderCycles = (m_cyclesPerSecond * (m_speedBoost ? SPEED_BOOST_MULTIPLIER : 1)) * AUDIO_RENDER_TIME_STEP;

        bool ret = false;
        uint32_t renderedAudioMs = 0;

        bool loop = false;

        if constexpr(waitForNewFrame)
            loop = true;
        else
            loop = m_updateCyclesAcc >= 1000;

        m_runningLoop = true;

        

        while(loop)
        {           
            if(--m_cpuCyclesAcc == 0) {

                m_cpuCyclesAcc = m_cpu.run();

                if constexpr(waitForNewFrame) {
                    // Keep audio strictly tied to emulated CPU time.
                    m_audioRenderCyclesAcc += m_cpuCyclesAcc * (AUDIO_RENDER_TIME_STEP * 1000);
                }
            }          

            if constexpr(!waitForNewFrame) {                
                m_updateCyclesAcc -= 1000;
                m_audioRenderCyclesAcc += AUDIO_RENDER_TIME_STEP*1000;
            }

            while(m_audioRenderCyclesAcc >= audioRenderCycles) {
                m_audioRenderCyclesAcc -= audioRenderCycles;
                if(m_vsyncAudioSkipMsDebt > 0) {
                    --m_vsyncAudioSkipMsDebt;
                }
                else {
                    renderAudioMs(AUDIO_RENDER_TIME_STEP);
                    renderedAudioMs += AUDIO_RENDER_TIME_STEP;
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
                loop = m_updateCyclesAcc >= 1000;

        }

        m_lastAudioRenderedMs = renderedAudioMs;
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
        if(m_paused) return false;
        return _update<false>(dt);
    }

    GERANES_INLINE bool updateUntilFrame(uint32_t dt) {
        if(m_paused) return true;

        if(!m_speedBoost) {
            const bool ret = _update<true>(dt);
            compensateVsyncAudioDrift(dt);
            return ret;
        }

        // In frame-locked mode (vsync path), run extra emulated frames while held.
        _update<true>(dt);
        for(int i = 1; i < SPEED_BOOST_MULTIPLIER; ++i) {
            _update<true>(0);
        }
        m_vsyncAudioCompMsAcc = 0.0;
        m_vsyncAudioSkipMsDebt = 0;
        return true;
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
        if(s.saveToFile(saveStateFileName())) {
            Logger::instance().log("State saved", Logger::Type::USER);
        }
        else {
            Logger::instance().log("Failed to save state", Logger::Type::ERROR);
        }
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
            resyncAudioAfterStateLoad();
            Logger::instance().log("State loaded", Logger::Type::USER);
        }
        else {
            Logger::instance().log("Failed to load state", Logger::Type::ERROR);
        }

        resetRewindSystem();
    }

    void loadState() {
        if(!m_runningLoop) _loadState();
        else m_loadStateFlag = true;
    }    

    void loadStateFromMemory(const std::vector<uint8_t>& data) override
    {
        auto old = m_updateCyclesAcc; //preserve this

        Deserialize d;
        d.setData(data);
        serialization(d);
        resyncAudioAfterStateLoad();

        m_updateCyclesAcc = old;
    }

    void loadStateFromMemory(const uint8_t* data, size_t size)
    {
        auto old = m_updateCyclesAcc; //preserve this

        Deserialize d;
        d.setData(data, size);
        serialization(d);
        resyncAudioAfterStateLoad();

        m_updateCyclesAcc = old;
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

    GERANES_INLINE std::optional<Settings::Device> getPortDevice(Settings::Port port)
    {
        return m_settings.getPortDevice(port);
    }

    GERANES_INLINE void setPortDevice(Settings::Port port, Settings::Device device)
    {
        m_settings.setPortDevice(port, device);
    }

    GERANES_INLINE Settings::ExpansionDevice getExpansionDevice() const
    {
        return m_settings.getExpansionDevice();
    }

    GERANES_INLINE void setExpansionDevice(Settings::ExpansionDevice device)
    {
        m_settings.setExpansionDevice(device);
    }

    bool overclocked()
    {
        return m_settings.overclockLines() > 0;
    }

    void setPaused(bool paused)
    {
        m_paused = paused;
    }

    bool paused() const
    {
        return m_paused;
    }

    void togglePaused()
    {
        m_paused = !m_paused;
        Logger::instance().log(m_paused ? "Emulation paused" : "Emulation resumed", Logger::Type::USER);
    }

    void enableOverclock(bool state)
    {
        m_settings.setOverclockLines(state ? m_settings.PPULinesPerFrame() : 0);
        updateCyclesPerSecond();
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
            m_audioOutput.init();
            m_rewind.reset();
            updateCyclesPerSecond();
        }
    }

    Settings::Region region()
    {
        return m_settings.region();
    }

    void serialization(SerializationBase& s) override
    {
        uint32_t saveStateMagic = SAVE_STATE_MAGIC;
        SERIALIZEDATA(s, saveStateMagic);
        if(saveStateMagic != SAVE_STATE_MAGIC) {
            Logger::instance().log("Invalid save state: incorrect magic header", Logger::Type::ERROR);
            return;
        }

        uint32_t saveStateVersion = SAVE_STATE_VERSION;
        SERIALIZEDATA(s, saveStateVersion);
        if(saveStateVersion != SAVE_STATE_VERSION) {
            Logger::instance().log("Incompatible save state: version mismatch", Logger::Type::ERROR);
            return;
        }

        uint32_t fileCrc = m_cartridge.romFile().fileCrc32();
        SERIALIZEDATA(s, fileCrc);
        if(fileCrc != m_cartridge.romFile().fileCrc32()) {
            Logger::instance().log("Save state mismatch: this state was created for a different ROM", Logger::Type::ERROR);
            return;
        }

        m_cpu.serialization(s);
        m_cartridge.serialization(s);
        m_ppu.serialization(s);
        m_apu.serialization(s);
        s.array(m_ram, 1, 0x800);
        m_controller1.serialization(s);
        m_controller2.serialization(s);
        m_zapper1.serialization(s);
        m_zapper2.serialization(s);
        m_bandaiHyperShot.serialization(s);
        m_settings.serialization(s);
        m_dma.serialization(s);

        SERIALIZEDATA(s, m_cpuCyclesAcc);
        SERIALIZEDATA(s, m_cyclesPerSecond);
  
        SERIALIZEDATA(s, m_audioRenderCyclesAcc);

        SERIALIZEDATA(s, m_openBus);

        SERIALIZEDATA(s, m_halt);

        SERIALIZEDATA(s, m_4011WriteCounter);
        SERIALIZEDATA(s, m_newFrame);
        SERIALIZEDATA(s, m_frameCount);
        m_hardwareActions.serialization(s);

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

    void setZapper(Settings::Port port, int x, int y, bool trigger)
    {
        switch(port) {

            case Settings::Port::P_1:
                m_zapper1.setCursorPosition(x,y);
                m_zapper1.setTrigger(trigger);
                break;

            case Settings::Port::P_2:
                m_zapper2.setCursorPosition(x,y);
                m_zapper2.setTrigger(trigger);
                break;
        }
    }

    void setBandaiHyperShotButtons(bool bA, bool bB, bool bSelect, bool bStart, bool bUp, bool bDown, bool bLeft, bool bRight)
    {
        m_bandaiHyperShot.setButtonsStatus(bA, bB, bSelect, bStart, bUp, bDown, bLeft, bRight);
    }

    void setBandaiHyperShot(int x, int y, bool trigger)
    {
        m_bandaiHyperShot.setCursorPosition(x, y);
        m_bandaiHyperShot.setTrigger(trigger);
    }

    void fdsSwitchDiskSide()
    {
        m_hardwareActions.fdsSwitchDiskSide();
    }

    void fdsEjectDisk()
    {
        m_hardwareActions.fdsEjectDisk();
    }

    void fdsInsertNextDisk()
    {
        m_hardwareActions.fdsInsertNextDisk();
    }

    void vsInsertCoin(int slot)
    {
        m_hardwareActions.vsInsertCoin(
            slot,
            m_cartridge.isValid() && m_cartridge.system() == GameDatabase::System::VsSystem
        );
    }

    void vsServiceButton(int button)
    {
        m_hardwareActions.vsServiceButton(
            button,
            m_cartridge.isValid() && m_cartridge.system() == GameDatabase::System::VsSystem
        );
    }

    Console& getConsole() {
        return m_console;
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

        switch(m_settings.region()) {

            case Settings::Region::NTSC:
                return 60;

            case Settings::Region::PAL:
                return 50;

            case Settings::Region::DENDY:
                return 50;

            default:
                return 60;
        }
    }

    int getFPS() override {
        return getRegionFPS();
    }

    uint32_t frameCount() {
        return m_frameCount;
    }

    void reset() {

        if(!m_cartridge.isValid()) return;

        m_cartridge.reset();
        m_dma.init();
        m_apu.reset();
        m_ppu.init();

        m_halt = false;
        m_updateCyclesAcc = 0;
        m_cpuCyclesAcc = 1;
        m_audioRenderCyclesAcc = 0;
        m_lastAudioRenderedMs = 0;
        m_vsyncAudioCompMsAcc = 0.0;
        m_vsyncAudioSkipMsDebt = 0;
        m_openBus = 0;
        m_4011WriteCounter = 0;
        m_newFrame = false;
        m_frameCount = 0;
        m_runningLoop = false;
        m_saveStateFlag = false;
        m_loadStateFlag = false;
        m_hardwareActions.reset();

        m_rewind.reset();
        updateCyclesPerSecond();
        m_cpu.reset();
        Logger::instance().log("Emulator reset", Logger::Type::USER);
    }

};
























