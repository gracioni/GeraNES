#ifndef SETTINGS_H
#define SETTINGS_H

#include "defines.h"
#include "Serialization.h"

class Settings {

public:
    enum Region { PAL, NTSC };

private:

    Region m_region = NTSC;
    int m_overclockLines = 0;    
    bool m_disableSpriteLimit = false;

public:

    void serialization(SerializationBase& s)
    {
        SERIALIZEDATA(s, m_region);
        SERIALIZEDATA(s,m_overclockLines);  
        SERIALIZEDATA(s,m_disableSpriteLimit);
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
        case NTSC: return 1789773;
        case PAL: return 1662607;
        }

        return 0;
    }

    GERANES_INLINE int PPULinesPerFrame()
    {
        if(m_region == PAL) return 312;
        return 262;
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


};

#endif // SETTINGS_H
