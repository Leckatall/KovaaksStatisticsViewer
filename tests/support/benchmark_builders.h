#ifndef KOVAAKSSTATSVIEWER_TESTS_BENCHMARK_BUILDERS_H
#define KOVAAKSSTATSVIEWER_TESTS_BENCHMARK_BUILDERS_H

#include <algorithm>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "benchmarks/benchmark_projection.h"
#include "run.h"

namespace ksv::tests_support {
    inline domain::Tier benchmarkTier(const std::string &id) { return {domain::TierId{id}, id, {}}; }

    inline domain::ScenarioEntry benchmarkEntry(
        const std::string &id, const std::string &name, std::optional<std::string> hash,
        const std::vector<std::pair<std::string, double> > &thresholds) {
        domain::ScenarioEntry entry{domain::ScenarioEntryId{id}, name, std::move(hash), {}};
        for (const auto &[tier, score]: thresholds) entry.thresholds.push_back({domain::TierId{tier}, score});
        return entry;
    }

    inline domain::ScenarioResolution resolvedEntry(const std::string &id, const std::string &hash) {
        return {domain::ScenarioEntryId{id}, domain::ScenarioMatchState::Resolved, hash, {}};
    }
    inline domain::ScenarioResolution unresolvedEntry(const std::string &id) {
        return {domain::ScenarioEntryId{id}, domain::ScenarioMatchState::Unresolved, std::nullopt, {}};
    }
    inline domain::ScenarioResolution mappedUnavailableEntry(const std::string &id, const std::string &hash) {
        return {domain::ScenarioEntryId{id}, domain::ScenarioMatchState::MappedUnavailable, hash, {}};
    }
    inline domain::ScenarioResolution ambiguousEntry(const std::string &id, const std::vector<std::string> &hashes) {
        domain::ScenarioResolution value{domain::ScenarioEntryId{id}, domain::ScenarioMatchState::Ambiguous,
                                         std::nullopt, {}};
        for (const auto &hash: hashes) value.candidates.push_back({hash, 1, std::nullopt});
        return value;
    }
    inline domain::ScenarioResolution autoMappableEntry(const std::string &id, const std::string &hash) {
        domain::ScenarioResolution value{domain::ScenarioEntryId{id}, domain::ScenarioMatchState::AutoMappable,
                                         std::nullopt, {}};
        value.candidates.push_back({hash, 1, std::nullopt});
        return value;
    }
    inline domain::RunFact benchmarkFact(const std::string &hash, long long startTime, float score,
                                         float duration = 0.0F) {
        return {{{"Scenario " + hash, hash}, startTime}, score, duration};
    }
    inline const domain::ScenarioProjection &scenarioNamed(const domain::BenchmarkProjection &projection,
                                                           const std::string &id) {
        const auto found = std::ranges::find_if(projection.scenarios, [&](const auto &scenario) {
            return scenario.entryId.value == id;
        });
        if (found == projection.scenarios.end()) throw std::out_of_range("no scenario projection " + id);
        return *found;
    }
}

#endif // KOVAAKSSTATSVIEWER_TESTS_BENCHMARK_BUILDERS_H
