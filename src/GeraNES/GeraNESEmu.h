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

#include "Serialization.h"
#include "util/Crc32.h"

#include "signal/signal.h"
#include "logger/logger.h"

#include "Rewind.h"
#include <memory>

enum class AccessType
{
    Read,
    Write
};

class GeraNESEmu : public Ibus, public SigSlot::SigSlotBase, public IRewindable
{
public:
    struct PendingNetplaySnapshot
    {
        uint32_t frame = 0;
        std::vector<uint8_t> data;
    };

private:
    struct PendingInputState
    {
        bool p1A = false;
        bool p1B = false;
        bool p1Select = false;
        bool p1Start = false;
        bool p1Up = false;
        bool p1Down = false;
        bool p1Left = false;
        bool p1Right = false;
        bool p1X = false;
        bool p1Y = false;
        bool p1L = false;
        bool p1R = false;

        bool p2A = false;
        bool p2B = false;
        bool p2Select = false;
        bool p2Start = false;
        bool p2Up = false;
        bool p2Down = false;
        bool p2Left = false;
        bool p2Right = false;
        bool p2X = false;
        bool p2Y = false;
        bool p2L = false;
        bool p2R = false;

        bool p3A = false;
        bool p3B = false;
        bool p3Select = false;
        bool p3Start = false;
        bool p3Up = false;
        bool p3Down = false;
        bool p3Left = false;
        bool p3Right = false;

        bool p4A = false;
        bool p4B = false;
        bool p4Select = false;
        bool p4Start = false;
        bool p4Up = false;
        bool p4Down = false;
        bool p4Left = false;
        bool p4Right = false;

        int zapperP1X = -1;
        int zapperP1Y = -1;
        bool zapperP1Trigger = false;
        int zapperP2X = -1;
        int zapperP2Y = -1;
        bool zapperP2Trigger = false;

        bool bandaiA = false;
        bool bandaiB = false;
        bool bandaiSelect = false;
        bool bandaiStart = false;
        bool bandaiUp = false;
        bool bandaiDown = false;
        bool bandaiLeft = false;
        bool bandaiRight = false;
        int bandaiX = -1;
        int bandaiY = -1;
        bool bandaiTrigger = false;

        float arkanoidP1Position = 0.5f;
        bool arkanoidP1Button = false;
        float arkanoidP2Position = 0.5f;
        bool arkanoidP2Button = false;
        float arkanoidFamicomPosition = 0.5f;
        bool arkanoidFamicomButton = false;

        bool konamiP1Run = false;
        bool konamiP1Jump = false;
        bool konamiP2Run = false;
        bool konamiP2Jump = false;

        int snesMouseP1DeltaX = 0;
        int snesMouseP1DeltaY = 0;
        bool snesMouseP1Left = false;
        bool snesMouseP1Right = false;
        int snesMouseP2DeltaX = 0;
        int snesMouseP2DeltaY = 0;
        bool snesMouseP2Left = false;
        bool snesMouseP2Right = false;

        bool vbP1A = false;
        bool vbP1B = false;
        bool vbP1Select = false;
        bool vbP1Start = false;
        bool vbP1Up0 = false;
        bool vbP1Down0 = false;
        bool vbP1Left0 = false;
        bool vbP1Right0 = false;
        bool vbP1Up1 = false;
        bool vbP1Down1 = false;
        bool vbP1Left1 = false;
        bool vbP1Right1 = false;
        bool vbP1L = false;
        bool vbP1R = false;

        bool vbP2A = false;
        bool vbP2B = false;
        bool vbP2Select = false;
        bool vbP2Start = false;
        bool vbP2Up0 = false;
        bool vbP2Down0 = false;
        bool vbP2Left0 = false;
        bool vbP2Right0 = false;
        bool vbP2Up1 = false;
        bool vbP2Down1 = false;
        bool vbP2Left1 = false;
        bool vbP2Right1 = false;
        bool vbP2L = false;
        bool vbP2R = false;

        int suborMouseP1DeltaX = 0;
        int suborMouseP1DeltaY = 0;
        bool suborMouseP1Left = false;
        bool suborMouseP1Right = false;
        int suborMouseP2DeltaX = 0;
        int suborMouseP2DeltaY = 0;
        bool suborMouseP2Left = false;
        bool suborMouseP2Right = false;

        IExpansionDevice::SuborKeyboardKeys suborKeyboardKeys = {};
        IExpansionDevice::FamilyBasicKeyboardKeys familyBasicKeyboardKeys = {};
        std::array<bool, 12> powerPadP1Buttons = {};
        std::array<bool, 12> powerPadP2Buttons = {};
    };


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

    bool m_halt;

    int m_cpuCyclesAcc;

    uint32_t m_cyclesPerSecond;

    uint32_t m_audioRenderCyclesAcc;
    uint32_t m_lastAudioRenderedMs;
    double m_vsyncAudioCompMsAcc;
    int m_vsyncAudioSkipMsDebt;

    uint8_t m_openBus;
    uint16_t m_prevControllerReadAddr;

    uint32_t m_frameCount;
    HardwareActions m_hardwareActions;

    bool m_runningLoop;
    bool m_speedBoost;
    bool m_paused;
    static constexpr int SPEED_BOOST_MULTIPLIER = 3;

    //do not serialize bellow atributtes
    bool m_saveStateFlag;
    bool m_loadStateFlag;    

    bool m_netplaySnapshotFlag = false;
    std::optional<PendingNetplaySnapshot> m_netplaySnapshotResult;
    bool m_netplayLoadStateFlag = false;
    bool m_netplayLoadStateCleanBoot = false;
    std::vector<uint8_t> m_netplayLoadStateData;
    std::optional<bool> m_netplayLoadStateResult;
    PendingInputState m_pendingInputState;
    bool m_processingFrameStart = false;

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
                data = m_ppu.readWrite<false>(addr, data);
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

        if constexpr(accessType == AccessType::Read)
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
        m_cartridge.applyExternalActions(
            m_hardwareActions.consumeFdsPendingActions(
                m_cartridge.isValid() && m_cartridge.system() == GameDatabase::System::FDS
            )
        );
        m_hardwareActions.onFrameStart();
        m_nsfPlayer.onFrameStart();
        updateCyclesPerSecond();
        m_processingFrameStart = true;
        signalFrameStart();
        m_processingFrameStart = false;
        applyPendingInputState();
    }

    void onFrameReady()
    {
        m_newFrame = true;
        ++m_frameCount;
        
        signalFrameReady();
    }

    void onScanlineStart()
    {
        if(m_portDevice1) m_portDevice1->onScanlineChanged();
        if(m_portDevice2) m_portDevice2->onScanlineChanged();
        if(m_expansionDevice) m_expansionDevice->onScanlineChanged();
        m_cartridge.onScanlineStart(m_ppu.isActivelyRendering(), m_ppu.scanline());
    }

    void onDMCRequest(uint16_t addr, bool reload) {
        m_cpu.startDmcDma(addr, reload);
    }

    void onDMCCancelRequest() {
        m_cpu.cancelDmcDma();
    }

    void onDMCImplicitAbortRequest() {
        m_cpu.scheduleImplicitDmcSingleCycleAbort();
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

    void resetVolatileStateAfterStateLoad()
    {
        // These fields are intentionally not serialized, so state loading must
        // reinitialize them to avoid behavior that depends on the instance's
        // history before the load.
        m_saveStateFlag = false;
        m_loadStateFlag = false;
        m_processingFrameStart = false;
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
    }

    void applyPendingInputState()
    {
        if(m_fourScore) m_fourScore->setControllerButtons(0, m_pendingInputState.p1A, m_pendingInputState.p1B, m_pendingInputState.p1Select, m_pendingInputState.p1Start, m_pendingInputState.p1Up, m_pendingInputState.p1Down, m_pendingInputState.p1Left, m_pendingInputState.p1Right);
        else if(m_HoriAdapter) m_HoriAdapter->setControllerButtons(0, m_pendingInputState.p1A, m_pendingInputState.p1B, m_pendingInputState.p1Select, m_pendingInputState.p1Start, m_pendingInputState.p1Up, m_pendingInputState.p1Down, m_pendingInputState.p1Left, m_pendingInputState.p1Right);
        else if(m_portDevice1) m_portDevice1->setButtonsStatusExtended(m_pendingInputState.p1A, m_pendingInputState.p1B, m_pendingInputState.p1Select, m_pendingInputState.p1Start, m_pendingInputState.p1Up, m_pendingInputState.p1Down, m_pendingInputState.p1Left, m_pendingInputState.p1Right, m_pendingInputState.p1X, m_pendingInputState.p1Y, m_pendingInputState.p1L, m_pendingInputState.p1R);

        if(m_fourScore) m_fourScore->setControllerButtons(1, m_pendingInputState.p2A, m_pendingInputState.p2B, m_pendingInputState.p2Select, m_pendingInputState.p2Start, m_pendingInputState.p2Up, m_pendingInputState.p2Down, m_pendingInputState.p2Left, m_pendingInputState.p2Right);
        else if(m_HoriAdapter) m_HoriAdapter->setControllerButtons(1, m_pendingInputState.p2A, m_pendingInputState.p2B, m_pendingInputState.p2Select, m_pendingInputState.p2Start, m_pendingInputState.p2Up, m_pendingInputState.p2Down, m_pendingInputState.p2Left, m_pendingInputState.p2Right);
        else if(m_portDevice2) m_portDevice2->setButtonsStatusExtended(m_pendingInputState.p2A, m_pendingInputState.p2B, m_pendingInputState.p2Select, m_pendingInputState.p2Start, m_pendingInputState.p2Up, m_pendingInputState.p2Down, m_pendingInputState.p2Left, m_pendingInputState.p2Right, m_pendingInputState.p2X, m_pendingInputState.p2Y, m_pendingInputState.p2L, m_pendingInputState.p2R);

        if(m_fourScore) m_fourScore->setControllerButtons(2, m_pendingInputState.p3A, m_pendingInputState.p3B, m_pendingInputState.p3Select, m_pendingInputState.p3Start, m_pendingInputState.p3Up, m_pendingInputState.p3Down, m_pendingInputState.p3Left, m_pendingInputState.p3Right);
        else if(m_HoriAdapter) m_HoriAdapter->setControllerButtons(2, m_pendingInputState.p3A, m_pendingInputState.p3B, m_pendingInputState.p3Select, m_pendingInputState.p3Start, m_pendingInputState.p3Up, m_pendingInputState.p3Down, m_pendingInputState.p3Left, m_pendingInputState.p3Right);
        else if(m_settings.getExpansionDevice() == Settings::ExpansionDevice::STANDARD_CONTROLLER_FAMICOM && m_expansionDevice) m_expansionDevice->setButtonsStatus(m_pendingInputState.p3A, m_pendingInputState.p3B, m_pendingInputState.p3Select, m_pendingInputState.p3Start, m_pendingInputState.p3Up, m_pendingInputState.p3Down, m_pendingInputState.p3Left, m_pendingInputState.p3Right);

        if(m_fourScore) m_fourScore->setControllerButtons(3, m_pendingInputState.p4A, m_pendingInputState.p4B, m_pendingInputState.p4Select, m_pendingInputState.p4Start, m_pendingInputState.p4Up, m_pendingInputState.p4Down, m_pendingInputState.p4Left, m_pendingInputState.p4Right);
        else if(m_HoriAdapter) m_HoriAdapter->setControllerButtons(3, m_pendingInputState.p4A, m_pendingInputState.p4B, m_pendingInputState.p4Select, m_pendingInputState.p4Start, m_pendingInputState.p4Up, m_pendingInputState.p4Down, m_pendingInputState.p4Left, m_pendingInputState.p4Right);

        processNsfControllerInput(m_pendingInputState.p1Select, m_pendingInputState.p1Start, m_pendingInputState.p1Left, m_pendingInputState.p1Right);

        if(m_settings.getPortDevice(Settings::Port::P_1) == std::optional<Settings::Device>(Settings::Device::ZAPPER) && m_portDevice1) {
            m_portDevice1->setCursorPosition(m_pendingInputState.zapperP1X, m_pendingInputState.zapperP1Y);
            m_portDevice1->setTrigger(m_pendingInputState.zapperP1Trigger);
        }
        if(m_settings.getPortDevice(Settings::Port::P_2) == std::optional<Settings::Device>(Settings::Device::ZAPPER) && m_portDevice2) {
            m_portDevice2->setCursorPosition(m_pendingInputState.zapperP2X, m_pendingInputState.zapperP2Y);
            m_portDevice2->setTrigger(m_pendingInputState.zapperP2Trigger);
        }

        if(m_settings.getExpansionDevice() == Settings::ExpansionDevice::BANDAI_HYPERSHOT && m_expansionDevice) {
            m_expansionDevice->setButtonsStatus(m_pendingInputState.bandaiA, m_pendingInputState.bandaiB, m_pendingInputState.bandaiSelect, m_pendingInputState.bandaiStart, m_pendingInputState.bandaiUp, m_pendingInputState.bandaiDown, m_pendingInputState.bandaiLeft, m_pendingInputState.bandaiRight);
            m_expansionDevice->setCursorPosition(m_pendingInputState.bandaiX, m_pendingInputState.bandaiY);
            m_expansionDevice->setTrigger(m_pendingInputState.bandaiTrigger);
        }

        if(m_settings.getPortDevice(Settings::Port::P_1) == std::optional<Settings::Device>(Settings::Device::ARKANOID_CONTROLLER) && m_portDevice1) {
            m_portDevice1->setPositionNormalized(std::clamp(m_pendingInputState.arkanoidP1Position, 0.0f, 1.0f));
            m_portDevice1->setTrigger(m_pendingInputState.arkanoidP1Button);
        }
        if(m_settings.getPortDevice(Settings::Port::P_2) == std::optional<Settings::Device>(Settings::Device::ARKANOID_CONTROLLER) && m_portDevice2) {
            m_portDevice2->setPositionNormalized(std::clamp(m_pendingInputState.arkanoidP2Position, 0.0f, 1.0f));
            m_portDevice2->setTrigger(m_pendingInputState.arkanoidP2Button);
        }
        if(m_settings.getExpansionDevice() == Settings::ExpansionDevice::ARKANOID_CONTROLLER && m_expansionDevice) {
            m_expansionDevice->setPositionNormalized(std::clamp(m_pendingInputState.arkanoidFamicomPosition, 0.0f, 1.0f));
            m_expansionDevice->setTrigger(m_pendingInputState.arkanoidFamicomButton);
        }

        if(m_settings.getExpansionDevice() == Settings::ExpansionDevice::KONAMI_HYPERSHOT && m_expansionDevice) {
            m_expansionDevice->setPlayersButtons(m_pendingInputState.konamiP1Run, m_pendingInputState.konamiP1Jump, m_pendingInputState.konamiP2Run, m_pendingInputState.konamiP2Jump);
        }

        if(m_settings.getPortDevice(Settings::Port::P_1) == std::optional<Settings::Device>(Settings::Device::SNES_MOUSE) && m_portDevice1) {
            m_portDevice1->addRelativeMotion(m_pendingInputState.snesMouseP1DeltaX, m_pendingInputState.snesMouseP1DeltaY);
            m_portDevice1->setTrigger(m_pendingInputState.snesMouseP1Left);
            m_portDevice1->setSecondaryTrigger(m_pendingInputState.snesMouseP1Right);
            m_pendingInputState.snesMouseP1DeltaX = 0;
            m_pendingInputState.snesMouseP1DeltaY = 0;
        }
        if(m_settings.getPortDevice(Settings::Port::P_2) == std::optional<Settings::Device>(Settings::Device::SNES_MOUSE) && m_portDevice2) {
            m_portDevice2->addRelativeMotion(m_pendingInputState.snesMouseP2DeltaX, m_pendingInputState.snesMouseP2DeltaY);
            m_portDevice2->setTrigger(m_pendingInputState.snesMouseP2Left);
            m_portDevice2->setSecondaryTrigger(m_pendingInputState.snesMouseP2Right);
            m_pendingInputState.snesMouseP2DeltaX = 0;
            m_pendingInputState.snesMouseP2DeltaY = 0;
        }

        if(m_settings.getPortDevice(Settings::Port::P_1) == std::optional<Settings::Device>(Settings::Device::VIRTUAL_BOY_CONTROLLER) && m_portDevice1) {
            m_portDevice1->setVirtualBoyButtons(m_pendingInputState.vbP1A, m_pendingInputState.vbP1B, m_pendingInputState.vbP1Select, m_pendingInputState.vbP1Start, m_pendingInputState.vbP1Up0, m_pendingInputState.vbP1Down0, m_pendingInputState.vbP1Left0, m_pendingInputState.vbP1Right0, m_pendingInputState.vbP1Up1, m_pendingInputState.vbP1Down1, m_pendingInputState.vbP1Left1, m_pendingInputState.vbP1Right1, m_pendingInputState.vbP1L, m_pendingInputState.vbP1R);
        }
        if(m_settings.getPortDevice(Settings::Port::P_2) == std::optional<Settings::Device>(Settings::Device::VIRTUAL_BOY_CONTROLLER) && m_portDevice2) {
            m_portDevice2->setVirtualBoyButtons(m_pendingInputState.vbP2A, m_pendingInputState.vbP2B, m_pendingInputState.vbP2Select, m_pendingInputState.vbP2Start, m_pendingInputState.vbP2Up0, m_pendingInputState.vbP2Down0, m_pendingInputState.vbP2Left0, m_pendingInputState.vbP2Right0, m_pendingInputState.vbP2Up1, m_pendingInputState.vbP2Down1, m_pendingInputState.vbP2Left1, m_pendingInputState.vbP2Right1, m_pendingInputState.vbP2L, m_pendingInputState.vbP2R);
        }

        if(m_settings.getPortDevice(Settings::Port::P_1) == std::optional<Settings::Device>(Settings::Device::SUBOR_MOUSE) && m_portDevice1) {
            m_portDevice1->addRelativeMotion(m_pendingInputState.suborMouseP1DeltaX, m_pendingInputState.suborMouseP1DeltaY);
            m_portDevice1->setTrigger(m_pendingInputState.suborMouseP1Left);
            m_portDevice1->setSecondaryTrigger(m_pendingInputState.suborMouseP1Right);
            m_pendingInputState.suborMouseP1DeltaX = 0;
            m_pendingInputState.suborMouseP1DeltaY = 0;
        }
        if(m_settings.getPortDevice(Settings::Port::P_2) == std::optional<Settings::Device>(Settings::Device::SUBOR_MOUSE) && m_portDevice2) {
            m_portDevice2->addRelativeMotion(m_pendingInputState.suborMouseP2DeltaX, m_pendingInputState.suborMouseP2DeltaY);
            m_portDevice2->setTrigger(m_pendingInputState.suborMouseP2Left);
            m_portDevice2->setSecondaryTrigger(m_pendingInputState.suborMouseP2Right);
            m_pendingInputState.suborMouseP2DeltaX = 0;
            m_pendingInputState.suborMouseP2DeltaY = 0;
        }

        if(m_settings.getExpansionDevice() == Settings::ExpansionDevice::SUBOR_KEYBOARD && m_expansionDevice) m_expansionDevice->setSuborKeyboardKeys(m_pendingInputState.suborKeyboardKeys);
        if(m_settings.getExpansionDevice() == Settings::ExpansionDevice::FAMILY_BASIC_KEYBOARD && m_expansionDevice) m_expansionDevice->setFamilyBasicKeyboardKeys(m_pendingInputState.familyBasicKeyboardKeys);

        const auto p1Device = m_settings.getPortDevice(Settings::Port::P_1);
        const auto p2Device = m_settings.getPortDevice(Settings::Port::P_2);
        const bool familyTrainer = m_settings.getExpansionDevice() == Settings::ExpansionDevice::FAMILY_TRAINER_SIDE_A || m_settings.getExpansionDevice() == Settings::ExpansionDevice::FAMILY_TRAINER_SIDE_B;
        if((p1Device == std::optional<Settings::Device>(Settings::Device::POWER_PAD_SIDE_A) || p1Device == std::optional<Settings::Device>(Settings::Device::POWER_PAD_SIDE_B)) && m_portDevice1) m_portDevice1->setPowerPadButtons(m_pendingInputState.powerPadP1Buttons);
        if((p2Device == std::optional<Settings::Device>(Settings::Device::POWER_PAD_SIDE_A) || p2Device == std::optional<Settings::Device>(Settings::Device::POWER_PAD_SIDE_B)) && m_portDevice2) m_portDevice2->setPowerPadButtons(m_pendingInputState.powerPadP2Buttons);
        if(familyTrainer && m_expansionDevice) m_expansionDevice->setPowerPadButtons(m_pendingInputState.powerPadP1Buttons);
    }

    void processDeferredNetplaySnapshot()
    {
        if(!m_netplaySnapshotFlag || !m_cartridge.isValid()) return;

        PendingNetplaySnapshot snapshot;
        snapshot.frame = m_frameCount;
        snapshot.data = saveStateToMemory();
        m_netplaySnapshotResult = std::move(snapshot);
        m_netplaySnapshotFlag = false;
    }

    void processDeferredNetplayLoad()
    {
        if(!m_netplayLoadStateFlag) return;

        const std::vector<uint8_t> data = m_netplayLoadStateData;
        const bool cleanBoot = m_netplayLoadStateCleanBoot;
        m_netplayLoadStateData.clear();
        m_netplayLoadStateFlag = false;
        m_netplayLoadStateCleanBoot = false;

        const bool loaded = cleanBoot
            ? loadStateFromMemoryOnCleanBoot(data)
            : (loadStateFromMemory(data), valid());
        m_netplayLoadStateResult = loaded;
    }

    GERANES_INLINE void renderAudioMs(uint32_t ms)
    {
        if(ms == 0) return;

        m_audioOutput.setRewinding(m_rewind.isRewinding());
        m_audioOutput.setExpansionAudioVolume(1.0f);
        bool enableAudio = m_rewind.rewindLimit() && !m_speedBoost;
        m_audioOutput.render(ms, !enableAudio || m_nsfPlayer.forceMute());
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
    SigSlot::Signal<> signalFrameStart;
    SigSlot::Signal<> signalFrameReady;

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
    m_rewind(*this),
    m_nsfPlayer(m_cartridge, m_apu, m_audioOutput, [this]() { this->reset(); }),
    m_console(m_cpu, m_ppu, m_apu, m_cartridge)
    {
        init();

        m_cpu.signalError.bind(&GeraNESEmu::onError, this);

        m_ppu.signalFrameStart.bind(&GeraNESEmu::onFrameStart, this);
        m_ppu.signalFrameReady.bind(&GeraNESEmu::onFrameReady, this);
        m_ppu.signalScanlineStart.bind(&GeraNESEmu::onScanlineStart, this);
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

        m_frameCount = 0;

        m_saveStateFlag = false;
        m_loadStateFlag = false;
        m_processingFrameStart = false;
        m_netplaySnapshotFlag = false;
        m_netplaySnapshotResult.reset();
        m_netplayLoadStateFlag = false;
        m_netplayLoadStateCleanBoot = false;
        m_netplayLoadStateData.clear();
        m_netplayLoadStateResult.reset();
        m_runningLoop = false;
        m_speedBoost = false;
        m_paused = false;
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
            m_audioOutput.discardQueuedAudio();
            m_audioOutput.clearAudioBuffers();
            m_lastAudioRenderedMs = 0;
            m_vsyncAudioCompMsAcc = 0.0;
            m_vsyncAudioSkipMsDebt = 0;
        }
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

            updateCyclesPerSecond();

            m_ppu.setVsPpuModel(m_cartridge.vsPpuModel());
            m_cartridge.reset();
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

        processDeferredNetplaySnapshot();
        processDeferredNetplayLoad();

        return ret;
    }

    GERANES_INLINE bool update(uint32_t dt) {
        applyPendingNsfControllerActions();
        if(m_paused) return false;
        return _update<false>(dt);
    }

    GERANES_INLINE bool updateUntilFrame(uint32_t dt) {
        applyPendingNsfControllerActions();
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
        Deserialize d;
        d.setData(data);
        serialization(d);
        resyncAudioAfterStateLoad();
        resetVolatileStateAfterStateLoad();
    }

    void loadStateFromMemory(const uint8_t* data, size_t size)
    {
        Deserialize d;
        d.setData(data, size);
        serialization(d);
        resyncAudioAfterStateLoad();
        resetVolatileStateAfterStateLoad();
    }

    bool loadStateFromMemoryOnCleanBoot(const std::vector<uint8_t>& data)
    {
        if(data.empty() || !m_cartridge.isValid()) return false;

        const std::string romPath = m_cartridge.romFile().fullPath();
        if(romPath.empty()) return false;
        if(!open(romPath) || !valid()) return false;

        loadStateFromMemory(data);
        return valid();
    }

    void requestNetplaySnapshotAtSafePoint()
    {
        if(!m_runningLoop) {
            processDeferredNetplaySnapshot();
            if(m_netplaySnapshotResult.has_value()) return;
            if(!m_cartridge.isValid()) return;

            PendingNetplaySnapshot snapshot;
            snapshot.frame = m_frameCount;
            snapshot.data = saveStateToMemory();
            m_netplaySnapshotResult = std::move(snapshot);
            return;
        }

        m_netplaySnapshotFlag = true;
    }

    std::optional<PendingNetplaySnapshot> consumeNetplaySnapshotAtSafePoint()
    {
        std::optional<PendingNetplaySnapshot> result = std::move(m_netplaySnapshotResult);
        m_netplaySnapshotResult.reset();
        return result;
    }

    void requestNetplayLoadStateAtSafePoint(const std::vector<uint8_t>& data, bool cleanBoot = false)
    {
        if(!m_runningLoop) {
            const bool loaded = cleanBoot
                ? loadStateFromMemoryOnCleanBoot(data)
                : (loadStateFromMemory(data), valid());
            m_netplayLoadStateResult = loaded;
            return;
        }

        m_netplayLoadStateData = data;
        m_netplayLoadStateCleanBoot = cleanBoot;
        m_netplayLoadStateFlag = true;
    }

    std::optional<bool> consumeNetplayLoadStateResult()
    {
        std::optional<bool> result = m_netplayLoadStateResult;
        m_netplayLoadStateResult.reset();
        return result;
    }

    std::vector<uint8_t> saveStateToMemory()
    {
        Serialize s;
        serialization(s);
        return s.getData();
    }

    uint32_t canonicalStateCrc32()
    {
        const std::vector<uint8_t> data = saveStateToMemory();
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

    GERANES_INLINE std::optional<Settings::Device> getPortDevice(Settings::Port port)
    {
        return m_settings.getPortDevice(port);
    }

    GERANES_INLINE void setPortDevice(Settings::Port port, Settings::Device device)
    {
        m_settings.setPortDevice(port, device);
        if(!isAnyMultitapActive()) recreatePortDevice(port);
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
        m_settings.serialization(s);
        if(dynamic_cast<Deserialize*>(&s) != nullptr) {
            recreateInputRouting();
        }
        if(m_portDevice1) m_portDevice1->serialization(s);
        if(m_portDevice2) m_portDevice2->serialization(s);
        if(m_expansionDevice) m_expansionDevice->serialization(s);
        if(m_fourScore) m_fourScore->serialization(s);
        if(m_HoriAdapter) m_HoriAdapter->serialization(s);
        SERIALIZEDATA(s, m_pendingInputState.p1A); SERIALIZEDATA(s, m_pendingInputState.p1B); SERIALIZEDATA(s, m_pendingInputState.p1Select); SERIALIZEDATA(s, m_pendingInputState.p1Start);
        SERIALIZEDATA(s, m_pendingInputState.p1Up); SERIALIZEDATA(s, m_pendingInputState.p1Down); SERIALIZEDATA(s, m_pendingInputState.p1Left); SERIALIZEDATA(s, m_pendingInputState.p1Right);
        SERIALIZEDATA(s, m_pendingInputState.p1X); SERIALIZEDATA(s, m_pendingInputState.p1Y); SERIALIZEDATA(s, m_pendingInputState.p1L); SERIALIZEDATA(s, m_pendingInputState.p1R);
        SERIALIZEDATA(s, m_pendingInputState.p2A); SERIALIZEDATA(s, m_pendingInputState.p2B); SERIALIZEDATA(s, m_pendingInputState.p2Select); SERIALIZEDATA(s, m_pendingInputState.p2Start);
        SERIALIZEDATA(s, m_pendingInputState.p2Up); SERIALIZEDATA(s, m_pendingInputState.p2Down); SERIALIZEDATA(s, m_pendingInputState.p2Left); SERIALIZEDATA(s, m_pendingInputState.p2Right);
        SERIALIZEDATA(s, m_pendingInputState.p2X); SERIALIZEDATA(s, m_pendingInputState.p2Y); SERIALIZEDATA(s, m_pendingInputState.p2L); SERIALIZEDATA(s, m_pendingInputState.p2R);
        SERIALIZEDATA(s, m_pendingInputState.p3A); SERIALIZEDATA(s, m_pendingInputState.p3B); SERIALIZEDATA(s, m_pendingInputState.p3Select); SERIALIZEDATA(s, m_pendingInputState.p3Start);
        SERIALIZEDATA(s, m_pendingInputState.p3Up); SERIALIZEDATA(s, m_pendingInputState.p3Down); SERIALIZEDATA(s, m_pendingInputState.p3Left); SERIALIZEDATA(s, m_pendingInputState.p3Right);
        SERIALIZEDATA(s, m_pendingInputState.p4A); SERIALIZEDATA(s, m_pendingInputState.p4B); SERIALIZEDATA(s, m_pendingInputState.p4Select); SERIALIZEDATA(s, m_pendingInputState.p4Start);
        SERIALIZEDATA(s, m_pendingInputState.p4Up); SERIALIZEDATA(s, m_pendingInputState.p4Down); SERIALIZEDATA(s, m_pendingInputState.p4Left); SERIALIZEDATA(s, m_pendingInputState.p4Right);
        SERIALIZEDATA(s, m_pendingInputState.zapperP1X); SERIALIZEDATA(s, m_pendingInputState.zapperP1Y); SERIALIZEDATA(s, m_pendingInputState.zapperP1Trigger);
        SERIALIZEDATA(s, m_pendingInputState.zapperP2X); SERIALIZEDATA(s, m_pendingInputState.zapperP2Y); SERIALIZEDATA(s, m_pendingInputState.zapperP2Trigger);
        SERIALIZEDATA(s, m_pendingInputState.bandaiA); SERIALIZEDATA(s, m_pendingInputState.bandaiB); SERIALIZEDATA(s, m_pendingInputState.bandaiSelect); SERIALIZEDATA(s, m_pendingInputState.bandaiStart);
        SERIALIZEDATA(s, m_pendingInputState.bandaiUp); SERIALIZEDATA(s, m_pendingInputState.bandaiDown); SERIALIZEDATA(s, m_pendingInputState.bandaiLeft); SERIALIZEDATA(s, m_pendingInputState.bandaiRight);
        SERIALIZEDATA(s, m_pendingInputState.bandaiX); SERIALIZEDATA(s, m_pendingInputState.bandaiY); SERIALIZEDATA(s, m_pendingInputState.bandaiTrigger);
        SERIALIZEDATA(s, m_pendingInputState.arkanoidP1Position); SERIALIZEDATA(s, m_pendingInputState.arkanoidP1Button);
        SERIALIZEDATA(s, m_pendingInputState.arkanoidP2Position); SERIALIZEDATA(s, m_pendingInputState.arkanoidP2Button);
        SERIALIZEDATA(s, m_pendingInputState.arkanoidFamicomPosition); SERIALIZEDATA(s, m_pendingInputState.arkanoidFamicomButton);
        SERIALIZEDATA(s, m_pendingInputState.konamiP1Run); SERIALIZEDATA(s, m_pendingInputState.konamiP1Jump); SERIALIZEDATA(s, m_pendingInputState.konamiP2Run); SERIALIZEDATA(s, m_pendingInputState.konamiP2Jump);
        SERIALIZEDATA(s, m_pendingInputState.snesMouseP1DeltaX); SERIALIZEDATA(s, m_pendingInputState.snesMouseP1DeltaY); SERIALIZEDATA(s, m_pendingInputState.snesMouseP1Left); SERIALIZEDATA(s, m_pendingInputState.snesMouseP1Right);
        SERIALIZEDATA(s, m_pendingInputState.snesMouseP2DeltaX); SERIALIZEDATA(s, m_pendingInputState.snesMouseP2DeltaY); SERIALIZEDATA(s, m_pendingInputState.snesMouseP2Left); SERIALIZEDATA(s, m_pendingInputState.snesMouseP2Right);
        SERIALIZEDATA(s, m_pendingInputState.vbP1A); SERIALIZEDATA(s, m_pendingInputState.vbP1B); SERIALIZEDATA(s, m_pendingInputState.vbP1Select); SERIALIZEDATA(s, m_pendingInputState.vbP1Start);
        SERIALIZEDATA(s, m_pendingInputState.vbP1Up0); SERIALIZEDATA(s, m_pendingInputState.vbP1Down0); SERIALIZEDATA(s, m_pendingInputState.vbP1Left0); SERIALIZEDATA(s, m_pendingInputState.vbP1Right0);
        SERIALIZEDATA(s, m_pendingInputState.vbP1Up1); SERIALIZEDATA(s, m_pendingInputState.vbP1Down1); SERIALIZEDATA(s, m_pendingInputState.vbP1Left1); SERIALIZEDATA(s, m_pendingInputState.vbP1Right1); SERIALIZEDATA(s, m_pendingInputState.vbP1L); SERIALIZEDATA(s, m_pendingInputState.vbP1R);
        SERIALIZEDATA(s, m_pendingInputState.vbP2A); SERIALIZEDATA(s, m_pendingInputState.vbP2B); SERIALIZEDATA(s, m_pendingInputState.vbP2Select); SERIALIZEDATA(s, m_pendingInputState.vbP2Start);
        SERIALIZEDATA(s, m_pendingInputState.vbP2Up0); SERIALIZEDATA(s, m_pendingInputState.vbP2Down0); SERIALIZEDATA(s, m_pendingInputState.vbP2Left0); SERIALIZEDATA(s, m_pendingInputState.vbP2Right0);
        SERIALIZEDATA(s, m_pendingInputState.vbP2Up1); SERIALIZEDATA(s, m_pendingInputState.vbP2Down1); SERIALIZEDATA(s, m_pendingInputState.vbP2Left1); SERIALIZEDATA(s, m_pendingInputState.vbP2Right1); SERIALIZEDATA(s, m_pendingInputState.vbP2L); SERIALIZEDATA(s, m_pendingInputState.vbP2R);
        SERIALIZEDATA(s, m_pendingInputState.suborMouseP1DeltaX); SERIALIZEDATA(s, m_pendingInputState.suborMouseP1DeltaY); SERIALIZEDATA(s, m_pendingInputState.suborMouseP1Left); SERIALIZEDATA(s, m_pendingInputState.suborMouseP1Right);
        SERIALIZEDATA(s, m_pendingInputState.suborMouseP2DeltaX); SERIALIZEDATA(s, m_pendingInputState.suborMouseP2DeltaY); SERIALIZEDATA(s, m_pendingInputState.suborMouseP2Left); SERIALIZEDATA(s, m_pendingInputState.suborMouseP2Right);
        SERIALIZEDATA(s, m_pendingInputState.suborKeyboardKeys); SERIALIZEDATA(s, m_pendingInputState.familyBasicKeyboardKeys);
        s.array(reinterpret_cast<uint8_t*>(m_pendingInputState.powerPadP1Buttons.data()), 1, m_pendingInputState.powerPadP1Buttons.size());
        s.array(reinterpret_cast<uint8_t*>(m_pendingInputState.powerPadP2Buttons.data()), 1, m_pendingInputState.powerPadP2Buttons.size());
        SERIALIZEDATA(s, m_updateCyclesAcc);
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

    void setController1Buttons(bool bA, bool bB, bool bSelect, bool bStart, bool bUp, bool bDown, bool bLeft, bool bRight,
                               bool bX = false, bool bY = false, bool bL = false, bool bR = false)
    {
        m_pendingInputState.p1A = bA; m_pendingInputState.p1B = bB; m_pendingInputState.p1Select = bSelect; m_pendingInputState.p1Start = bStart;
        m_pendingInputState.p1Up = bUp; m_pendingInputState.p1Down = bDown; m_pendingInputState.p1Left = bLeft; m_pendingInputState.p1Right = bRight;
        m_pendingInputState.p1X = bX; m_pendingInputState.p1Y = bY; m_pendingInputState.p1L = bL; m_pendingInputState.p1R = bR;
    }

    void setController2Buttons(bool bA, bool bB, bool bSelect, bool bStart, bool bUp, bool bDown, bool bLeft, bool bRight,
                               bool bX = false, bool bY = false, bool bL = false, bool bR = false)
    {
        m_pendingInputState.p2A = bA; m_pendingInputState.p2B = bB; m_pendingInputState.p2Select = bSelect; m_pendingInputState.p2Start = bStart;
        m_pendingInputState.p2Up = bUp; m_pendingInputState.p2Down = bDown; m_pendingInputState.p2Left = bLeft; m_pendingInputState.p2Right = bRight;
        m_pendingInputState.p2X = bX; m_pendingInputState.p2Y = bY; m_pendingInputState.p2L = bL; m_pendingInputState.p2R = bR;
    }

    void setController3Buttons(bool bA, bool bB, bool bSelect, bool bStart, bool bUp, bool bDown, bool bLeft, bool bRight)
    {
        m_pendingInputState.p3A = bA; m_pendingInputState.p3B = bB; m_pendingInputState.p3Select = bSelect; m_pendingInputState.p3Start = bStart;
        m_pendingInputState.p3Up = bUp; m_pendingInputState.p3Down = bDown; m_pendingInputState.p3Left = bLeft; m_pendingInputState.p3Right = bRight;
    }

    void setController4Buttons(bool bA, bool bB, bool bSelect, bool bStart, bool bUp, bool bDown, bool bLeft, bool bRight)
    {
        m_pendingInputState.p4A = bA; m_pendingInputState.p4B = bB; m_pendingInputState.p4Select = bSelect; m_pendingInputState.p4Start = bStart;
        m_pendingInputState.p4Up = bUp; m_pendingInputState.p4Down = bDown; m_pendingInputState.p4Left = bLeft; m_pendingInputState.p4Right = bRight;
    }

    void setZapper(Settings::Port port, int x, int y, bool trigger)
    {
        if(port == Settings::Port::P_1) { m_pendingInputState.zapperP1X = x; m_pendingInputState.zapperP1Y = y; m_pendingInputState.zapperP1Trigger = trigger; }
        else if(port == Settings::Port::P_2) { m_pendingInputState.zapperP2X = x; m_pendingInputState.zapperP2Y = y; m_pendingInputState.zapperP2Trigger = trigger; }
    }

    void setBandaiHyperShotButtons(bool bA, bool bB, bool bSelect, bool bStart, bool bUp, bool bDown, bool bLeft, bool bRight)
    {
        m_pendingInputState.bandaiA = bA; m_pendingInputState.bandaiB = bB; m_pendingInputState.bandaiSelect = bSelect; m_pendingInputState.bandaiStart = bStart;
        m_pendingInputState.bandaiUp = bUp; m_pendingInputState.bandaiDown = bDown; m_pendingInputState.bandaiLeft = bLeft; m_pendingInputState.bandaiRight = bRight;
    }

    void setBandaiHyperShot(int x, int y, bool trigger)
    {
        m_pendingInputState.bandaiX = x; m_pendingInputState.bandaiY = y; m_pendingInputState.bandaiTrigger = trigger;
    }

    void setArkanoidController(Settings::Port port, float positionNormalized, bool button)
    {
        if(port == Settings::Port::P_1) { m_pendingInputState.arkanoidP1Position = positionNormalized; m_pendingInputState.arkanoidP1Button = button; }
        else if(port == Settings::Port::P_2) { m_pendingInputState.arkanoidP2Position = positionNormalized; m_pendingInputState.arkanoidP2Button = button; }
    }

    void setKonamiHyperShotButtons(bool p1Run, bool p1Jump, bool p2Run, bool p2Jump)
    {
        m_pendingInputState.konamiP1Run = p1Run; m_pendingInputState.konamiP1Jump = p1Jump; m_pendingInputState.konamiP2Run = p2Run; m_pendingInputState.konamiP2Jump = p2Jump;
    }

    void setArkanoidControllerFamicom(float positionNormalized, bool button)
    {
        m_pendingInputState.arkanoidFamicomPosition = positionNormalized; m_pendingInputState.arkanoidFamicomButton = button;
    }

    void setSnesMouse(Settings::Port port, int deltaX, int deltaY, bool leftButton, bool rightButton)
    {
        if(port == Settings::Port::P_1) { m_pendingInputState.snesMouseP1DeltaX += deltaX; m_pendingInputState.snesMouseP1DeltaY += deltaY; m_pendingInputState.snesMouseP1Left = leftButton; m_pendingInputState.snesMouseP1Right = rightButton; }
        else if(port == Settings::Port::P_2) { m_pendingInputState.snesMouseP2DeltaX += deltaX; m_pendingInputState.snesMouseP2DeltaY += deltaY; m_pendingInputState.snesMouseP2Left = leftButton; m_pendingInputState.snesMouseP2Right = rightButton; }
    }

    void setVirtualBoyControllerButtons(Settings::Port port,
                                        bool bA, bool bB, bool bSelect, bool bStart,
                                        bool bUp0, bool bDown0, bool bLeft0, bool bRight0,
                                        bool bUp1, bool bDown1, bool bLeft1, bool bRight1,
                                        bool bL, bool bR)
    {
        if(port == Settings::Port::P_1) {
            m_pendingInputState.vbP1A = bA; m_pendingInputState.vbP1B = bB; m_pendingInputState.vbP1Select = bSelect; m_pendingInputState.vbP1Start = bStart;
            m_pendingInputState.vbP1Up0 = bUp0; m_pendingInputState.vbP1Down0 = bDown0; m_pendingInputState.vbP1Left0 = bLeft0; m_pendingInputState.vbP1Right0 = bRight0;
            m_pendingInputState.vbP1Up1 = bUp1; m_pendingInputState.vbP1Down1 = bDown1; m_pendingInputState.vbP1Left1 = bLeft1; m_pendingInputState.vbP1Right1 = bRight1; m_pendingInputState.vbP1L = bL; m_pendingInputState.vbP1R = bR;
        } else if(port == Settings::Port::P_2) {
            m_pendingInputState.vbP2A = bA; m_pendingInputState.vbP2B = bB; m_pendingInputState.vbP2Select = bSelect; m_pendingInputState.vbP2Start = bStart;
            m_pendingInputState.vbP2Up0 = bUp0; m_pendingInputState.vbP2Down0 = bDown0; m_pendingInputState.vbP2Left0 = bLeft0; m_pendingInputState.vbP2Right0 = bRight0;
            m_pendingInputState.vbP2Up1 = bUp1; m_pendingInputState.vbP2Down1 = bDown1; m_pendingInputState.vbP2Left1 = bLeft1; m_pendingInputState.vbP2Right1 = bRight1; m_pendingInputState.vbP2L = bL; m_pendingInputState.vbP2R = bR;
        }
    }

    void setSuborMouse(Settings::Port port, int deltaX, int deltaY, bool leftButton, bool rightButton)
    {
        if(port == Settings::Port::P_1) { m_pendingInputState.suborMouseP1DeltaX += deltaX; m_pendingInputState.suborMouseP1DeltaY += deltaY; m_pendingInputState.suborMouseP1Left = leftButton; m_pendingInputState.suborMouseP1Right = rightButton; }
        else if(port == Settings::Port::P_2) { m_pendingInputState.suborMouseP2DeltaX += deltaX; m_pendingInputState.suborMouseP2DeltaY += deltaY; m_pendingInputState.suborMouseP2Left = leftButton; m_pendingInputState.suborMouseP2Right = rightButton; }
    }

    void setSuborKeyboardKeys(const IExpansionDevice::SuborKeyboardKeys& keys)
    {
        m_pendingInputState.suborKeyboardKeys = keys;
    }

    void setFamilyBasicKeyboardKeys(const IExpansionDevice::FamilyBasicKeyboardKeys& keys)
    {
        m_pendingInputState.familyBasicKeyboardKeys = keys;
    }

    void setPowerPadButtons(Settings::Port port, const std::array<bool, 12>& buttons)
    {
        if(port == Settings::Port::P_1) m_pendingInputState.powerPadP1Buttons = buttons;
        else if(port == Settings::Port::P_2) m_pendingInputState.powerPadP2Buttons = buttons;
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
        preloadNsfMemory();
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
        m_prevControllerReadAddr = 0xFFFF;
        m_4011WriteCounter = 0;
        m_newFrame = false;
        m_frameCount = 0;
        m_runningLoop = false;
        m_saveStateFlag = false;
        m_loadStateFlag = false;
        m_processingFrameStart = false;
        m_netplaySnapshotFlag = false;
        m_netplaySnapshotResult.reset();
        m_netplayLoadStateFlag = false;
        m_netplayLoadStateCleanBoot = false;
        m_netplayLoadStateData.clear();
        m_netplayLoadStateResult.reset();
        m_nsfPlayer.onEmulatorReset();
        m_hardwareActions.reset();

        m_rewind.reset();
        updateCyclesPerSecond();
        m_cpu.reset();
        Logger::instance().log("Emulator reset", Logger::Type::USER);
    }

};




















