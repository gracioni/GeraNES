#include "GeraNESApp/ControllerInfo.h"
#include "GeraNESApp/KonamiHyperShotInfo.h"
#include "GeraNESApp/SnesControllerInfo.h"
#include "GeraNESApp/SystemInputInfo.h"
#include "GeraNESApp/VirtualBoyControllerInfo.h"

void ControllerInfo::mapInit()
{
    if(_map != nullptr) return;

    _map = new std::map<const std::string, std::string*>;

    _map->insert(std::make_pair(BUTTONS[0], &a));
    _map->insert(std::make_pair(BUTTONS[1], &b));
    _map->insert(std::make_pair(BUTTONS[2], &select));
    _map->insert(std::make_pair(BUTTONS[3], &start));
    _map->insert(std::make_pair(BUTTONS[4], &up));
    _map->insert(std::make_pair(BUTTONS[5], &down));
    _map->insert(std::make_pair(BUTTONS[6], &left));
    _map->insert(std::make_pair(BUTTONS[7], &right));
}

ControllerInfo::ControllerInfo() = default;

ControllerInfo::~ControllerInfo()
{
    if(_map != nullptr) delete _map;
}

ControllerInfo::ControllerInfo(const ControllerInfo& other)
{
    *this = other;
}

ControllerInfo& ControllerInfo::operator=(const ControllerInfo& other)
{
    a = other.a;
    b = other.b;
    select = other.select;
    start = other.start;
    up = other.up;
    down = other.down;
    left = other.left;
    right = other.right;
    return *this;
}

const std::string& ControllerInfo::getByButtonName(const std::string& name)
{
    static std::string empty = "";

    mapInit();
    if(_map->count(name)) {
        return *(*_map)[name];
    }

    return empty;
}

void ControllerInfo::setByButtonIndex(int index, const std::string& input)
{
    switch(index){
        case 0: a = input; break;
        case 1: b = input; break;
        case 2: select = input; break;
        case 3: start = input; break;
        case 4: up = input; break;
        case 5: down = input; break;
        case 6: left = input; break;
        case 7: right = input; break;
    }
}

size_t ControllerInfo::bindingCount() const
{
    return BUTTONS.size();
}

const char* ControllerInfo::bindingLabel(size_t index) const
{
    return BUTTONS[index];
}

const std::string& ControllerInfo::getBinding(size_t index) const
{
    return const_cast<ControllerInfo*>(this)->getByButtonName(BUTTONS[index]);
}

void ControllerInfo::setBinding(size_t index, const std::string& input)
{
    setByButtonIndex(static_cast<int>(index), input);
}

void SnesControllerInfo::mapInit()
{
    if(m_map != nullptr) return;

    m_map = new std::map<const std::string, std::string*>;
    m_map->insert(std::make_pair(BUTTONS[0], &a));
    m_map->insert(std::make_pair(BUTTONS[1], &b));
    m_map->insert(std::make_pair(BUTTONS[2], &x));
    m_map->insert(std::make_pair(BUTTONS[3], &y));
    m_map->insert(std::make_pair(BUTTONS[4], &l));
    m_map->insert(std::make_pair(BUTTONS[5], &r));
    m_map->insert(std::make_pair(BUTTONS[6], &select));
    m_map->insert(std::make_pair(BUTTONS[7], &start));
    m_map->insert(std::make_pair(BUTTONS[8], &up));
    m_map->insert(std::make_pair(BUTTONS[9], &down));
    m_map->insert(std::make_pair(BUTTONS[10], &left));
    m_map->insert(std::make_pair(BUTTONS[11], &right));
}

SnesControllerInfo::SnesControllerInfo() = default;

SnesControllerInfo::~SnesControllerInfo()
{
    if(m_map != nullptr) delete m_map;
}

SnesControllerInfo::SnesControllerInfo(const SnesControllerInfo& other)
{
    *this = other;
}

SnesControllerInfo& SnesControllerInfo::operator=(const SnesControllerInfo& other)
{
    a = other.a;
    b = other.b;
    x = other.x;
    y = other.y;
    l = other.l;
    r = other.r;
    select = other.select;
    start = other.start;
    up = other.up;
    down = other.down;
    left = other.left;
    right = other.right;
    return *this;
}

const std::string& SnesControllerInfo::getByButtonName(const std::string& name)
{
    static std::string empty = "";

    mapInit();
    if(m_map->count(name)) {
        return *(*m_map)[name];
    }
    return empty;
}

void SnesControllerInfo::setByButtonIndex(int index, const std::string& input)
{
    switch(index) {
        case 0: a = input; break;
        case 1: b = input; break;
        case 2: x = input; break;
        case 3: y = input; break;
        case 4: l = input; break;
        case 5: r = input; break;
        case 6: select = input; break;
        case 7: start = input; break;
        case 8: up = input; break;
        case 9: down = input; break;
        case 10: left = input; break;
        case 11: right = input; break;
    }
}

size_t SnesControllerInfo::bindingCount() const
{
    return BUTTONS.size();
}

const char* SnesControllerInfo::bindingLabel(size_t index) const
{
    return BUTTONS[index];
}

const std::string& SnesControllerInfo::getBinding(size_t index) const
{
    return const_cast<SnesControllerInfo*>(this)->getByButtonName(BUTTONS[index]);
}

void SnesControllerInfo::setBinding(size_t index, const std::string& input)
{
    setByButtonIndex(static_cast<int>(index), input);
}

void VirtualBoyControllerInfo::mapInit()
{
    if(m_map != nullptr) return;

    m_map = new std::map<const std::string, std::string*>;
    m_map->insert(std::make_pair(BUTTONS[0], &a));
    m_map->insert(std::make_pair(BUTTONS[1], &b));
    m_map->insert(std::make_pair(BUTTONS[2], &l));
    m_map->insert(std::make_pair(BUTTONS[3], &r));
    m_map->insert(std::make_pair(BUTTONS[4], &select));
    m_map->insert(std::make_pair(BUTTONS[5], &start));
    m_map->insert(std::make_pair(BUTTONS[6], &up));
    m_map->insert(std::make_pair(BUTTONS[7], &down));
    m_map->insert(std::make_pair(BUTTONS[8], &left));
    m_map->insert(std::make_pair(BUTTONS[9], &right));
    m_map->insert(std::make_pair(BUTTONS[10], &up2));
    m_map->insert(std::make_pair(BUTTONS[11], &down2));
    m_map->insert(std::make_pair(BUTTONS[12], &left2));
    m_map->insert(std::make_pair(BUTTONS[13], &right2));
}

VirtualBoyControllerInfo::VirtualBoyControllerInfo() = default;

VirtualBoyControllerInfo::~VirtualBoyControllerInfo()
{
    if(m_map != nullptr) delete m_map;
}

VirtualBoyControllerInfo::VirtualBoyControllerInfo(const VirtualBoyControllerInfo& other)
{
    *this = other;
}

VirtualBoyControllerInfo& VirtualBoyControllerInfo::operator=(const VirtualBoyControllerInfo& other)
{
    a = other.a;
    b = other.b;
    l = other.l;
    r = other.r;
    select = other.select;
    start = other.start;
    up = other.up;
    down = other.down;
    left = other.left;
    right = other.right;
    up2 = other.up2;
    down2 = other.down2;
    left2 = other.left2;
    right2 = other.right2;
    return *this;
}

const std::string& VirtualBoyControllerInfo::getByButtonName(const std::string& name)
{
    static std::string empty = "";

    mapInit();
    if(m_map->count(name)) {
        return *(*m_map)[name];
    }
    return empty;
}

void VirtualBoyControllerInfo::setByButtonIndex(int index, const std::string& input)
{
    switch(index) {
        case 0: a = input; break;
        case 1: b = input; break;
        case 2: l = input; break;
        case 3: r = input; break;
        case 4: select = input; break;
        case 5: start = input; break;
        case 6: up = input; break;
        case 7: down = input; break;
        case 8: left = input; break;
        case 9: right = input; break;
        case 10: up2 = input; break;
        case 11: down2 = input; break;
        case 12: left2 = input; break;
        case 13: right2 = input; break;
    }
}

size_t VirtualBoyControllerInfo::bindingCount() const
{
    return BUTTONS.size();
}

const char* VirtualBoyControllerInfo::bindingLabel(size_t index) const
{
    return BUTTONS[index];
}

const std::string& VirtualBoyControllerInfo::getBinding(size_t index) const
{
    return const_cast<VirtualBoyControllerInfo*>(this)->getByButtonName(BUTTONS[index]);
}

void VirtualBoyControllerInfo::setBinding(size_t index, const std::string& input)
{
    setByButtonIndex(static_cast<int>(index), input);
}

void KonamiHyperShotInfo::mapInit()
{
    if(m_map != nullptr) return;

    m_map = new std::map<const std::string, std::string*>;
    m_map->insert(std::make_pair(BUTTONS[0], &p1Run));
    m_map->insert(std::make_pair(BUTTONS[1], &p1Jump));
    m_map->insert(std::make_pair(BUTTONS[2], &p2Run));
    m_map->insert(std::make_pair(BUTTONS[3], &p2Jump));
}

KonamiHyperShotInfo::KonamiHyperShotInfo() = default;

KonamiHyperShotInfo::~KonamiHyperShotInfo()
{
    if(m_map != nullptr) delete m_map;
}

KonamiHyperShotInfo::KonamiHyperShotInfo(const KonamiHyperShotInfo& other)
{
    *this = other;
}

KonamiHyperShotInfo& KonamiHyperShotInfo::operator=(const KonamiHyperShotInfo& other)
{
    p1Run = other.p1Run;
    p1Jump = other.p1Jump;
    p2Run = other.p2Run;
    p2Jump = other.p2Jump;
    return *this;
}

const std::string& KonamiHyperShotInfo::getByButtonName(const std::string& name)
{
    static std::string empty = "";

    mapInit();
    if(m_map->count(name)) {
        return *(*m_map)[name];
    }
    return empty;
}

void KonamiHyperShotInfo::setByButtonIndex(int index, const std::string& input)
{
    switch(index) {
        case 0: p1Run = input; break;
        case 1: p1Jump = input; break;
        case 2: p2Run = input; break;
        case 3: p2Jump = input; break;
    }
}

size_t KonamiHyperShotInfo::bindingCount() const
{
    return BUTTONS.size();
}

const char* KonamiHyperShotInfo::bindingLabel(size_t index) const
{
    return BUTTONS[index];
}

const std::string& KonamiHyperShotInfo::getBinding(size_t index) const
{
    return const_cast<KonamiHyperShotInfo*>(this)->getByButtonName(BUTTONS[index]);
}

void KonamiHyperShotInfo::setBinding(size_t index, const std::string& input)
{
    setByButtonIndex(static_cast<int>(index), input);
}

void SystemInputInfo::mapInit()
{
    if(m_map != nullptr) return;

    m_map = new std::map<const std::string, std::string*>;
    m_map->insert(std::make_pair(BUTTONS[0], &saveState));
    m_map->insert(std::make_pair(BUTTONS[1], &loadState));
    m_map->insert(std::make_pair(BUTTONS[2], &rewind));
    m_map->insert(std::make_pair(BUTTONS[3], &speed));
}

SystemInputInfo::SystemInputInfo() = default;

SystemInputInfo::~SystemInputInfo()
{
    if(m_map != nullptr) delete m_map;
}

SystemInputInfo::SystemInputInfo(const SystemInputInfo& other)
{
    *this = other;
}

SystemInputInfo& SystemInputInfo::operator=(const SystemInputInfo& other)
{
    saveState = other.saveState;
    loadState = other.loadState;
    rewind = other.rewind;
    speed = other.speed;
    return *this;
}

const std::string& SystemInputInfo::getByButtonName(const std::string& name)
{
    static std::string empty = "";

    mapInit();
    if(m_map->count(name)) {
        return *(*m_map)[name];
    }
    return empty;
}

void SystemInputInfo::setByButtonIndex(int index, const std::string& input)
{
    switch(index) {
        case 0: saveState = input; break;
        case 1: loadState = input; break;
        case 2: rewind = input; break;
        case 3: speed = input; break;
    }
}

size_t SystemInputInfo::bindingCount() const
{
    return BUTTONS.size();
}

const char* SystemInputInfo::bindingLabel(size_t index) const
{
    return BUTTONS[index];
}

const std::string& SystemInputInfo::getBinding(size_t index) const
{
    return const_cast<SystemInputInfo*>(this)->getByButtonName(BUTTONS[index]);
}

void SystemInputInfo::setBinding(size_t index, const std::string& input)
{
    setByButtonIndex(static_cast<int>(index), input);
}
