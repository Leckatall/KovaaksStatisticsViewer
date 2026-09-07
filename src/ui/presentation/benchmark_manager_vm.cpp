#include "benchmark_manager_vm.h"

#include <algorithm>
#include <type_traits>
#include <utility>
#include <variant>

namespace ksv::presentation {
    namespace {
        QString idString(const auto &id) { return QString::fromStdString(id.value); }

        QColor toQColor(const domain::BenchmarkColor &color) {
            return {color.red, color.green, color.blue, color.alpha};
        }

        domain::BenchmarkColor toDomainColor(const QColor &color) {
            return {static_cast<uint8_t>(color.red()), static_cast<uint8_t>(color.green()),
                    static_cast<uint8_t>(color.blue()), static_cast<uint8_t>(color.alpha())};
        }

        QString draftErrorText(application::BenchmarkDraftError error) {
            switch (error) {
                case application::BenchmarkDraftError::NoDraft:
                    return QObject::tr("No benchmark is open.");
                case application::BenchmarkDraftError::UnknownTier:
                    return QObject::tr("That tier no longer exists.");
                case application::BenchmarkDraftError::UnknownGroup:
                    return QObject::tr("That group no longer exists.");
                case application::BenchmarkDraftError::UnknownEntry:
                    return QObject::tr("That scenario no longer exists.");
                case application::BenchmarkDraftError::DuplicateScenarioName:
                    return QObject::tr("That scenario is already in this benchmark.");
                case application::BenchmarkDraftError::PositionOutOfRange:
                    return QObject::tr("That position is out of range.");
                case application::BenchmarkDraftError::MixedContent:
                    return QObject::tr("A category cannot hold both scenarios and subcategories.");
            }
            return {};
        }

        QVariantMap errorMap(const QString &message) {
            return QVariantMap{{"ok", false}, {"error", message}};
        }

        QVariantMap toResultMap(const application::BenchmarkDraftResult &result) {
            if (result.error) return errorMap(draftErrorText(*result.error));
            QVariantMap map{{"ok", true}};
            if (result.createdTier) map["createdId"] = idString(*result.createdTier);
            else if (result.createdGroup) map["createdId"] = idString(*result.createdGroup);
            else if (result.createdEntry) map["createdId"] = idString(*result.createdEntry);
            return map;
        }

        // Only tree elements (tiers, groups, scenarios) are focusable from a validation issue;
        // a benchmark-wide issue carries no target the dialog can scroll to.
        std::string targetIdString(const domain::IssueTarget &target) {
            return std::visit([](const auto &value) -> std::string {
                using T = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<T, std::monostate> || std::is_same_v<T, domain::BenchmarkId>)
                    return {};
                else return value.value;
            }, target);
        }

        std::unique_ptr<BenchmarkScenarioNode> makeScenarioNode(const domain::ScenarioEntry &entry,
                                                                const std::vector<domain::Tier> &tiers) {
            QVariantList thresholds;
            thresholds.reserve(static_cast<qsizetype>(tiers.size()));
            for (const auto &tier: tiers) {
                const auto existing = std::ranges::find_if(
                    entry.thresholds,
                    [&](const domain::Threshold &threshold) { return threshold.tierId == tier.id; });
                thresholds.push_back(QVariantMap{
                    {"tierId", idString(tier.id)},
                    {"tierName", QString::fromStdString(tier.name)},
                    {"score", existing != entry.thresholds.end() ? existing->score : 0.0},
                    {"hasValue", existing != entry.thresholds.end()},
                });
            }
            return std::make_unique<BenchmarkScenarioNode>(idString(entry.id),
                                                           QString::fromStdString(entry.name),
                                                           entry.hash.has_value(), std::move(thresholds));
        }
    }

    BenchmarkManagerViewModel::BenchmarkManagerViewModel(
        std::shared_ptr<application::IBenchmarkLibraryService> service, QObject *parent)
        : QObject(parent), m_service(std::move(service)) {
        m_service->onDraftChanged([this] { rebuildDraftState(); });
        m_service->onChanged([this] {
            rebuildLibrary();
            emit libraryChanged();
        });
        rebuildDraftState();
        rebuildLibrary();
    }

    void BenchmarkManagerViewModel::rebuildDraftState() {
        const auto draft = m_service->draft();
        m_benchmarkName = draft ? QString::fromStdString(draft->name) : QString();
        m_draftId = draft ? idString(draft->id) : QString();

        m_validationIssues.clear();
        if (m_service->hasDraft()) {
            for (const auto &issue: m_service->draftValidation().issues)
                m_validationIssues.push_back(QVariantMap{
                    {"message", benchmarkIssueText(issue.code)},
                    {"targetId", QString::fromStdString(targetIdString(issue.target))},
                });
        }
        m_tiers.clear();
        if (draft) {
            for (const auto &tier: draft->tiers)
                m_tiers.push_back(QVariantMap{
                    {"id", idString(tier.id)},
                    {"name", QString::fromStdString(tier.name)},
                    {"color", toQColor(tier.color)},
                });
        }

        rebuildTree(draft);
        emit draftChanged();
    }

    void BenchmarkManagerViewModel::rebuildTree(const std::optional<domain::Benchmark> &draft) {
        auto rebuilt = std::make_unique<BenchmarkGroupNode>(QString(), QStringLiteral("uncategorized"),
                                                            tr("Uncategorized"), QColor());
        if (draft) {
            for (const auto &entry: draft->uncategorized)
                rebuilt->appendChild(makeScenarioNode(entry, draft->tiers));
            for (const auto &category: draft->categories) {
                auto categoryNode = std::make_unique<BenchmarkGroupNode>(
                    idString(category.id), QStringLiteral("category"),
                    QString::fromStdString(category.name), toQColor(category.color));
                for (const auto &entry: category.scenarios)
                    categoryNode->appendChild(makeScenarioNode(entry, draft->tiers));
                for (const auto &sub: category.subcategories) {
                    auto subNode = std::make_unique<BenchmarkGroupNode>(
                        idString(sub.id), QStringLiteral("subcategory"),
                        QString::fromStdString(sub.name), toQColor(sub.color));
                    for (const auto &entry: sub.scenarios)
                        subNode->appendChild(makeScenarioNode(entry, draft->tiers));
                    categoryNode->appendChild(std::move(subNode));
                }
                rebuilt->appendChild(std::move(categoryNode));
            }
        }
        // Move-assign so m_root points at the new tree before the old one is destroyed: QML
        // re-reads `root` on the draftChanged notification while stale delegates may still
        // touch the previous tree.
        m_root = std::move(rebuilt);
    }

    void BenchmarkManagerViewModel::rebuildLibrary() {
        m_libraryEntries.clear();
        const auto snapshot = m_service->snapshot();
        if (!snapshot) return;
        for (const auto &entry: snapshot->entries) {
            QVariantMap row;
            row["filename"] = QString::fromStdString(entry.filename);
            if (const auto *loaded = std::get_if<application::LoadedBenchmark>(&entry.content)) {
                row["id"] = idString(loaded->benchmark.id);
                const auto name = QString::fromStdString(loaded->benchmark.name);
                row["name"] = name.isEmpty() ? row["filename"] : name;
                row["classification"] = loaded->completeness.completeness == domain::Completeness::Trackable
                                            ? QStringLiteral("Trackable")
                                            : QStringLiteral("Incomplete");
                row["openable"] = true;
            } else {
                const auto &problem = std::get<application::ProblemBenchmark>(entry.content);
                row["id"] = problem.id ? idString(*problem.id) : QString();
                const auto name = problem.displayName ? QString::fromStdString(*problem.displayName) : QString();
                row["name"] = !name.isEmpty() ? name : row["filename"];
                row["classification"] = problem.problem == application::BenchmarkFileProblem::Unsupported
                                            ? QStringLiteral("Unsupported")
                                            : QStringLiteral("Invalid");
                row["openable"] = false;
            }
            // Problem entries have no trustworthy id+digest pair for the remove precondition,
            // so they are never offered for deletion.
            row["deletable"] = std::holds_alternative<application::LoadedBenchmark>(entry.content);
            m_libraryEntries.push_back(row);
        }
    }

    void BenchmarkManagerViewModel::beginNewBenchmark() {
        m_service->beginNewDraft();
    }

    bool BenchmarkManagerViewModel::openBenchmark(const QString &id) {
        return m_service->openDraft(domain::BenchmarkId{id.toStdString()});
    }

    void BenchmarkManagerViewModel::discard() {
        m_service->discardDraft();
    }

    QVariantMap BenchmarkManagerViewModel::importPlaylist(const QUrl &file) {
        const auto result = m_service->importPlaylist(file.toLocalFile().toStdString());
        if (result.failure) {
            switch (*result.failure) {
                case application::PlaylistImportFailure::FileUnreadable:
                    return errorMap(tr("The playlist file could not be read."));
                case application::PlaylistImportFailure::MalformedJson:
                    return errorMap(tr("That file is not a readable Kovaak's playlist."));
                case application::PlaylistImportFailure::NoUsableScenarioList:
                    return errorMap(tr("That playlist has no usable scenarios."));
            }
            return errorMap(tr("The playlist could not be imported."));
        }
        QVariantList skipped;
        skipped.reserve(static_cast<qsizetype>(result.skippedDuplicateIndices.size()));
        for (const int index: result.skippedDuplicateIndices) skipped.push_back(index);
        return QVariantMap{{"ok", true}, {"skipped", skipped}};
    }

    QVariantMap BenchmarkManagerViewModel::save() {
        const auto result = m_service->saveDraft();
        if (result.error) {
            switch (*result.error) {
                case application::BenchmarkSaveError::NoDraft:
                    return errorMap(tr("No benchmark is open."));
                case application::BenchmarkSaveError::EmptyName:
                    return errorMap(tr("Give the benchmark a name before saving."));
                case application::BenchmarkSaveError::Conflict:
                    return errorMap(tr("The file changed on disk. Refresh the library before saving."));
                case application::BenchmarkSaveError::WriteFailed:
                    return errorMap(tr("Saving failed. The stored benchmark is unchanged."));
            }
            return errorMap(tr("Saving failed."));
        }
        return QVariantMap{{"ok", true}};
    }

    QVariantMap BenchmarkManagerViewModel::deleteBenchmark(const QString &id) {
        const auto result = m_service->deleteBenchmark(domain::BenchmarkId{id.toStdString()});
        if (result.error) {
            switch (*result.error) {
                case application::BenchmarkDeleteError::NotFound:
                    return errorMap(tr("That benchmark no longer exists in the library."));
                case application::BenchmarkDeleteError::Conflict:
                    return errorMap(tr("The file changed on disk. Refresh the library before deleting."));
                case application::BenchmarkDeleteError::WriteFailed:
                    return errorMap(tr("Deleting failed. The stored benchmark is unchanged."));
            }
            return errorMap(tr("Deleting failed."));
        }
        return QVariantMap{{"ok", true}};
    }

    void BenchmarkManagerViewModel::setBenchmarkName(const QString &name) {
        m_service->renameBenchmark(name.toStdString());
    }

    void BenchmarkManagerViewModel::refresh() {
        m_service->refresh();
    }

    QVariantMap BenchmarkManagerViewModel::addTier(const QString &name) {
        return toResultMap(m_service->addTier(name.toStdString()));
    }

    QVariantMap BenchmarkManagerViewModel::renameTier(const QString &id, const QString &name) {
        return toResultMap(m_service->renameTier(domain::TierId{id.toStdString()}, name.toStdString()));
    }

    QVariantMap BenchmarkManagerViewModel::setTierColor(const QString &id, const QColor &color) {
        return toResultMap(m_service->setTierColor(domain::TierId{id.toStdString()}, toDomainColor(color)));
    }

    QVariantMap BenchmarkManagerViewModel::reorderTier(const QString &id, int position) {
        return toResultMap(m_service->reorderTier(domain::TierId{id.toStdString()},
                                                  static_cast<size_t>(std::max(position, 0))));
    }

    QVariantMap BenchmarkManagerViewModel::removeTier(const QString &id) {
        return toResultMap(m_service->removeTier(domain::TierId{id.toStdString()}));
    }

    QVariantMap BenchmarkManagerViewModel::addUnplayedScenario(const QString &name) {
        return toResultMap(m_service->addUnplayedScenario(name.toStdString()));
    }

    QVariantMap BenchmarkManagerViewModel::addKnownScenario(const QString &name, const QString &hash) {
        return toResultMap(m_service->addKnownScenario(name.toStdString(), hash.toStdString()));
    }

    QVariantMap BenchmarkManagerViewModel::renameScenario(const QString &id, const QString &name) {
        return toResultMap(m_service->renameScenario(domain::ScenarioEntryId{id.toStdString()},
                                                     name.toStdString()));
    }

    QVariantMap BenchmarkManagerViewModel::removeScenario(const QString &id) {
        return toResultMap(m_service->removeScenario(domain::ScenarioEntryId{id.toStdString()}));
    }

    QVariantMap BenchmarkManagerViewModel::setThreshold(const QString &entryId, const QString &tierId,
                                                        double score) {
        return toResultMap(m_service->setThreshold(domain::ScenarioEntryId{entryId.toStdString()},
                                                   domain::TierId{tierId.toStdString()}, score));
    }

    QVariantMap BenchmarkManagerViewModel::clearThreshold(const QString &entryId, const QString &tierId) {
        return toResultMap(m_service->clearThreshold(domain::ScenarioEntryId{entryId.toStdString()},
                                                     domain::TierId{tierId.toStdString()}));
    }

    QVariantMap BenchmarkManagerViewModel::addCategory(const QString &name) {
        return toResultMap(m_service->addCategory(name.toStdString()));
    }

    QVariantMap BenchmarkManagerViewModel::renameGroup(const QString &id, const QString &name) {
        return toResultMap(m_service->renameGroup(domain::GroupId{id.toStdString()}, name.toStdString()));
    }

    QVariantMap BenchmarkManagerViewModel::setGroupColor(const QString &id, const QColor &color) {
        return toResultMap(m_service->setGroupColor(domain::GroupId{id.toStdString()}, toDomainColor(color)));
    }

    QVariantMap BenchmarkManagerViewModel::reorderCategory(const QString &id, int position) {
        return toResultMap(m_service->reorderCategory(domain::GroupId{id.toStdString()},
                                                      static_cast<size_t>(std::max(position, 0))));
    }

    QVariantMap BenchmarkManagerViewModel::removeCategory(const QString &id) {
        return toResultMap(m_service->removeCategory(domain::GroupId{id.toStdString()}));
    }

    QVariantMap BenchmarkManagerViewModel::addSubcategory(const QString &categoryId, const QString &name) {
        return toResultMap(m_service->addSubcategory(domain::GroupId{categoryId.toStdString()},
                                                     name.toStdString()));
    }

    QVariantMap BenchmarkManagerViewModel::moveScenario(const QString &entryId, const QString &targetGroupId) {
        const application::DraftGroupTarget target =
            targetGroupId.isEmpty()
                ? application::DraftGroupTarget{std::monostate{}}
                : application::DraftGroupTarget{domain::GroupId{targetGroupId.toStdString()}};
        return toResultMap(m_service->moveScenario(domain::ScenarioEntryId{entryId.toStdString()}, target));
    }
}
