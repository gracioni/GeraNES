#pragma once

#include "defines.h"
#include "Serialization.h"
#include "util/map_util.h"
#include <optional>

namespace GeraNES {

class Settings {

public:
    enum class Region {
        PAL = 0,
        NTSC = 1,
        DENDY = 2
    };
    enum class Device {
        NONE = 0,
        CONTROLLER = 1,
        ZAPPER = 2,
        ARKANOID_CONTROLLER = 3,
        BANDAI_HYPERSHOT = 4,
        SNES_MOUSE = 5,
        SNES_CONTROLLER = 6,
        POWER_PAD_SIDE_A = 7,
        POWER_PAD_SIDE_B = 8,
        FAMICOM_CONTROLLER = 9,
        SUBOR_MOUSE = 10,
        VIRTUAL_BOY_CONTROLLER = 11
    };
    enum class ExpansionDevice {
        NONE = 0,
        STANDARD_CONTROLLER_FAMICOM = 1,
        BANDAI_HYPERSHOT = 2,
        KONAMI_HYPERSHOT = 3,
        ARKANOID_CONTROLLER = 4,
        FAMILY_TRAINER_SIDE_A = 5,
        FAMILY_TRAINER_SIDE_B = 6,
        SUBOR_KEYBOARD = 7,
        FAMILY_BASIC_KEYBOARD = 8
    };
    enum class NesMultitapDevice {
        NONE = 0,
        FOUR_SCORE = 1
    };
    enum class FamicomMultitapDevice {
        NONE = 0,
        HORI_ADAPTER = 1
    };
    enum class Port {
        P_1 = 0,
        P_2 = 1
    };

private:

    Region m_region = Region::NTSC;
    int m_overclockLines = 0;    
    bool m_disableSpriteLimit = false;

    std::map<Port, Device> m_portDevice = {
        {Port::P_1, Device::CONTROLLER},
        {Port::P_2, Device::CONTROLLER}
    };
    ExpansionDevice m_expansionDevice = ExpansionDevice::NONE;
    NesMultitapDevice m_nesMultitapDevice = NesMultitapDevice::NONE;
    FamicomMultitapDevice m_famicomMultitapDevice = FamicomMultitapDevice::NONE;

public:

    void serialization(SerializationBase& s)
    {
        SERIALIZEDATA(s, m_region);
        SERIALIZEDATA(s, m_overclockLines);  
        SERIALIZEDATA(s, m_disableSpriteLimit);
        serialize_map(s, m_portDevice);
        SERIALIZEDATA(s, m_expansionDevice);
        SERIALIZEDATA(s, m_nesMultitapDevice);
        SERIALIZEDATA(s, m_famicomMultitapDevice);
    }

    GERANES_INLINE void setRegion(Region r)
    {
        m_region = r;
    }

    GERANES_INLINE Region region() const
    {
        return m_region;
    }

    GERANES_INLINE int CPUClockHz() //Hz
    {
        switch(m_region) {
            case Region::NTSC: return 1789773;
            case Region::PAL: return 1662607;
            case Region::DENDY: return 1773448;
        }

        return 0;
    }

    GERANES_INLINE int PPULinesPerFrame()
    {
        switch(m_region) {

            case Region::NTSC:
                return 262;

            case Region::PAL:
                return 312;

            case Region::DENDY:
                return 312;

            default:
                return 262;
        }
    }

    GERANES_INLINE int overclockLines() const
    {
        return m_overclockLines;
    }

    GERANES_INLINE void setOverclockLines(int lines)
    {
        m_overclockLines = lines;
    }    

    GERANES_INLINE void disableSpriteLimit(bool flag)
    {
        m_disableSpriteLimit = flag;
    }

    GERANES_INLINE bool spriteLimitDisabled() const
    {
        return m_disableSpriteLimit;
    }

    GERANES_INLINE std::optional<Device> getPortDevice(Port port) const
    {
        auto it = m_portDevice.find(port);
        if (it != m_portDevice.end())
            return it->second;

        return std::nullopt;
    }

    GERANES_INLINE void setPortDevice(Port port, Device device)
    {
        if (auto it = m_portDevice.find(port); it != m_portDevice.end())
            it->second = device;
    }

    GERANES_INLINE ExpansionDevice getExpansionDevice() const
    {
        return m_expansionDevice;
    }

    GERANES_INLINE void setExpansionDevice(ExpansionDevice device)
    {
        m_expansionDevice = device;
    }

    GERANES_INLINE NesMultitapDevice getNesMultitapDevice() const
    {
        return m_nesMultitapDevice;
    }

    GERANES_INLINE void setNesMultitapDevice(NesMultitapDevice device)
    {
        m_nesMultitapDevice = device;
    }

    GERANES_INLINE FamicomMultitapDevice getFamicomMultitapDevice() const
    {
        return m_famicomMultitapDevice;
    }

    GERANES_INLINE void setFamicomMultitapDevice(FamicomMultitapDevice device)
    {
        m_famicomMultitapDevice = device;
    }
};

} // namespace GeraNES
