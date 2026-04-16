#pragma once

#include <array>
#include <map>
#include <string>

#include <nlohmann/json.hpp>

#include "GeraNESApp/InputBindingInfo.h"

class KonamiHyperShotInfo : public InputBindingInfo
{
public:
    static constexpr std::array<const char*, 4> BUTTONS {"P1 Run", "P1 Jump", "P2 Run", "P2 Jump"};

    std::string p1Run;
    std::string p1Jump;
    std::string p2Run;
    std::string p2Jump;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(KonamiHyperShotInfo, p1Run, p1Jump, p2Run, p2Jump)

private:
    std::map<const std::string, std::string*>* m_map = nullptr;

    void mapInit();

public:
    KonamiHyperShotInfo();
    ~KonamiHyperShotInfo() override;
    KonamiHyperShotInfo(const KonamiHyperShotInfo& other);
    KonamiHyperShotInfo& operator=(const KonamiHyperShotInfo& other);

    const std::string& getByButtonName(const std::string& name);
    void setByButtonIndex(int index, const std::string& input);

    size_t bindingCount() const override;
    const char* bindingLabel(size_t index) const override;
    const std::string& getBinding(size_t index) const override;
    void setBinding(size_t index, const std::string& input) override;
};
