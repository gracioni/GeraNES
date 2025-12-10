#pragma once
#include <map>
#include <vector>
#include <type_traits>
#include <utility>

template<typename Map>
auto map_to_array(const Map& m)
{
    using K = typename Map::key_type;
    using V = typename Map::mapped_type;

    using KeyRaw = std::conditional_t<std::is_enum_v<K>, std::underlying_type_t<K>, K>;
    using ValRaw = std::conditional_t<std::is_enum_v<V>, std::underlying_type_t<V>, V>;

    using PairRaw = std::pair<KeyRaw, ValRaw>;

    std::vector<PairRaw> out;
    out.reserve(m.size());
    for (const auto& kv : m) {
        out.emplace_back(static_cast<KeyRaw>(kv.first),
                         static_cast<ValRaw>(kv.second));
    }
    return out;
}

// Converte std::vector<PairRaw> -> std::map<K,V>
// Não tenta chamar reserve() em std::map.
template<typename PairRaw, typename Map>
void array_to_map(const std::vector<PairRaw>& arr, Map& m)
{
    using K = typename Map::key_type;
    using V = typename Map::mapped_type;

    m.clear();
    for (const auto& p : arr) {
        K k = static_cast<K>(p.first);
        V v = static_cast<V>(p.second);
        m.emplace(k, v);
    }
}
