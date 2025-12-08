#pragma once

#include <string>

static std::string getMapperName(int num)
{
    switch(num)
    {
    case 0: return "NROM";
    case 1: return "Nintendo MMC1";
    case 2: return "UNROM switch";
    case 3: return "CNROM switch";
    case 4: return "Nintendo MMC3";
    case 5: return "Nintendo MMC5";
    case 6: return "FFE F4xxx";
    case 7: return "AOROM switch";
    case 8: return "FFE F3xxx";
    case 9: return "Nintendo MMC2";
    case 10: return "Nintendo MMC4";
    case 11: return "Color Dreams chip";
    case 12: return "FFE F6xxx";
    case 15: return "100-in-1 switch";
    case 16: return "Bandai chip";
    case 17: return "FFE F8xxx";
    case 18: return "Jalesco SS8806 chip";
    case 19: return "Namcot 106 chip";
    case 20: return "Nintendo DiskSystem";
    case 21: return "Konami VRC4a";
    case 22: return "Konami VRC2a";
    case 23: return "Konami VRC2a";
    case 24: return "Konami VRC6";
    case 25: return "Konami VRC4b";
    case 32: return "Irem G-101 chip";
    case 33: return "Taito TC0190/TC0350";
    case 34: return "32KB ROM switch";
    case 64: return "Tengen RAMBO-1 chip";
    case 65: return "Irem H-3001 chip";
    case 66: return "GNROM switch";
    case 67: return "Sunsoft3 chip";
    case 68: return "Sunsoft4 chip";
    case 69: return "Sunsoft5 FME-7 chip";
    case 71: return "Camerica chip";
    case 78: return "Irem 74HC161/32-based";
    case 90: return "Pirate HK-SF3 chip";
    case 118: return "TxSROM";
    case 119: return "TQROM";
    }

    return "Unknow";
}
