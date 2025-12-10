#pragma once

#include "defines.h"
#include "Serialization.h"
#include "util/map_util.h"

class Settings {

public:
    enum class Region { PAL, NTSC, DENDY };
    enum class Device { CONTROLLER, ZAPPER };
    enum class Port { P_1, P_2 };

private:

    Region m_region = Region::NTSC;
    int m_overclockLines = 0;    
    bool m_disableSpriteLimit = false;

    std::map<Port, Device> m_portDevice = {
        {Port::P_1, Device::CONTROLLER},
        {Port::P_2, Device::CONTROLLER}
    };

public:

    void serialization(SerializationBase& s)
    {
        SERIALIZEDATA(s, m_region);
        SERIALIZEDATA(s, m_overclockLines);  
        SERIALIZEDATA(s, m_disableSpriteLimit);
        serialize_map(s, m_portDevice);
    }

    GERANES_INLINE void setRegion(Region r)
    {
        m_region = r;
    }

    GERANES_INLINE Region region()
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
                return 59;

            default:
                return 262;
        }
    }

    GERANES_INLINE int overclockLines()
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

    GERANES_INLINE bool spriteLimitDisabled()
    {
        return m_disableSpriteLimit;
    }

    GERANES_INLINE std::optional<Device> getPortDevice(Port port)
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
};
