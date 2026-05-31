#pragma once

#include "defines.h"
#include "IBus.h"
#include "Cartridge.h"
#include "CPU2A03.h"
#include "PPU.h"
#include "APU/APU.h"
#include "NesStandardController.h"
#include "FamicomController.h"
#include "FamicomExpansionStandardController.h"
#include "Zapper.h"
#include "PowerPad.h"
#include "FamilyTrainer.h"
#include "FamilyBasicKeyboard.h"
#include "ArkanoidControllerNes.h"
#include "ArkanoidControllerFamicom.h"
#include "SnesMouse.h"
#include "SuborMouse.h"
#include "SuborKeyboard.h"
#include "SnesController.h"
#include "VirtualBoyController.h"
#include "FourScore.h"
#include "HoriAdapter.h"
#include "BandaiHyperShot.h"
#include "KonamiHyperShot.h"
#include "Settings.h"
#include "IAudioOutput.h"
#include "Console.h"
#include "HardwareActions.h"
#include "NsfPlayer.h"
#include "InputFrame.h"

#include "Serialization.h"
#include "util/Crc32.h"

#include "signal/signal.h"
#include "logger/logger.h"

#include "Rewind.h"

#include <filesystem>
#include <array>
#include <memory>
#include <string>
#include <utility>

enum class AccessType
{
    Read,
    Write
};

class GeraNESEmu : public Ibus, public SigSlot::SigSlotBase, public IRewindable
{
public:
    struct DebugBreakpointConfig
    {
        bool enabled = false;
        bool breakOnNmiStart = false;
        bool breakOnNmiEnd = false;
        bool breakOnIrqStart = false;
        bool breakOnIrqEnd = false;
        bool breakOnSpriteZeroHit = false;
        bool breakOnPpuRegisterWrite = false;
        bool breakOnPpuRegisterRead = false;
        bool breakOnApuRegisterWrite = false;
        bool breakOnApuRegisterRead = false;
        bool breakOnControllerWrite = false;
        bool breakOnControllerRead = false;
        bool breakOnMapperRegisterWrite = false;
        bool breakOnMapperRegisterRead = false;
        bool breakOnOamDmaStart = false;
        bool breakOnDmcDmaStart = false;
        bool breakOnExactCpuRead = false;
        bool breakOnExactCpuWrite = false;
        uint16_t exactCpuReadAddress = 0x0000;
        uint16_t exactCpuWriteAddress = 0x0000;
    };

    struct DebugBreakpointHit
    {
        bool valid = false;
        std::string reason;
        bool hasAddress = false;
        uint16_t address = 0x0000;
        uint8_t value = 0x00;
        bool isWrite = false;
        uint32_t frame = 0;
        uint32_t cpuCycle = 0;
        uint64_t emulationTick = 0;
        int ppuScanline = 0;
        int ppuCycle = 0;
        uint64_t sequence = 0;
    };

    struct PpuRegisterAccessEvent
    {
        uint16_t address = 0x0000;
        uint8_t value = 0x00;
        uint16_t scanline = 0;
        uint16_t cycle = 0;
        uint32_t frame = 0;
        bool isWrite = false;
    };

    struct PpuViewerScanlineState
    {
        bool valid = false;
        uint16_t rawScrollX = 0;
        uint16_t rawScrollY = 0;
        uint16_t virtualScrollX = 0;
        uint16_t virtualScrollY = 0;
        uint16_t backgroundPatternTableAddress = 0x0000;
        uint16_t snapshotIndex = 0xFFFF;
    };

    struct PpuViewerScanlineSnapshot
    {
        uint32_t chrGeneration = 0;
        uint32_t nametableGeneration = 0;
        uint32_t paletteGeneration = 0;
        uint32_t mapperWriteGeneration = 0;
        std::array<uint8_t, 0x2000> chrData = {};
        std::array<uint8_t, 0x1000> nametableData = {};
        std::array<uint8_t, 0x20> paletteData = {};
    };

    enum class StateLoadAudioPolicy
    {
        ResetOutput,
        PreserveContinuousOutput
    };

private:
    const uint32_t MAX_4011_WRITES_TO_DISABLE_OVERCLOCK = 2;

    Settings m_settings;
    IAudioOutput& m_audioOutput;
    Cartridge m_cartridge;
    CPU2A03 m_cpu;
    PPU m_ppu;
    APU m_apu;
    uint8_t m_ram[0x800]; //2K
    std::unique_ptr<IControllerPortDevice> m_portDevice1;
    std::unique_ptr<IControllerPortDevice> m_portDevice2;
    std::unique_ptr<IExpansionDevice> m_expansionDevice;
    std::unique_ptr<FourScore> m_fourScore;
    std::unique_ptr<HoriAdapter> m_HoriAdapter;
    Console m_console;

    uint32_t m_updateCyclesAcc;

    uint32_t m_4011WriteCounter;
    bool m_newFrame;
    bool m_frameStarted;

    bool m_halt;

    int m_cpuCyclesAcc;

    uint32_t m_cyclesPerSecond;

    uint32_t m_audioRenderCyclesAcc;
    uint32_t m_lastAudioRenderedMs;
    double m_vsyncAudioCompMsAcc;
    int m_vsyncAudioSkipMsDebt;
    uint64_t m_emulationTickCounter;
    bool m_audioOutputRewinding = false;

    uint8_t m_openBus;
    uint16_t m_prevControllerReadAddr;

    uint32_t m_frameCounter;
    HardwareActions m_hardwareActions;

    bool m_runningLoop;
    bool m_paused;

    DebugBreakpointConfig m_debugBreakpointConfig;
    DebugBreakpointHit m_debugBreakpointHit;
    bool m_debugBreakpointsArmed = false;
    bool m_ppuEventTraceEnabled = false;
    std::vector<PpuRegisterAccessEvent> m_ppuRegisterAccessEvents;
    static constexpr size_t MAX_PPU_REGISTER_ACCESS_EVENTS = 4096;
    std::function<bool(uint16_t, uint8_t)> m_externalCpuWriteHandler;
    std::function<std::optional<uint8_t>(uint16_t)> m_externalCpuReadHandler;

    //do not serialize bellow atributtes
    bool m_ppuViewerScanlineTraceEnabled = false;
    uint32_t m_ppuViewerMapperWriteGeneration = 0;
    std::vector<PpuViewerScanlineState> m_ppuViewerScanlineStates;
    std::vector<PpuViewerScanlineSnapshot> m_ppuViewerScanlineSnapshots;
    bool m_saveStateFlag;
    bool m_loadStateFlag;
    uint8_t m_pendingSaveStateSlot = 0;
    uint8_t m_pendingLoadStateSlot = 0;
    bool m_resetRequested;

    uint32_t m_manualResetGeneration = 0;
    uint32_t m_manualLoadStateGeneration = 0;
    bool m_forceSilentAudio = false;
    std::optional<uint32_t> m_lastAudiblyRenderedPlaybackFrame;
    bool m_currentPlaybackFrameRenderedAudibly = false;

    InputFrame m_currentInputFrame;

    Rewind m_rewind;
    NsfPlayer m_nsfPlayer;

    // NSF controller shortcuts are handled in core from P1 button edges.
    bool m_prevNsfSelect = false;
    bool m_prevNsfStart = false;
    bool m_prevNsfLeft = false;
    bool m_prevNsfRight = false;
    bool m_pendingNsfTogglePlayPause = false;
    bool m_pendingNsfStop = false;
    bool m_pendingNsfNextSong = false;
    bool m_pendingNsfPrevSong = false;
    bool m_applyingPendingNsfActions = false;

    bool isNesMultitapActive() const
    {
        return m_settings.getNesMultitapDevice() != Settings::NesMultitapDevice::NONE;
    }

    bool isFamicomMultitapActive() const
    {
        return m_settings.getFamicomMultitapDevice() != Settings::FamicomMultitapDevice::NONE;
    }

    bool isAnyMultitapActive() const
    {
        return isNesMultitapActive() || isFamicomMultitapActive();
    }

    bool isFamicomLikeSystem() const
    {
        auto& cartridge = const_cast<Cartridge&>(m_cartridge);

        if(!cartridge.hasCartridgeData()) {
            return false;
        }

        const auto system = cartridge.system();
        return system == GameDatabase::System::Famicom || system == GameDatabase::System::FDS;
    }

    Settings::Device standardPortDeviceForCurrentSystem() const
    {
        return isFamicomLikeSystem() ? Settings::Device::FAMICOM_CONTROLLER
                                     : Settings::Device::CONTROLLER;
    }

    std::unique_ptr<IControllerPortDevice> createPortDevice(Settings::Port port, Settings::Device device)
    {
        switch(device) {
            case Settings::Device::NONE:
                return nullptr;
            case Settings::Device::CONTROLLER:
                return std::make_unique<NesStandardController>();
            case Settings::Device::FAMICOM_CONTROLLER:
                return std::make_unique<FamicomController>(port == Settings::Port::P_2);
            case Settings::Device::ZAPPER:
                return std::make_unique<Zapper>();
            case Settings::Device::ARKANOID_CONTROLLER:
                return std::make_unique<ArkanoidControllerNes>();
            case Settings::Device::BANDAI_HYPERSHOT:
                return std::make_unique<NesStandardController>();
            case Settings::Device::SNES_MOUSE:
                return std::make_unique<SnesMouse>();
            case Settings::Device::SUBOR_MOUSE:
                return std::make_unique<SuborMouse>();
            case Settings::Device::SNES_CONTROLLER:
                return std::make_unique<SnesController>();
            case Settings::Device::VIRTUAL_BOY_CONTROLLER:
                return std::make_unique<VirtualBoyController>();
            case Settings::Device::POWER_PAD_SIDE_A:
                return std::make_unique<PowerPad>(false);
            case Settings::Device::POWER_PAD_SIDE_B:
                return std::make_unique<PowerPad>(true);
        }

        return createPortDevice(port, standardPortDeviceForCurrentSystem());
    }

    std::unique_ptr<IExpansionDevice> createExpansionDevice(Settings::ExpansionDevice device)
    {
        switch(device) {
            case Settings::ExpansionDevice::NONE:
                return nullptr;
            case Settings::ExpansionDevice::STANDARD_CONTROLLER_FAMICOM:
                return std::make_unique<FamicomExpansionStandardController>();
            case Settings::ExpansionDevice::BANDAI_HYPERSHOT:
                return std::make_unique<BandaiHyperShot>();
            case Settings::ExpansionDevice::KONAMI_HYPERSHOT:
                return std::make_unique<KonamiHyperShot>();
            case Settings::ExpansionDevice::ARKANOID_CONTROLLER:
                return std::make_unique<ArkanoidControllerFamicom>();
            case Settings::ExpansionDevice::FAMILY_TRAINER_SIDE_A:
                return std::make_unique<FamilyTrainer>(false);
            case Settings::ExpansionDevice::FAMILY_TRAINER_SIDE_B:
                return std::make_unique<FamilyTrainer>(true);
            case Settings::ExpansionDevice::SUBOR_KEYBOARD:
                return std::make_unique<SuborKeyboard>();
            case Settings::ExpansionDevice::FAMILY_BASIC_KEYBOARD:
                return std::make_unique<FamilyBasicKeyboard>();
        }

        return nullptr;
    }

    bool matchesPortDeviceType(Settings::Port port, const IControllerPortDevice* device, Settings::Device type) const
    {
        switch(type) {
            case Settings::Device::NONE:
                return device == nullptr;
            case Settings::Device::CONTROLLER:
                return dynamic_cast<const NesStandardController*>(device) != nullptr &&
                       dynamic_cast<const FamicomController*>(device) == nullptr;
            case Settings::Device::FAMICOM_CONTROLLER:
            {
                const auto* famicom = dynamic_cast<const FamicomController*>(device);
                return famicom != nullptr && famicom->hasMic() == (port == Settings::Port::P_2);
            }
            case Settings::Device::ZAPPER:
                return dynamic_cast<const Zapper*>(device) != nullptr;
            case Settings::Device::ARKANOID_CONTROLLER:
                return dynamic_cast<const ArkanoidControllerNes*>(device) != nullptr;
            case Settings::Device::BANDAI_HYPERSHOT:
                return dynamic_cast<const NesStandardController*>(device) != nullptr;
            case Settings::Device::SNES_MOUSE:
                return dynamic_cast<const SnesMouse*>(device) != nullptr;
            case Settings::Device::SUBOR_MOUSE:
                return dynamic_cast<const SuborMouse*>(device) != nullptr;
            case Settings::Device::SNES_CONTROLLER:
                return dynamic_cast<const SnesController*>(device) != nullptr;
            case Settings::Device::VIRTUAL_BOY_CONTROLLER:
                return dynamic_cast<const VirtualBoyController*>(device) != nullptr;
            case Settings::Device::POWER_PAD_SIDE_A:
            {
                const auto* powerPad = dynamic_cast<const PowerPad*>(device);
                return powerPad != nullptr && !powerPad->isSideB();
            }
            case Settings::Device::POWER_PAD_SIDE_B:
            {
                const auto* powerPad = dynamic_cast<const PowerPad*>(device);
                return powerPad != nullptr && powerPad->isSideB();
            }
        }

        return false;
    }

    bool matchesExpansionDeviceType(const IExpansionDevice* device, Settings::ExpansionDevice type) const
    {
        switch(type) {
            case Settings::ExpansionDevice::NONE:
                return device == nullptr;
            case Settings::ExpansionDevice::STANDARD_CONTROLLER_FAMICOM:
                return dynamic_cast<const FamicomExpansionStandardController*>(device) != nullptr;
            case Settings::ExpansionDevice::BANDAI_HYPERSHOT:
                return dynamic_cast<const BandaiHyperShot*>(device) != nullptr;
            case Settings::ExpansionDevice::KONAMI_HYPERSHOT:
                return dynamic_cast<const KonamiHyperShot*>(device) != nullptr;
            case Settings::ExpansionDevice::ARKANOID_CONTROLLER:
                return dynamic_cast<const ArkanoidControllerFamicom*>(device) != nullptr;
            case Settings::ExpansionDevice::FAMILY_TRAINER_SIDE_A:
            {
                const auto* familyTrainer = dynamic_cast<const FamilyTrainer*>(device);
                return familyTrainer != nullptr && !familyTrainer->isSideB();
            }
            case Settings::ExpansionDevice::FAMILY_TRAINER_SIDE_B:
            {
                const auto* familyTrainer = dynamic_cast<const FamilyTrainer*>(device);
                return familyTrainer != nullptr && familyTrainer->isSideB();
            }
            case Settings::ExpansionDevice::SUBOR_KEYBOARD:
                return dynamic_cast<const SuborKeyboard*>(device) != nullptr;
            case Settings::ExpansionDevice::FAMILY_BASIC_KEYBOARD:
                return dynamic_cast<const FamilyBasicKeyboard*>(device) != nullptr;
        }

        return false;
    }

    /**
     * Return true on new frame
     */
    template<bool waitForNewFrame>
    GERANES_INLINE bool _update(uint32_t dt, bool renderAudio = true) //miliseconds
    {
        const int AUDIO_RENDER_TIME_STEP = 1; //ms

        if(!m_cartridge.isValid()) return false;        

        dt = std::min(dt, (uint32_t)1000/10);  //0.1s

        if constexpr(!waitForNewFrame)
            m_updateCyclesAcc += m_cyclesPerSecond * dt;

        const uint32_t audioRenderCycles = m_cyclesPerSecond * AUDIO_RENDER_TIME_STEP;

        bool newFrame = false;
        uint32_t renderedAudioMs = 0;

        bool loop = false;

        if constexpr(waitForNewFrame)
            loop = true;
        else
            loop = m_updateCyclesAcc >= 1000;

        m_runningLoop = true;

        

        const bool silentAudio = m_forceSilentAudio || !renderAudio;

        while(loop)
        {
            bool advanced = false;
            if constexpr(waitForNewFrame) {
                advanced = stepEmulationTick<false>(audioRenderCycles, renderedAudioMs, newFrame, silentAudio);
            }
            else {
                advanced = stepEmulationTick<true>(audioRenderCycles, renderedAudioMs, newFrame, silentAudio);
            }
            if(!advanced) break;

            if(m_paused) {
                break;
            }

            if constexpr(waitForNewFrame)
                loop = !newFrame;
            else
                loop = m_updateCyclesAcc >= 1000;

        }

        m_lastAudioRenderedMs = renderedAudioMs;
        m_runningLoop = false;

        if(m_saveStateFlag) {
            _saveState(m_pendingSaveStateSlot);
            m_saveStateFlag = false;
        }

        if(m_loadStateFlag) {
            _loadState(m_pendingLoadStateSlot);
            m_loadStateFlag = false;
        }

        return newFrame;
    }

    GERANES_INLINE bool update(uint32_t dt, bool renderAudio = true) {
        applyPendingNsfControllerActions();
        if(m_paused) return false;
        return _update<false>(dt, renderAudio);
    }

    void updateInputDevicePixelCheckers()
    {
        auto pixelChecker = [&](int x, int y) {
            uint32_t pixel = m_ppu.getZapperPixel(x, y);

            int r = pixel & 0xFF;
            int g = (pixel >> 8) & 0xFF;
            int b = (pixel >> 16) & 0xFF;

            return 0.2126f * r + 0.7152f * g + 0.0722f * b;
        };

        if(m_portDevice1) m_portDevice1->setPixelChecker(pixelChecker);
        if(m_portDevice2) m_portDevice2->setPixelChecker(pixelChecker);
        if(m_expansionDevice) m_expansionDevice->setPixelChecker(pixelChecker);
    }

    void recreatePortDevice(Settings::Port port)
    {
        const Settings::Device device = m_settings.getPortDevice(port).value_or(standardPortDeviceForCurrentSystem());
        IControllerPortDevice* currentDevice =
            (port == Settings::Port::P_1) ? m_portDevice1.get() : m_portDevice2.get();
        if(matchesPortDeviceType(port, currentDevice, device)) {
            updateInputDevicePixelCheckers();
            return;
        }

        std::unique_ptr<IControllerPortDevice> instance = createPortDevice(port, device);
        if(port == Settings::Port::P_1) m_portDevice1 = std::move(instance);
        else m_portDevice2 = std::move(instance);
        updateInputDevicePixelCheckers();
    }

    void recreateExpansionDevice()
    {
        if(matchesExpansionDeviceType(m_expansionDevice.get(), m_settings.getExpansionDevice())) {
            updateInputDevicePixelCheckers();
            return;
        }

        m_expansionDevice = createExpansionDevice(m_settings.getExpansionDevice());
        updateInputDevicePixelCheckers();
    }

    void recreateInputRouting()
    {
        if(isNesMultitapActive()) {
            if(!m_fourScore) m_fourScore = std::make_unique<FourScore>();
            m_HoriAdapter.reset();
            m_portDevice1.reset();
            m_portDevice2.reset();
            m_expansionDevice.reset();
            return;
        }

        if(isFamicomMultitapActive()) {
            if(!m_HoriAdapter) m_HoriAdapter = std::make_unique<HoriAdapter>();
            m_fourScore.reset();
            m_portDevice1.reset();
            m_portDevice2.reset();
            m_expansionDevice.reset();
            return;
        }

        m_fourScore.reset();
        m_HoriAdapter.reset();
        recreatePortDevice(Settings::Port::P_1);
        recreatePortDevice(Settings::Port::P_2);
        recreateExpansionDevice();
    }

    InputFrame makeDefaultInputFrame(uint32_t frame)
    {
        InputFrame inputFrame;
        inputFrame.frame = frame;
        inputFrame.state.topology.port1Device = m_settings.getPortDevice(Settings::Port::P_1);
        inputFrame.state.topology.port2Device = m_settings.getPortDevice(Settings::Port::P_2);
        inputFrame.state.topology.expansionDevice = m_settings.getExpansionDevice();
        inputFrame.state.topology.nesMultitapDevice = m_settings.getNesMultitapDevice();
        inputFrame.state.topology.famicomMultitapDevice = m_settings.getFamicomMultitapDevice();
        return inputFrame;
    }

    void applyInputFrame(const InputFrame& inputFrame)
    {
        const InputTopology& topology = inputFrame.state.topology;
        const Settings::Device port1Device = topology.port1Device.value_or(Settings::Device::NONE);
        const Settings::Device port2Device = topology.port2Device.value_or(Settings::Device::NONE);
        const Settings::ExpansionDevice expansionDevice = topology.expansionDevice;
        const Settings::NesMultitapDevice nesMultitapDevice = topology.nesMultitapDevice;
        const Settings::FamicomMultitapDevice famicomMultitapDevice = topology.famicomMultitapDevice;
        const InputState::PadButtons p1 = inputFrame.state.portButtons(1);
        const InputState::PadButtons p2 = inputFrame.state.portButtons(2);
        const InputState::PadButtons p3 = inputFrame.state.portButtons(3);
        const InputState::PadButtons p4 = inputFrame.state.portButtons(4);
        const InputState::PadButtons bandaiButtons = inputFrame.state.bandaiButtons();
        const InputState::PointerState zapper1 = inputFrame.state.zapper(1);
        const InputState::PointerState zapper2 = inputFrame.state.zapper(2);
        const InputState::PointerState bandaiPointer = inputFrame.state.bandaiPointer();
        const InputState::ArkanoidState arkanoid1 = inputFrame.state.arkanoidController(1);
        const InputState::ArkanoidState arkanoid2 = inputFrame.state.arkanoidController(2);
        const InputState::ArkanoidState arkanoidExpansion = inputFrame.state.arkanoidExpansion();
        const InputState::KonamiHyperShotState konami = inputFrame.state.konamiHyperShot();
        const InputState::RelativePointerState snes1 = inputFrame.state.snesMouse(1);
        const InputState::RelativePointerState snes2 = inputFrame.state.snesMouse(2);
        const InputState::RelativePointerState subor1 = inputFrame.state.suborMouse(1);
        const InputState::RelativePointerState subor2 = inputFrame.state.suborMouse(2);

        if(m_settings.getNesMultitapDevice() != nesMultitapDevice) {
            setNesMultitapDevice(nesMultitapDevice);
        }
        if(m_settings.getFamicomMultitapDevice() != famicomMultitapDevice) {
            setFamicomMultitapDevice(famicomMultitapDevice);
        }
        if(m_settings.getPortDevice(Settings::Port::P_1).value_or(Settings::Device::NONE) != port1Device) {
            setPortDevice(Settings::Port::P_1, port1Device);
        }
        if(m_settings.getPortDevice(Settings::Port::P_2).value_or(Settings::Device::NONE) != port2Device) {
            setPortDevice(Settings::Port::P_2, port2Device);
        }
        if(m_settings.getExpansionDevice() != expansionDevice) {
            setExpansionDevice(expansionDevice);
        }

        if(m_fourScore) m_fourScore->setControllerButtons(0, p1.a, p1.b, p1.select, p1.start, p1.up, p1.down, p1.left, p1.right);
        else if(m_HoriAdapter) m_HoriAdapter->setControllerButtons(0, p1.a, p1.b, p1.select, p1.start, p1.up, p1.down, p1.left, p1.right);
        else if(m_portDevice1) m_portDevice1->setButtonsStatusExtended(p1.a, p1.b, p1.select, p1.start, p1.up, p1.down, p1.left, p1.right, p1.x, p1.y, p1.l, p1.r);

        if(m_fourScore) m_fourScore->setControllerButtons(1, p2.a, p2.b, p2.select, p2.start, p2.up, p2.down, p2.left, p2.right);
        else if(m_HoriAdapter) m_HoriAdapter->setControllerButtons(1, p2.a, p2.b, p2.select, p2.start, p2.up, p2.down, p2.left, p2.right);
        else if(m_portDevice2) m_portDevice2->setButtonsStatusExtended(p2.a, p2.b, p2.select, p2.start, p2.up, p2.down, p2.left, p2.right, p2.x, p2.y, p2.l, p2.r);

        if(m_fourScore) m_fourScore->setControllerButtons(2, p3.a, p3.b, p3.select, p3.start, p3.up, p3.down, p3.left, p3.right);
        else if(m_HoriAdapter) m_HoriAdapter->setControllerButtons(2, p3.a, p3.b, p3.select, p3.start, p3.up, p3.down, p3.left, p3.right);
        else if(m_settings.getExpansionDevice() == Settings::ExpansionDevice::STANDARD_CONTROLLER_FAMICOM && m_expansionDevice) m_expansionDevice->setButtonsStatus(p3.a, p3.b, p3.select, p3.start, p3.up, p3.down, p3.left, p3.right);

        if(m_fourScore) m_fourScore->setControllerButtons(3, p4.a, p4.b, p4.select, p4.start, p4.up, p4.down, p4.left, p4.right);
        else if(m_HoriAdapter) m_HoriAdapter->setControllerButtons(3, p4.a, p4.b, p4.select, p4.start, p4.up, p4.down, p4.left, p4.right);

        processNsfControllerInput(p1.select, p1.start, p1.left, p1.right);

        if(m_settings.getPortDevice(Settings::Port::P_1) == std::optional<Settings::Device>(Settings::Device::ZAPPER) && m_portDevice1) {
            m_portDevice1->setCursorPosition(zapper1.x, zapper1.y);
            m_portDevice1->setTrigger(zapper1.trigger);
        }
        if(m_settings.getPortDevice(Settings::Port::P_2) == std::optional<Settings::Device>(Settings::Device::ZAPPER) && m_portDevice2) {
            m_portDevice2->setCursorPosition(zapper2.x, zapper2.y);
            m_portDevice2->setTrigger(zapper2.trigger);
        }

        if(m_settings.getExpansionDevice() == Settings::ExpansionDevice::BANDAI_HYPERSHOT && m_expansionDevice) {
            m_expansionDevice->setButtonsStatus(bandaiButtons.a, bandaiButtons.b, bandaiButtons.select, bandaiButtons.start, bandaiButtons.up, bandaiButtons.down, bandaiButtons.left, bandaiButtons.right);
            m_expansionDevice->setCursorPosition(bandaiPointer.x, bandaiPointer.y);
            m_expansionDevice->setTrigger(bandaiPointer.trigger);
        }

        if(m_settings.getPortDevice(Settings::Port::P_1) == std::optional<Settings::Device>(Settings::Device::ARKANOID_CONTROLLER) && m_portDevice1) {
            m_portDevice1->setPositionNormalized(std::clamp(arkanoid1.position, 0.0f, 1.0f));
            m_portDevice1->setTrigger(arkanoid1.button);
        }
        if(m_settings.getPortDevice(Settings::Port::P_2) == std::optional<Settings::Device>(Settings::Device::ARKANOID_CONTROLLER) && m_portDevice2) {
            m_portDevice2->setPositionNormalized(std::clamp(arkanoid2.position, 0.0f, 1.0f));
            m_portDevice2->setTrigger(arkanoid2.button);
        }
        if(m_settings.getExpansionDevice() == Settings::ExpansionDevice::ARKANOID_CONTROLLER && m_expansionDevice) {
            m_expansionDevice->setPositionNormalized(std::clamp(arkanoidExpansion.position, 0.0f, 1.0f));
            m_expansionDevice->setTrigger(arkanoidExpansion.button);
        }

        if(m_settings.getExpansionDevice() == Settings::ExpansionDevice::KONAMI_HYPERSHOT && m_expansionDevice) {
            m_expansionDevice->setPlayersButtons(konami.p1Run, konami.p1Jump, konami.p2Run, konami.p2Jump);
        }

        if(m_settings.getPortDevice(Settings::Port::P_1) == std::optional<Settings::Device>(Settings::Device::SNES_MOUSE) && m_portDevice1) {
            m_portDevice1->addRelativeMotion(snes1.deltaX, snes1.deltaY);
            m_portDevice1->setTrigger(snes1.primary);
            m_portDevice1->setSecondaryTrigger(snes1.secondary);
        }
        if(m_settings.getPortDevice(Settings::Port::P_2) == std::optional<Settings::Device>(Settings::Device::SNES_MOUSE) && m_portDevice2) {
            m_portDevice2->addRelativeMotion(snes2.deltaX, snes2.deltaY);
            m_portDevice2->setTrigger(snes2.primary);
            m_portDevice2->setSecondaryTrigger(snes2.secondary);
        }

        if(m_settings.getPortDevice(Settings::Port::P_1) == std::optional<Settings::Device>(Settings::Device::VIRTUAL_BOY_CONTROLLER) && m_portDevice1) {
            m_portDevice1->setVirtualBoyButtons(p1.a, p1.b, p1.select, p1.start, p1.up, p1.down, p1.left, p1.right, p1.up2, p1.down2, p1.left2, p1.right2, p1.l, p1.r);
        }
        if(m_settings.getPortDevice(Settings::Port::P_2) == std::optional<Settings::Device>(Settings::Device::VIRTUAL_BOY_CONTROLLER) && m_portDevice2) {
            m_portDevice2->setVirtualBoyButtons(p2.a, p2.b, p2.select, p2.start, p2.up, p2.down, p2.left, p2.right, p2.up2, p2.down2, p2.left2, p2.right2, p2.l, p2.r);
        }

        if(m_settings.getPortDevice(Settings::Port::P_1) == std::optional<Settings::Device>(Settings::Device::SUBOR_MOUSE) && m_portDevice1) {
            m_portDevice1->addRelativeMotion(subor1.deltaX, subor1.deltaY);
            m_portDevice1->setTrigger(subor1.primary);
            m_portDevice1->setSecondaryTrigger(subor1.secondary);
        }
        if(m_settings.getPortDevice(Settings::Port::P_2) == std::optional<Settings::Device>(Settings::Device::SUBOR_MOUSE) && m_portDevice2) {
            m_portDevice2->addRelativeMotion(subor2.deltaX, subor2.deltaY);
            m_portDevice2->setTrigger(subor2.primary);
            m_portDevice2->setSecondaryTrigger(subor2.secondary);
        }

        if(m_settings.getExpansionDevice() == Settings::ExpansionDevice::SUBOR_KEYBOARD && m_expansionDevice) m_expansionDevice->setSuborKeyboardKeys(inputFrame.state.suborKeyboardKeys());
        if(m_settings.getExpansionDevice() == Settings::ExpansionDevice::FAMILY_BASIC_KEYBOARD && m_expansionDevice) m_expansionDevice->setFamilyBasicKeyboardKeys(inputFrame.state.familyBasicKeyboardKeys());

        const auto p1Device = m_settings.getPortDevice(Settings::Port::P_1);
        const auto p2Device = m_settings.getPortDevice(Settings::Port::P_2);
        const bool familyTrainer =
            m_settings.getExpansionDevice() == Settings::ExpansionDevice::FAMILY_TRAINER_SIDE_A ||
            m_settings.getExpansionDevice() == Settings::ExpansionDevice::FAMILY_TRAINER_SIDE_B;
        if((p1Device == std::optional<Settings::Device>(Settings::Device::POWER_PAD_SIDE_A) || p1Device == std::optional<Settings::Device>(Settings::Device::POWER_PAD_SIDE_B)) && m_portDevice1) m_portDevice1->setPowerPadButtons(inputFrame.state.powerPadButtons(1));
        if((p2Device == std::optional<Settings::Device>(Settings::Device::POWER_PAD_SIDE_A) || p2Device == std::optional<Settings::Device>(Settings::Device::POWER_PAD_SIDE_B)) && m_portDevice2) m_portDevice2->setPowerPadButtons(inputFrame.state.powerPadButtons(2));
        if(familyTrainer && m_expansionDevice) m_expansionDevice->setPowerPadButtons(inputFrame.state.powerPadButtons(1));
    }

    GERANES_INLINE void setPortDevice(Settings::Port port, Settings::Device device)
    {
        m_settings.setPortDevice(port, device);
        if(!isAnyMultitapActive()) recreatePortDevice(port);
    }

    GERANES_INLINE void setExpansionDevice(Settings::ExpansionDevice device)
    {
        m_settings.setExpansionDevice(device);
        if(!isAnyMultitapActive()) recreateExpansionDevice();
    }

    GERANES_INLINE void setNesMultitapDevice(Settings::NesMultitapDevice device)
    {
        m_settings.setNesMultitapDevice(device);
        if(device != Settings::NesMultitapDevice::NONE) {
            m_settings.setFamicomMultitapDevice(Settings::FamicomMultitapDevice::NONE);
        }
        recreateInputRouting();
    }

    GERANES_INLINE void setFamicomMultitapDevice(Settings::FamicomMultitapDevice device)
    {
        m_settings.setFamicomMultitapDevice(device);
        if(device != Settings::FamicomMultitapDevice::NONE) {
            m_settings.setNesMultitapDevice(Settings::NesMultitapDevice::NONE);
        }
        recreateInputRouting();
    }

    void processNsfControllerInput(bool selectPressed, bool startPressed, bool leftPressed, bool rightPressed)
    {
        const bool startJustPressed = startPressed && !m_prevNsfStart;
        const bool selectJustPressed = selectPressed && !m_prevNsfSelect;
        const bool leftJustPressed = leftPressed && !m_prevNsfLeft;
        const bool rightJustPressed = rightPressed && !m_prevNsfRight;

        if(m_nsfPlayer.isLoaded()) {
            if(startJustPressed) {
                m_pendingNsfTogglePlayPause = true;
            }
            if(selectJustPressed) m_pendingNsfStop = true;
            if(rightJustPressed) m_pendingNsfNextSong = true;
            if(leftJustPressed) m_pendingNsfPrevSong = true;
        }

        m_prevNsfSelect = selectPressed;
        m_prevNsfStart = startPressed;
        m_prevNsfLeft = leftPressed;
        m_prevNsfRight = rightPressed;
    }

    void applyPendingNsfControllerActions()
    {
        if(m_applyingPendingNsfActions) return;
        m_applyingPendingNsfActions = true;

        const bool pendingTogglePlayPause = m_pendingNsfTogglePlayPause;
        const bool pendingStop = m_pendingNsfStop;
        const bool pendingNextSong = m_pendingNsfNextSong;
        const bool pendingPrevSong = m_pendingNsfPrevSong;

        // Clear first to avoid reentrant re-application when an action triggers reset().
        m_pendingNsfTogglePlayPause = false;
        m_pendingNsfStop = false;
        m_pendingNsfNextSong = false;
        m_pendingNsfPrevSong = false;

        if(!m_nsfPlayer.isLoaded()) {
            m_applyingPendingNsfActions = false;
            return;
        }

        if(pendingTogglePlayPause) {
            if(m_nsfPlayer.isPlaying()) m_nsfPlayer.pause();
            else m_nsfPlayer.play();
        }
        if(pendingStop) m_nsfPlayer.stop();
        if(pendingNextSong) m_nsfPlayer.nextSong();
        if(pendingPrevSong) m_nsfPlayer.prevSong();

        m_applyingPendingNsfActions = false;
    }

    template<AccessType accessType>
    auto accessBus(int addr, uint8_t data = 0) -> std::conditional_t<accessType == AccessType::Write, void, uint8_t>
    {
        if constexpr(accessType == AccessType::Read) data = m_openBus;

        if constexpr(accessType == AccessType::Write) {
            if(m_externalCpuWriteHandler && m_externalCpuWriteHandler(static_cast<uint16_t>(addr), data)) {
                return;
            }
        } else {
            if(m_externalCpuReadHandler) {
                if(const std::optional<uint8_t> customValue = m_externalCpuReadHandler(static_cast<uint16_t>(addr)); customValue.has_value()) {
                    m_openBus = *customValue;
                    return *customValue;
                }
            }
        }

        bool updateOpenBusOnRead = true;
        switch(addr>>12)
        {
        case 0:
        case 1:
            if constexpr(accessType == AccessType::Write) m_ram[addr&0x7FF] = data;
            else data = m_ram[addr&0x7FF];
            break;
        case 2:
        case 3:
            if constexpr(accessType == AccessType::Read) {
                m_cartridge.onCpuRead(static_cast<uint16_t>(addr));
                m_ppu.setCpuDmaReadInProgress(m_cpu.isDmaReadInProgress());
                data = m_ppu.readWrite<false>(addr, data);
                m_ppu.setCpuDmaReadInProgress(false);
            }
            else {
                m_ppu.readWrite<true>(addr, data);
            }
            break;
        case 4:
        case 5:
            if(addr < 0x4014) { //APU registers

                if constexpr(accessType == AccessType::Write) {

                    m_apu.write(addr&0x3FFF, data, (m_cpu.cycleCounter() & 0x01) != 0);

                    //disable overclock when the game generate PCM audio
                    if(addr == 0x4011){
                        if(++m_4011WriteCounter == MAX_4011_WRITES_TO_DISABLE_OVERCLOCK && m_ppu.isOverclockFrame()) {
                            m_ppu.setOverclockFrame(false);
                            updateCyclesPerSecond();                            
                         }
                    }
                }
                else {
                    data = m_openBus;
                }

            }
            else if(addr < 0x4018) {

                switch(addr) {
                //DMA transfer
                case 0x4014: //acess: write only
                {
                    if constexpr(accessType == AccessType::Write)
                    {
                        uint16_t addr = static_cast<uint16_t>(data) << 8;
                        if(m_debugBreakpointConfig.breakOnOamDmaStart) {
                            triggerDebugBreakpoint("OAM DMA start", static_cast<uint16_t>(0x4014), data, true, true);
                        }
                        m_cpu.startOamDma(addr);
                    }
                    break;
                }

                case 0x4015: //APU
                {
                    if constexpr(accessType == AccessType::Write) {
                        m_apu.write(addr&0x3FFF, data, (m_cpu.cycleCounter() & 0x01) != 0);
                    }
                    else {
                        data = m_apu.read(addr&0x3FFF, (m_cpu.cycleCounter() & 0x01) != 0);
                        data = static_cast<uint8_t>((data & ~0x20) | (m_openBus & 0x20));
                        updateOpenBusOnRead = false;
                    }
                    break;
                 }

                case 0x4016: //controller 1
                {
                    if constexpr(accessType == AccessType::Write)
                    {
                        if(m_fourScore) {
                            m_fourScore->write(data);
                        }
                        else if(m_HoriAdapter) {
                            m_HoriAdapter->write4016(data);
                        }
                        else {
                            if(m_portDevice1) m_portDevice1->write(data);
                            if(m_portDevice2) m_portDevice2->write(data);
                            if(m_expansionDevice) m_expansionDevice->write4016(data);
                        }
                    }
                    else {
                        bool outputEnabled =
                            (!m_cpu.isDmaReadInProgress() || m_cpu.isDmaInputClockEnabled(0x4016));

                        if(m_fourScore) {
                            data = m_fourScore->readPort(0, outputEnabled);
                        }
                        else if(m_HoriAdapter) {
                            data = m_HoriAdapter->read4016(outputEnabled);
                        }
                        else {
                            data = m_portDevice1 ? m_portDevice1->read(outputEnabled) : 0x00;
                        }

                        if(m_expansionDevice && !isAnyMultitapActive()) {
                            data = static_cast<uint8_t>((data & ~0x02) | m_expansionDevice->read4016(outputEnabled));
                        }

                        if(!isAnyMultitapActive() && m_portDevice2) {
                            data = static_cast<uint8_t>(data | m_portDevice2->extraRead4016Bits());
                        }

                        data = m_cartridge.readMapperRegister(addr & 0x1FFF, data);
                        data = m_hardwareActions.applyVsSystemRead4016(
                            data,
                            m_cartridge.isValid() && m_cartridge.system() == GameDatabase::System::VsSystem
                        );
                        data = (data & 0x1F) | (m_openBus & 0xE0);
                    }
                    break;
                }

                case 0x4017: //controller 2
                {
                    if constexpr(accessType == AccessType::Write) m_apu.write(addr&0x3FFF, data, (m_cpu.cycleCounter() & 0x01) != 0);
                    else {

                        bool outputEnabled =
                            (!m_cpu.isDmaReadInProgress() || m_cpu.isDmaInputClockEnabled(0x4017));

                        if(m_fourScore) {
                            data = m_fourScore->readPort(1, outputEnabled);
                        }
                        else if(m_HoriAdapter) {
                            data = m_HoriAdapter->read4017(outputEnabled);
                        }
                        else {
                            data = m_portDevice2 ? m_portDevice2->read(outputEnabled) : 0x00;
                        }

                        if(m_expansionDevice && !isAnyMultitapActive()) {
                            const uint8_t expData = m_expansionDevice->read4017(outputEnabled);
                            // Expansion devices may drive bit1 (Arkanoid Famicom),
                            // bits1-4 (Konami Hyper Shot), and/or bits3-4 (Bandai Hyper Shot) on $4017.
                            data = static_cast<uint8_t>((data & ~0x1E) | (expData & 0x1E));
                        }

                        data = m_cartridge.readMapperRegister(addr & 0x1FFF, data);
                        data = m_hardwareActions.applyVsSystemRead4017(
                            data,
                            m_cartridge.isValid() && m_cartridge.system() == GameDatabase::System::VsSystem
                        );
                        data = (data & 0x1F) | (m_openBus & 0xE0);
                    }
                    break;
                 }                

                }
            }
            else if( addr < 0x4020) { //unallocated IO space
                data = m_openBus;
            }
            else {

                if constexpr(accessType == AccessType::Write) {
                    if(m_cartridge.isNsf()) {
                        m_cartridge.writeMapperRegisterAbsolute(static_cast<uint16_t>(addr), data);
                    }
                    else {
                        m_cartridge.writeMapperRegister(addr&0x1FFF, data);
                    }
                    ++m_ppuViewerMapperWriteGeneration;
                }
                else {
                    if(m_cartridge.isNsf()) {
                        data = m_cartridge.readMapperRegisterAbsolute(static_cast<uint16_t>(addr), data);
                    }
                    else {
                        data = m_cartridge.readMapperRegister(addr&0x1FFF,data);
                    }
                }

                m_openBus = data;
            }

            break;

        case 6:
        case 7:
            if constexpr(accessType == AccessType::Write) m_cartridge.writeSaveRam(addr&0x1FFF, data);
            else data = m_cartridge.readSaveRam(addr&0x1FFF);
            break;

        default: // >= 8
            if constexpr(accessType == AccessType::Write) {
                if(m_cartridge.isNsf()) {
                    m_cartridge.writeMapperRegisterAbsolute(static_cast<uint16_t>(addr), data);
                }
                else {
                    m_cartridge.writePrg(addr&0x7FFF, data);
                }
                ++m_ppuViewerMapperWriteGeneration;
            }
            else {
                m_cartridge.onCpuRead(static_cast<uint16_t>(addr));
                data = m_cartridge.readPrg(addr&0x7FFF);
            }
            break;

        }

        if constexpr(accessType == AccessType::Write) {
            m_openBus = data;
            // Mapper CPU-write hooks must not see DMA writes.
            m_cartridge.onCpuWrite(static_cast<uint16_t>(addr), data);
            

        }
        else if(updateOpenBusOnRead) {
            m_openBus = data;
        }

        m_prevControllerReadAddr = (accessType == AccessType::Read && (addr == 0x4016 || addr == 0x4017))
            ? static_cast<uint16_t>(addr)
            : 0xFFFF;

        processDebugBusAccess<accessType>(static_cast<uint16_t>(addr), data);

        if constexpr(accessType == AccessType::Read)
            return data;
    }

    void onError(const std::string& e)
    {
        signalError(e);
        Logger::instance().log(e, Logger::Type::ERROR);
        m_halt = true;
    }    

    void onFrameStart() {
        m_4011WriteCounter = 0;
        m_ppuRegisterAccessEvents.clear();
        m_cartridge.applyExternalActions(
            m_hardwareActions.consumeFdsPendingActions(
                m_cartridge.isValid() && m_cartridge.system() == GameDatabase::System::FDS
            )
        );
        m_hardwareActions.onFrameStart();
        m_nsfPlayer.onFrameStart();
        updateCyclesPerSecond();        
        
        if(m_currentInputFrame.frame == m_frameCounter) {
            applyInputFrame(m_currentInputFrame);
            signalInputFrameSelected(m_currentInputFrame);
        }

        
    }    

    void onFrameReady() {
        ++m_frameCounter;
    }

    void onPPUScanlineStart()
    {
        if(m_portDevice1) m_portDevice1->onScanlineChanged();
        if(m_portDevice2) m_portDevice2->onScanlineChanged();
        if(m_expansionDevice) m_expansionDevice->onScanlineChanged();
        m_cartridge.onScanlineStart(m_ppu.isActivelyRendering(), m_ppu.scanline());
        if(m_ppuViewerScanlineTraceEnabled) {
            const int scanline = m_ppu.scanline();
            if(scanline >= 0 && scanline < 240) {
                if(m_ppuViewerScanlineStates.size() != 240u) {
                    m_ppuViewerScanlineStates.resize(240u);
                }

                auto& lineState = m_ppuViewerScanlineStates[static_cast<size_t>(scanline)];
                lineState.valid = true;
                const int rawScrollX = m_ppu.getRawScrollX();
                const int rawScrollY = m_ppu.getRawScrollY();
                const int virtualScrollX = m_ppu.getVirtualScrollX();
                const int virtualScrollY = m_ppu.getVirtualScrollY();
                const uint32_t chrGeneration = m_ppu.debugChrGeneration();
                const uint32_t nametableGeneration = m_ppu.debugNametableGeneration();
                const uint32_t paletteGeneration = m_ppu.debugPaletteGeneration();
                const uint32_t mapperWriteGeneration = m_ppuViewerMapperWriteGeneration;
                lineState.rawScrollX = static_cast<uint16_t>(rawScrollX);
                lineState.rawScrollY = static_cast<uint16_t>(rawScrollY);
                lineState.virtualScrollX = static_cast<uint16_t>(virtualScrollX);
                lineState.virtualScrollY = static_cast<uint16_t>(virtualScrollY);
                lineState.backgroundPatternTableAddress =
                    static_cast<uint16_t>(m_ppu.debugBackgroundPatternTableAddress());
                lineState.snapshotIndex = 0xFFFF;

                if(scanline > 0) {
                    const auto& prevLineState =
                        m_ppuViewerScanlineStates[static_cast<size_t>(scanline - 1)];
                    if(prevLineState.valid &&
                       prevLineState.rawScrollX == lineState.rawScrollX &&
                       prevLineState.rawScrollY == lineState.rawScrollY &&
                       prevLineState.virtualScrollX == lineState.virtualScrollX &&
                       prevLineState.virtualScrollY == lineState.virtualScrollY &&
                       prevLineState.backgroundPatternTableAddress == lineState.backgroundPatternTableAddress &&
                       prevLineState.snapshotIndex != 0xFFFF &&
                       prevLineState.snapshotIndex < m_ppuViewerScanlineSnapshots.size()) {
                        const auto& prevSnapshot =
                            m_ppuViewerScanlineSnapshots[prevLineState.snapshotIndex];
                        if(prevSnapshot.chrGeneration == chrGeneration &&
                           prevSnapshot.nametableGeneration == nametableGeneration &&
                           prevSnapshot.paletteGeneration == paletteGeneration &&
                           prevSnapshot.mapperWriteGeneration == mapperWriteGeneration) {
                            lineState.snapshotIndex = prevLineState.snapshotIndex;
                        }
                    }
                }

                if(lineState.snapshotIndex == 0xFFFF) {
                    for(size_t snapshotIndex = 0; snapshotIndex < m_ppuViewerScanlineSnapshots.size(); ++snapshotIndex) {
                        const auto& existingSnapshot = m_ppuViewerScanlineSnapshots[snapshotIndex];
                        if(existingSnapshot.chrGeneration == chrGeneration &&
                           existingSnapshot.nametableGeneration == nametableGeneration &&
                           existingSnapshot.paletteGeneration == paletteGeneration &&
                           existingSnapshot.mapperWriteGeneration == mapperWriteGeneration) {
                            lineState.snapshotIndex = static_cast<uint16_t>(snapshotIndex);
                            break;
                        }
                    }
                }

                if(lineState.snapshotIndex == 0xFFFF) {
                    PpuViewerScanlineSnapshot snapshot;
                    snapshot.chrGeneration = chrGeneration;
                    snapshot.nametableGeneration = nametableGeneration;
                    snapshot.paletteGeneration = paletteGeneration;
                    snapshot.mapperWriteGeneration = mapperWriteGeneration;
                    for(uint16_t addr = 0; addr < 0x2000; ++addr) {
                        snapshot.chrData[addr] = m_ppu.debugPeekPpuMemory(addr);
                    }
                    for(uint16_t addr = 0; addr < 0x1000; ++addr) {
                        snapshot.nametableData[addr] =
                            m_ppu.debugPeekPpuMemory(static_cast<uint16_t>(0x2000 + addr));
                    }
                    for(uint16_t addr = 0; addr < 0x20; ++addr) {
                        snapshot.paletteData[addr] =
                            m_ppu.debugPeekPpuMemory(static_cast<uint16_t>(0x3F00 + addr));
                    }
                    m_ppuViewerScanlineSnapshots.push_back(std::move(snapshot));
                    lineState.snapshotIndex =
                        static_cast<uint16_t>(m_ppuViewerScanlineSnapshots.size() - 1);
                }
            }
        }
    }

    void onDMCRequest(uint16_t addr, bool reload) {
        if(m_debugBreakpointConfig.breakOnDmcDmaStart) {
            triggerDebugBreakpoint(reload ? "DMC DMA reload" : "DMC DMA start", addr, 0x00, false, true);
        }
        m_cpu.startDmcDma(addr, reload);
    }

    void onDMCCancelRequest() {
        m_cpu.cancelDmcDma();
    }

    void onDMCImplicitAbortRequest() {
        m_cpu.scheduleImplicitDmcSingleCycleAbort();
    }

    void resyncAudioAfterStateLoad(StateLoadAudioPolicy audioPolicy = StateLoadAudioPolicy::ResetOutput)
    {
        // Audio output internals (wave generators/FIFOs) are not part of save states.
        // Hard resync/manual load resets the live output, but ordinary recovery
        // must preserve the active device/queue so transient jitter does not
        // create long audible dropouts.
        if(audioPolicy == StateLoadAudioPolicy::ResetOutput) {
            m_audioOutput.discardQueuedAudio();
            m_audioOutput.clearAudioBuffers();
            m_lastAudioRenderedMs = 0;
            m_vsyncAudioCompMsAcc = 0.0;
            m_vsyncAudioSkipMsDebt = 0;
            if(m_frameCounter == 0) {
                m_lastAudiblyRenderedPlaybackFrame.reset();
            }
            else {
                m_lastAudiblyRenderedPlaybackFrame = m_frameCounter - 1u;
            }
        }
        m_audioOutputRewinding = m_rewind.isRewinding();
        m_audioOutput.setRewinding(m_audioOutputRewinding);
        m_audioOutput.setExpansionSourceRateHz(m_settings.CPUClockHz());
        m_audioOutput.setExpansionAudioVolume(1.0f);
        m_apu.updateAudioOutput();
    }

    void resetVolatileStateAfterStateLoad()
    {
        // These fields are intentionally not serialized, so state loading must
        // reinitialize them to avoid behavior that depends on the instance's
        // history before the load.
        m_saveStateFlag = false;
        m_loadStateFlag = false;
        m_resetRequested = false;
        m_prevControllerReadAddr = 0xFFFF;
        m_cpu.resetVolatileStateAfterLoad();
        m_prevNsfSelect = false;
        m_prevNsfStart = false;
        m_prevNsfLeft = false;
        m_prevNsfRight = false;
        m_pendingNsfTogglePlayPause = false;
        m_pendingNsfStop = false;
        m_pendingNsfNextSong = false;
        m_pendingNsfPrevSong = false;
        m_applyingPendingNsfActions = false;
        m_forceSilentAudio = false;
        m_audioOutputRewinding = m_rewind.isRewinding();
        m_currentPlaybackFrameRenderedAudibly = false;
        ++m_ppuViewerMapperWriteGeneration;
        m_ppuViewerScanlineSnapshots.clear();
        for(auto& lineState : m_ppuViewerScanlineStates) {
            lineState.valid = false;
            lineState.snapshotIndex = 0xFFFF;
        }
    }

    bool debugBreakpointsActive() const
    {
        return m_debugBreakpointsArmed &&
               m_debugBreakpointConfig.enabled &&
               m_cartridge.isValid() &&
               !m_halt;
    }

    void triggerDebugBreakpoint(const std::string& reason,
                                uint16_t address = 0x0000,
                                uint8_t value = 0x00,
                                bool isWrite = false,
                                bool hasAddress = false)
    {
        if(!debugBreakpointsActive()) {
            return;
        }

        m_debugBreakpointHit.valid = true;
        m_debugBreakpointHit.reason = reason;
        m_debugBreakpointHit.hasAddress = hasAddress;
        m_debugBreakpointHit.address = address;
        m_debugBreakpointHit.value = value;
        m_debugBreakpointHit.isWrite = isWrite;
        m_debugBreakpointHit.frame = m_frameCounter;
        m_debugBreakpointHit.cpuCycle = static_cast<uint32_t>(m_cpu.cycleCounter());
        m_debugBreakpointHit.emulationTick = m_emulationTickCounter;
        m_debugBreakpointHit.ppuScanline = m_ppu.scanline();
        m_debugBreakpointHit.ppuCycle = m_ppu.cycle();
        ++m_debugBreakpointHit.sequence;
        m_paused = true;
    }

    template<AccessType accessType>
    void processDebugBusAccess(uint16_t addr, uint8_t value)
    {
        const bool isWrite = accessType == AccessType::Write;

        if(addr >= 0x2000 && addr < 0x4000 && m_ppuEventTraceEnabled) {
            const uint16_t basePpuRegister = static_cast<uint16_t>(0x2000 | (addr & 0x0007));
            const bool shouldRecordWrite =
                isWrite &&
                (basePpuRegister == 0x2000 ||
                 basePpuRegister == 0x2001 ||
                 basePpuRegister == 0x2003 ||
                 basePpuRegister == 0x2004 ||
                 basePpuRegister == 0x2005 ||
                 basePpuRegister == 0x2006 ||
                 basePpuRegister == 0x2007);
            const bool shouldRecordRead =
                !isWrite &&
                (basePpuRegister == 0x2002 ||
                 basePpuRegister == 0x2004 ||
                 basePpuRegister == 0x2007);

            if((shouldRecordWrite || shouldRecordRead) &&
               m_ppuRegisterAccessEvents.size() < MAX_PPU_REGISTER_ACCESS_EVENTS) {
                m_ppuRegisterAccessEvents.push_back({
                    basePpuRegister,
                    value,
                    static_cast<uint16_t>(std::max(0, m_ppu.scanline())),
                    static_cast<uint16_t>(std::max(0, m_ppu.cycle())),
                    m_frameCounter,
                    isWrite
                });
            }
        }

        if(!debugBreakpointsActive()) {
            return;
        }

        if(!isWrite && m_debugBreakpointConfig.breakOnExactCpuRead && addr == m_debugBreakpointConfig.exactCpuReadAddress) {
            triggerDebugBreakpoint("CPU read watch", addr, value, false, true);
            return;
        }
        if(isWrite && m_debugBreakpointConfig.breakOnExactCpuWrite && addr == m_debugBreakpointConfig.exactCpuWriteAddress) {
            triggerDebugBreakpoint("CPU write watch", addr, value, true, true);
            return;
        }

        if(addr >= 0x2000 && addr < 0x4000) {
            const uint16_t basePpuRegister = static_cast<uint16_t>(0x2000 | (addr & 0x0007));
            if(isWrite && m_debugBreakpointConfig.breakOnPpuRegisterWrite) {
                switch(basePpuRegister) {
                    case 0x2000:
                    case 0x2001:
                    case 0x2003:
                    case 0x2004:
                    case 0x2005:
                    case 0x2006:
                    case 0x2007:
                        triggerDebugBreakpoint("PPU register write", basePpuRegister, value, true, true);
                        return;
                }
            }
            if(!isWrite && m_debugBreakpointConfig.breakOnPpuRegisterRead) {
                switch(basePpuRegister) {
                    case 0x2002:
                    case 0x2004:
                    case 0x2007:
                        triggerDebugBreakpoint("PPU register read", basePpuRegister, value, false, true);
                        return;
                }
            }
        }

        if(addr >= 0x4000 && addr < 0x4018) {
            if(isWrite && m_debugBreakpointConfig.breakOnApuRegisterWrite) {
                if((addr <= 0x4015 || addr == 0x4017) && addr != 0x4016) {
                    triggerDebugBreakpoint("APU register write", addr, value, true, true);
                    return;
                }
            }
            if(!isWrite && m_debugBreakpointConfig.breakOnApuRegisterRead && addr == 0x4015) {
                triggerDebugBreakpoint("APU register read", addr, value, false, true);
                return;
            }
            if(isWrite && m_debugBreakpointConfig.breakOnControllerWrite && addr == 0x4016) {
                triggerDebugBreakpoint("Controller register write", addr, value, true, true);
                return;
            }
            if(!isWrite && m_debugBreakpointConfig.breakOnControllerRead && (addr == 0x4016 || addr == 0x4017)) {
                triggerDebugBreakpoint("Controller register read", addr, value, false, true);
                return;
            }
        }

        if(isWrite && m_debugBreakpointConfig.breakOnMapperRegisterWrite && addr >= 0x4020) {
            triggerDebugBreakpoint("Mapper register write", addr, value, true, true);
            return;
        }

        if(!isWrite &&
           m_debugBreakpointConfig.breakOnMapperRegisterRead &&
           addr >= 0x4020 &&
           addr < 0x6000) {
            triggerDebugBreakpoint("Mapper register read", addr, value, false, true);
        }
    }

    GERANES_INLINE bool renderAudioMs(uint32_t ms, bool skipRender = false, bool forceSilence = false)
    {
        if(ms == 0) return false;
        if(skipRender) {
            // Silent catch-up/resimulation must not touch the live output device.
            // Drop only the transient generated sample buffers.
            m_audioOutput.clearAudioBuffers();
            return false;
        }

        const bool rewinding = m_rewind.isRewinding();
        if(m_audioOutputRewinding != rewinding) {
            m_audioOutput.setRewinding(rewinding);
            m_audioOutputRewinding = rewinding;
        }
        bool enableAudio = m_rewind.rewindLimit();
        const bool silentRender = forceSilence || !enableAudio || m_nsfPlayer.forceMute();
        m_audioOutput.render(ms, silentRender);
        return !silentRender;
    }

    template<bool consumeUpdateBudget>
    GERANES_INLINE bool stepEmulationTick(uint32_t audioRenderCycles,
                                          uint32_t& renderedAudioMs,
                                          bool& frameReady,
                                          bool silentAudio)
    {
        const uint32_t playbackFrame = m_frameCounter;
        if(m_currentInputFrame.frame != playbackFrame) {
            return false;
        }
        const bool playbackFrameAlreadyRenderedAudibly =
            m_lastAudiblyRenderedPlaybackFrame.has_value() &&
            playbackFrame <= *m_lastAudiblyRenderedPlaybackFrame;
        const bool tickSkipAudioRender =
            silentAudio || playbackFrameAlreadyRenderedAudibly;
        const bool nmiBefore = m_ppu.nmiLineActive();
        const bool irqBefore = m_apu.getInterruptFlag() || m_cartridge.getInterruptFlag();
        const bool sprite0Before = m_ppu.sprite0Hit();
        ++m_emulationTickCounter;

        if(--m_cpuCyclesAcc == 0) {
            m_cpuCyclesAcc = m_cpu.run();

            if constexpr(!consumeUpdateBudget) {
                m_audioRenderCyclesAcc += m_cpuCyclesAcc * 1000;
            }

            if(m_frameStarted) {
                onFrameStart();
                m_frameStarted = false;
            }

            if(m_newFrame) {
                const bool completedFrameRenderedAudibly = m_currentPlaybackFrameRenderedAudibly;
                onFrameReady();
                if(completedFrameRenderedAudibly) {
                    m_lastAudiblyRenderedPlaybackFrame = playbackFrame;
                }
                m_currentPlaybackFrameRenderedAudibly = false;
                m_rewind.newFrame();
                frameReady = true;
                m_newFrame = false;
            }
        }

        if constexpr(consumeUpdateBudget) {
            m_updateCyclesAcc -= 1000;
            m_audioRenderCyclesAcc += 1000;
        }

        while(m_audioRenderCyclesAcc >= audioRenderCycles) {
            m_audioRenderCyclesAcc -= audioRenderCycles;
            if(m_vsyncAudioSkipMsDebt > 0) {
                --m_vsyncAudioSkipMsDebt;
            }
            else {
                if(renderAudioMs(1, tickSkipAudioRender)) {
                    m_currentPlaybackFrameRenderedAudibly = true;
                }
                renderedAudioMs += 1;
            }
        }        

        if(m_resetRequested) {
            _reset();
            m_resetRequested = false;
            return false;
        }

        if(m_halt) {
            closeRom();
            return false;
        }

        const bool nmiAfter = m_ppu.nmiLineActive();
        if(!nmiBefore && nmiAfter && m_debugBreakpointConfig.breakOnNmiStart) {
            triggerDebugBreakpoint("PPU NMI start");
        } else if(nmiBefore && !nmiAfter && m_debugBreakpointConfig.breakOnNmiEnd) {
            triggerDebugBreakpoint("PPU NMI end");
        }

        const bool irqAfter = m_apu.getInterruptFlag() || m_cartridge.getInterruptFlag();
        if(!irqBefore && irqAfter && m_debugBreakpointConfig.breakOnIrqStart) {
            triggerDebugBreakpoint("IRQ start");
        } else if(irqBefore && !irqAfter && m_debugBreakpointConfig.breakOnIrqEnd) {
            triggerDebugBreakpoint("IRQ end");
        }

        if(!sprite0Before && m_ppu.sprite0Hit() && m_debugBreakpointConfig.breakOnSpriteZeroHit) {
            triggerDebugBreakpoint("PPU sprite zero hit");
        }

        return true;
    }

    GERANES_INLINE void compensateVsyncAudioDrift(uint32_t dt)
    {
        if(dt == 0) return;

        // Large hitches are treated as timeline discontinuities. Carrying their
        // audio backlog forward leaves playback permanently behind the video
        // until the device is manually restarted.
        if(dt > 34) {
            m_audioOutput.discardQueuedAudio();
            m_audioOutput.clearAudioBuffers();
            m_apu.refreshAudioOutputState();
            m_lastAudioRenderedMs = 0;
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

    static uint8_t clampSaveStateSlot(uint8_t slot)
    {
        return static_cast<uint8_t>(std::min<uint8_t>(slot, 9u));
    }

    const std::string saveStateFileName(uint8_t slot) {
        const std::string romStem = std::filesystem::path(m_cartridge.romFile().fileName()).stem().string();
        return std::string(STATES_FOLDER) +
               romStem +
               "." +
               std::to_string(static_cast<unsigned>(clampSaveStateSlot(slot))) +
               ".state";
    }

    void preloadNsfMemory()
    {
#ifdef ENABLE_NSF_PLAYER
        if(!m_cartridge.isNsf()) {
            return;
        }

        memset(m_ram, 0, sizeof(m_ram));
        m_cartridge.preloadNsfMemory(m_ram, sizeof(m_ram));
#endif
    }

public:

    uint8_t getOpenBus() const override
    {
        return m_openBus;
    }

    void setOpenBus(uint8_t data) override
    {
        m_openBus = data;
    }

    void onCpuBusAccessEnd(int addr, bool write) override
    {
        m_ppu.onCpuBusAccessEnd(static_cast<uint16_t>(addr), write);
    }

    void onCpuGetToPutTransition() override
    {
        if(m_fourScore) m_fourScore->onCpuGetToPutTransition();
        else if(m_HoriAdapter) m_HoriAdapter->onCpuGetToPutTransition();
        else {
            if(m_portDevice1) m_portDevice1->onCpuGetToPutTransition();
            if(m_portDevice2) m_portDevice2->onCpuGetToPutTransition();
        }
    }

    SigSlot::Signal<const std::string&> signalError;
    SigSlot::Signal<const InputFrame&> signalInputFrameSelected;
    SigSlot::Signal<uint32_t> signalResetExecuted;
    SigSlot::Signal<uint32_t> signalLoadExecuted;

    void onPPUFrameStart()
    {
        if(m_ppuViewerScanlineTraceEnabled) {
            if(m_ppuViewerScanlineStates.size() != 240u) {
                m_ppuViewerScanlineStates.resize(240u);
            }
            for(auto& lineState : m_ppuViewerScanlineStates) {
                lineState.valid = false;
                lineState.snapshotIndex = 0xFFFF;
            }
            m_ppuViewerScanlineSnapshots.clear();
        }
        m_frameStarted = true;        
    }

    void onPPUFrameReady()
    {
        m_newFrame = true;        
    }

    GeraNESEmu(IAudioOutput& audioOutput = DummyAudioOutput::instance()) :
    m_settings(),
    m_audioOutput(audioOutput),
    m_cartridge(),
    m_cpu(*this, m_console),
    m_ppu(m_settings, m_cartridge),
    m_apu(m_audioOutput,m_settings),
    m_portDevice1(),
    m_portDevice2(),
    m_expansionDevice(),
    m_console(m_cpu, m_ppu, m_apu, m_cartridge),
    m_rewind(*this),
    m_nsfPlayer(m_cartridge, m_apu, m_audioOutput, [this]() { this->reset(); })
    {
        init();

        m_cpu.signalError.bind(&GeraNESEmu::onError, this);
        

        m_ppu.signalFrameStart.bind(&GeraNESEmu::onPPUFrameStart, this);
        m_ppu.signalFrameReady.bind(&GeraNESEmu::onPPUFrameReady, this);
        m_ppu.signalScanlineStart.bind(&GeraNESEmu::onPPUScanlineStart, this);
        m_apu.getSampleChannel().dmcRequest.bind(&GeraNESEmu::onDMCRequest, this);
        m_apu.getSampleChannel().dmcCancelRequest.bind(&GeraNESEmu::onDMCCancelRequest, this);
        m_apu.getSampleChannel().dmcImplicitAbortRequest.bind(&GeraNESEmu::onDMCImplicitAbortRequest, this);

        recreatePortDevice(Settings::Port::P_1);
        recreatePortDevice(Settings::Port::P_2);
        recreateExpansionDevice();
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
        m_emulationTickCounter = 0;
        m_4011WriteCounter = 0;
        m_newFrame = false;
        m_frameStarted = false;
        m_lastAudiblyRenderedPlaybackFrame.reset();
        m_currentPlaybackFrameRenderedAudibly = false;

        m_frameCounter = 0;
        m_currentInputFrame = makeDefaultInputFrame(0);

        m_saveStateFlag = false;
        m_loadStateFlag = false;
        m_resetRequested = false;
        m_runningLoop = false;
        m_paused = false;
        clearDebugBreakpointHit();
        m_nsfPlayer.init();
        m_prevNsfSelect = false;
        m_prevNsfStart = false;
        m_prevNsfLeft = false;
        m_prevNsfRight = false;
        m_pendingNsfTogglePlayPause = false;
        m_pendingNsfStop = false;
        m_pendingNsfNextSong = false;
        m_pendingNsfPrevSong = false;
        m_applyingPendingNsfActions = false;

        m_openBus = 0;
        m_prevControllerReadAddr = 0xFFFF;

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
        const bool wasRewinding = m_rewind.isRewinding();
        m_rewind.setRewind(state);
        const bool isRewinding = m_rewind.isRewinding();

        if(wasRewinding != isRewinding) {
            m_audioOutput.setRewinding(isRewinding);
            m_audioOutputRewinding = isRewinding;
            m_audioOutput.discardQueuedAudio();
            m_audioOutput.clearAudioBuffers();
            m_apu.refreshAudioOutputState();
            m_lastAudioRenderedMs = 0;
            m_vsyncAudioCompMsAcc = 0.0;
            m_vsyncAudioSkipMsDebt = 0;
        }
    }

    void resetRewindSystem()
    {
        m_rewind.reset();
    }

    void closeRom()
    {
        m_cartridge.closeRom();
        m_ppu.clearFramebuffer();
        m_ppuRegisterAccessEvents.clear();
        m_rewind.destroy();
    }

    bool openRom(const std::string& filename, bool autoConfigureInputTopologyOnRomLoad = true)
    {
        m_audioOutput.clearAudioBuffers();
        m_ppu.clearFramebuffer();

        bool result = m_cartridge.openRom(filename);

        if(result) { //no errors

            init();            

            switch(m_cartridge.system()) {

                case GameDatabase::System::NesPal:
                    m_settings.setRegion(Settings::Region::PAL);
                    break;

                case GameDatabase::System::NesNtsc:
                case GameDatabase::System::FDS:
                case GameDatabase::System::Famicom:
                    m_settings.setRegion(Settings::Region::NTSC);
                    break;

                case GameDatabase::System::Dendy:
                    m_settings.setRegion(Settings::Region::DENDY);
                    break;

                default:
                    m_settings.setRegion(Settings::Region::NTSC);

            }  
            m_audioOutput.setExpansionSourceRateHz(m_settings.CPUClockHz());

            setNesMultitapDevice(Settings::NesMultitapDevice::NONE);
            setFamicomMultitapDevice(Settings::FamicomMultitapDevice::NONE);

            if(autoConfigureInputTopologyOnRomLoad) {
                switch(m_cartridge.inputType()) {

                    case GameDatabase::InputType::FourScore:
                        setNesMultitapDevice(Settings::NesMultitapDevice::FOUR_SCORE);
                        break;

                    case GameDatabase::InputType::FourPlayerAdapter:
                        setFamicomMultitapDevice(Settings::FamicomMultitapDevice::HORI_ADAPTER);
                        break;

                    case GameDatabase::InputType::VsZapper:
                    case GameDatabase::InputType::Zapper:
                        setPortDevice(Settings::Port::P_1, standardPortDeviceForCurrentSystem());
                        setPortDevice(Settings::Port::P_2, Settings::Device::ZAPPER);
                        setExpansionDevice(Settings::ExpansionDevice::NONE);
                        break;

                    case GameDatabase::InputType::TwoZappers:
                        setPortDevice(Settings::Port::P_1, Settings::Device::ZAPPER);
                        setPortDevice(Settings::Port::P_2, Settings::Device::ZAPPER);
                        setExpansionDevice(Settings::ExpansionDevice::NONE);
                        break;

                    case GameDatabase::InputType::BandaiHypershot:
                        setPortDevice(Settings::Port::P_1, standardPortDeviceForCurrentSystem());
                        setPortDevice(Settings::Port::P_2, standardPortDeviceForCurrentSystem());
                        setExpansionDevice(Settings::ExpansionDevice::BANDAI_HYPERSHOT);
                        break;

                    case GameDatabase::InputType::KonamiHyperShot:
                        setPortDevice(Settings::Port::P_1, standardPortDeviceForCurrentSystem());
                        setPortDevice(Settings::Port::P_2, standardPortDeviceForCurrentSystem());
                        setExpansionDevice(Settings::ExpansionDevice::KONAMI_HYPERSHOT);
                        break;

                    case GameDatabase::InputType::StandardControllers:
                        setPortDevice(Settings::Port::P_1, standardPortDeviceForCurrentSystem());
                        setPortDevice(Settings::Port::P_2, standardPortDeviceForCurrentSystem());
                        setExpansionDevice(Settings::ExpansionDevice::NONE);
                        break;

                    case GameDatabase::InputType::PowerPadSideA:
                        setPortDevice(Settings::Port::P_1, standardPortDeviceForCurrentSystem());
                        setPortDevice(Settings::Port::P_2, Settings::Device::POWER_PAD_SIDE_A);
                        setExpansionDevice(Settings::ExpansionDevice::NONE);
                        break;

                    case GameDatabase::InputType::PowerPadSideB:
                        setPortDevice(Settings::Port::P_1, standardPortDeviceForCurrentSystem());
                        setPortDevice(Settings::Port::P_2, Settings::Device::POWER_PAD_SIDE_B);
                        setExpansionDevice(Settings::ExpansionDevice::NONE);
                        break;

                    case GameDatabase::InputType::FamilyTrainerSideA:
                        setPortDevice(Settings::Port::P_1, standardPortDeviceForCurrentSystem());
                        setPortDevice(Settings::Port::P_2, standardPortDeviceForCurrentSystem());
                        setExpansionDevice(Settings::ExpansionDevice::FAMILY_TRAINER_SIDE_A);
                        break;

                    case GameDatabase::InputType::FamilyTrainerSideB:
                        setPortDevice(Settings::Port::P_1, standardPortDeviceForCurrentSystem());
                        setPortDevice(Settings::Port::P_2, standardPortDeviceForCurrentSystem());
                        setExpansionDevice(Settings::ExpansionDevice::FAMILY_TRAINER_SIDE_B);
                        break;

                    case GameDatabase::InputType::ArkanoidControllerNes:
                        setPortDevice(Settings::Port::P_1, standardPortDeviceForCurrentSystem());
                        setPortDevice(Settings::Port::P_2, Settings::Device::ARKANOID_CONTROLLER);
                        setExpansionDevice(Settings::ExpansionDevice::NONE);
                        break;

                    case GameDatabase::InputType::ArkanoidControllerFamicom:
                        setPortDevice(Settings::Port::P_1, standardPortDeviceForCurrentSystem());
                        setPortDevice(Settings::Port::P_2, standardPortDeviceForCurrentSystem());
                        setExpansionDevice(Settings::ExpansionDevice::ARKANOID_CONTROLLER);
                        break;

                    case GameDatabase::InputType::SnesMouse:
                        setPortDevice(Settings::Port::P_1, Settings::Device::SNES_MOUSE);
                        setPortDevice(Settings::Port::P_2, standardPortDeviceForCurrentSystem());
                        setExpansionDevice(Settings::ExpansionDevice::NONE);
                        break;

                    case GameDatabase::InputType::SuborKeyboard:
                        setPortDevice(Settings::Port::P_1, standardPortDeviceForCurrentSystem());
                        setPortDevice(Settings::Port::P_2, standardPortDeviceForCurrentSystem());
                        setExpansionDevice(Settings::ExpansionDevice::SUBOR_KEYBOARD);
                        break;

                    case GameDatabase::InputType::FamilyBasicKeyboard:
                        setPortDevice(Settings::Port::P_1, standardPortDeviceForCurrentSystem());
                        setPortDevice(Settings::Port::P_2, standardPortDeviceForCurrentSystem());
                        setExpansionDevice(Settings::ExpansionDevice::FAMILY_BASIC_KEYBOARD);
                        break;

                    case GameDatabase::InputType::SuborKeyboardMouse1:
                    case GameDatabase::InputType::SuborKeyboardMouse2:
                        setPortDevice(Settings::Port::P_1, standardPortDeviceForCurrentSystem());
                        setPortDevice(Settings::Port::P_2, Settings::Device::SUBOR_MOUSE);
                        setExpansionDevice(Settings::ExpansionDevice::SUBOR_KEYBOARD);
                        break;

                    case GameDatabase::InputType::SnesControllers:
                        setPortDevice(Settings::Port::P_1, Settings::Device::SNES_CONTROLLER);
                        setPortDevice(Settings::Port::P_2, Settings::Device::SNES_CONTROLLER);
                        setExpansionDevice(Settings::ExpansionDevice::NONE);
                        break;

                    case GameDatabase::InputType::DoubleArkanoidController:
                        setPortDevice(Settings::Port::P_1, standardPortDeviceForCurrentSystem());
                        setPortDevice(Settings::Port::P_2, Settings::Device::ARKANOID_CONTROLLER);
                        setExpansionDevice(Settings::ExpansionDevice::ARKANOID_CONTROLLER);
                        break;

                    default:
                        setPortDevice(Settings::Port::P_1, standardPortDeviceForCurrentSystem());
                        setPortDevice(Settings::Port::P_2, standardPortDeviceForCurrentSystem());
                        setExpansionDevice(Settings::ExpansionDevice::NONE);
                }
            } 

            updateCyclesPerSecond();

            m_currentInputFrame = makeDefaultInputFrame(0);

            m_ppu.setVsPpuModel(m_cartridge.vsPpuModel());
            m_cartridge.reset();
            ++m_ppuViewerMapperWriteGeneration;
            preloadNsfMemory();
            m_ppu.init();
            m_cpu.init();
            m_apu.init();
            m_nsfPlayer.onOpen();

            resetRewindSystem();
        }

        return result;
    }

    GERANES_HOT uint8_t read(int addr) override
    {
        return accessBus<AccessType::Read>(addr);
    }

    GERANES_HOT void write(int addr, uint8_t data) override
    {
        accessBus<AccessType::Write>(addr,data);
    }    

    uint8_t debugPeekCpuMemory(uint16_t addr) const
    {
        switch(addr >> 12) {
            case 0:
            case 1:
                return m_ram[addr & 0x7FF];

            case 6:
            case 7:
                return const_cast<Cartridge&>(m_cartridge).readSaveRam(addr & 0x1FFF);

            default:
                break;
        }

        if(addr >= 0x8000) {
            return const_cast<Cartridge&>(m_cartridge).readPrg(addr & 0x7FFF);
        }

        if(addr >= 0x4020) {
            if(m_cartridge.isNsf()) {
                return const_cast<Cartridge&>(m_cartridge).readMapperRegisterAbsolute(addr, m_openBus);
            }
            return const_cast<Cartridge&>(m_cartridge).readMapperRegister(addr & 0x1FFF, m_openBus);
        }

        return m_openBus;
    }

    void debugWriteCpuMemory(uint16_t addr, uint8_t data)
    {
        switch(addr >> 12) {
            case 0:
            case 1:
                m_ram[addr & 0x7FF] = data;
                return;

            case 6:
            case 7:
                m_cartridge.writeSaveRam(addr & 0x1FFF, data);
                return;

            default:
                break;
        }

        if(addr >= 0x8000) {
            m_cartridge.writePrg(addr & 0x7FFF, data);
        } else if(addr >= 0x4020) {
            if(m_cartridge.isNsf()) {
                m_cartridge.writeMapperRegisterAbsolute(addr, data);
            } else {
                m_cartridge.writeMapperRegister(addr & 0x1FFF, data);
            }
        }
    }    

    GERANES_INLINE bool updateUntilFrame(uint32_t dt, bool renderAudio = true) {
        applyPendingNsfControllerActions();
        if(m_paused) return true;

        const bool ret = _update<true>(dt, renderAudio);
        if(renderAudio && ret) {
            compensateVsyncAudioDrift(dt);
        }
        return ret;
    }

    bool debugStepInstruction()
    {
        applyPendingNsfControllerActions();
        if(!m_cartridge.isValid() || m_halt) return false;

        const bool wasPaused = m_paused;
        m_paused = false;

        const uint32_t audioRenderCycles = m_cyclesPerSecond;
        uint32_t renderedAudioMs = 0;
        bool frameReady = false;
        bool steppedInstruction = false;
        bool advancedAny = false;

        m_runningLoop = true;
        while(!steppedInstruction) {
            const int cyclesBefore = m_cpuCyclesAcc;
            if(!stepEmulationTick<false>(audioRenderCycles, renderedAudioMs, frameReady, true)) {
                break;
            }

            if(m_paused) {
                break;
            }

            advancedAny = true;
            if(cyclesBefore == 1 && m_cpuCyclesAcc > 1) {
                steppedInstruction = true;
            }
        }
        m_lastAudioRenderedMs = renderedAudioMs;
        m_runningLoop = false;

        if(m_saveStateFlag) {
            _saveState(m_pendingSaveStateSlot);
            m_saveStateFlag = false;
        }

        if(m_loadStateFlag) {
            _loadState(m_pendingLoadStateSlot);
            m_loadStateFlag = false;
        }

        m_paused = wasPaused;
        return advancedAny && steppedInstruction;
    }

    uint32_t pendingCpuCycles() const
    {
        return static_cast<uint32_t>(std::max(m_cpuCyclesAcc, 0));
    }

    uint64_t emulationTickCount() const
    {
        return m_emulationTickCounter;
    }

    GERANES_INLINE const uint32_t* getFramebuffer() const
    {
        return m_ppu.getFramebuffer();
    }

    void _saveState(uint8_t slot = 0)
    {
        if(!m_cartridge.isValid()) return;

        Serialize s;
        serialization(s);
        const uint8_t clampedSlot = clampSaveStateSlot(slot);
        if(s.saveToFile(saveStateFileName(clampedSlot))) {
            Logger::instance().log("State saved to slot " + std::to_string(static_cast<unsigned>(clampedSlot)), Logger::Type::USER);
        }
        else {
            Logger::instance().log("Failed to save state", Logger::Type::ERROR);
        }
    }    

    void saveState(uint8_t slot = 0) {
        const uint8_t clampedSlot = clampSaveStateSlot(slot);
        if(!m_runningLoop) _saveState(clampedSlot);
        else {
            m_pendingSaveStateSlot = clampedSlot;
            m_saveStateFlag = true;
        }
    }

    void _loadState(uint8_t slot = 0)
    {
        if(!m_cartridge.isValid()) return;

        Deserialize d;
        const uint8_t clampedSlot = clampSaveStateSlot(slot);
        const std::string fileName = saveStateFileName(clampedSlot);

        if(!fs::exists(fileName)) {
            Logger::instance().log(
                "State slot " + std::to_string(static_cast<unsigned>(clampedSlot)) + " not found",
                Logger::Type::USER
            );
            return;
        }

        if(d.loadFromFile(fileName)) {
            serialization(d);
            resyncAudioAfterStateLoad();
            resetVolatileStateAfterStateLoad();
            ++m_manualLoadStateGeneration;
            signalLoadExecuted(m_frameCounter);
            Logger::instance().log("State loaded from slot " + std::to_string(static_cast<unsigned>(clampedSlot)), Logger::Type::USER);
        }
        else {
            Logger::instance().log("Failed to load state", Logger::Type::ERROR);
        }

        resetRewindSystem();
    }

    void loadState(uint8_t slot = 0) {
        const uint8_t clampedSlot = clampSaveStateSlot(slot);
        if(!m_runningLoop) _loadState(clampedSlot);
        else {
            m_pendingLoadStateSlot = clampedSlot;
            m_loadStateFlag = true;
        }
    }    

    void loadStateFromMemoryWithAudioPolicy(
        const std::vector<uint8_t>& data,
        StateLoadAudioPolicy audioPolicy)
    {
        Deserialize d;
        d.setData(data);
        serialization(d);
        resyncAudioAfterStateLoad(audioPolicy);
        resetVolatileStateAfterStateLoad();
    }

    void loadStateFromMemory(const std::vector<uint8_t>& data) override
    {
        loadStateFromMemoryWithAudioPolicy(data, StateLoadAudioPolicy::ResetOutput);
    }

    void loadStateFromMemoryWithAudioPolicy(
        const uint8_t* data,
        size_t size,
        StateLoadAudioPolicy audioPolicy)
    {
        Deserialize d;
        d.setData(data, size);
        serialization(d);
        resyncAudioAfterStateLoad(audioPolicy);
        resetVolatileStateAfterStateLoad();
    }

    bool loadStateFromMemoryOnCleanBoot(
        const std::vector<uint8_t>& data,
        StateLoadAudioPolicy audioPolicy = StateLoadAudioPolicy::ResetOutput)
    {
        if(data.empty() || !m_cartridge.isValid()) return false;

        const std::string romPath = m_cartridge.romFile().fullPath();
        if(romPath.empty()) return false;
        if(!openRom(romPath) || !valid()) return false;

        loadStateFromMemoryWithAudioPolicy(data, audioPolicy);
        return valid();
    }

    std::vector<uint8_t> saveStateToMemory()
    {
        const InputFrame savedCurrentInputFrame = m_currentInputFrame;
        const bool savedNewFrame = m_newFrame;
        const bool savedFrameStarted = m_frameStarted;
        const bool savedRunningLoop = m_runningLoop;
        const HardwareActions savedHardwareActions = m_hardwareActions;
        const uint32_t savedUpdateCyclesAcc = m_updateCyclesAcc;
        const uint32_t savedAudioRenderCyclesAcc = m_audioRenderCyclesAcc;
        InputFrame serializedPlaybackInput = m_currentInputFrame;
        if(serializedPlaybackInput.frame != m_frameCounter) {
            serializedPlaybackInput = InputFrame::repeatedFrom(serializedPlaybackInput, m_frameCounter);
        }
        m_currentInputFrame = serializedPlaybackInput;
        m_newFrame = false;
        m_frameStarted = false;
        m_runningLoop = false;
        m_hardwareActions.reset();
        m_updateCyclesAcc = 0;
        m_audioRenderCyclesAcc = 0;

        Serialize s;
        static thread_local size_t reserveHint = 0;
        if(reserveHint > 0) {
            s.reserve(reserveHint);
        }
        serialization(s);
        std::vector<uint8_t> data = s.takeData();
        reserveHint = data.size();

        m_currentInputFrame = savedCurrentInputFrame;
        m_newFrame = savedNewFrame;
        m_frameStarted = savedFrameStarted;
        m_runningLoop = savedRunningLoop;
        m_hardwareActions = savedHardwareActions;
        m_updateCyclesAcc = savedUpdateCyclesAcc;
        m_audioRenderCyclesAcc = savedAudioRenderCyclesAcc;
        return data;
    }

    uint32_t canonicalStateCrc32() const
    {
        const std::vector<uint8_t> data = const_cast<GeraNESEmu*>(this)->saveStateToMemory();
        if(data.empty()) return 0;
        return Crc32::calc(reinterpret_cast<const char*>(data.data()), data.size());
    }

    uint32_t canonicalNetplayStateCrc32() const
    {
        const std::vector<uint8_t> data = const_cast<GeraNESEmu*>(this)->saveStateToMemory();
        if(data.empty()) return 0;
        return Crc32::calc(reinterpret_cast<const char*>(data.data()), data.size());
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

    GERANES_INLINE std::optional<Settings::Device> getPortDevice(Settings::Port port) const
    {
        return m_settings.getPortDevice(port);
    }

    void configurePortDevice(Settings::Port port, Settings::Device device)
    {
        setPortDevice(port, device);
    }

    GERANES_INLINE Settings::ExpansionDevice getExpansionDevice() const
    {
        return m_settings.getExpansionDevice();
    }

    GERANES_INLINE Settings::NesMultitapDevice getNesMultitapDevice() const
    {
        return m_settings.getNesMultitapDevice();
    }

    GERANES_INLINE Settings::FamicomMultitapDevice getFamicomMultitapDevice() const
    {
        return m_settings.getFamicomMultitapDevice();
    }

    bool overclocked() const
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

    void setDebugBreakpointsArmed(bool armed)
    {
        if(m_debugBreakpointsArmed == armed) {
            return;
        }
        m_debugBreakpointsArmed = armed;
        if(!armed) {
            clearDebugBreakpointHit();
        }
    }

    void setDebugBreakpointConfig(const DebugBreakpointConfig& config)
    {
        m_debugBreakpointConfig = config;
        if(!m_debugBreakpointConfig.enabled) {
            clearDebugBreakpointHit();
        }
    }

    const DebugBreakpointConfig& debugBreakpointConfig() const
    {
        return m_debugBreakpointConfig;
    }

    const DebugBreakpointHit& debugBreakpointHit() const
    {
        return m_debugBreakpointHit;
    }

    void enablePpuEventTrace(bool enabled)
    {
        m_ppuEventTraceEnabled = enabled;
        if(!enabled) {
            m_ppuRegisterAccessEvents.clear();
        }
    }

    bool ppuEventTraceEnabled() const
    {
        return m_ppuEventTraceEnabled;
    }

    const std::vector<PpuRegisterAccessEvent>& ppuRegisterAccessEvents() const
    {
        return m_ppuRegisterAccessEvents;
    }

    void enablePpuViewerScanlineTrace(bool enabled)
    {
        m_ppuViewerScanlineTraceEnabled = enabled;
        if(enabled) {
            if(m_ppuViewerScanlineStates.size() != 240u) {
                m_ppuViewerScanlineStates.resize(240u);
            }
        } else {
            m_ppuViewerScanlineStates.clear();
            m_ppuViewerScanlineSnapshots.clear();
        }
    }

    bool ppuViewerScanlineTraceEnabled() const
    {
        return m_ppuViewerScanlineTraceEnabled;
    }

    const std::vector<PpuViewerScanlineState>& ppuViewerScanlineStates() const
    {
        return m_ppuViewerScanlineStates;
    }

    const std::vector<PpuViewerScanlineSnapshot>& ppuViewerScanlineSnapshots() const
    {
        return m_ppuViewerScanlineSnapshots;
    }

    void clearDebugBreakpointHit()
    {
        m_debugBreakpointHit.valid = false;
        m_debugBreakpointHit.reason.clear();
        m_debugBreakpointHit.hasAddress = false;
        m_debugBreakpointHit.address = 0x0000;
        m_debugBreakpointHit.value = 0x00;
        m_debugBreakpointHit.isWrite = false;
        m_debugBreakpointHit.frame = 0;
        m_debugBreakpointHit.cpuCycle = 0;
        m_debugBreakpointHit.emulationTick = 0;
        m_debugBreakpointHit.ppuScanline = 0;
        m_debugBreakpointHit.ppuCycle = 0;
    }

    void setExternalCpuIoHandlers(
        std::function<bool(uint16_t, uint8_t)> writeHandler,
        std::function<std::optional<uint8_t>(uint16_t)> readHandler)
    {
        m_externalCpuWriteHandler = std::move(writeHandler);
        m_externalCpuReadHandler = std::move(readHandler);
    }

    void clearExternalCpuIoHandlers()
    {
        m_externalCpuWriteHandler = {};
        m_externalCpuReadHandler = {};
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

    bool spriteLimitDisabled() const
    {
        return m_settings.spriteLimitDisabled();
    }

    void setRegion(Settings::Region value)
    {
        if(value != m_settings.region()) {
            m_settings.setRegion(value);
            m_audioOutput.init();
            m_apu.refreshAudioOutputState();
            m_rewind.reset();
            updateCyclesPerSecond();
        }
    }

    Settings::Region region() const
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
        m_settings.serialization(s);
        if(dynamic_cast<Deserialize*>(&s) != nullptr) {
            recreateInputRouting();
        }
        if(m_portDevice1) m_portDevice1->serialization(s);
        if(m_portDevice2) m_portDevice2->serialization(s);
        if(m_expansionDevice) m_expansionDevice->serialization(s);
        if(m_fourScore) m_fourScore->serialization(s);
        if(m_HoriAdapter) m_HoriAdapter->serialization(s);

        SERIALIZEDATA(s, m_updateCyclesAcc);
        SERIALIZEDATA(s, m_cpuCyclesAcc);
        SERIALIZEDATA(s, m_cyclesPerSecond);
  
        SERIALIZEDATA(s, m_audioRenderCyclesAcc);
        SERIALIZEDATA(s, m_emulationTickCounter);

        SERIALIZEDATA(s, m_openBus);

        SERIALIZEDATA(s, m_halt);

        SERIALIZEDATA(s, m_4011WriteCounter);
        SERIALIZEDATA(s, m_newFrame);
        SERIALIZEDATA(s, m_frameStarted);
        SERIALIZEDATA(s, m_frameCounter);
        m_hardwareActions.serialization(s);

        SERIALIZEDATA(s, m_runningLoop);
    }

    InputFrame createInputFrame(uint32_t frame)
    {
        return makeDefaultInputFrame(frame);
    }

    bool setPlaybackInputFrame(const InputFrame& inputFrame)
    {
        if(inputFrame.frame != m_frameCounter) {
            return false;
        }
        m_currentInputFrame = inputFrame;
        m_currentInputFrame.frame = m_frameCounter;
        return true;
    }

    bool hasPlaybackInputFrame(uint32_t frame) const
    {
        return m_currentInputFrame.frame == frame;
    }

    void setForceSilentAudio(bool silent)
    {
        m_forceSilentAudio = silent;
    }

    bool forceSilentAudio() const
    {
        return m_forceSilentAudio;
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

    bool isNsfLoaded() const
    {
        return m_nsfPlayer.isLoaded();
    }

    int nsfTotalSongs() const
    {
        return m_nsfPlayer.totalSongs();
    }

    int nsfCurrentSong() const
    {
        return m_nsfPlayer.currentSong();
    }

    bool nsfIsPlaying() const
    {
        return m_nsfPlayer.isPlaying();
    }

    bool nsfIsPaused() const
    {
        return m_nsfPlayer.isPaused();
    }

    bool nsfHasEnded() const
    {
        return m_nsfPlayer.hasEnded();
    }

    void nsfPlay()
    {
        m_nsfPlayer.play();
    }

    void nsfStop()
    {
        m_nsfPlayer.stop();
    }

    void nsfPause()
    {
        m_nsfPlayer.pause();
    }

    void nsfNextSong()
    {
        m_nsfPlayer.nextSong();
    }

    void nsfPrevSong()
    {
        m_nsfPlayer.prevSong();
    }

    void nsfSetSong(int song1Based)
    {
        m_nsfPlayer.setSong(song1Based);
    }

    Console& getConsole() {
        return m_console;
    }

    const Console& getConsole() const {
        return m_console;
    }

    bool valid() const //ready to run
    {
        return m_cartridge.isValid();
    }

    bool isRewinding() const
    {
        return m_rewind.isRewinding();
    }

    uint32_t getRegionFPS() const {

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

    uint32_t frameCount() const {
        return m_frameCounter;
    }

    void reset() {
        if(!m_runningLoop) _reset();
        else m_resetRequested = true;        
    }

    void _reset() {

        if(!m_cartridge.isValid()) return;

        m_cartridge.reset();
        ++m_ppuViewerMapperWriteGeneration;
        preloadNsfMemory();
        m_apu.reset();
        m_ppu.init();

        //TODO: verify comments are ok
        //m_halt = false;
        //m_updateCyclesAcc = 0;
        //m_cpuCyclesAcc = 1;
        //m_audioRenderCyclesAcc = 0;
        //m_lastAudioRenderedMs = 0;
        //m_vsyncAudioCompMsAcc = 0.0;
        //m_vsyncAudioSkipMsDebt = 0;
        m_openBus = 0;
        m_prevControllerReadAddr = 0xFFFF;
        m_4011WriteCounter = 0;
        //m_newFrame = false;
        //m_frameStarted = false;
        //m_runningLoop = false;
        //m_saveStateFlag = false;
        //m_loadStateFlag = false;
        // m_frameCounter = 0;

        m_nsfPlayer.onEmulatorReset();
        m_hardwareActions.reset();

        m_rewind.reset();
        updateCyclesPerSecond();
        m_cpu.reset();
        ++m_manualResetGeneration;
        signalResetExecuted(m_frameCounter);
        Logger::instance().log("Emulator reset", Logger::Type::USER);
    }

    uint32_t manualResetGeneration() const
    {
        return m_manualResetGeneration;
    }

    uint32_t manualLoadStateGeneration() const
    {
        return m_manualLoadStateGeneration;
    }

};

















