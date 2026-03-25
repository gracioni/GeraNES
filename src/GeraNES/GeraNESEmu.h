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
#include "InputBuffer.h"

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

    uint8_t m_openBus;
    uint16_t m_prevControllerReadAddr;

    uint32_t m_frameCounter;
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
    InputBuffer m_inputBuffer;
    InputFrame m_lastAppliedInputFrame;
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

    InputFrame makeDefaultInputFrame(uint32_t frame)
    {
        InputFrame inputFrame;
        inputFrame.frame = frame;
        inputFrame.port1Device = m_settings.getPortDevice(Settings::Port::P_1).value_or(Settings::Device::NONE);
        inputFrame.port2Device = m_settings.getPortDevice(Settings::Port::P_2).value_or(Settings::Device::NONE);
        inputFrame.expansionDevice = m_settings.getExpansionDevice();
        inputFrame.nesMultitapDevice = m_settings.getNesMultitapDevice();
        inputFrame.famicomMultitapDevice = m_settings.getFamicomMultitapDevice();
        return inputFrame;
    }

    void applyInputFrame(const InputFrame& inputFrame)
    {
        const Settings::Device port1Device = inputFrame.port1Device;
        const Settings::Device port2Device = inputFrame.port2Device;
        const Settings::ExpansionDevice expansionDevice = inputFrame.expansionDevice;
        const Settings::NesMultitapDevice nesMultitapDevice = inputFrame.nesMultitapDevice;
        const Settings::FamicomMultitapDevice famicomMultitapDevice = inputFrame.famicomMultitapDevice;

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

        if(m_fourScore) m_fourScore->setControllerButtons(0, inputFrame.p1A, inputFrame.p1B, inputFrame.p1Select, inputFrame.p1Start, inputFrame.p1Up, inputFrame.p1Down, inputFrame.p1Left, inputFrame.p1Right);
        else if(m_HoriAdapter) m_HoriAdapter->setControllerButtons(0, inputFrame.p1A, inputFrame.p1B, inputFrame.p1Select, inputFrame.p1Start, inputFrame.p1Up, inputFrame.p1Down, inputFrame.p1Left, inputFrame.p1Right);
        else if(m_portDevice1) m_portDevice1->setButtonsStatusExtended(inputFrame.p1A, inputFrame.p1B, inputFrame.p1Select, inputFrame.p1Start, inputFrame.p1Up, inputFrame.p1Down, inputFrame.p1Left, inputFrame.p1Right, inputFrame.p1X, inputFrame.p1Y, inputFrame.p1L, inputFrame.p1R);

        if(m_fourScore) m_fourScore->setControllerButtons(1, inputFrame.p2A, inputFrame.p2B, inputFrame.p2Select, inputFrame.p2Start, inputFrame.p2Up, inputFrame.p2Down, inputFrame.p2Left, inputFrame.p2Right);
        else if(m_HoriAdapter) m_HoriAdapter->setControllerButtons(1, inputFrame.p2A, inputFrame.p2B, inputFrame.p2Select, inputFrame.p2Start, inputFrame.p2Up, inputFrame.p2Down, inputFrame.p2Left, inputFrame.p2Right);
        else if(m_portDevice2) m_portDevice2->setButtonsStatusExtended(inputFrame.p2A, inputFrame.p2B, inputFrame.p2Select, inputFrame.p2Start, inputFrame.p2Up, inputFrame.p2Down, inputFrame.p2Left, inputFrame.p2Right, inputFrame.p2X, inputFrame.p2Y, inputFrame.p2L, inputFrame.p2R);

        if(m_fourScore) m_fourScore->setControllerButtons(2, inputFrame.p3A, inputFrame.p3B, inputFrame.p3Select, inputFrame.p3Start, inputFrame.p3Up, inputFrame.p3Down, inputFrame.p3Left, inputFrame.p3Right);
        else if(m_HoriAdapter) m_HoriAdapter->setControllerButtons(2, inputFrame.p3A, inputFrame.p3B, inputFrame.p3Select, inputFrame.p3Start, inputFrame.p3Up, inputFrame.p3Down, inputFrame.p3Left, inputFrame.p3Right);
        else if(m_settings.getExpansionDevice() == Settings::ExpansionDevice::STANDARD_CONTROLLER_FAMICOM && m_expansionDevice) m_expansionDevice->setButtonsStatus(inputFrame.p3A, inputFrame.p3B, inputFrame.p3Select, inputFrame.p3Start, inputFrame.p3Up, inputFrame.p3Down, inputFrame.p3Left, inputFrame.p3Right);

        if(m_fourScore) m_fourScore->setControllerButtons(3, inputFrame.p4A, inputFrame.p4B, inputFrame.p4Select, inputFrame.p4Start, inputFrame.p4Up, inputFrame.p4Down, inputFrame.p4Left, inputFrame.p4Right);
        else if(m_HoriAdapter) m_HoriAdapter->setControllerButtons(3, inputFrame.p4A, inputFrame.p4B, inputFrame.p4Select, inputFrame.p4Start, inputFrame.p4Up, inputFrame.p4Down, inputFrame.p4Left, inputFrame.p4Right);

        processNsfControllerInput(inputFrame.p1Select, inputFrame.p1Start, inputFrame.p1Left, inputFrame.p1Right);

        if(m_settings.getPortDevice(Settings::Port::P_1) == std::optional<Settings::Device>(Settings::Device::ZAPPER) && m_portDevice1) {
            m_portDevice1->setCursorPosition(inputFrame.zapperP1X, inputFrame.zapperP1Y);
            m_portDevice1->setTrigger(inputFrame.zapperP1Trigger);
        }
        if(m_settings.getPortDevice(Settings::Port::P_2) == std::optional<Settings::Device>(Settings::Device::ZAPPER) && m_portDevice2) {
            m_portDevice2->setCursorPosition(inputFrame.zapperP2X, inputFrame.zapperP2Y);
            m_portDevice2->setTrigger(inputFrame.zapperP2Trigger);
        }

        if(m_settings.getExpansionDevice() == Settings::ExpansionDevice::BANDAI_HYPERSHOT && m_expansionDevice) {
            m_expansionDevice->setButtonsStatus(inputFrame.bandaiA, inputFrame.bandaiB, inputFrame.bandaiSelect, inputFrame.bandaiStart, inputFrame.bandaiUp, inputFrame.bandaiDown, inputFrame.bandaiLeft, inputFrame.bandaiRight);
            m_expansionDevice->setCursorPosition(inputFrame.bandaiX, inputFrame.bandaiY);
            m_expansionDevice->setTrigger(inputFrame.bandaiTrigger);
        }

        if(m_settings.getPortDevice(Settings::Port::P_1) == std::optional<Settings::Device>(Settings::Device::ARKANOID_CONTROLLER) && m_portDevice1) {
            m_portDevice1->setPositionNormalized(std::clamp(inputFrame.arkanoidP1Position, 0.0f, 1.0f));
            m_portDevice1->setTrigger(inputFrame.arkanoidP1Button);
        }
        if(m_settings.getPortDevice(Settings::Port::P_2) == std::optional<Settings::Device>(Settings::Device::ARKANOID_CONTROLLER) && m_portDevice2) {
            m_portDevice2->setPositionNormalized(std::clamp(inputFrame.arkanoidP2Position, 0.0f, 1.0f));
            m_portDevice2->setTrigger(inputFrame.arkanoidP2Button);
        }
        if(m_settings.getExpansionDevice() == Settings::ExpansionDevice::ARKANOID_CONTROLLER && m_expansionDevice) {
            m_expansionDevice->setPositionNormalized(std::clamp(inputFrame.arkanoidFamicomPosition, 0.0f, 1.0f));
            m_expansionDevice->setTrigger(inputFrame.arkanoidFamicomButton);
        }

        if(m_settings.getExpansionDevice() == Settings::ExpansionDevice::KONAMI_HYPERSHOT && m_expansionDevice) {
            m_expansionDevice->setPlayersButtons(inputFrame.konamiP1Run, inputFrame.konamiP1Jump, inputFrame.konamiP2Run, inputFrame.konamiP2Jump);
        }

        if(m_settings.getPortDevice(Settings::Port::P_1) == std::optional<Settings::Device>(Settings::Device::SNES_MOUSE) && m_portDevice1) {
            m_portDevice1->addRelativeMotion(inputFrame.snesMouseP1DeltaX, inputFrame.snesMouseP1DeltaY);
            m_portDevice1->setTrigger(inputFrame.snesMouseP1Left);
            m_portDevice1->setSecondaryTrigger(inputFrame.snesMouseP1Right);
        }
        if(m_settings.getPortDevice(Settings::Port::P_2) == std::optional<Settings::Device>(Settings::Device::SNES_MOUSE) && m_portDevice2) {
            m_portDevice2->addRelativeMotion(inputFrame.snesMouseP2DeltaX, inputFrame.snesMouseP2DeltaY);
            m_portDevice2->setTrigger(inputFrame.snesMouseP2Left);
            m_portDevice2->setSecondaryTrigger(inputFrame.snesMouseP2Right);
        }

        if(m_settings.getPortDevice(Settings::Port::P_1) == std::optional<Settings::Device>(Settings::Device::VIRTUAL_BOY_CONTROLLER) && m_portDevice1) {
            m_portDevice1->setVirtualBoyButtons(inputFrame.vbP1A, inputFrame.vbP1B, inputFrame.vbP1Select, inputFrame.vbP1Start, inputFrame.vbP1Up0, inputFrame.vbP1Down0, inputFrame.vbP1Left0, inputFrame.vbP1Right0, inputFrame.vbP1Up1, inputFrame.vbP1Down1, inputFrame.vbP1Left1, inputFrame.vbP1Right1, inputFrame.vbP1L, inputFrame.vbP1R);
        }
        if(m_settings.getPortDevice(Settings::Port::P_2) == std::optional<Settings::Device>(Settings::Device::VIRTUAL_BOY_CONTROLLER) && m_portDevice2) {
            m_portDevice2->setVirtualBoyButtons(inputFrame.vbP2A, inputFrame.vbP2B, inputFrame.vbP2Select, inputFrame.vbP2Start, inputFrame.vbP2Up0, inputFrame.vbP2Down0, inputFrame.vbP2Left0, inputFrame.vbP2Right0, inputFrame.vbP2Up1, inputFrame.vbP2Down1, inputFrame.vbP2Left1, inputFrame.vbP2Right1, inputFrame.vbP2L, inputFrame.vbP2R);
        }

        if(m_settings.getPortDevice(Settings::Port::P_1) == std::optional<Settings::Device>(Settings::Device::SUBOR_MOUSE) && m_portDevice1) {
            m_portDevice1->addRelativeMotion(inputFrame.suborMouseP1DeltaX, inputFrame.suborMouseP1DeltaY);
            m_portDevice1->setTrigger(inputFrame.suborMouseP1Left);
            m_portDevice1->setSecondaryTrigger(inputFrame.suborMouseP1Right);
        }
        if(m_settings.getPortDevice(Settings::Port::P_2) == std::optional<Settings::Device>(Settings::Device::SUBOR_MOUSE) && m_portDevice2) {
            m_portDevice2->addRelativeMotion(inputFrame.suborMouseP2DeltaX, inputFrame.suborMouseP2DeltaY);
            m_portDevice2->setTrigger(inputFrame.suborMouseP2Left);
            m_portDevice2->setSecondaryTrigger(inputFrame.suborMouseP2Right);
        }

        if(m_settings.getExpansionDevice() == Settings::ExpansionDevice::SUBOR_KEYBOARD && m_expansionDevice) m_expansionDevice->setSuborKeyboardKeys(inputFrame.suborKeyboardKeys);
        if(m_settings.getExpansionDevice() == Settings::ExpansionDevice::FAMILY_BASIC_KEYBOARD && m_expansionDevice) m_expansionDevice->setFamilyBasicKeyboardKeys(inputFrame.familyBasicKeyboardKeys);

        const auto p1Device = m_settings.getPortDevice(Settings::Port::P_1);
        const auto p2Device = m_settings.getPortDevice(Settings::Port::P_2);
        const bool familyTrainer =
            m_settings.getExpansionDevice() == Settings::ExpansionDevice::FAMILY_TRAINER_SIDE_A ||
            m_settings.getExpansionDevice() == Settings::ExpansionDevice::FAMILY_TRAINER_SIDE_B;
        if((p1Device == std::optional<Settings::Device>(Settings::Device::POWER_PAD_SIDE_A) || p1Device == std::optional<Settings::Device>(Settings::Device::POWER_PAD_SIDE_B)) && m_portDevice1) m_portDevice1->setPowerPadButtons(inputFrame.powerPadP1Buttons);
        if((p2Device == std::optional<Settings::Device>(Settings::Device::POWER_PAD_SIDE_A) || p2Device == std::optional<Settings::Device>(Settings::Device::POWER_PAD_SIDE_B)) && m_portDevice2) m_portDevice2->setPowerPadButtons(inputFrame.powerPadP2Buttons);
        if(familyTrainer && m_expansionDevice) m_expansionDevice->setPowerPadButtons(inputFrame.powerPadP1Buttons);
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

    void onFrameStart() {
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
        const uint32_t playbackFrame = m_frameCounter;
        if(const InputFrame* inputFrame = m_inputBuffer.findByFrame(playbackFrame); inputFrame != nullptr) {
            applyInputFrame(*inputFrame);
            m_lastAppliedInputFrame = *inputFrame;
        } else {
            InputFrame repeated = InputFrame::repeatedFrom(m_lastAppliedInputFrame, playbackFrame);
            applyInputFrame(repeated);
            m_lastAppliedInputFrame = repeated;
        }
    }    

    void onFrameReady() {
        ++m_frameCounter;        
        signalFrameReady();
    }

    void onPPUScanlineStart()
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

    void processDeferredNetplaySnapshot()
    {
        if(!m_netplaySnapshotFlag || !m_cartridge.isValid()) return;

        PendingNetplaySnapshot snapshot;
            snapshot.frame = m_frameCounter;
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

    void onPPUFrameStart()
    {
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
    m_rewind(*this),
    m_nsfPlayer(m_cartridge, m_apu, m_audioOutput, [this]() { this->reset(); }),
    m_console(m_cpu, m_ppu, m_apu, m_cartridge)
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

        m_frameCounter = 0;
        m_inputBuffer.clear();
        m_lastAppliedInputFrame = makeDefaultInputFrame(0);
        m_inputBuffer.push(m_lastAppliedInputFrame);

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

            m_inputBuffer.clear();
            m_lastAppliedInputFrame = makeDefaultInputFrame(0);
            m_inputBuffer.push(m_lastAppliedInputFrame);

            m_ppu.setVsPpuModel(m_cartridge.vsPpuModel());
            m_cartridge.reset();
            preloadNsfMemory();
            m_ppu.init();
            m_cpu.init();
            m_apu.init();
            m_nsfPlayer.onOpen();

            resetRewindSystem();

            queueInputFrame(makeDefaultInputFrame(0)); //first input frame empty
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
            const uint32_t playbackFrame = m_frameCounter;
            if(const InputFrame* inputFrame = m_inputBuffer.findByFrame(playbackFrame); inputFrame == nullptr) {
                return false;
            }

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
            
            if(m_frameStarted) {
                onFrameStart();
                m_frameStarted = false;
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
        snapshot.frame = m_frameCounter;
            snapshot.data = saveNetplayStateToMemory();
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

    std::vector<uint8_t> saveNetplayStateToMemory()
    {
        const InputBuffer savedInputBuffer = m_inputBuffer;
        m_inputBuffer.clear();
        m_inputBuffer.push(m_lastAppliedInputFrame);

        Serialize s;
        serialization(s);

        m_inputBuffer = savedInputBuffer;
        return s.getData();
    }

    uint32_t canonicalStateCrc32()
    {
        const std::vector<uint8_t> data = saveStateToMemory();
        if(data.empty()) return 0;
        return Crc32::calc(reinterpret_cast<const char*>(data.data()), data.size());
    }

    uint32_t canonicalNetplayStateCrc32()
    {
        const std::vector<uint8_t> data = saveNetplayStateToMemory();
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
        m_lastAppliedInputFrame.serialization(s);
        m_inputBuffer.serialization(s);
        SERIALIZEDATA(s, m_updateCyclesAcc);
        SERIALIZEDATA(s, m_cpuCyclesAcc);
        SERIALIZEDATA(s, m_cyclesPerSecond);
  
        SERIALIZEDATA(s, m_audioRenderCyclesAcc);

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

    void queueInputFrame(const InputFrame& inputFrame)
    {
        m_inputBuffer.push(inputFrame);
    }

    const InputBuffer& inputBuffer() const
    {
        return m_inputBuffer;
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
        return m_frameCounter;
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
        m_frameCounter = 0;
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




















