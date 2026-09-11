#ifndef KOVAAKSSTATSVIEWER_BENCHMARK_MANAGER_VM_H
#define KOVAAKSSTATSVIEWER_BENCHMARK_MANAGER_VM_H

#include <QColor>
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include "app/contracts/benchmark_editor_seed.h"
#include "app/contracts/benchmark_manager_state.h"
#include "app/contracts/i_benchmark_manager_use_case.h"
#include "benchmarks/benchmark.h"
#include "benchmarks/benchmark_editor.h"
#include "benchmarks/benchmark_resolution.h"
#include "benchmarks/benchmark_validation.h"
#include "data/interfaces/i_benchmarks_service.h"
#include "presentation/benchmark_issue_text.h"
#include "presentation/benchmark_tree_node.h"

namespace ksv::presentation {
    // Adapts IBenchmarkManagerUseCase for the manager dialog. Accepted-library and resolution state
    // come from the use case's coherent BenchmarkManagerState; the editable working copy, its dirty
    // baseline, validation, and local editor mutations (over domain::BenchmarkEditor) are owned here.
    class BenchmarkManagerViewModel : public QObject {
        Q_OBJECT
        QML_ELEMENT
        Q_PROPERTY(bool hasDraft READ hasDraft NOTIFY draftChanged)
        Q_PROPERTY(bool dirty READ dirty NOTIFY draftChanged)
        // The accepted entry this editor's token addresses has changed digest, been removed, or
        // been reclassified as a problem entry since it was opened or last saved.
        Q_PROPERTY(bool baselineStale READ baselineStale NOTIFY draftChanged)
        Q_PROPERTY(QString benchmarkName READ benchmarkName NOTIFY draftChanged)
        // Empty when no draft is open; lets the dialog tell a delete that targets the draft
        // being edited from one that targets an unrelated library entry.
        Q_PROPERTY(QString draftId READ draftId NOTIFY draftChanged)
        Q_PROPERTY(bool draftTrackable READ draftTrackable NOTIFY draftChanged)
        Q_PROPERTY(bool draftFromLibrary READ draftFromLibrary NOTIFY draftChanged)
        Q_PROPERTY(QVariantList libraryEntries READ libraryEntries NOTIFY libraryChanged)
        Q_PROPERTY(QVariantList validationIssues READ validationIssues NOTIFY draftChanged)
        Q_PROPERTY(ksv::presentation::BenchmarkGroupNode *root READ root NOTIFY draftChanged)
        Q_PROPERTY(QVariantList tiers READ tiers NOTIFY draftChanged)
        Q_PROPERTY(bool refreshFailed READ refreshFailed NOTIFY libraryChanged)
        Q_PROPERTY(QVariantList scenarioCatalogue READ scenarioCatalogue NOTIFY libraryChanged)
        Q_PROPERTY(bool resolutionWriteFailed READ resolutionWriteFailed NOTIFY libraryChanged)
        Q_PROPERTY(QString managedDirectoryPath READ managedDirectoryPath CONSTANT)

    public:
        explicit BenchmarkManagerViewModel(std::shared_ptr<application::IBenchmarkManagerUseCase> useCase,
                                           QObject *parent = nullptr);

        [[nodiscard]] bool hasDraft() const { return m_draft.has_value(); }
        [[nodiscard]] bool dirty() const { return m_draftDirty; }
        [[nodiscard]] bool baselineStale() const { return m_baselineStale; }
        [[nodiscard]] bool draftFromLibrary() const { return m_draftFromLibrary; }
        [[nodiscard]] bool draftTrackable() const {
            return m_draft.has_value() &&
                   m_draftCompleteness.completeness == domain::Completeness::Trackable;
        }
        [[nodiscard]] const QString &benchmarkName() const { return m_benchmarkName; }
        [[nodiscard]] const QString &draftId() const { return m_draftId; }
        [[nodiscard]] const QVariantList &libraryEntries() const { return m_libraryEntries; }
        [[nodiscard]] const QVariantList &validationIssues() const { return m_validationIssues; }
        [[nodiscard]] BenchmarkGroupNode *root() const { return m_root.get(); }
        [[nodiscard]] const QVariantList &tiers() const { return m_tiers; }
        [[nodiscard]] bool refreshFailed() const { return m_useCase->state().refreshFailed; }
        [[nodiscard]] const QVariantList &scenarioCatalogue() const { return m_scenarioCatalogue; }
        [[nodiscard]] bool resolutionWriteFailed() const { return m_useCase->state().resolutionWriteFailed; }
        [[nodiscard]] QString managedDirectoryPath() const {
            return QString::fromStdString(m_useCase->state().managedDirectoryPath);
        }

        Q_INVOKABLE void beginNewBenchmark();
        Q_INVOKABLE bool openBenchmark(const QString &id);
        Q_INVOKABLE void discard();
        Q_INVOKABLE QVariantMap importPlaylist(const QUrl &file);
        Q_INVOKABLE QVariantMap save();
        Q_INVOKABLE QVariantMap deleteBenchmark(const QString &id);
        Q_INVOKABLE void setBenchmarkName(const QString &name);
        Q_INVOKABLE void refresh();

        Q_INVOKABLE QVariantMap addTier(const QString &name);
        Q_INVOKABLE QVariantMap renameTier(const QString &id, const QString &name);
        Q_INVOKABLE QVariantMap setTierColor(const QString &id, const QColor &color);
        Q_INVOKABLE QVariantMap reorderTier(const QString &id, int position);
        Q_INVOKABLE QVariantMap removeTier(const QString &id);
        Q_INVOKABLE QVariantMap addUnplayedScenario(const QString &name);
        Q_INVOKABLE QVariantMap addKnownScenario(const QString &name, const QString &hash);
        Q_INVOKABLE QVariantMap renameScenario(const QString &id, const QString &name);
        Q_INVOKABLE QVariantMap setScenarioHash(const QString &entryId, const QString &hash);
        Q_INVOKABLE QVariantMap removeScenario(const QString &id);
        Q_INVOKABLE QVariantMap setThreshold(const QString &entryId, const QString &tierId, double score);
        Q_INVOKABLE QVariantMap clearThreshold(const QString &entryId, const QString &tierId);
        Q_INVOKABLE QVariantMap addCategory(const QString &name);
        Q_INVOKABLE QVariantMap renameGroup(const QString &id, const QString &name);
        Q_INVOKABLE QVariantMap setGroupColor(const QString &id, const QColor &color);
        Q_INVOKABLE QVariantMap reorderCategory(const QString &id, int position);
        Q_INVOKABLE QVariantMap removeCategory(const QString &id);
        Q_INVOKABLE QVariantMap addSubcategory(const QString &categoryId, const QString &name);
        Q_INVOKABLE QVariantMap moveScenario(const QString &entryId, const QString &targetGroupId);

    signals:
        void draftChanged();
        void libraryChanged();

    private:
        void adaptState();
        void adoptSeed(application::BenchmarkEditorSeed seed, bool fromLibrary, bool dirty);
        void clearWorkingCopy();
        void emitChanged();
        // Runs `op` against a BenchmarkEditor bound to the local working copy, then rebuilds
        // validation, dirty state, resolution and projections. Returns the QML result map
        // (`ok` plus `createdId` / `error`).
        QVariantMap applyEdit(
            const std::function<domain::BenchmarkEditResult(domain::BenchmarkEditor &)> &op);
        void recomputeAfterLocalEdit();

        std::shared_ptr<application::IBenchmarkManagerUseCase> m_useCase;

        // Presentation-owned working copy. `m_baseline` is the last accepted content this editor
        // was seeded from or saved to; its absence means a never-saved copy, which is always dirty.
        std::optional<domain::Benchmark> m_draft;
        std::optional<domain::Benchmark> m_baseline;
        std::optional<data::BenchmarkEditToken> m_token;
        domain::CompletenessResult m_draftCompleteness;
        std::vector<domain::ScenarioResolution> m_draftResolutions;
        bool m_draftDirty = false;
        bool m_draftFromLibrary = false;
        bool m_baselineStale = false;
        std::function<std::string()> m_idFactory;

        QString m_benchmarkName;
        QString m_draftId;
        QVariantList m_libraryEntries;
        QVariantList m_validationIssues;
        QVariantList m_tiers;
        QVariantList m_scenarioCatalogue;
        std::unique_ptr<BenchmarkGroupNode> m_root;
    };
}

#endif
