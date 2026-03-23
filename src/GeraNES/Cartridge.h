#pragma once

#include "defines.h"
#include "util/MapperUtil.h"
#include "NesCartridgeData/ICartridgeData.h"
#include "NesCartridgeData/_INesFormat.h"
#include "NesCartridgeData/_FdsFormat.h"
#ifdef ENABLE_NSF_PLAYER
#include "NesCartridgeData/_NsfFormat.h"
#endif
#include "NesCartridgeData/DbOverwriteCartridgeData.h"
#include "logger/logger.h"
#include "util/Crc32.h"

#include "Mappers/DummyMapper.h"

#include "Mappers/BaseMapper.h"
#include "Mappers/Mapper000.h"
#include "Mappers/Mapper001.h"
#include "Mappers/Mapper002.h"
#include "Mappers/Mapper003.h"
#include "Mappers/Mapper004.h"
#include "Mappers/Mapper004_3.h"
#include "Mappers/Mapper005.h"
#include "Mappers/Mapper006.h"
#include "Mappers/Mapper007.h"
#include "Mappers/Mapper008.h"
#include "Mappers/Mapper009.h"
#include "Mappers/Mapper010.h"
#include "Mappers/Mapper011.h"
#include "Mappers/Mapper012.h"
#include "Mappers/Mapper013.h"
#include "Mappers/Mapper014.h"
#include "Mappers/Mapper015.h"
#include "Mappers/Mapper016.h"
#include "Mappers/Mapper017.h"
#include "Mappers/Mapper018.h"
#include "Mappers/Mapper019.h"
#include "Mappers/Mapper020.h"
#include "Mappers/Mapper021.h"
#include "Mappers/Mapper022.h"
#include "Mappers/Mapper023.h"
#include "Mappers/Mapper024.h"
#include "Mappers/Mapper025.h"
#include "Mappers/Mapper026.h"
#include "Mappers/Mapper027.h"
#include "Mappers/Mapper028.h"
#include "Mappers/Mapper029.h"
#include "Mappers/Mapper030.h"
#include "Mappers/Mapper031.h"
#include "Mappers/Mapper032.h"
#include "Mappers/Mapper033.h"
#include "Mappers/Mapper034.h"
#include "Mappers/Mapper035.h"
#include "Mappers/Mapper036.h"
#include "Mappers/Mapper037.h"
#include "Mappers/Mapper038.h"
#include "Mappers/Mapper039.h"
#include "Mappers/Mapper040.h"
#include "Mappers/Mapper041.h"
#include "Mappers/Mapper042.h"
#include "Mappers/Mapper043.h"
#include "Mappers/Mapper044.h"
#include "Mappers/Mapper045.h"
#include "Mappers/Mapper046.h"
#include "Mappers/Mapper047.h"
#include "Mappers/Mapper048.h"
#include "Mappers/Mapper049.h"
#include "Mappers/Mapper050.h"
#include "Mappers/Mapper051.h"
#include "Mappers/Mapper052.h"
#include "Mappers/Mapper053.h"
#include "Mappers/Mapper054.h"
#include "Mappers/Mapper055.h"
#include "Mappers/Mapper056.h"
#include "Mappers/Mapper057.h"
#include "Mappers/Mapper058.h"
#include "Mappers/Mapper059.h"
#include "Mappers/Mapper060.h"
#include "Mappers/Mapper061.h"
#include "Mappers/Mapper062.h"
#include "Mappers/Mapper063.h"
#include "Mappers/Mapper064.h"
#include "Mappers/Mapper065.h"
#include "Mappers/Mapper066.h"
#include "Mappers/Mapper067.h"
#include "Mappers/Mapper068.h"
#include "Mappers/Mapper069.h"
#include "Mappers/Mapper070.h"
#include "Mappers/Mapper071.h"
#include "Mappers/Mapper072.h"
#include "Mappers/Mapper073.h"
#include "Mappers/Mapper074.h"
#include "Mappers/Mapper075.h"
#include "Mappers/Mapper076.h"
#include "Mappers/Mapper077.h"
#include "Mappers/Mapper078.h"
#include "Mappers/Mapper079.h"
#include "Mappers/Mapper080.h"
#include "Mappers/Mapper081.h"
#include "Mappers/Mapper082.h"
#include "Mappers/Mapper083.h"
#include "Mappers/Mapper084.h"
#include "Mappers/Mapper085.h"
#include "Mappers/Mapper086.h"
#include "Mappers/Mapper087.h"
#include "Mappers/Mapper088.h"
#include "Mappers/Mapper089.h"
#include "Mappers/Mapper090.h"
#include "Mappers/Mapper091.h"
#include "Mappers/Mapper092.h"
#include "Mappers/Mapper093.h"
#include "Mappers/Mapper094.h"
#include "Mappers/Mapper095.h"
#include "Mappers/Mapper096.h"
#include "Mappers/Mapper097.h"
#include "Mappers/Mapper098.h"
#include "Mappers/Mapper100.h"
#include "Mappers/Mapper109.h"
#include "Mappers/Mapper110.h"
#include "Mappers/Mapper099.h"
#include "Mappers/Mapper101.h"
#include "Mappers/Mapper102.h"
#include "Mappers/Mapper103.h"
#include "Mappers/Mapper104.h"
#include "Mappers/Mapper105.h"
#include "Mappers/Mapper106.h"
#include "Mappers/Mapper107.h"
#include "Mappers/Mapper108.h"
#include "Mappers/Mapper111.h"
#include "Mappers/Mapper112.h"
#include "Mappers/Mapper113.h"
#include "Mappers/Mapper114.h"
#include "Mappers/Mapper115.h"
#include "Mappers/Mapper116.h"
#include "Mappers/Mapper117.h"
#include "Mappers/Mapper118.h"
#include "Mappers/Mapper119.h"
#include "Mappers/Mapper120.h"
#include "Mappers/Mapper121.h"
#include "Mappers/Mapper122.h"
#include "Mappers/Mapper123.h"
#include "Mappers/Mapper124.h"
#include "Mappers/Mapper125.h"
#include "Mappers/Mapper126.h"
#include "Mappers/Mapper127.h"
#include "Mappers/Mapper128.h"
#include "Mappers/Mapper129.h"
#include "Mappers/Mapper130.h"
#include "Mappers/Mapper131.h"
#include "Mappers/Mapper132.h"
#include "Mappers/Mapper133.h"
#include "Mappers/Mapper134.h"
#include "Mappers/Mapper135.h"
#include "Mappers/Mapper136.h"
#include "Mappers/Mapper137.h"
#include "Mappers/Mapper138.h"
#include "Mappers/Mapper139.h"
#include "Mappers/Mapper140.h"
#include "Mappers/Mapper141.h"
#include "Mappers/Mapper142.h"
#include "Mappers/Mapper143.h"
#include "Mappers/Mapper144.h"
#include "Mappers/Mapper145.h"
#include "Mappers/Mapper146.h"
#include "Mappers/Mapper147.h"
#include "Mappers/Mapper148.h"
#include "Mappers/Mapper149.h"
#include "Mappers/Mapper150.h"
#include "Mappers/Mapper151.h"
#include "Mappers/Mapper152.h"
#include "Mappers/Mapper153.h"
#include "Mappers/Mapper154.h"
#include "Mappers/Mapper155.h"
#include "Mappers/Mapper156.h"
#include "Mappers/Mapper157.h"
#include "Mappers/Mapper158.h"
#include "Mappers/Mapper159.h"
#include "Mappers/Mapper160.h"
#include "Mappers/Mapper161.h"
#include "Mappers/Mapper162.h"
#include "Mappers/Mapper163.h"
#include "Mappers/Mapper164.h"
#include "Mappers/Mapper165.h"
#include "Mappers/Mapper166.h"
#include "Mappers/Mapper167.h"
#include "Mappers/Mapper168.h"
#include "Mappers/Mapper169.h"
#include "Mappers/Mapper170.h"
#include "Mappers/Mapper171.h"
#include "Mappers/Mapper172.h"
#include "Mappers/Mapper173.h"
#include "Mappers/Mapper174.h"
#include "Mappers/Mapper175.h"
#include "Mappers/Mapper176.h"
#include "Mappers/Mapper177.h"
#include "Mappers/Mapper178.h"
#include "Mappers/Mapper179.h"
#include "Mappers/Mapper180.h"
#include "Mappers/Mapper181.h"
#include "Mappers/Mapper182.h"
#include "Mappers/Mapper184.h"
#include "Mappers/Mapper185.h"
#include "Mappers/Mapper186.h"
#include "Mappers/Mapper187.h"
#include "Mappers/Mapper188.h"
#include "Mappers/Mapper189.h"
#include "Mappers/Mapper190.h"
#include "Mappers/Mapper191.h"
#include "Mappers/Mapper192.h"
#include "Mappers/Mapper193.h"
#include "Mappers/Mapper194.h"
#include "Mappers/Mapper195.h"
#include "Mappers/Mapper196.h"
#include "Mappers/Mapper197.h"
#include "Mappers/Mapper198.h"
#include "Mappers/Mapper199.h"
#include "Mappers/Mapper200.h"
#include "Mappers/Mapper201.h"
#include "Mappers/Mapper203.h"
#include "Mappers/Mapper204.h"
#include "Mappers/Mapper205.h"
#include "Mappers/Mapper206.h"
#include "Mappers/Mapper207.h"
#include "Mappers/Mapper208.h"
#include "Mappers/Mapper209.h"
#include "Mappers/Mapper210.h"
#include "Mappers/Mapper211.h"
#include "Mappers/Mapper212.h"
#include "Mappers/Mapper213.h"
#include "Mappers/Mapper214.h"
#include "Mappers/Mapper215.h"
#include "Mappers/Mapper216.h"
#include "Mappers/Mapper217.h"
#include "Mappers/Mapper218.h"
#include "Mappers/Mapper219.h"
#include "Mappers/Mapper220.h"
#include "Mappers/Mapper221.h"
#include "Mappers/Mapper222.h"
#include "Mappers/Mapper223.h"
#include "Mappers/Mapper224.h"
#include "Mappers/Mapper225.h"
#include "Mappers/Mapper226.h"
#include "Mappers/Mapper227.h"
#include "Mappers/Mapper228.h"
#include "Mappers/Mapper229.h"
#include "Mappers/Mapper230.h"
#include "Mappers/Mapper231.h"
#include "Mappers/Mapper232.h"
#include "Mappers/Mapper233.h"
#include "Mappers/Mapper234.h"
#include "Mappers/Mapper235.h"
#include "Mappers/Mapper236.h"
#include "Mappers/Mapper237.h"
#include "Mappers/Mapper238.h"
#include "Mappers/Mapper239.h"
#include "Mappers/Mapper240.h"
#include "Mappers/Mapper241.h"
#include "Mappers/Mapper242.h"
#include "Mappers/Mapper243.h"
#include "Mappers/Mapper244.h"
#include "Mappers/Mapper245.h"
#include "Mappers/Mapper246.h"
#include "Mappers/Mapper247.h"
#include "Mappers/Mapper248.h"
#include "Mappers/Mapper249.h"
#include "Mappers/Mapper250.h"
#include "Mappers/Mapper251.h"
#include "Mappers/Mapper252.h"
#include "Mappers/Mapper253.h"
#include "Mappers/Mapper254.h"
#include "Mappers/Mapper255.h"
#ifdef ENABLE_NSF_PLAYER
#include "Mappers/MapperNSF.h"
#endif

#include "RomFile.h"

#include "Serialization.h"

#include "GameDatabase.h"

class Cartridge
{
private:

    BaseMapper* m_mapper;
    ICartridgeData* m_nesCartridgeData;
    DummyMapper m_dummyMapper;

    bool m_isValid;

    RomFile m_romFile;

    BaseMapper* CreateMapper()
    {
        switch(m_nesCartridgeData->mapperId())
        {
        case 0: return BaseMapper::create<Mapper000>(*m_nesCartridgeData);
        case 1: return BaseMapper::create<Mapper001>(*m_nesCartridgeData);
        case 2: return BaseMapper::create<Mapper002>(*m_nesCartridgeData);
        case 3: return BaseMapper::create<Mapper003>(*m_nesCartridgeData);
        case 4: {
            if(m_nesCartridgeData->subMapperId() == 3) {
                return BaseMapper::create<Mapper004_3>(*m_nesCartridgeData);
            }
            return BaseMapper::create<Mapper004>(*m_nesCartridgeData);
        }
        case 5: return BaseMapper::create<Mapper005>(*m_nesCartridgeData);
        case 6: return BaseMapper::create<Mapper006>(*m_nesCartridgeData);
        case 7: return BaseMapper::create<Mapper007>(*m_nesCartridgeData);
        case 8: return BaseMapper::create<Mapper008>(*m_nesCartridgeData);
        case 9: return BaseMapper::create<Mapper009>(*m_nesCartridgeData);
        case 10: return BaseMapper::create<Mapper010>(*m_nesCartridgeData);
        case 11: return BaseMapper::create<Mapper011>(*m_nesCartridgeData);
        case 12: return BaseMapper::create<Mapper012>(*m_nesCartridgeData);
        case 13: return BaseMapper::create<Mapper013>(*m_nesCartridgeData);
        case 14: return BaseMapper::create<Mapper014>(*m_nesCartridgeData);
        case 15: return BaseMapper::create<Mapper015>(*m_nesCartridgeData);
        case 16: return BaseMapper::create<Mapper016>(*m_nesCartridgeData);
        case 17: return BaseMapper::create<Mapper017>(*m_nesCartridgeData);
        case 18: return BaseMapper::create<Mapper018>(*m_nesCartridgeData);
        case 19: return BaseMapper::create<Mapper019>(*m_nesCartridgeData);
        case 20: return BaseMapper::create<Mapper020>(*m_nesCartridgeData);
        case 21: return BaseMapper::create<Mapper021>(*m_nesCartridgeData);
        case 22: return BaseMapper::create<Mapper022>(*m_nesCartridgeData);
        case 23: return BaseMapper::create<Mapper023>(*m_nesCartridgeData);
        case 24: return BaseMapper::create<Mapper024>(*m_nesCartridgeData);
        case 25: return BaseMapper::create<Mapper025>(*m_nesCartridgeData);
        case 26: return BaseMapper::create<Mapper026>(*m_nesCartridgeData);
        case 27: return BaseMapper::create<Mapper027>(*m_nesCartridgeData);
        case 28: return BaseMapper::create<Mapper028>(*m_nesCartridgeData);
        case 29: return BaseMapper::create<Mapper029>(*m_nesCartridgeData);
        case 30: return BaseMapper::create<Mapper030>(*m_nesCartridgeData);
        case 31: return BaseMapper::create<Mapper031>(*m_nesCartridgeData);
        case 32: return BaseMapper::create<Mapper032>(*m_nesCartridgeData);
        case 33: return BaseMapper::create<Mapper033>(*m_nesCartridgeData);
        case 34: return BaseMapper::create<Mapper034>(*m_nesCartridgeData);
        case 35: return BaseMapper::create<Mapper035>(*m_nesCartridgeData);
        case 36: return BaseMapper::create<Mapper036>(*m_nesCartridgeData);
        case 37: return BaseMapper::create<Mapper037>(*m_nesCartridgeData);
        case 38: return BaseMapper::create<Mapper038>(*m_nesCartridgeData);
        case 39: return BaseMapper::create<Mapper039>(*m_nesCartridgeData);
        case 40: return BaseMapper::create<Mapper040>(*m_nesCartridgeData);
        case 41: return BaseMapper::create<Mapper041>(*m_nesCartridgeData);
        case 42: return BaseMapper::create<Mapper042>(*m_nesCartridgeData);
        case 43: return BaseMapper::create<Mapper043>(*m_nesCartridgeData);
        case 44: return BaseMapper::create<Mapper044>(*m_nesCartridgeData);
        case 45: return BaseMapper::create<Mapper045>(*m_nesCartridgeData);
        case 46: return BaseMapper::create<Mapper046>(*m_nesCartridgeData);
        case 47: return BaseMapper::create<Mapper047>(*m_nesCartridgeData);
        case 48: return BaseMapper::create<Mapper048>(*m_nesCartridgeData);
        case 49: return BaseMapper::create<Mapper049>(*m_nesCartridgeData);
        case 50: return BaseMapper::create<Mapper050>(*m_nesCartridgeData);
        case 51: return BaseMapper::create<Mapper051>(*m_nesCartridgeData);
        case 52: return BaseMapper::create<Mapper052>(*m_nesCartridgeData);
        case 53: return BaseMapper::create<Mapper053>(*m_nesCartridgeData);
        case 54: return BaseMapper::create<Mapper054>(*m_nesCartridgeData);
        case 55: return BaseMapper::create<Mapper055>(*m_nesCartridgeData);
        case 56: return BaseMapper::create<Mapper056>(*m_nesCartridgeData);
        case 57: return BaseMapper::create<Mapper057>(*m_nesCartridgeData);
        case 58: return BaseMapper::create<Mapper058>(*m_nesCartridgeData);
        case 59: return BaseMapper::create<Mapper059>(*m_nesCartridgeData);
        case 60: return BaseMapper::create<Mapper060>(*m_nesCartridgeData);
        case 61: return BaseMapper::create<Mapper061>(*m_nesCartridgeData);
        case 62: return BaseMapper::create<Mapper062>(*m_nesCartridgeData);
        case 63: return BaseMapper::create<Mapper063>(*m_nesCartridgeData);
        case 64: return BaseMapper::create<Mapper064>(*m_nesCartridgeData);
        case 65: return BaseMapper::create<Mapper065>(*m_nesCartridgeData);
        case 66: return BaseMapper::create<Mapper066>(*m_nesCartridgeData);
        case 67: return BaseMapper::create<Mapper067>(*m_nesCartridgeData);
        case 68: return BaseMapper::create<Mapper068>(*m_nesCartridgeData);
        case 69: return BaseMapper::create<Mapper069>(*m_nesCartridgeData);
        case 70: return BaseMapper::create<Mapper070>(*m_nesCartridgeData);
        case 71: return BaseMapper::create<Mapper071>(*m_nesCartridgeData);
        case 72: return BaseMapper::create<Mapper072>(*m_nesCartridgeData);
        case 73: return BaseMapper::create<Mapper073>(*m_nesCartridgeData);
        case 74: return BaseMapper::create<Mapper074>(*m_nesCartridgeData);
        case 75: return BaseMapper::create<Mapper075>(*m_nesCartridgeData);
        case 76: return BaseMapper::create<Mapper076>(*m_nesCartridgeData);
        case 77: return BaseMapper::create<Mapper077>(*m_nesCartridgeData);
        case 78: return BaseMapper::create<Mapper078>(*m_nesCartridgeData);
        case 79: return BaseMapper::create<Mapper079>(*m_nesCartridgeData);
        case 80: return BaseMapper::create<Mapper080>(*m_nesCartridgeData);
        case 81: return BaseMapper::create<Mapper081>(*m_nesCartridgeData);
        case 82: return BaseMapper::create<Mapper082>(*m_nesCartridgeData);
        case 83: return BaseMapper::create<Mapper083>(*m_nesCartridgeData);
        case 84: return BaseMapper::create<Mapper084>(*m_nesCartridgeData);
        case 85: return BaseMapper::create<Mapper085>(*m_nesCartridgeData);
        case 86: return BaseMapper::create<Mapper086>(*m_nesCartridgeData);
        case 87: return BaseMapper::create<Mapper087>(*m_nesCartridgeData);
        case 88: return BaseMapper::create<Mapper088>(*m_nesCartridgeData);
        case 89: return BaseMapper::create<Mapper089>(*m_nesCartridgeData);
        case 90: return BaseMapper::create<Mapper090>(*m_nesCartridgeData);
        case 91: return BaseMapper::create<Mapper091>(*m_nesCartridgeData);
        case 92: return BaseMapper::create<Mapper092>(*m_nesCartridgeData);
        case 93: return BaseMapper::create<Mapper093>(*m_nesCartridgeData);
        case 94: return BaseMapper::create<Mapper094>(*m_nesCartridgeData);
        case 95: return BaseMapper::create<Mapper095>(*m_nesCartridgeData);
        case 96: return BaseMapper::create<Mapper096>(*m_nesCartridgeData);
        case 97: return BaseMapper::create<Mapper097>(*m_nesCartridgeData);
        case 98: return BaseMapper::create<Mapper098>(*m_nesCartridgeData);
        case 99: return BaseMapper::create<Mapper099>(*m_nesCartridgeData);
        case 100: return BaseMapper::create<Mapper100>(*m_nesCartridgeData);
        case 101: return BaseMapper::create<Mapper101>(*m_nesCartridgeData);
        case 102: return BaseMapper::create<Mapper102>(*m_nesCartridgeData);
        case 103: return BaseMapper::create<Mapper103>(*m_nesCartridgeData);
        case 104: return BaseMapper::create<Mapper104>(*m_nesCartridgeData);
        case 105: return BaseMapper::create<Mapper105>(*m_nesCartridgeData);
        case 106: return BaseMapper::create<Mapper106>(*m_nesCartridgeData);
        case 107: return BaseMapper::create<Mapper107>(*m_nesCartridgeData);
        case 108: return BaseMapper::create<Mapper108>(*m_nesCartridgeData);
        case 109: return BaseMapper::create<Mapper109>(*m_nesCartridgeData);
        case 110: return BaseMapper::create<Mapper110>(*m_nesCartridgeData);
        case 111: return BaseMapper::create<Mapper111>(*m_nesCartridgeData);
        case 112: return BaseMapper::create<Mapper112>(*m_nesCartridgeData);
        case 113: return BaseMapper::create<Mapper113>(*m_nesCartridgeData);
        case 114: return BaseMapper::create<Mapper114>(*m_nesCartridgeData);
        case 115: return BaseMapper::create<Mapper115>(*m_nesCartridgeData);
        case 116: return BaseMapper::create<Mapper116>(*m_nesCartridgeData);
        case 117: return BaseMapper::create<Mapper117>(*m_nesCartridgeData);
        case 118: return BaseMapper::create<Mapper118>(*m_nesCartridgeData);
        case 119: return BaseMapper::create<Mapper119>(*m_nesCartridgeData);
        case 120: return BaseMapper::create<Mapper120>(*m_nesCartridgeData);
        case 121: return BaseMapper::create<Mapper121>(*m_nesCartridgeData);
        case 122: return BaseMapper::create<Mapper122>(*m_nesCartridgeData);
        case 123: return BaseMapper::create<Mapper123>(*m_nesCartridgeData);
        case 124: return BaseMapper::create<Mapper124>(*m_nesCartridgeData);
        case 125: return BaseMapper::create<Mapper125>(*m_nesCartridgeData);
        case 126: return BaseMapper::create<Mapper126>(*m_nesCartridgeData);
        case 127: return BaseMapper::create<Mapper127>(*m_nesCartridgeData);
        case 128: return BaseMapper::create<Mapper128>(*m_nesCartridgeData);
        case 129: return BaseMapper::create<Mapper129>(*m_nesCartridgeData);
        case 130: return BaseMapper::create<Mapper130>(*m_nesCartridgeData);
        case 131: return BaseMapper::create<Mapper131>(*m_nesCartridgeData);
        case 132: return BaseMapper::create<Mapper132>(*m_nesCartridgeData);
        case 133: return BaseMapper::create<Mapper133>(*m_nesCartridgeData);
        case 134: return BaseMapper::create<Mapper134>(*m_nesCartridgeData);
        case 135: return BaseMapper::create<Mapper135>(*m_nesCartridgeData);
        case 136: return BaseMapper::create<Mapper136>(*m_nesCartridgeData);
        case 137: return BaseMapper::create<Mapper137>(*m_nesCartridgeData);
        case 138: return BaseMapper::create<Mapper138>(*m_nesCartridgeData);
        case 139: return BaseMapper::create<Mapper139>(*m_nesCartridgeData);
        case 140: return BaseMapper::create<Mapper140>(*m_nesCartridgeData);
        case 141: return BaseMapper::create<Mapper141>(*m_nesCartridgeData);
        case 142: return BaseMapper::create<Mapper142>(*m_nesCartridgeData);
        case 143: return BaseMapper::create<Mapper143>(*m_nesCartridgeData);
        case 144: return BaseMapper::create<Mapper144>(*m_nesCartridgeData);
        case 145: return BaseMapper::create<Mapper145>(*m_nesCartridgeData);
        case 146: return BaseMapper::create<Mapper146>(*m_nesCartridgeData);
        case 147: return BaseMapper::create<Mapper147>(*m_nesCartridgeData);
        case 148: return BaseMapper::create<Mapper148>(*m_nesCartridgeData);
        case 149: return BaseMapper::create<Mapper149>(*m_nesCartridgeData);
        case 150: return BaseMapper::create<Mapper150>(*m_nesCartridgeData);
        case 151: return BaseMapper::create<Mapper151>(*m_nesCartridgeData);
        case 152: return BaseMapper::create<Mapper152>(*m_nesCartridgeData);
        case 153: return BaseMapper::create<Mapper153>(*m_nesCartridgeData);
        case 154: return BaseMapper::create<Mapper154>(*m_nesCartridgeData);
        case 155: return BaseMapper::create<Mapper155>(*m_nesCartridgeData);
        case 156: return BaseMapper::create<Mapper156>(*m_nesCartridgeData);
        case 157: return BaseMapper::create<Mapper157>(*m_nesCartridgeData);
        case 158: return BaseMapper::create<Mapper158>(*m_nesCartridgeData);
        case 159: return BaseMapper::create<Mapper159>(*m_nesCartridgeData);
        case 160: return BaseMapper::create<Mapper160>(*m_nesCartridgeData);
        case 161: return BaseMapper::create<Mapper161>(*m_nesCartridgeData);
        case 162: return BaseMapper::create<Mapper162>(*m_nesCartridgeData);
        case 163: return BaseMapper::create<Mapper163>(*m_nesCartridgeData);
        case 164: return BaseMapper::create<Mapper164>(*m_nesCartridgeData);
        case 165: return BaseMapper::create<Mapper165>(*m_nesCartridgeData);
        case 166: return BaseMapper::create<Mapper166>(*m_nesCartridgeData);
        case 167: return BaseMapper::create<Mapper167>(*m_nesCartridgeData);
        case 168: return BaseMapper::create<Mapper168>(*m_nesCartridgeData);
        case 169: return BaseMapper::create<Mapper169>(*m_nesCartridgeData);
        case 170: return BaseMapper::create<Mapper170>(*m_nesCartridgeData);
        case 171: return BaseMapper::create<Mapper171>(*m_nesCartridgeData);
        case 172: return BaseMapper::create<Mapper172>(*m_nesCartridgeData);
        case 173: return BaseMapper::create<Mapper173>(*m_nesCartridgeData);
        case 174: return BaseMapper::create<Mapper174>(*m_nesCartridgeData);
        case 175: return BaseMapper::create<Mapper175>(*m_nesCartridgeData);
        case 176: return BaseMapper::create<Mapper176>(*m_nesCartridgeData);
        case 177: return BaseMapper::create<Mapper177>(*m_nesCartridgeData);
        case 178: return BaseMapper::create<Mapper178>(*m_nesCartridgeData);
        case 179: return BaseMapper::create<Mapper179>(*m_nesCartridgeData);
        case 180: return BaseMapper::create<Mapper180>(*m_nesCartridgeData);
        case 181: return BaseMapper::create<Mapper181>(*m_nesCartridgeData);
        case 182: return BaseMapper::create<Mapper182>(*m_nesCartridgeData);
        case 184: return BaseMapper::create<Mapper184>(*m_nesCartridgeData);
        case 185: return BaseMapper::create<Mapper185>(*m_nesCartridgeData);
        case 186: return BaseMapper::create<Mapper186>(*m_nesCartridgeData);
        case 187: return BaseMapper::create<Mapper187>(*m_nesCartridgeData);
        case 188: return BaseMapper::create<Mapper188>(*m_nesCartridgeData);
        case 189: return BaseMapper::create<Mapper189>(*m_nesCartridgeData);
        case 190: return BaseMapper::create<Mapper190>(*m_nesCartridgeData);
        case 191: return BaseMapper::create<Mapper191>(*m_nesCartridgeData);
        case 192: return BaseMapper::create<Mapper192>(*m_nesCartridgeData);
        case 193: return BaseMapper::create<Mapper193>(*m_nesCartridgeData);
        case 194: return BaseMapper::create<Mapper194>(*m_nesCartridgeData);
        case 195: return BaseMapper::create<Mapper195>(*m_nesCartridgeData);
        case 196: return BaseMapper::create<Mapper196>(*m_nesCartridgeData);
        case 197: return BaseMapper::create<Mapper197>(*m_nesCartridgeData);
        case 198: return BaseMapper::create<Mapper198>(*m_nesCartridgeData);
        case 199: return BaseMapper::create<Mapper199>(*m_nesCartridgeData);
        case 200: return BaseMapper::create<Mapper200>(*m_nesCartridgeData);
        case 201: return BaseMapper::create<Mapper201>(*m_nesCartridgeData);
        case 203: return BaseMapper::create<Mapper203>(*m_nesCartridgeData);
        case 204: return BaseMapper::create<Mapper204>(*m_nesCartridgeData);
        case 205: return BaseMapper::create<Mapper205>(*m_nesCartridgeData);
        case 206: return BaseMapper::create<Mapper206>(*m_nesCartridgeData);
        case 207: return BaseMapper::create<Mapper207>(*m_nesCartridgeData);
        case 208: return BaseMapper::create<Mapper208>(*m_nesCartridgeData);
        case 209: return BaseMapper::create<Mapper209>(*m_nesCartridgeData);
        case 210: return BaseMapper::create<Mapper210>(*m_nesCartridgeData);
        case 211: return BaseMapper::create<Mapper211>(*m_nesCartridgeData);
        case 212: return BaseMapper::create<Mapper212>(*m_nesCartridgeData);
        case 213: return BaseMapper::create<Mapper213>(*m_nesCartridgeData);
        case 214: return BaseMapper::create<Mapper214>(*m_nesCartridgeData);
        case 215: return BaseMapper::create<Mapper215>(*m_nesCartridgeData);
        case 216: return BaseMapper::create<Mapper216>(*m_nesCartridgeData);
        case 217: return BaseMapper::create<Mapper217>(*m_nesCartridgeData);
        case 218: return BaseMapper::create<Mapper218>(*m_nesCartridgeData);
        case 219: return BaseMapper::create<Mapper219>(*m_nesCartridgeData);
        case 220: return BaseMapper::create<Mapper220>(*m_nesCartridgeData);
        case 221: return BaseMapper::create<Mapper221>(*m_nesCartridgeData);
        case 222: return BaseMapper::create<Mapper222>(*m_nesCartridgeData);
        case 223: return BaseMapper::create<Mapper223>(*m_nesCartridgeData);
        case 224: return BaseMapper::create<Mapper224>(*m_nesCartridgeData);
        case 225: return BaseMapper::create<Mapper225>(*m_nesCartridgeData);
        case 226: return BaseMapper::create<Mapper226>(*m_nesCartridgeData);
        case 227: return BaseMapper::create<Mapper227>(*m_nesCartridgeData);
        case 228: return BaseMapper::create<Mapper228>(*m_nesCartridgeData);
        case 229: return BaseMapper::create<Mapper229>(*m_nesCartridgeData);
        case 230: return BaseMapper::create<Mapper230>(*m_nesCartridgeData);
        case 231: return BaseMapper::create<Mapper231>(*m_nesCartridgeData);
        case 232: return BaseMapper::create<Mapper232>(*m_nesCartridgeData);
        case 233: return BaseMapper::create<Mapper233>(*m_nesCartridgeData);
        case 234: return BaseMapper::create<Mapper234>(*m_nesCartridgeData);
        case 235: return BaseMapper::create<Mapper235>(*m_nesCartridgeData);
        case 236: return BaseMapper::create<Mapper236>(*m_nesCartridgeData);
        case 237: return BaseMapper::create<Mapper237>(*m_nesCartridgeData);
        case 238: return BaseMapper::create<Mapper238>(*m_nesCartridgeData);
        case 239: return BaseMapper::create<Mapper239>(*m_nesCartridgeData);
        case 240: return BaseMapper::create<Mapper240>(*m_nesCartridgeData);
        case 241: return BaseMapper::create<Mapper241>(*m_nesCartridgeData);
        case 242: return BaseMapper::create<Mapper242>(*m_nesCartridgeData);
        case 243: return BaseMapper::create<Mapper243>(*m_nesCartridgeData);
        case 244: return BaseMapper::create<Mapper244>(*m_nesCartridgeData);
        case 245: return BaseMapper::create<Mapper245>(*m_nesCartridgeData);
        case 246: return BaseMapper::create<Mapper246>(*m_nesCartridgeData);
        case 247: return BaseMapper::create<Mapper247>(*m_nesCartridgeData);
        case 248: return BaseMapper::create<Mapper248>(*m_nesCartridgeData);
        case 249: return BaseMapper::create<Mapper249>(*m_nesCartridgeData);
        case 250: return BaseMapper::create<Mapper250>(*m_nesCartridgeData);
        case 251: return BaseMapper::create<Mapper251>(*m_nesCartridgeData);
        case 252: return BaseMapper::create<Mapper252>(*m_nesCartridgeData);
        case 253: return BaseMapper::create<Mapper253>(*m_nesCartridgeData);
        case 254: return BaseMapper::create<Mapper254>(*m_nesCartridgeData);
        case 255: return BaseMapper::create<Mapper255>(*m_nesCartridgeData);
#ifdef ENABLE_NSF_PLAYER
        case _NsfFormat::NSF_MAPPER_ID: return BaseMapper::create<MapperNSF>(*m_nesCartridgeData);
#endif

        }         

        return &m_dummyMapper;
    }    

public:

    Cartridge()
    {
        m_isValid = false;
        m_mapper = &m_dummyMapper;
        m_nesCartridgeData = NULL;
    }

    ~Cartridge()
    {
        if(m_mapper != &m_dummyMapper) delete m_mapper;
        if(m_nesCartridgeData != NULL) delete m_nesCartridgeData;
    }

    void clear()
    {
        if(m_mapper != &m_dummyMapper) delete m_mapper;
        if(m_nesCartridgeData != NULL) delete m_nesCartridgeData;

        m_romFile = RomFile();

        m_mapper = &m_dummyMapper;
        m_nesCartridgeData = NULL;

        m_isValid = false;        
    }

    bool open(const std::string& filename)
    {
        clear();

        m_romFile.open(filename);

        if(m_romFile.error() != "") {            
            Logger::instance().log(std::string("Error processing file '") + filename + "': " + m_romFile.error(), Logger::Type::ERROR);
            clear();
            return false;
        }

        // Try iNES first, then FDS, then NSF.
        _INesFormat* iNes = new _INesFormat(m_romFile);
        if(iNes->valid()) {
            m_nesCartridgeData = iNes;
        }
        else {
            const bool iNesSizeMismatch = (iNes->error() == "file length does not match header information");
            delete iNes;
            iNes = nullptr;

            _FdsFormat* fds = new _FdsFormat(m_romFile);
            if(fds->valid()) {
                m_nesCartridgeData = fds;
            }
            else {
                const std::string fdsError = fds->error();
                delete fds;
                fds = nullptr;

                if(fs::path(filename).extension() == ".fds") {
                    clear();
                    Logger::instance().log(
                        fdsError.empty() ? "Invalid FDS image" : fdsError,
                        Logger::Type::ERROR
                    );
                    return false;
                }

#ifdef ENABLE_NSF_PLAYER
                _NsfFormat* nsf = new _NsfFormat(m_romFile);
                if(nsf->valid()) {
                    m_nesCartridgeData = nsf;
                }
                else {
                    delete nsf;
                    nsf = nullptr;
                    clear();
                    if(iNesSizeMismatch) {
                        Logger::instance().log("ROM file size/header mismatch detected (iNES). Aborting load.", Logger::Type::ERROR);
                    }
                    else {
                        Logger::instance().log("Invalid ROM", Logger::Type::USER);
                    }
                    return false;
                }
#else
                clear();
                if(iNesSizeMismatch) {
                    Logger::instance().log("ROM file size/header mismatch detected (iNES). Aborting load.", Logger::Type::ERROR);
                }
                else {
                    Logger::instance().log("Invalid ROM", Logger::Type::USER);
                }
                return false;
#endif
            }
        }

        uint32_t prgCrc = m_nesCartridgeData->prgCrc32();
        uint32_t prgChrCrc = m_nesCartridgeData->prgChrCrc32();

        std::string prgCrcStr = Crc32::toString(prgCrc);
        std::string prgChrCrcStr = Crc32::toString(prgChrCrc);

        // NSF is not an iNES cartridge dump, so DB header overwrite doesn't apply.
        if(
#ifdef ENABLE_NSF_PLAYER
           dynamic_cast<_NsfFormat*>(m_nesCartridgeData) == nullptr &&
#endif
           dynamic_cast<_FdsFormat*>(m_nesCartridgeData) == nullptr) {
            GameDatabase::Item* item = GameDatabase::instance().findByCrc(prgChrCrcStr);

            if(item != nullptr) {
                Logger::instance().log("ROM found in database\nUsing DB header", Logger::Type::INFO);
                m_nesCartridgeData = new DbOverwriteCartridgeData(m_nesCartridgeData, item);
                m_nesCartridgeData->log("(DB)");
            }
            else {
                Logger::instance().log("ROM not found in database\nUsing default header", Logger::Type::INFO);
            }
        }
        else {
            if(dynamic_cast<_FdsFormat*>(m_nesCartridgeData) != nullptr) {
                Logger::instance().log("FDS file detected\nUsing FDS mapper", Logger::Type::INFO);
            }
#ifdef ENABLE_NSF_PLAYER
            else {
                Logger::instance().log("NSF file detected\nUsing NSF player mapper", Logger::Type::INFO);
            }
#endif
        }

        m_mapper = CreateMapper();

        if(m_mapper == &m_dummyMapper)
        {
            char num[64];

            sprintf(num, "%d", m_nesCartridgeData->mapperId());

            auto msg = std::string("Mapper not supported: (") + num + ")" +
                getMapperName(m_nesCartridgeData->mapperId());

            Logger::instance().log(msg, Logger::Type::ERROR);

            clear();

            return false;
        }

        m_isValid = true;

        return true;
    }

    void reset()
    {
        if(m_mapper != nullptr) {
            m_mapper->reset();
        }
    }

    static std::vector<std::string> listFiles(const fs::path& directory) {

        std::vector<std::string> filePaths;

        for (const auto& entry : fs::recursive_directory_iterator(directory)) {
            // Verifica se é um arquivo (não um diretório)
            if (fs::is_regular_file(entry.status())) {
                filePaths.push_back(entry.path().string());
            }
        }

        return filePaths;
    }

    GERANES_INLINE void writePrg(int addr, uint8_t data)
    {
        m_mapper->writePrg(addr,data);
    }

    GERANES_INLINE uint8_t readPrg(int addr)
    {
        return m_mapper->readPrg(addr);
    }

    GERANES_INLINE void writeSaveRam(int addr, uint8_t data)
    {
        m_mapper->writeSaveRam(addr,data);
    }

    GERANES_INLINE uint8_t readSaveRam(int addr)
    {
        return m_mapper->readSaveRam(addr);
    }

    GERANES_INLINE void writeChr(int addr, uint8_t data)
    {
        m_mapper->writeChr(addr,data);
    }

    GERANES_INLINE uint8_t readChr(int addr)
    {
        return m_mapper->readChr(addr);
    }

    GERANES_INLINE void writeMapperRegister(int addr, uint8_t data)
    {
        m_mapper->writeMapperRegister(addr,data);
    }

    GERANES_INLINE uint8_t readMapperRegister(int addr, uint8_t openBusData)
    {
        return m_mapper->readMapperRegister(addr,openBusData);
    }

    GERANES_INLINE void writeMapperRegisterAbsolute(uint16_t addr, uint8_t data)
    {
        m_mapper->writeMapperRegisterAbsolute(addr, data);
    }

    GERANES_INLINE uint8_t readMapperRegisterAbsolute(uint16_t addr, uint8_t openBusData)
    {
        return m_mapper->readMapperRegisterAbsolute(addr, openBusData);
    }

    GERANES_INLINE MirroringType getMirroringType()
    {
        return m_mapper->mirroringType();
    }

    //return nametable index with preperly mirroring
    GERANES_INLINE_HOT uint8_t mirroring(uint8_t blockIndex)
    {
        static const uint8_t HORIZONTAL_MIRROR[] = {0,0,1,1};
        static const uint8_t VERTICAL_MIRROR[] = {0,1,0,1};
        static const uint8_t FOUR_SCREEN_MIRROR[] = {0,1,2,3};

        switch(m_mapper->mirroringType()){

            case MirroringType::HORIZONTAL:
                return HORIZONTAL_MIRROR[blockIndex];
            case MirroringType::VERTICAL:
                return VERTICAL_MIRROR[blockIndex];
            case MirroringType::SINGLE_SCREEN_A:
                return 0;
            case MirroringType::SINGLE_SCREEN_B:
                return 1;
            case MirroringType::FOUR_SCREEN:
                return FOUR_SCREEN_MIRROR[blockIndex];
            default: //CUSTOM
                return m_mapper->customMirroring(blockIndex);
        }
    }    

    GERANES_INLINE bool getInterruptFlag()
    {
        return m_mapper->getInterruptFlag();
    }

    GERANES_INLINE void setA12State(bool state)
    {
        m_mapper->setA12State(state);
    }

    GERANES_INLINE void cycle()
    {    
        m_mapper->cycle();
    }    

    GERANES_INLINE_HOT bool useCustomNameTable(uint8_t index)
    {
        return m_mapper->useCustomNameTable(index);
    }

    GERANES_INLINE_HOT uint8_t readCustomNameTable(uint8_t index, uint16_t addr)
    {
        return m_mapper->readCustomNameTable(index,addr);
    }

    GERANES_INLINE_HOT void writeCustomNameTable(uint8_t index, uint16_t addr, uint8_t data)
    {
        m_mapper->writeCustomNameTable(index,addr,data);
    }

    GERANES_INLINE void onScanlineStart(bool renderingEnabled, int scanline)
    {
        m_mapper->onScanlineStart(renderingEnabled, scanline);
    }

    GERANES_INLINE void setPpuFetchSource(bool isSpriteFetch)
    {
        m_mapper->setPpuFetchSource(isSpriteFetch);
    }

    GERANES_INLINE uint8_t transformNameTableRead(uint8_t index, uint16_t addr, uint8_t value)
    {
        return m_mapper->transformNameTableRead(index, addr, value);
    }

    GERANES_INLINE void setSpriteSize8x16(bool sprite8x16)
    {
        m_mapper->setSpriteSize8x16(sprite8x16);
    }

    GERANES_INLINE void setPpuMask(uint8_t mask)
    {
        m_mapper->setPpuMask(mask);
    }

    GERANES_INLINE void onPpuStatusRead(bool vblankSet)
    {
        m_mapper->onPpuStatusRead(vblankSet);
    }

    GERANES_INLINE void onPpuRead(uint16_t addr)
    {
        m_mapper->onPpuRead(addr);
    }

    GERANES_INLINE void onPpuCycle(int scanline, int cycle, bool isRendering, bool isPreLine)
    {
        m_mapper->onPpuCycle(scanline, cycle, isRendering, isPreLine);
    }

    GERANES_INLINE void onCpuRead(uint16_t addr)
    {
        m_mapper->onCpuRead(addr);
    }

    GERANES_INLINE void onCpuWrite(uint16_t addr, uint8_t data)
    {
        m_mapper->onCpuWrite(addr, data);
    }

#ifdef ENABLE_NSF_PLAYER
    GERANES_INLINE bool consumeNsfPlayerInstructionRedirect(uint16_t& cpuAddr)
    {
        return m_mapper->consumeNsfPlayerInstructionRedirect(cpuAddr);
    }

    GERANES_INLINE void preloadNsfMemory(uint8_t* cpuRam, size_t cpuRamSize)
    {
        m_mapper->preloadNsfMemory(cpuRam, cpuRamSize);
    }
#endif

    GERANES_INLINE float getExpansionAudioSample()
    {
        return m_mapper->getExpansionAudioSample();
    }

    GERANES_INLINE float getMixWeight() const
    {
        return m_mapper->getMixWeight();
    }

    GERANES_INLINE float getExpansionOutputGain() const
    {
        return m_mapper->getExpansionOutputGain();
    }

    GERANES_INLINE std::string getMapperAudioChannelsJson() const
    {
        return m_mapper->getAudioChannelsJson();
    }

    GERANES_INLINE bool setMapperAudioChannelVolumeById(const std::string& id, float volume)
    {
        return m_mapper->setAudioChannelVolumeById(id, volume);
    }

    GERANES_INLINE void applyExternalActions(uint8_t pending)
    {
        m_mapper->applyExternalActions(pending);
    }

    GERANES_INLINE GameDatabase::System system()
    {
        return m_nesCartridgeData->sistem();
    }

    GERANES_INLINE GameDatabase::InputType inputType()
    {
        return m_nesCartridgeData->inputType();
    }

    GERANES_INLINE GameDatabase::PpuModel vsPpuModel()
    {
        return m_nesCartridgeData->vsPpuModel();
    }

    GERANES_INLINE bool isValid()
    {
        return m_isValid;
    }

    GERANES_INLINE bool hasCartridgeData() const
    {
        return m_nesCartridgeData != NULL;
    }

    GERANES_INLINE int mapperId() const
    {
        if(m_nesCartridgeData == NULL) return -1;
        return m_nesCartridgeData->mapperId();
    }

    GERANES_INLINE int subMapperId() const
    {
        if(m_nesCartridgeData == NULL) return -1;
        return m_nesCartridgeData->subMapperId();
    }

    GERANES_INLINE int prgSize() const
    {
        if(m_nesCartridgeData == NULL) return -1;
        return m_nesCartridgeData->prgSize();
    }

    GERANES_INLINE int chrSize() const
    {
        if(m_nesCartridgeData == NULL) return -1;
        return m_nesCartridgeData->chrSize();
    }

    GERANES_INLINE int chrRamSize() const
    {
        if(m_nesCartridgeData == NULL) return -1;
        return m_nesCartridgeData->chrRamSize();
    }

    GERANES_INLINE int ramSize() const
    {
        if(m_nesCartridgeData == NULL) return -1;
        return m_nesCartridgeData->ramSize();
    }

    GERANES_INLINE int dbSaveRamSize() const
    {
        if(m_nesCartridgeData == NULL) return -1;
        return m_nesCartridgeData->saveRamSize();
    }

    GERANES_INLINE bool hasBattery() const
    {
        if(m_nesCartridgeData == NULL) return false;
        return m_nesCartridgeData->hasBattery();
    }

    GERANES_INLINE std::string chip() const
    {
        if(m_nesCartridgeData == NULL) return "";
        return m_nesCartridgeData->chip();
    }

    GERANES_INLINE std::string prgChrCrc32String() const
    {
        if(m_nesCartridgeData == NULL) return "";
        return Crc32::toString(m_nesCartridgeData->prgChrCrc32());
    }

    GERANES_INLINE uint8_t* saveRamData()
    {
        return m_mapper->saveRamData();
    }

    GERANES_INLINE size_t saveRamSize() const
    {
        return m_mapper->saveRamSize();
    }

    GERANES_INLINE bool hasBatterySaveRam() const
    {
        return m_mapper->hasBatterySaveRam();
    }

    GERANES_INLINE const RomFile& romFile() {
        return m_romFile;
    }

    GERANES_INLINE bool isNsf() const
    {
#ifdef ENABLE_NSF_PLAYER
        return m_nesCartridgeData != NULL && m_nesCartridgeData->mapperId() == _NsfFormat::NSF_MAPPER_ID;
#else
        return false;
#endif
    }

    GERANES_INLINE int nsfTotalSongs() const
    {
#ifdef ENABLE_NSF_PLAYER
        if(!isNsf()) return 0;
        const auto* mapper = dynamic_cast<const MapperNSF*>(m_mapper);
        return mapper != nullptr ? mapper->totalSongs() : 0;
#else
        return 0;
#endif
    }

    GERANES_INLINE int nsfCurrentSong() const
    {
#ifdef ENABLE_NSF_PLAYER
        if(!isNsf()) return 0;
        const auto* mapper = dynamic_cast<const MapperNSF*>(m_mapper);
        return mapper != nullptr ? mapper->currentSong() : 0;
#else
        return 0;
#endif
    }

    GERANES_INLINE bool nsfIsPlaying() const
    {
#ifdef ENABLE_NSF_PLAYER
        if(!isNsf()) return false;
        const auto* mapper = dynamic_cast<const MapperNSF*>(m_mapper);
        return mapper != nullptr ? mapper->isPlaying() : false;
#else
        return false;
#endif
    }

    GERANES_INLINE bool nsfSetPlaying(bool playing)
    {
#ifdef ENABLE_NSF_PLAYER
        if(!isNsf()) return false;
        auto* mapper = dynamic_cast<MapperNSF*>(m_mapper);
        if(mapper == nullptr) return false;
        mapper->setPlaying(playing);
        return true;
#else
        (void)playing;
        return false;
#endif
    }

    GERANES_INLINE bool nsfSetSong(int song1Based)
    {
#ifdef ENABLE_NSF_PLAYER
        if(!isNsf()) return false;
        auto* mapper = dynamic_cast<MapperNSF*>(m_mapper);
        if(mapper == nullptr) return false;
        mapper->setSong(song1Based);
        return true;
#else
        (void)song1Based;
        return false;
#endif
    }

    GERANES_INLINE bool nsfNextSong()
    {
#ifdef ENABLE_NSF_PLAYER
        if(!isNsf()) return false;
        auto* mapper = dynamic_cast<MapperNSF*>(m_mapper);
        if(mapper == nullptr) return false;
        mapper->nextSong();
        return true;
#else
        return false;
#endif
    }

    GERANES_INLINE bool nsfPrevSong()
    {
#ifdef ENABLE_NSF_PLAYER
        if(!isNsf()) return false;
        auto* mapper = dynamic_cast<MapperNSF*>(m_mapper);
        if(mapper == nullptr) return false;
        mapper->prevSong();
        return true;
#else
        return false;
#endif
    }

    GERANES_INLINE bool nsfRequestSongInit()
    {
#ifdef ENABLE_NSF_PLAYER
        if(!isNsf()) return false;
        auto* mapper = dynamic_cast<MapperNSF*>(m_mapper);
        if(mapper == nullptr) return false;
        mapper->requestSongInit();
        return true;
#else
        return false;
#endif
    }

    GERANES_INLINE bool nsfSongInitPending()
    {
#ifdef ENABLE_NSF_PLAYER
        if(!isNsf()) return false;
        auto* mapper = dynamic_cast<MapperNSF*>(m_mapper);
        if(mapper == nullptr) return false;
        return mapper->songInitPending();
#else
        return false;
#endif
    }

    void serialization(SerializationBase& s)
    {
        SERIALIZEDATA(s, m_isValid);
        m_mapper->serialization(s);
    }

};




