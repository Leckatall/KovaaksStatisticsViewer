#ifndef KOVAAKSSTATSVIEWER_BENCHMARK_EDITOR_H
#define KOVAAKSSTATSVIEWER_BENCHMARK_EDITOR_H

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <variant>
#include <utility>
#include <vector>

#include "benchmarks/benchmark_definition.h"
#include "benchmark_validation.h"

namespace ksv::domain {
    enum class BenchmarkEditError {
        UnknownTier, UnknownGroup, UnknownEntry,
        DuplicateScenarioName, PositionOutOfRange
    };

    struct BenchmarkEditResult {
        std::optional<BenchmarkEditError> error;
        std::optional<TierId> createdTier;
        std::optional<GroupId> createdGroup;
        std::optional<BenchmarkScenarioId> createdEntry;
        std::vector<BenchmarkScenarioId> createdEntries;
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
        BenchmarkEditor(BenchmarkDefinition &target, std::function<std::string()> idFactory)
            : m_target(target), m_idFactory(std::move(idFactory)) {}

        BenchmarkEditResult renameBenchmark(const std::string &name);
        BenchmarkEditResult addTier(const std::string &name);
        BenchmarkEditResult renameTier(const TierId &id, const std::string &name);
        BenchmarkEditResult setTierColor(const TierId &id, BenchmarkColor color);
        BenchmarkEditResult reorderTier(const TierId &id, std::size_t position);
        BenchmarkEditResult removeTier(const TierId &id);
        BenchmarkEditResult addUnplayedScenario(const std::string &name);
        BenchmarkEditResult addKnownScenario(const std::string &name, const std::string &hash);
        BenchmarkEditResult renameScenario(const BenchmarkScenarioId &id, const std::string &name);
        BenchmarkEditResult setScenarioHash(const BenchmarkScenarioId &id, const std::optional<std::string> &hash);
        BenchmarkEditResult setScenarioHashes(const BenchmarkScenarioId &id, const std::vector<std::string> &hashes);
        BenchmarkEditResult removeScenario(const BenchmarkScenarioId &id);
        BenchmarkEditResult setThreshold(const BenchmarkScenarioId &entryId, const TierId &tierId, double score);
        BenchmarkEditResult clearThreshold(const BenchmarkScenarioId &entryId, const TierId &tierId);
        BenchmarkEditResult addCategory(const std::string &name);
        BenchmarkEditResult renameGroup(const GroupId &id, const std::string &name);
        BenchmarkEditResult setGroupColor(const GroupId &id, BenchmarkColor color);
        BenchmarkEditResult reorderCategory(const GroupId &id, std::size_t position);
        BenchmarkEditResult removeCategory(const GroupId &id);
        BenchmarkEditResult addSubcategory(const GroupId &categoryId, const std::string &name);
        BenchmarkEditResult moveScenario(const BenchmarkScenarioId &id, const EditorGroupTarget &to);

        BenchmarkEditResult appendUnnamedScenarios(std::size_t count);
        // `orderedIds` is already in the caller's visible order and lands in that order: before
        // `before`, which must be in the target (when it is itself moving, before the next entry
        // that is not), or after the target's existing entries when absent.
        BenchmarkEditResult assignScenarios(const std::vector<BenchmarkScenarioId> &orderedIds,
                                            const EditorGroupTarget &to,
                                            const std::optional<BenchmarkScenarioId> &before = std::nullopt);
        BenchmarkEditResult reorderSubcategory(const GroupId &id, std::size_t position);
        // Moves an entry within the collection that already holds it.
        BenchmarkEditResult reorderScenario(const BenchmarkScenarioId &id, std::size_t position);

        [[nodiscard]] CompletenessResult validation() const { return validateBenchmark(m_target); }

    private:
        std::string newId() const { return m_idFactory(); }

        BenchmarkDefinition &m_target;
        std::function<std::string()> m_idFactory;
    };
}

#endif // KOVAAKSSTATSVIEWER_BENCHMARK_EDITOR_H
