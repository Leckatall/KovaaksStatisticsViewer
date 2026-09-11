#ifndef KOVAAKSSTATSVIEWER_BENCHMARK_EDITOR_H
#define KOVAAKSSTATSVIEWER_BENCHMARK_EDITOR_H

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <variant>
#include <utility>

#include "benchmark.h"
#include "benchmark_validation.h"

namespace ksv::domain {
    enum class BenchmarkEditError {
        UnknownTier, UnknownGroup, UnknownEntry,
        DuplicateScenarioName, PositionOutOfRange, MixedContent
    };

    struct BenchmarkEditResult {
        std::optional<BenchmarkEditError> error;
        std::optional<TierId> createdTier;
        std::optional<GroupId> createdGroup;
        std::optional<ScenarioEntryId> createdEntry;
        [[nodiscard]] bool ok() const { return !error.has_value(); }
    };

    // monostate targets the fixed Uncategorized collection; a GroupId targets a
    // category or subcategory.
    using EditorGroupTarget = std::variant<std::monostate, GroupId>;

    // Structural editing over a caller-owned Benchmark, mutated in place. Carries no
    // working-copy, dirty, persistence, or profile state: the caller recomputes dirty
    // and validation, and supplies element IDs through `idFactory` rather than the
    // editor generating them.
    class BenchmarkEditor {
    public:
        BenchmarkEditor(Benchmark &target, std::function<std::string()> idFactory)
            : m_target(target), m_idFactory(std::move(idFactory)) {}

        BenchmarkEditResult renameBenchmark(const std::string &name);
        BenchmarkEditResult addTier(const std::string &name);
        BenchmarkEditResult renameTier(const TierId &id, const std::string &name);
        BenchmarkEditResult setTierColor(const TierId &id, BenchmarkColor color);
        BenchmarkEditResult reorderTier(const TierId &id, std::size_t position);
        BenchmarkEditResult removeTier(const TierId &id);
        BenchmarkEditResult addUnplayedScenario(const std::string &name);
        BenchmarkEditResult addKnownScenario(const std::string &name, const std::string &hash);
        BenchmarkEditResult renameScenario(const ScenarioEntryId &id, const std::string &name);
        BenchmarkEditResult setScenarioHash(const ScenarioEntryId &id, const std::optional<std::string> &hash);
        BenchmarkEditResult removeScenario(const ScenarioEntryId &id);
        BenchmarkEditResult setThreshold(const ScenarioEntryId &entryId, const TierId &tierId, double score);
        BenchmarkEditResult clearThreshold(const ScenarioEntryId &entryId, const TierId &tierId);
        BenchmarkEditResult addCategory(const std::string &name);
        BenchmarkEditResult renameGroup(const GroupId &id, const std::string &name);
        BenchmarkEditResult setGroupColor(const GroupId &id, BenchmarkColor color);
        BenchmarkEditResult reorderCategory(const GroupId &id, std::size_t position);
        BenchmarkEditResult removeCategory(const GroupId &id);
        BenchmarkEditResult addSubcategory(const GroupId &categoryId, const std::string &name);
        BenchmarkEditResult moveScenario(const ScenarioEntryId &id, const EditorGroupTarget &to);

        [[nodiscard]] CompletenessResult validation() const { return validateBenchmark(m_target); }

    private:
        std::string newId() const { return m_idFactory(); }

        Benchmark &m_target;
        std::function<std::string()> m_idFactory;
    };
}

#endif // KOVAAKSSTATSVIEWER_BENCHMARK_EDITOR_H
