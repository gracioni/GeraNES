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
    using MapperWritePrgFn = void (*)(BaseMapper*, int, uint8_t);
    using MapperReadPrgFn = uint8_t (*)(BaseMapper*, int);
    using MapperWriteSaveRamFn = void (*)(BaseMapper*, int, uint8_t);
    using MapperReadSaveRamFn = uint8_t (*)(BaseMapper*, int);
    using MapperWriteChrFn = void (*)(BaseMapper*, int, uint8_t);
    using MapperReadChrFn = uint8_t (*)(BaseMapper*, int);
    using MapperWriteMapperRegisterFn = void (*)(BaseMapper*, int, uint8_t);
    using MapperReadMapperRegisterFn = uint8_t (*)(BaseMapper*, int, uint8_t);
    using MapperWriteMapperRegisterAbsoluteFn = void (*)(BaseMapper*, uint16_t, uint8_t);
    using MapperReadMapperRegisterAbsoluteFn = uint8_t (*)(BaseMapper*, uint16_t, uint8_t);
    using MapperMirroringTypeFn = MirroringType (*)(BaseMapper*);
    using MapperGetInterruptFlagFn = bool (*)(BaseMapper*);
    using MapperSetA12StateFn = void (*)(BaseMapper*, bool);
    using MapperCycleFn = void (*)(BaseMapper*);
    using MapperUseCustomNameTableFn = bool (*)(BaseMapper*, uint8_t);
    using MapperReadCustomNameTableFn = uint8_t (*)(BaseMapper*, uint8_t, uint16_t);
    using MapperWriteCustomNameTableFn = void (*)(BaseMapper*, uint8_t, uint16_t, uint8_t);
    using MapperOnScanlineStartFn = void (*)(BaseMapper*, bool, int);
    using MapperSetPpuFetchSourceFn = void (*)(BaseMapper*, bool);
    using MapperTransformNameTableReadFn = uint8_t (*)(BaseMapper*, uint8_t, uint16_t, uint8_t);
    using MapperSetSpriteSize8x16Fn = void (*)(BaseMapper*, bool);
    using MapperSetPpuMaskFn = void (*)(BaseMapper*, uint8_t);
    using MapperOnPpuStatusReadFn = void (*)(BaseMapper*, bool);
    using MapperOnPpuReadFn = void (*)(BaseMapper*, uint16_t);
    using MapperOnPpuCycleFn = void (*)(BaseMapper*, int, int, bool, bool);
    using MapperOnCpuReadFn = void (*)(BaseMapper*, uint16_t);
    using MapperOnCpuWriteFn = void (*)(BaseMapper*, uint16_t, uint8_t);
    using MapperApplyExternalActionsFn = void (*)(BaseMapper*, uint8_t);
    using MapperGetExpansionAudioSampleFn = float (*)(BaseMapper*);
    using MapperGetMixWeightFn = float (*)(BaseMapper*);
    using MapperGetExpansionOutputGainFn = float (*)(BaseMapper*);

    template<typename MapperT>
    static void fastWritePrg(BaseMapper* mapper, int addr, uint8_t data) { static_cast<MapperT*>(mapper)->MapperT::writePrg(addr, data); }
    template<typename MapperT>
    static uint8_t fastReadPrg(BaseMapper* mapper, int addr) { return static_cast<MapperT*>(mapper)->MapperT::readPrg(addr); }
    template<typename MapperT>
    static void fastWriteSaveRam(BaseMapper* mapper, int addr, uint8_t data) { static_cast<MapperT*>(mapper)->MapperT::writeSaveRam(addr, data); }
    template<typename MapperT>
    static uint8_t fastReadSaveRam(BaseMapper* mapper, int addr) { return static_cast<MapperT*>(mapper)->MapperT::readSaveRam(addr); }
    template<typename MapperT>
    static void fastWriteChr(BaseMapper* mapper, int addr, uint8_t data) { static_cast<MapperT*>(mapper)->MapperT::writeChr(addr, data); }
    template<typename MapperT>
    static uint8_t fastReadChr(BaseMapper* mapper, int addr) { return static_cast<MapperT*>(mapper)->MapperT::readChr(addr); }
    template<typename MapperT>
    static void fastWriteMapperRegister(BaseMapper* mapper, int addr, uint8_t data) { static_cast<MapperT*>(mapper)->MapperT::writeMapperRegister(addr, data); }
    template<typename MapperT>
    static uint8_t fastReadMapperRegister(BaseMapper* mapper, int addr, uint8_t openBusData) { return static_cast<MapperT*>(mapper)->MapperT::readMapperRegister(addr, openBusData); }
    template<typename MapperT>
    static void fastWriteMapperRegisterAbsolute(BaseMapper* mapper, uint16_t addr, uint8_t data) { static_cast<MapperT*>(mapper)->MapperT::writeMapperRegisterAbsolute(addr, data); }
    template<typename MapperT>
    static uint8_t fastReadMapperRegisterAbsolute(BaseMapper* mapper, uint16_t addr, uint8_t openBusData) { return static_cast<MapperT*>(mapper)->MapperT::readMapperRegisterAbsolute(addr, openBusData); }
    template<typename MapperT>
    static MirroringType fastMirroringType(BaseMapper* mapper) { return static_cast<MapperT*>(mapper)->MapperT::mirroringType(); }
    template<typename MapperT>
    static bool fastGetInterruptFlag(BaseMapper* mapper) { return static_cast<MapperT*>(mapper)->MapperT::getInterruptFlag(); }
    template<typename MapperT>
    static void fastSetA12State(BaseMapper* mapper, bool state) { static_cast<MapperT*>(mapper)->MapperT::setA12State(state); }
    template<typename MapperT>
    static void fastCycle(BaseMapper* mapper) { static_cast<MapperT*>(mapper)->MapperT::cycle(); }
    template<typename MapperT>
    static bool fastUseCustomNameTable(BaseMapper* mapper, uint8_t index) { return static_cast<MapperT*>(mapper)->MapperT::useCustomNameTable(index); }
    template<typename MapperT>
    static uint8_t fastReadCustomNameTable(BaseMapper* mapper, uint8_t index, uint16_t addr) { return static_cast<MapperT*>(mapper)->MapperT::readCustomNameTable(index, addr); }
    template<typename MapperT>
    static void fastWriteCustomNameTable(BaseMapper* mapper, uint8_t index, uint16_t addr, uint8_t data) { static_cast<MapperT*>(mapper)->MapperT::writeCustomNameTable(index, addr, data); }
    template<typename MapperT>
    static void fastOnScanlineStart(BaseMapper* mapper, bool renderingEnabled, int scanline) { static_cast<MapperT*>(mapper)->MapperT::onScanlineStart(renderingEnabled, scanline); }
    template<typename MapperT>
    static void fastSetPpuFetchSource(BaseMapper* mapper, bool isSpriteFetch) { static_cast<MapperT*>(mapper)->MapperT::setPpuFetchSource(isSpriteFetch); }
    template<typename MapperT>
    static uint8_t fastTransformNameTableRead(BaseMapper* mapper, uint8_t index, uint16_t addr, uint8_t value) { return static_cast<MapperT*>(mapper)->MapperT::transformNameTableRead(index, addr, value); }
    template<typename MapperT>
    static void fastSetSpriteSize8x16(BaseMapper* mapper, bool sprite8x16) { static_cast<MapperT*>(mapper)->MapperT::setSpriteSize8x16(sprite8x16); }
    template<typename MapperT>
    static void fastSetPpuMask(BaseMapper* mapper, uint8_t mask) { static_cast<MapperT*>(mapper)->MapperT::setPpuMask(mask); }
    template<typename MapperT>
    static void fastOnPpuStatusRead(BaseMapper* mapper, bool vblankSet) { static_cast<MapperT*>(mapper)->MapperT::onPpuStatusRead(vblankSet); }
    template<typename MapperT>
    static void fastOnPpuRead(BaseMapper* mapper, uint16_t addr) { static_cast<MapperT*>(mapper)->MapperT::onPpuRead(addr); }
    template<typename MapperT>
    static void fastOnPpuCycle(BaseMapper* mapper, int scanline, int cycle, bool isRendering, bool isPreLine) { static_cast<MapperT*>(mapper)->MapperT::onPpuCycle(scanline, cycle, isRendering, isPreLine); }
    template<typename MapperT>
    static void fastOnCpuRead(BaseMapper* mapper, uint16_t addr) { static_cast<MapperT*>(mapper)->MapperT::onCpuRead(addr); }
    template<typename MapperT>
    static void fastOnCpuWrite(BaseMapper* mapper, uint16_t addr, uint8_t data) { static_cast<MapperT*>(mapper)->MapperT::onCpuWrite(addr, data); }
    template<typename MapperT>
    static void fastApplyExternalActions(BaseMapper* mapper, uint8_t pending) { static_cast<MapperT*>(mapper)->MapperT::applyExternalActions(pending); }
    template<typename MapperT>
    static float fastGetExpansionAudioSample(BaseMapper* mapper) { return static_cast<MapperT*>(mapper)->MapperT::getExpansionAudioSample(); }
    template<typename MapperT>
    static float fastGetMixWeight(BaseMapper* mapper) { return static_cast<MapperT*>(mapper)->MapperT::getMixWeight(); }
    template<typename MapperT>
    static float fastGetExpansionOutputGain(BaseMapper* mapper) { return static_cast<MapperT*>(mapper)->MapperT::getExpansionOutputGain(); }

    BaseMapper* m_mapper;
    ICartridgeData* m_nesCartridgeData;
    DummyMapper m_dummyMapper;
    MapperWritePrgFn m_writePrgFn;
    MapperReadPrgFn m_readPrgFn;
    MapperWriteSaveRamFn m_writeSaveRamFn;
    MapperReadSaveRamFn m_readSaveRamFn;
    MapperWriteChrFn m_writeChrFn;
    MapperReadChrFn m_readChrFn;
    MapperWriteMapperRegisterFn m_writeMapperRegisterFn;
    MapperReadMapperRegisterFn m_readMapperRegisterFn;
    MapperWriteMapperRegisterAbsoluteFn m_writeMapperRegisterAbsoluteFn;
    MapperReadMapperRegisterAbsoluteFn m_readMapperRegisterAbsoluteFn;
    MapperMirroringTypeFn m_mirroringTypeFn;
    MapperGetInterruptFlagFn m_getInterruptFlagFn;
    MapperSetA12StateFn m_setA12StateFn;
    MapperCycleFn m_cycleFn;
    MapperUseCustomNameTableFn m_useCustomNameTableFn;
    MapperReadCustomNameTableFn m_readCustomNameTableFn;
    MapperWriteCustomNameTableFn m_writeCustomNameTableFn;
    MapperOnScanlineStartFn m_onScanlineStartFn;
    MapperSetPpuFetchSourceFn m_setPpuFetchSourceFn;
    MapperTransformNameTableReadFn m_transformNameTableReadFn;
    MapperSetSpriteSize8x16Fn m_setSpriteSize8x16Fn;
    MapperSetPpuMaskFn m_setPpuMaskFn;
    MapperOnPpuStatusReadFn m_onPpuStatusReadFn;
    MapperOnPpuReadFn m_onPpuReadFn;
    MapperOnPpuCycleFn m_onPpuCycleFn;
    MapperOnCpuReadFn m_onCpuReadFn;
    MapperOnCpuWriteFn m_onCpuWriteFn;
    MapperApplyExternalActionsFn m_applyExternalActionsFn;
    MapperGetExpansionAudioSampleFn m_getExpansionAudioSampleFn;
    MapperGetMixWeightFn m_getMixWeightFn;
    MapperGetExpansionOutputGainFn m_getExpansionOutputGainFn;
    uint32_t m_mapperHookCaps;

    bool m_isValid;

    RomFile m_romFile;

    template<typename MapperT>
    void assignMapperDispatch()
    {
        m_mapperHookCaps = MapperT::kMapperHookCaps;
        m_writePrgFn = &fastWritePrg<MapperT>;
        m_readPrgFn = &fastReadPrg<MapperT>;
        m_writeSaveRamFn = &fastWriteSaveRam<MapperT>;
        m_readSaveRamFn = &fastReadSaveRam<MapperT>;
        m_writeChrFn = &fastWriteChr<MapperT>;
        m_readChrFn = &fastReadChr<MapperT>;
        m_writeMapperRegisterFn = &fastWriteMapperRegister<MapperT>;
        m_readMapperRegisterFn = &fastReadMapperRegister<MapperT>;
        m_writeMapperRegisterAbsoluteFn = &fastWriteMapperRegisterAbsolute<MapperT>;
        m_readMapperRegisterAbsoluteFn = &fastReadMapperRegisterAbsolute<MapperT>;
        m_mirroringTypeFn = &fastMirroringType<MapperT>;
        m_getInterruptFlagFn = &fastGetInterruptFlag<MapperT>;
        m_setA12StateFn = &fastSetA12State<MapperT>;
        m_cycleFn = &fastCycle<MapperT>;
        m_useCustomNameTableFn = &fastUseCustomNameTable<MapperT>;
        m_readCustomNameTableFn = &fastReadCustomNameTable<MapperT>;
        m_writeCustomNameTableFn = &fastWriteCustomNameTable<MapperT>;
        m_onScanlineStartFn = &fastOnScanlineStart<MapperT>;
        m_setPpuFetchSourceFn = &fastSetPpuFetchSource<MapperT>;
        m_transformNameTableReadFn = &fastTransformNameTableRead<MapperT>;
        m_setSpriteSize8x16Fn = &fastSetSpriteSize8x16<MapperT>;
        m_setPpuMaskFn = &fastSetPpuMask<MapperT>;
        m_onPpuStatusReadFn = &fastOnPpuStatusRead<MapperT>;
        m_onPpuReadFn = &fastOnPpuRead<MapperT>;
        m_onPpuCycleFn = &fastOnPpuCycle<MapperT>;
        m_onCpuReadFn = &fastOnCpuRead<MapperT>;
        m_onCpuWriteFn = &fastOnCpuWrite<MapperT>;
        m_applyExternalActionsFn = &fastApplyExternalActions<MapperT>;
        m_getExpansionAudioSampleFn = &fastGetExpansionAudioSample<MapperT>;
        m_getMixWeightFn = &fastGetMixWeight<MapperT>;
        m_getExpansionOutputGainFn = &fastGetExpansionOutputGain<MapperT>;
    }

    GERANES_INLINE bool hasMapperHookCap(uint32_t cap) const
    {
        return (m_mapperHookCaps & cap) != 0;
    }

    void assignDefaultMapperDispatch()
    {
        assignMapperDispatch<BaseMapper>();
    }

    template<typename MapperT>
    BaseMapper* createMapperAndBind(ICartridgeData& cd)
    {
        assignMapperDispatch<MapperT>();
        return BaseMapper::create<MapperT>(cd);
    }

    void configureMapperDispatch()
    {
        if(m_nesCartridgeData == nullptr || m_mapper == &m_dummyMapper) {
            assignDefaultMapperDispatch();
            return;
        }

        switch(m_nesCartridgeData->mapperId()) {
            case 0: assignMapperDispatch<Mapper000>(); return;
            case 1: assignMapperDispatch<Mapper001>(); return;
            case 2: assignMapperDispatch<Mapper002>(); return;
            case 3: assignMapperDispatch<Mapper003>(); return;
            case 4:
                if(m_nesCartridgeData->subMapperId() == 3) assignMapperDispatch<Mapper004_3>();
                else assignMapperDispatch<Mapper004>();
                return;
            default:
                assignDefaultMapperDispatch();
                return;
        }
    }

    BaseMapper* CreateMapper()
    {
        switch(m_nesCartridgeData->mapperId())
        {
        case 0: return createMapperAndBind<Mapper000>(*m_nesCartridgeData);
        case 1: return createMapperAndBind<Mapper001>(*m_nesCartridgeData);
        case 2: return createMapperAndBind<Mapper002>(*m_nesCartridgeData);
        case 3: return createMapperAndBind<Mapper003>(*m_nesCartridgeData);
        case 4: {
            if(m_nesCartridgeData->subMapperId() == 3) {
                return createMapperAndBind<Mapper004_3>(*m_nesCartridgeData);
            }
            return createMapperAndBind<Mapper004>(*m_nesCartridgeData);
        }
        case 5: return createMapperAndBind<Mapper005>(*m_nesCartridgeData);
        case 6: return createMapperAndBind<Mapper006>(*m_nesCartridgeData);
        case 7: return createMapperAndBind<Mapper007>(*m_nesCartridgeData);
        case 8: return createMapperAndBind<Mapper008>(*m_nesCartridgeData);
        case 9: return createMapperAndBind<Mapper009>(*m_nesCartridgeData);
        case 10: return createMapperAndBind<Mapper010>(*m_nesCartridgeData);
        case 11: return createMapperAndBind<Mapper011>(*m_nesCartridgeData);
        case 12: return createMapperAndBind<Mapper012>(*m_nesCartridgeData);
        case 13: return createMapperAndBind<Mapper013>(*m_nesCartridgeData);
        case 14: return createMapperAndBind<Mapper014>(*m_nesCartridgeData);
        case 15: return createMapperAndBind<Mapper015>(*m_nesCartridgeData);
        case 16: return createMapperAndBind<Mapper016>(*m_nesCartridgeData);
        case 17: return createMapperAndBind<Mapper017>(*m_nesCartridgeData);
        case 18: return createMapperAndBind<Mapper018>(*m_nesCartridgeData);
        case 19: return createMapperAndBind<Mapper019>(*m_nesCartridgeData);
        case 20: return createMapperAndBind<Mapper020>(*m_nesCartridgeData);
        case 21: return createMapperAndBind<Mapper021>(*m_nesCartridgeData);
        case 22: return createMapperAndBind<Mapper022>(*m_nesCartridgeData);
        case 23: return createMapperAndBind<Mapper023>(*m_nesCartridgeData);
        case 24: return createMapperAndBind<Mapper024>(*m_nesCartridgeData);
        case 25: return createMapperAndBind<Mapper025>(*m_nesCartridgeData);
        case 26: return createMapperAndBind<Mapper026>(*m_nesCartridgeData);
        case 27: return createMapperAndBind<Mapper027>(*m_nesCartridgeData);
        case 28: return createMapperAndBind<Mapper028>(*m_nesCartridgeData);
        case 29: return createMapperAndBind<Mapper029>(*m_nesCartridgeData);
        case 30: return createMapperAndBind<Mapper030>(*m_nesCartridgeData);
        case 31: return createMapperAndBind<Mapper031>(*m_nesCartridgeData);
        case 32: return createMapperAndBind<Mapper032>(*m_nesCartridgeData);
        case 33: return createMapperAndBind<Mapper033>(*m_nesCartridgeData);
        case 34: return createMapperAndBind<Mapper034>(*m_nesCartridgeData);
        case 35: return createMapperAndBind<Mapper035>(*m_nesCartridgeData);
        case 36: return createMapperAndBind<Mapper036>(*m_nesCartridgeData);
        case 37: return createMapperAndBind<Mapper037>(*m_nesCartridgeData);
        case 38: return createMapperAndBind<Mapper038>(*m_nesCartridgeData);
        case 39: return createMapperAndBind<Mapper039>(*m_nesCartridgeData);
        case 40: return createMapperAndBind<Mapper040>(*m_nesCartridgeData);
        case 41: return createMapperAndBind<Mapper041>(*m_nesCartridgeData);
        case 42: return createMapperAndBind<Mapper042>(*m_nesCartridgeData);
        case 43: return createMapperAndBind<Mapper043>(*m_nesCartridgeData);
        case 44: return createMapperAndBind<Mapper044>(*m_nesCartridgeData);
        case 45: return createMapperAndBind<Mapper045>(*m_nesCartridgeData);
        case 46: return createMapperAndBind<Mapper046>(*m_nesCartridgeData);
        case 47: return createMapperAndBind<Mapper047>(*m_nesCartridgeData);
        case 48: return createMapperAndBind<Mapper048>(*m_nesCartridgeData);
        case 49: return createMapperAndBind<Mapper049>(*m_nesCartridgeData);
        case 50: return createMapperAndBind<Mapper050>(*m_nesCartridgeData);
        case 51: return createMapperAndBind<Mapper051>(*m_nesCartridgeData);
        case 52: return createMapperAndBind<Mapper052>(*m_nesCartridgeData);
        case 53: return createMapperAndBind<Mapper053>(*m_nesCartridgeData);
        case 54: return createMapperAndBind<Mapper054>(*m_nesCartridgeData);
        case 55: return createMapperAndBind<Mapper055>(*m_nesCartridgeData);
        case 56: return createMapperAndBind<Mapper056>(*m_nesCartridgeData);
        case 57: return createMapperAndBind<Mapper057>(*m_nesCartridgeData);
        case 58: return createMapperAndBind<Mapper058>(*m_nesCartridgeData);
        case 59: return createMapperAndBind<Mapper059>(*m_nesCartridgeData);
        case 60: return createMapperAndBind<Mapper060>(*m_nesCartridgeData);
        case 61: return createMapperAndBind<Mapper061>(*m_nesCartridgeData);
        case 62: return createMapperAndBind<Mapper062>(*m_nesCartridgeData);
        case 63: return createMapperAndBind<Mapper063>(*m_nesCartridgeData);
        case 64: return createMapperAndBind<Mapper064>(*m_nesCartridgeData);
        case 65: return createMapperAndBind<Mapper065>(*m_nesCartridgeData);
        case 66: return createMapperAndBind<Mapper066>(*m_nesCartridgeData);
        case 67: return createMapperAndBind<Mapper067>(*m_nesCartridgeData);
        case 68: return createMapperAndBind<Mapper068>(*m_nesCartridgeData);
        case 69: return createMapperAndBind<Mapper069>(*m_nesCartridgeData);
        case 70: return createMapperAndBind<Mapper070>(*m_nesCartridgeData);
        case 71: return createMapperAndBind<Mapper071>(*m_nesCartridgeData);
        case 72: return createMapperAndBind<Mapper072>(*m_nesCartridgeData);
        case 73: return createMapperAndBind<Mapper073>(*m_nesCartridgeData);
        case 74: return createMapperAndBind<Mapper074>(*m_nesCartridgeData);
        case 75: return createMapperAndBind<Mapper075>(*m_nesCartridgeData);
        case 76: return createMapperAndBind<Mapper076>(*m_nesCartridgeData);
        case 77: return createMapperAndBind<Mapper077>(*m_nesCartridgeData);
        case 78: return createMapperAndBind<Mapper078>(*m_nesCartridgeData);
        case 79: return createMapperAndBind<Mapper079>(*m_nesCartridgeData);
        case 80: return createMapperAndBind<Mapper080>(*m_nesCartridgeData);
        case 81: return createMapperAndBind<Mapper081>(*m_nesCartridgeData);
        case 82: return createMapperAndBind<Mapper082>(*m_nesCartridgeData);
        case 83: return createMapperAndBind<Mapper083>(*m_nesCartridgeData);
        case 84: return createMapperAndBind<Mapper084>(*m_nesCartridgeData);
        case 85: return createMapperAndBind<Mapper085>(*m_nesCartridgeData);
        case 86: return createMapperAndBind<Mapper086>(*m_nesCartridgeData);
        case 87: return createMapperAndBind<Mapper087>(*m_nesCartridgeData);
        case 88: return createMapperAndBind<Mapper088>(*m_nesCartridgeData);
        case 89: return createMapperAndBind<Mapper089>(*m_nesCartridgeData);
        case 90: return createMapperAndBind<Mapper090>(*m_nesCartridgeData);
        case 91: return createMapperAndBind<Mapper091>(*m_nesCartridgeData);
        case 92: return createMapperAndBind<Mapper092>(*m_nesCartridgeData);
        case 93: return createMapperAndBind<Mapper093>(*m_nesCartridgeData);
        case 94: return createMapperAndBind<Mapper094>(*m_nesCartridgeData);
        case 95: return createMapperAndBind<Mapper095>(*m_nesCartridgeData);
        case 96: return createMapperAndBind<Mapper096>(*m_nesCartridgeData);
        case 97: return createMapperAndBind<Mapper097>(*m_nesCartridgeData);
        case 98: return createMapperAndBind<Mapper098>(*m_nesCartridgeData);
        case 99: return createMapperAndBind<Mapper099>(*m_nesCartridgeData);
        case 100: return createMapperAndBind<Mapper100>(*m_nesCartridgeData);
        case 101: return createMapperAndBind<Mapper101>(*m_nesCartridgeData);
        case 102: return createMapperAndBind<Mapper102>(*m_nesCartridgeData);
        case 103: return createMapperAndBind<Mapper103>(*m_nesCartridgeData);
        case 104: return createMapperAndBind<Mapper104>(*m_nesCartridgeData);
        case 105: return createMapperAndBind<Mapper105>(*m_nesCartridgeData);
        case 106: return createMapperAndBind<Mapper106>(*m_nesCartridgeData);
        case 107: return createMapperAndBind<Mapper107>(*m_nesCartridgeData);
        case 108: return createMapperAndBind<Mapper108>(*m_nesCartridgeData);
        case 109: return createMapperAndBind<Mapper109>(*m_nesCartridgeData);
        case 110: return createMapperAndBind<Mapper110>(*m_nesCartridgeData);
        case 111: return createMapperAndBind<Mapper111>(*m_nesCartridgeData);
        case 112: return createMapperAndBind<Mapper112>(*m_nesCartridgeData);
        case 113: return createMapperAndBind<Mapper113>(*m_nesCartridgeData);
        case 114: return createMapperAndBind<Mapper114>(*m_nesCartridgeData);
        case 115: return createMapperAndBind<Mapper115>(*m_nesCartridgeData);
        case 116: return createMapperAndBind<Mapper116>(*m_nesCartridgeData);
        case 117: return createMapperAndBind<Mapper117>(*m_nesCartridgeData);
        case 118: return createMapperAndBind<Mapper118>(*m_nesCartridgeData);
        case 119: return createMapperAndBind<Mapper119>(*m_nesCartridgeData);
        case 120: return createMapperAndBind<Mapper120>(*m_nesCartridgeData);
        case 121: return createMapperAndBind<Mapper121>(*m_nesCartridgeData);
        case 122: return createMapperAndBind<Mapper122>(*m_nesCartridgeData);
        case 123: return createMapperAndBind<Mapper123>(*m_nesCartridgeData);
        case 124: return createMapperAndBind<Mapper124>(*m_nesCartridgeData);
        case 125: return createMapperAndBind<Mapper125>(*m_nesCartridgeData);
        case 126: return createMapperAndBind<Mapper126>(*m_nesCartridgeData);
        case 127: return createMapperAndBind<Mapper127>(*m_nesCartridgeData);
        case 128: return createMapperAndBind<Mapper128>(*m_nesCartridgeData);
        case 129: return createMapperAndBind<Mapper129>(*m_nesCartridgeData);
        case 130: return createMapperAndBind<Mapper130>(*m_nesCartridgeData);
        case 131: return createMapperAndBind<Mapper131>(*m_nesCartridgeData);
        case 132: return createMapperAndBind<Mapper132>(*m_nesCartridgeData);
        case 133: return createMapperAndBind<Mapper133>(*m_nesCartridgeData);
        case 134: return createMapperAndBind<Mapper134>(*m_nesCartridgeData);
        case 135: return createMapperAndBind<Mapper135>(*m_nesCartridgeData);
        case 136: return createMapperAndBind<Mapper136>(*m_nesCartridgeData);
        case 137: return createMapperAndBind<Mapper137>(*m_nesCartridgeData);
        case 138: return createMapperAndBind<Mapper138>(*m_nesCartridgeData);
        case 139: return createMapperAndBind<Mapper139>(*m_nesCartridgeData);
        case 140: return createMapperAndBind<Mapper140>(*m_nesCartridgeData);
        case 141: return createMapperAndBind<Mapper141>(*m_nesCartridgeData);
        case 142: return createMapperAndBind<Mapper142>(*m_nesCartridgeData);
        case 143: return createMapperAndBind<Mapper143>(*m_nesCartridgeData);
        case 144: return createMapperAndBind<Mapper144>(*m_nesCartridgeData);
        case 145: return createMapperAndBind<Mapper145>(*m_nesCartridgeData);
        case 146: return createMapperAndBind<Mapper146>(*m_nesCartridgeData);
        case 147: return createMapperAndBind<Mapper147>(*m_nesCartridgeData);
        case 148: return createMapperAndBind<Mapper148>(*m_nesCartridgeData);
        case 149: return createMapperAndBind<Mapper149>(*m_nesCartridgeData);
        case 150: return createMapperAndBind<Mapper150>(*m_nesCartridgeData);
        case 151: return createMapperAndBind<Mapper151>(*m_nesCartridgeData);
        case 152: return createMapperAndBind<Mapper152>(*m_nesCartridgeData);
        case 153: return createMapperAndBind<Mapper153>(*m_nesCartridgeData);
        case 154: return createMapperAndBind<Mapper154>(*m_nesCartridgeData);
        case 155: return createMapperAndBind<Mapper155>(*m_nesCartridgeData);
        case 156: return createMapperAndBind<Mapper156>(*m_nesCartridgeData);
        case 157: return createMapperAndBind<Mapper157>(*m_nesCartridgeData);
        case 158: return createMapperAndBind<Mapper158>(*m_nesCartridgeData);
        case 159: return createMapperAndBind<Mapper159>(*m_nesCartridgeData);
        case 160: return createMapperAndBind<Mapper160>(*m_nesCartridgeData);
        case 161: return createMapperAndBind<Mapper161>(*m_nesCartridgeData);
        case 162: return createMapperAndBind<Mapper162>(*m_nesCartridgeData);
        case 163: return createMapperAndBind<Mapper163>(*m_nesCartridgeData);
        case 164: return createMapperAndBind<Mapper164>(*m_nesCartridgeData);
        case 165: return createMapperAndBind<Mapper165>(*m_nesCartridgeData);
        case 166: return createMapperAndBind<Mapper166>(*m_nesCartridgeData);
        case 167: return createMapperAndBind<Mapper167>(*m_nesCartridgeData);
        case 168: return createMapperAndBind<Mapper168>(*m_nesCartridgeData);
        case 169: return createMapperAndBind<Mapper169>(*m_nesCartridgeData);
        case 170: return createMapperAndBind<Mapper170>(*m_nesCartridgeData);
        case 171: return createMapperAndBind<Mapper171>(*m_nesCartridgeData);
        case 172: return createMapperAndBind<Mapper172>(*m_nesCartridgeData);
        case 173: return createMapperAndBind<Mapper173>(*m_nesCartridgeData);
        case 174: return createMapperAndBind<Mapper174>(*m_nesCartridgeData);
        case 175: return createMapperAndBind<Mapper175>(*m_nesCartridgeData);
        case 176: return createMapperAndBind<Mapper176>(*m_nesCartridgeData);
        case 177: return createMapperAndBind<Mapper177>(*m_nesCartridgeData);
        case 178: return createMapperAndBind<Mapper178>(*m_nesCartridgeData);
        case 179: return createMapperAndBind<Mapper179>(*m_nesCartridgeData);
        case 180: return createMapperAndBind<Mapper180>(*m_nesCartridgeData);
        case 181: return createMapperAndBind<Mapper181>(*m_nesCartridgeData);
        case 182: return createMapperAndBind<Mapper182>(*m_nesCartridgeData);
        case 184: return createMapperAndBind<Mapper184>(*m_nesCartridgeData);
        case 185: return createMapperAndBind<Mapper185>(*m_nesCartridgeData);
        case 186: return createMapperAndBind<Mapper186>(*m_nesCartridgeData);
        case 187: return createMapperAndBind<Mapper187>(*m_nesCartridgeData);
        case 188: return createMapperAndBind<Mapper188>(*m_nesCartridgeData);
        case 189: return createMapperAndBind<Mapper189>(*m_nesCartridgeData);
        case 190: return createMapperAndBind<Mapper190>(*m_nesCartridgeData);
        case 191: return createMapperAndBind<Mapper191>(*m_nesCartridgeData);
        case 192: return createMapperAndBind<Mapper192>(*m_nesCartridgeData);
        case 193: return createMapperAndBind<Mapper193>(*m_nesCartridgeData);
        case 194: return createMapperAndBind<Mapper194>(*m_nesCartridgeData);
        case 195: return createMapperAndBind<Mapper195>(*m_nesCartridgeData);
        case 196: return createMapperAndBind<Mapper196>(*m_nesCartridgeData);
        case 197: return createMapperAndBind<Mapper197>(*m_nesCartridgeData);
        case 198: return createMapperAndBind<Mapper198>(*m_nesCartridgeData);
        case 199: return createMapperAndBind<Mapper199>(*m_nesCartridgeData);
        case 200: return createMapperAndBind<Mapper200>(*m_nesCartridgeData);
        case 201: return createMapperAndBind<Mapper201>(*m_nesCartridgeData);
        case 203: return createMapperAndBind<Mapper203>(*m_nesCartridgeData);
        case 204: return createMapperAndBind<Mapper204>(*m_nesCartridgeData);
        case 205: return createMapperAndBind<Mapper205>(*m_nesCartridgeData);
        case 206: return createMapperAndBind<Mapper206>(*m_nesCartridgeData);
        case 207: return createMapperAndBind<Mapper207>(*m_nesCartridgeData);
        case 208: return createMapperAndBind<Mapper208>(*m_nesCartridgeData);
        case 209: return createMapperAndBind<Mapper209>(*m_nesCartridgeData);
        case 210: return createMapperAndBind<Mapper210>(*m_nesCartridgeData);
        case 211: return createMapperAndBind<Mapper211>(*m_nesCartridgeData);
        case 212: return createMapperAndBind<Mapper212>(*m_nesCartridgeData);
        case 213: return createMapperAndBind<Mapper213>(*m_nesCartridgeData);
        case 214: return createMapperAndBind<Mapper214>(*m_nesCartridgeData);
        case 215: return createMapperAndBind<Mapper215>(*m_nesCartridgeData);
        case 216: return createMapperAndBind<Mapper216>(*m_nesCartridgeData);
        case 217: return createMapperAndBind<Mapper217>(*m_nesCartridgeData);
        case 218: return createMapperAndBind<Mapper218>(*m_nesCartridgeData);
        case 219: return createMapperAndBind<Mapper219>(*m_nesCartridgeData);
        case 220: return createMapperAndBind<Mapper220>(*m_nesCartridgeData);
        case 221: return createMapperAndBind<Mapper221>(*m_nesCartridgeData);
        case 222: return createMapperAndBind<Mapper222>(*m_nesCartridgeData);
        case 223: return createMapperAndBind<Mapper223>(*m_nesCartridgeData);
        case 224: return createMapperAndBind<Mapper224>(*m_nesCartridgeData);
        case 225: return createMapperAndBind<Mapper225>(*m_nesCartridgeData);
        case 226: return createMapperAndBind<Mapper226>(*m_nesCartridgeData);
        case 227: return createMapperAndBind<Mapper227>(*m_nesCartridgeData);
        case 228: return createMapperAndBind<Mapper228>(*m_nesCartridgeData);
        case 229: return createMapperAndBind<Mapper229>(*m_nesCartridgeData);
        case 230: return createMapperAndBind<Mapper230>(*m_nesCartridgeData);
        case 231: return createMapperAndBind<Mapper231>(*m_nesCartridgeData);
        case 232: return createMapperAndBind<Mapper232>(*m_nesCartridgeData);
        case 233: return createMapperAndBind<Mapper233>(*m_nesCartridgeData);
        case 234: return createMapperAndBind<Mapper234>(*m_nesCartridgeData);
        case 235: return createMapperAndBind<Mapper235>(*m_nesCartridgeData);
        case 236: return createMapperAndBind<Mapper236>(*m_nesCartridgeData);
        case 237: return createMapperAndBind<Mapper237>(*m_nesCartridgeData);
        case 238: return createMapperAndBind<Mapper238>(*m_nesCartridgeData);
        case 239: return createMapperAndBind<Mapper239>(*m_nesCartridgeData);
        case 240: return createMapperAndBind<Mapper240>(*m_nesCartridgeData);
        case 241: return createMapperAndBind<Mapper241>(*m_nesCartridgeData);
        case 242: return createMapperAndBind<Mapper242>(*m_nesCartridgeData);
        case 243: return createMapperAndBind<Mapper243>(*m_nesCartridgeData);
        case 244: return createMapperAndBind<Mapper244>(*m_nesCartridgeData);
        case 245: return createMapperAndBind<Mapper245>(*m_nesCartridgeData);
        case 246: return createMapperAndBind<Mapper246>(*m_nesCartridgeData);
        case 247: return createMapperAndBind<Mapper247>(*m_nesCartridgeData);
        case 248: return createMapperAndBind<Mapper248>(*m_nesCartridgeData);
        case 249: return createMapperAndBind<Mapper249>(*m_nesCartridgeData);
        case 250: return createMapperAndBind<Mapper250>(*m_nesCartridgeData);
        case 251: return createMapperAndBind<Mapper251>(*m_nesCartridgeData);
        case 252: return createMapperAndBind<Mapper252>(*m_nesCartridgeData);
        case 253: return createMapperAndBind<Mapper253>(*m_nesCartridgeData);
        case 254: return createMapperAndBind<Mapper254>(*m_nesCartridgeData);
        case 255: return createMapperAndBind<Mapper255>(*m_nesCartridgeData);
#ifdef ENABLE_NSF_PLAYER
        case _NsfFormat::NSF_MAPPER_ID: return createMapperAndBind<MapperNSF>(*m_nesCartridgeData);
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
        assignDefaultMapperDispatch();
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
        assignDefaultMapperDispatch();

        m_isValid = false;        
    }

    bool open(const std::string& filename)
    {
        clear();

        m_romFile.open(filename);
        const std::string sourceName = m_romFile.fileName().empty() ? fs::path(filename).filename().string() : m_romFile.fileName();
        const std::string sourceExtension = fs::path(sourceName).extension().string();

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

                if(sourceExtension == ".fds") {
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
                    const std::string nsfError = nsf->error();
                    delete nsf;
                    nsf = nullptr;
                    clear();
                    if(iNesSizeMismatch) {
                        Logger::instance().log("ROM file size/header mismatch detected (iNES). Aborting load.", Logger::Type::ERROR);
                    }
                    else if(sourceExtension == ".nsf") {
                        Logger::instance().log(
                            nsfError.empty() ? "Invalid NSF file" : nsfError,
                            Logger::Type::ERROR
                        );
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

        const auto* iNesData = dynamic_cast<_INesFormat*>(m_nesCartridgeData);
        const bool skipDatabaseHeaderOverwrite =
            (iNesData != nullptr && iNesData->isNes20()) ||
#ifdef ENABLE_NSF_PLAYER
            dynamic_cast<_NsfFormat*>(m_nesCartridgeData) != nullptr ||
#endif
            dynamic_cast<_FdsFormat*>(m_nesCartridgeData) != nullptr;

        // NSF/FDS are not iNES cartridge dumps, and NES 2.0 headers already provide
        // explicit mapper/submapper/RAM metadata that we prefer over DB overrides.
        if(!skipDatabaseHeaderOverwrite) {
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
            if(iNesData != nullptr && iNesData->isNes20()) {
                Logger::instance().log("NES 2.0 ROM detected\nUsing file header directly", Logger::Type::INFO);
            }
            else if(dynamic_cast<_FdsFormat*>(m_nesCartridgeData) != nullptr) {
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
        m_writePrgFn(m_mapper, addr, data);
    }

    GERANES_INLINE BaseMapper* mapper()
    {
        return m_mapper;
    }

    GERANES_INLINE const BaseMapper* mapper() const
    {
        return m_mapper;
    }

    GERANES_INLINE uint8_t readPrg(int addr)
    {
        return m_readPrgFn(m_mapper, addr);
    }

    GERANES_INLINE void writeSaveRam(int addr, uint8_t data)
    {
        m_writeSaveRamFn(m_mapper, addr, data);
    }

    GERANES_INLINE uint8_t readSaveRam(int addr)
    {
        return m_readSaveRamFn(m_mapper, addr);
    }

    GERANES_INLINE void writeChr(int addr, uint8_t data)
    {
        m_writeChrFn(m_mapper, addr, data);
    }

    GERANES_INLINE uint8_t readChr(int addr)
    {
        return m_readChrFn(m_mapper, addr);
    }

    GERANES_INLINE void writeMapperRegister(int addr, uint8_t data)
    {
        m_writeMapperRegisterFn(m_mapper, addr, data);
    }

    GERANES_INLINE uint8_t readMapperRegister(int addr, uint8_t openBusData)
    {
        return m_readMapperRegisterFn(m_mapper, addr, openBusData);
    }

    GERANES_INLINE void writeMapperRegisterAbsolute(uint16_t addr, uint8_t data)
    {
        m_writeMapperRegisterAbsoluteFn(m_mapper, addr, data);
    }

    GERANES_INLINE uint8_t readMapperRegisterAbsolute(uint16_t addr, uint8_t openBusData)
    {
        return m_readMapperRegisterAbsoluteFn(m_mapper, addr, openBusData);
    }

    GERANES_INLINE MirroringType getMirroringType()
    {
        return m_mirroringTypeFn(m_mapper);
    }

    //return nametable index with preperly mirroring
    GERANES_INLINE_HOT uint8_t mirroring(uint8_t blockIndex)
    {
        static const uint8_t HORIZONTAL_MIRROR[] = {0,0,1,1};
        static const uint8_t VERTICAL_MIRROR[] = {0,1,0,1};
        static const uint8_t FOUR_SCREEN_MIRROR[] = {0,1,2,3};

        switch(m_mirroringTypeFn(m_mapper)){

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
        return m_getInterruptFlagFn(m_mapper);
    }

    GERANES_INLINE void setA12State(bool state)
    {
        if(!hasMapperHookCap(BaseMapper::HookCap_SetA12State)) {
            return;
        }
        m_setA12StateFn(m_mapper, state);
    }

    GERANES_INLINE void cycle()
    {    
        m_cycleFn(m_mapper);
    }    

    GERANES_INLINE_HOT bool useCustomNameTable(uint8_t index)
    {
        if(!hasMapperHookCap(BaseMapper::HookCap_UseCustomNameTable)) {
            return false;
        }
        return m_useCustomNameTableFn(m_mapper, index);
    }

    GERANES_INLINE_HOT uint8_t readCustomNameTable(uint8_t index, uint16_t addr)
    {
        return m_readCustomNameTableFn(m_mapper, index, addr);
    }

    GERANES_INLINE_HOT void writeCustomNameTable(uint8_t index, uint16_t addr, uint8_t data)
    {
        m_writeCustomNameTableFn(m_mapper, index, addr, data);
    }

    GERANES_INLINE void onScanlineStart(bool renderingEnabled, int scanline)
    {
        m_onScanlineStartFn(m_mapper, renderingEnabled, scanline);
    }

    GERANES_INLINE void setPpuFetchSource(bool isSpriteFetch)
    {
        if(!hasMapperHookCap(BaseMapper::HookCap_SetPpuFetchSource)) {
            return;
        }
        m_setPpuFetchSourceFn(m_mapper, isSpriteFetch);
    }

    GERANES_INLINE uint8_t transformNameTableRead(uint8_t index, uint16_t addr, uint8_t value)
    {
        if(!hasMapperHookCap(BaseMapper::HookCap_TransformNameTableRead)) {
            return value;
        }
        return m_transformNameTableReadFn(m_mapper, index, addr, value);
    }

    GERANES_INLINE void setSpriteSize8x16(bool sprite8x16)
    {
        m_setSpriteSize8x16Fn(m_mapper, sprite8x16);
    }

    GERANES_INLINE void setPpuMask(uint8_t mask)
    {
        m_setPpuMaskFn(m_mapper, mask);
    }

    GERANES_INLINE void onPpuStatusRead(bool vblankSet)
    {
        m_onPpuStatusReadFn(m_mapper, vblankSet);
    }

    GERANES_INLINE void onPpuRead(uint16_t addr)
    {
        if(!hasMapperHookCap(BaseMapper::HookCap_OnPpuRead)) {
            return;
        }
        m_onPpuReadFn(m_mapper, addr);
    }

    GERANES_INLINE void onPpuCycle(int scanline, int cycle, bool isRendering, bool isPreLine)
    {
        if(!hasMapperHookCap(BaseMapper::HookCap_OnPpuCycle)) {
            return;
        }
        m_onPpuCycleFn(m_mapper, scanline, cycle, isRendering, isPreLine);
    }

    GERANES_INLINE void onCpuRead(uint16_t addr)
    {
        if(!hasMapperHookCap(BaseMapper::HookCap_OnCpuRead)) {
            return;
        }
        m_onCpuReadFn(m_mapper, addr);
    }

    GERANES_INLINE void onCpuWrite(uint16_t addr, uint8_t data)
    {
        if(!hasMapperHookCap(BaseMapper::HookCap_OnCpuWrite)) {
            return;
        }
        m_onCpuWriteFn(m_mapper, addr, data);
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
        return m_getExpansionAudioSampleFn(m_mapper);
    }

    GERANES_INLINE float getMixWeight() const
    {
        return m_getMixWeightFn(m_mapper);
    }

    GERANES_INLINE float getExpansionOutputGain() const
    {
        return m_getExpansionOutputGainFn(m_mapper);
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
        m_applyExternalActionsFn(m_mapper, pending);
    }

    GERANES_INLINE GameDatabase::System system() const
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

    GERANES_INLINE bool isValid() const
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



