#ifndef KOVAAKSSTATSVIEWER_BENCHMARK_IDS_H
#define KOVAAKSSTATSVIEWER_BENCHMARK_IDS_H

#include <compare>
#include <cstdint>
#include <string>

namespace ksv::domain {
    // Opaque immutable identity of a benchmark element. Identity is the string value
    // alone — display names and ordering never participate. The Tag parameter keeps
    // the four id kinds from being cross-assignable.
    template<class Tag>
    struct OpaqueId {
        std::string value;
        auto operator<=>(const OpaqueId &) const = default;
    };

    struct BenchmarkIdTag {};
    struct TierIdTag {};
    struct GroupIdTag {};
    struct ScenarioEntryIdTag {};

    using BenchmarkId = OpaqueId<BenchmarkIdTag>;
    using TierId = OpaqueId<TierIdTag>;
    using GroupId = OpaqueId<GroupIdTag>;
    using ScenarioEntryId = OpaqueId<ScenarioEntryIdTag>;

    struct BenchmarkColor {
        uint8_t red = 0;
        uint8_t green = 0;
        uint8_t blue = 0;
        uint8_t alpha = 255;
        auto operator<=>(const BenchmarkColor &) const = default;
    };
}

#endif // KOVAAKSSTATSVIEWER_BENCHMARK_IDS_H
