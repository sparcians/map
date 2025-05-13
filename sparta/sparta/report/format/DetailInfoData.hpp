#pragma once

#include <functional>
#include <string>
#include <vector>
#include <utility>

namespace sparta::report::format
{
    using StringPair = std::pair<std::string, std::string>;

    struct info_data {
        std::string name;
        std::string desc;
        uint64_t vis;
        uint64_t n_class;
        std::vector<StringPair> metadata;

        bool operator==(const info_data& other) const
        {
            return name == other.name && desc == other.desc &&
                   vis == other.vis && n_class == other.n_class &&
                   metadata == other.metadata;
        }
    };
} // end namespace sparta::report::format

// Hash combine helper
template <typename T>
inline void hash_combine(std::size_t& seed, const T& val) {
    seed ^= std::hash<T>{}(val) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

namespace std
{
    template <>
    struct hash<sparta::report::format::StringPair> {
        std::size_t operator()(const sparta::report::format::StringPair& p) const {
            std::size_t h = 0;
            hash_combine(h, p.first);
            hash_combine(h, p.second);
            return h;
        }
    };

    template <>
    struct hash<sparta::report::format::info_data> {
        std::size_t operator()(const sparta::report::format::info_data& data) const {
            std::size_t seed = 0;
            hash_combine(seed, data.name);
            hash_combine(seed, data.desc);
            hash_combine(seed, data.vis);
            hash_combine(seed, data.n_class);
            for (const auto& pair : data.metadata) {
                hash_combine(seed, pair);
            }
            return seed;
        }
    };

} // end namespace std
