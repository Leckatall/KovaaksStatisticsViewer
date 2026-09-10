#include "benchmark_manager_vm.h"

#include <algorithm>
#include <QDateTime>
#include <QTimeZone>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>

#include "benchmark_format.h"

namespace ksv::presentation {
    namespace {
        QString idString(const auto &id) { return QString::fromStdString(id.value); }

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

        QString matchStateText(const domain::ScenarioMatchState state) {
            switch (state) {
                case domain::ScenarioMatchState::Resolved: return QStringLiteral("resolved");
                case domain::ScenarioMatchState::MappedUnavailable: return QStringLiteral("mappedUnavailable");
                case domain::ScenarioMatchState::Unresolved: return QStringLiteral("unresolved");
                case domain::ScenarioMatchState::Ambiguous: return QStringLiteral("ambiguous");
                case domain::ScenarioMatchState::AutoMappable: return QStringLiteral("autoMappable");
            }
            return QStringLiteral("unresolved");
        }

        QVariantList candidateList(const std::vector<domain::ScenarioCandidate> &candidates) {
            QVariantList list;
            list.reserve(static_cast<qsizetype>(candidates.size()));
            for (const auto &candidate: candidates) {
                const auto played = candidate.lastPlayed
                    ? QDateTime::fromSecsSinceEpoch(candidate.lastPlayed->time_since_epoch().count(),
                                                    QTimeZone::utc())
                    : QDateTime();
                list.push_back(QVariantMap{
                    {"hash", QString::fromStdString(candidate.hash)},
                    {"runCount", candidate.runCount},
                    {"lastPlayed", played},
                });
            }
            return list;
        }

        std::unique_ptr<BenchmarkScenarioNode> makeScenarioNode(
            const domain::ScenarioEntry &entry, const std::vector<domain::Tier> &tiers,
            const domain::ScenarioResolution *resolution) {
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
                                                           entry.hash.has_value(), std::move(thresholds),
                                                           matchStateText(resolution ? resolution->state
                                                                                     : domain::ScenarioMatchState::Unresolved),
                                                           resolution ? candidateList(resolution->candidates)
                                                                      : QVariantList{});
        }

        std::unique_ptr<BenchmarkGroupNode> buildTree(
            const std::optional<domain::Benchmark> &draft,
            const std::vector<domain::ScenarioResolution> &resolutions) {
            std::unordered_map<std::string, const domain::ScenarioResolution *> byEntry;
            for (const auto &resolution: resolutions) byEntry.emplace(resolution.entryId.value, &resolution);
            const auto lookup = [&](const domain::ScenarioEntry &entry) -> const domain::ScenarioResolution * {
                const auto found = byEntry.find(entry.id.value);
                return found == byEntry.end() ? nullptr : found->second;
            };
            auto root = std::make_unique<BenchmarkGroupNode>(QString(), QStringLiteral("uncategorized"),
                                                             QObject::tr("Uncategorized"), QColor());
            if (draft) {
                for (const auto &entry: draft->uncategorized)
                    root->appendChild(makeScenarioNode(entry, draft->tiers, lookup(entry)));
                for (const auto &category: draft->categories) {
                    auto categoryNode = std::make_unique<BenchmarkGroupNode>(
                        idString(category.id), QStringLiteral("category"),
                        QString::fromStdString(category.name), toQColor(category.color));
                    for (const auto &entry: category.scenarios)
                        categoryNode->appendChild(makeScenarioNode(entry, draft->tiers, lookup(entry)));
                    for (const auto &sub: category.subcategories) {
                        auto subNode = std::make_unique<BenchmarkGroupNode>(
                            idString(sub.id), QStringLiteral("subcategory"),
                            QString::fromStdString(sub.name), toQColor(sub.color));
                        for (const auto &entry: sub.scenarios)
                            subNode->appendChild(makeScenarioNode(entry, draft->tiers, lookup(entry)));
                        categoryNode->appendChild(std::move(subNode));
                    }
                    root->appendChild(std::move(categoryNode));
                }
            }
            return root;
        }

        QVariantList buildLibraryEntries(const std::optional<application::BenchmarkLibrarySnapshot> &snapshot) {
            QVariantList entries;
            if (!snapshot) return entries;
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
                entries.push_back(row);
            }
            return entries;
        }
    }

    BenchmarkManagerViewModel::BenchmarkManagerViewModel(
        std::shared_ptr<application::IBenchmarkManagerUseCase> useCase, QObject *parent)
        : QObject(parent), m_useCase(std::move(useCase)) {
        m_useCase->onChanged([this] {
            adaptState();
            emit draftChanged();
            emit libraryChanged();
        });
        adaptState();
    }

    void BenchmarkManagerViewModel::adaptState() {
        const application::BenchmarkManagerState &state = m_useCase->state();

        QString benchmarkName = state.draft ? QString::fromStdString(state.draft->name) : QString();
        QString draftId = state.draft ? idString(state.draft->id) : QString();

        QVariantList validationIssues;
        if (state.draft) {
            for (const auto &issue: state.draftCompleteness.issues)
                validationIssues.push_back(QVariantMap{
                    {"message", benchmarkIssueText(issue.code)},
                    {"targetId", QString::fromStdString(targetIdString(issue.target))},
                });
        }

        QVariantList tiers;
        if (state.draft) {
            for (const auto &tier: state.draft->tiers)
                tiers.push_back(QVariantMap{
                    {"id", idString(tier.id)},
                    {"name", QString::fromStdString(tier.name)},
                    {"color", toQColor(tier.color)},
                });
        }

        QVariantList scenarioCatalogue;
        for (const auto &scenario: state.scenarioCatalogue)
            scenarioCatalogue.push_back(QVariantMap{
                {"name", QString::fromStdString(scenario.name)},
                {"hash", QString::fromStdString(scenario.hash)},
            });

        QVariantList libraryEntries = buildLibraryEntries(state.library);
        auto root = buildTree(state.draft, state.draftResolutions);

        // One notification installs one coherent revision: every projection is built from the
        // single state() read above, then swapped in together before any signal is emitted, so a
        // QML re-read never sees a half-updated mix of old and new fields.
        m_benchmarkName = std::move(benchmarkName);
        m_draftId = std::move(draftId);
        m_validationIssues = std::move(validationIssues);
        m_tiers = std::move(tiers);
        m_scenarioCatalogue = std::move(scenarioCatalogue);
        m_libraryEntries = std::move(libraryEntries);
        m_root = std::move(root);
    }

    void BenchmarkManagerViewModel::beginNewBenchmark() {
        m_useCase->beginNewDraft();
    }

    bool BenchmarkManagerViewModel::openBenchmark(const QString &id) {
        return m_useCase->openDraft(domain::BenchmarkId{id.toStdString()});
    }

    void BenchmarkManagerViewModel::discard() {
        m_useCase->discardDraft();
    }

    QVariantMap BenchmarkManagerViewModel::importPlaylist(const QUrl &file) {
        const auto result = m_useCase->importPlaylist(file.toLocalFile().toStdString());
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
        const auto result = m_useCase->saveDraft();
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
        const auto result = m_useCase->deleteBenchmark(domain::BenchmarkId{id.toStdString()});
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
        m_useCase->renameBenchmark(name.toStdString());
    }

    void BenchmarkManagerViewModel::refresh() {
        m_useCase->refresh();
    }

    QVariantMap BenchmarkManagerViewModel::addTier(const QString &name) {
        return toResultMap(m_useCase->addTier(name.toStdString()));
    }

    QVariantMap BenchmarkManagerViewModel::renameTier(const QString &id, const QString &name) {
        return toResultMap(m_useCase->renameTier(domain::TierId{id.toStdString()}, name.toStdString()));
    }

    QVariantMap BenchmarkManagerViewModel::setTierColor(const QString &id, const QColor &color) {
        return toResultMap(m_useCase->setTierColor(domain::TierId{id.toStdString()}, toDomainColor(color)));
    }

    QVariantMap BenchmarkManagerViewModel::reorderTier(const QString &id, int position) {
        return toResultMap(m_useCase->reorderTier(domain::TierId{id.toStdString()},
                                                  static_cast<size_t>(std::max(position, 0))));
    }

    QVariantMap BenchmarkManagerViewModel::removeTier(const QString &id) {
        return toResultMap(m_useCase->removeTier(domain::TierId{id.toStdString()}));
    }

    QVariantMap BenchmarkManagerViewModel::addUnplayedScenario(const QString &name) {
        return toResultMap(m_useCase->addUnplayedScenario(name.toStdString()));
    }

    QVariantMap BenchmarkManagerViewModel::addKnownScenario(const QString &name, const QString &hash) {
        return toResultMap(m_useCase->addKnownScenario(name.toStdString(), hash.toStdString()));
    }

    QVariantMap BenchmarkManagerViewModel::renameScenario(const QString &id, const QString &name) {
        return toResultMap(m_useCase->renameScenario(domain::ScenarioEntryId{id.toStdString()},
                                                     name.toStdString()));
    }

    QVariantMap BenchmarkManagerViewModel::setScenarioHash(const QString &entryId, const QString &hash) {
        return toResultMap(m_useCase->setScenarioHash(
            domain::ScenarioEntryId{entryId.toStdString()},
            hash.isEmpty() ? std::nullopt : std::optional{hash.toStdString()}));
    }

    QVariantMap BenchmarkManagerViewModel::removeScenario(const QString &id) {
        return toResultMap(m_useCase->removeScenario(domain::ScenarioEntryId{id.toStdString()}));
    }

    QVariantMap BenchmarkManagerViewModel::setThreshold(const QString &entryId, const QString &tierId,
                                                        double score) {
        return toResultMap(m_useCase->setThreshold(domain::ScenarioEntryId{entryId.toStdString()},
                                                   domain::TierId{tierId.toStdString()}, score));
    }

    QVariantMap BenchmarkManagerViewModel::clearThreshold(const QString &entryId, const QString &tierId) {
        return toResultMap(m_useCase->clearThreshold(domain::ScenarioEntryId{entryId.toStdString()},
                                                     domain::TierId{tierId.toStdString()}));
    }

    QVariantMap BenchmarkManagerViewModel::addCategory(const QString &name) {
        return toResultMap(m_useCase->addCategory(name.toStdString()));
    }

    QVariantMap BenchmarkManagerViewModel::renameGroup(const QString &id, const QString &name) {
        return toResultMap(m_useCase->renameGroup(domain::GroupId{id.toStdString()}, name.toStdString()));
    }

    QVariantMap BenchmarkManagerViewModel::setGroupColor(const QString &id, const QColor &color) {
        return toResultMap(m_useCase->setGroupColor(domain::GroupId{id.toStdString()}, toDomainColor(color)));
    }

    QVariantMap BenchmarkManagerViewModel::reorderCategory(const QString &id, int position) {
        return toResultMap(m_useCase->reorderCategory(domain::GroupId{id.toStdString()},
                                                      static_cast<size_t>(std::max(position, 0))));
    }

    QVariantMap BenchmarkManagerViewModel::removeCategory(const QString &id) {
        return toResultMap(m_useCase->removeCategory(domain::GroupId{id.toStdString()}));
    }

    QVariantMap BenchmarkManagerViewModel::addSubcategory(const QString &categoryId, const QString &name) {
        return toResultMap(m_useCase->addSubcategory(domain::GroupId{categoryId.toStdString()},
                                                     name.toStdString()));
    }

    QVariantMap BenchmarkManagerViewModel::moveScenario(const QString &entryId, const QString &targetGroupId) {
        const application::DraftGroupTarget target =
            targetGroupId.isEmpty()
                ? application::DraftGroupTarget{std::monostate{}}
                : application::DraftGroupTarget{domain::GroupId{targetGroupId.toStdString()}};
        return toResultMap(m_useCase->moveScenario(domain::ScenarioEntryId{entryId.toStdString()}, target));
    }
}
