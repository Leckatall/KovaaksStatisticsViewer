#include "benchmark_manager_vm.h"

#include <algorithm>
#include <QDateTime>
#include <QTimeZone>
#include <QUuid>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>

#include "benchmark_format.h"

namespace ksv::presentation {
    namespace {
        QString idString(const auto &id) { return QString::fromStdString(id.value); }

        QVariantMap errorMap(const QString &message) {
            return QVariantMap{{"ok", false}, {"error", message}};
        }

        domain::BenchmarkColor toDomainColor(const QColor &color) {
            return {static_cast<uint8_t>(color.red()), static_cast<uint8_t>(color.green()),
                    static_cast<uint8_t>(color.blue()), static_cast<uint8_t>(color.alpha())};
        }

        QString editErrorText(domain::BenchmarkEditError error) {
            switch (error) {
                case domain::BenchmarkEditError::UnknownTier:
                    return QObject::tr("That tier no longer exists.");
                case domain::BenchmarkEditError::UnknownGroup:
                    return QObject::tr("That group no longer exists.");
                case domain::BenchmarkEditError::UnknownEntry:
                    return QObject::tr("That scenario no longer exists.");
                case domain::BenchmarkEditError::DuplicateScenarioName:
                    return QObject::tr("That scenario is already in this benchmark.");
                case domain::BenchmarkEditError::PositionOutOfRange:
                    return QObject::tr("That position is out of range.");
                case domain::BenchmarkEditError::MixedContent:
                    return QObject::tr("A category cannot hold both scenarios and subcategories.");
            }
            return {};
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

        QVariantList buildLibraryEntries(const std::optional<data::BenchmarkLibrarySnapshot> &snapshot) {
            QVariantList entries;
            if (!snapshot) return entries;
            for (const auto &entry: snapshot->entries) {
                QVariantMap row;
                row["filename"] = QString::fromStdString(entry.filename);
                if (const auto *loaded = std::get_if<data::LoadedBenchmark>(&entry.content)) {
                    row["id"] = idString(loaded->benchmark.id);
                    const auto name = QString::fromStdString(loaded->benchmark.name);
                    row["name"] = name.isEmpty() ? row["filename"] : name;
                    row["classification"] = loaded->completeness.completeness == domain::Completeness::Trackable
                                                ? QStringLiteral("Trackable")
                                                : QStringLiteral("Incomplete");
                    row["openable"] = true;
                } else {
                    const auto &problem = std::get<data::ProblemBenchmark>(entry.content);
                    row["id"] = problem.id ? idString(*problem.id) : QString();
                    const auto name = problem.displayName ? QString::fromStdString(*problem.displayName) : QString();
                    row["name"] = !name.isEmpty() ? name : row["filename"];
                    row["classification"] = problem.problem == data::BenchmarkFileProblem::Unsupported
                                                ? QStringLiteral("Unsupported")
                                                : QStringLiteral("Invalid");
                    row["openable"] = false;
                }
                // Problem entries have no trustworthy id+digest pair for the remove precondition,
                // so they are never offered for deletion.
                row["deletable"] = std::holds_alternative<data::LoadedBenchmark>(entry.content);
                entries.push_back(row);
            }
            return entries;
        }
    }

    BenchmarkManagerViewModel::BenchmarkManagerViewModel(
        std::shared_ptr<application::IBenchmarkManagerUseCase> useCase, QObject *parent)
        : QObject(parent), m_useCase(std::move(useCase)),
          m_idFactory([] {
              return QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
          }) {
        m_useCase->onChanged([this] { emitChanged(); });
        adaptState();
    }

    void BenchmarkManagerViewModel::emitChanged() {
        adaptState();
        emit draftChanged();
        emit libraryChanged();
    }

    void BenchmarkManagerViewModel::adaptState() {
        const application::BenchmarkManagerState &state = m_useCase->state();

        QString benchmarkName = m_draft ? QString::fromStdString(m_draft->name) : QString();
        QString draftId = m_draft ? idString(m_draft->id) : QString();

        QVariantList validationIssues;
        if (m_draft) {
            for (const auto &issue: m_draftCompleteness.issues)
                validationIssues.push_back(QVariantMap{
                    {"message", benchmarkIssueText(issue.code)},
                    {"targetId", QString::fromStdString(targetIdString(issue.target))},
                });
        }

        QVariantList tiers;
        if (m_draft) {
            for (const auto &tier: m_draft->tiers)
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
        auto root = buildTree(m_draft, m_draftResolutions);

        // A never-saved working copy has no token and can never be stale; otherwise the token is
        // stale unless the accepted snapshot still holds a loaded entry with the same filename and
        // digest it was admitted under.
        bool baselineStale = false;
        if (m_token) {
            baselineStale = true;
            if (state.library) {
                for (const auto &entry: state.library->entries) {
                    if (entry.filename != m_token->filename) continue;
                    const auto *loaded = std::get_if<data::LoadedBenchmark>(&entry.content);
                    baselineStale = !loaded || entry.digest != m_token->digest;
                    break;
                }
            }
        }

        m_baselineStale = baselineStale;
        m_benchmarkName = std::move(benchmarkName);
        m_draftId = std::move(draftId);
        m_validationIssues = std::move(validationIssues);
        m_tiers = std::move(tiers);
        m_scenarioCatalogue = std::move(scenarioCatalogue);
        m_libraryEntries = std::move(libraryEntries);
        m_root = std::move(root);
    }

    void BenchmarkManagerViewModel::adoptSeed(application::BenchmarkEditorSeed seed, bool fromLibrary,
                                              bool dirty) {
        m_draft = std::move(seed.benchmark);
        m_token = std::move(seed.token);
        m_baseline = dirty ? std::nullopt : std::optional<domain::Benchmark>(*m_draft);
        m_draftFromLibrary = fromLibrary;
        m_draftDirty = dirty;
        m_draftCompleteness = domain::validateBenchmark(*m_draft);
        m_draftResolutions = m_useCase->resolve(*m_draft);
        emitChanged();
    }

    void BenchmarkManagerViewModel::clearWorkingCopy() {
        m_draft.reset();
        m_baseline.reset();
        m_token.reset();
        m_draftCompleteness = {};
        m_draftResolutions.clear();
        m_draftDirty = false;
        m_draftFromLibrary = false;
        emitChanged();
    }

    QVariantMap BenchmarkManagerViewModel::applyEdit(
        const std::function<domain::BenchmarkEditResult(domain::BenchmarkEditor &)> &op) {
        if (!m_draft) return errorMap(tr("No benchmark is open."));
        domain::BenchmarkEditor editor{*m_draft, m_idFactory};
        const auto result = op(editor);
        if (result.error) return errorMap(editErrorText(*result.error));
        recomputeAfterLocalEdit();
        QVariantMap map{{"ok", true}};
        if (result.createdTier) map["createdId"] = idString(*result.createdTier);
        else if (result.createdGroup) map["createdId"] = idString(*result.createdGroup);
        else if (result.createdEntry) map["createdId"] = idString(*result.createdEntry);
        return map;
    }

    void BenchmarkManagerViewModel::recomputeAfterLocalEdit() {
        m_draftCompleteness = domain::validateBenchmark(*m_draft);
        m_draftDirty = !m_baseline || *m_draft != *m_baseline;
        m_draftResolutions = m_useCase->resolve(*m_draft);
        emitChanged();
    }

    void BenchmarkManagerViewModel::beginNewBenchmark() {
        adoptSeed(m_useCase->beginNewBenchmark(), false, true);
    }

    bool BenchmarkManagerViewModel::openBenchmark(const QString &id) {
        auto seed = m_useCase->openBenchmark(domain::BenchmarkId{id.toStdString()});
        if (!seed) return false;
        const bool fromLibrary = seed->token.has_value();
        adoptSeed(std::move(*seed), fromLibrary, false);
        return true;
    }

    void BenchmarkManagerViewModel::discard() {
        m_useCase->closeEditor();
        clearWorkingCopy();
    }

    QVariantMap BenchmarkManagerViewModel::importPlaylist(const QUrl &file) {
        auto result = m_useCase->importPlaylistSeed(file.toLocalFile().toStdString());
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
        adoptSeed(std::move(*result.seed), false, true);
        QVariantList skipped;
        skipped.reserve(static_cast<qsizetype>(result.skippedDuplicateIndices.size()));
        for (const int index: result.skippedDuplicateIndices) skipped.push_back(index);
        return QVariantMap{{"ok", true}, {"skipped", skipped}};
    }

    QVariantMap BenchmarkManagerViewModel::save() {
        if (!m_draft) return errorMap(tr("No benchmark is open."));
        const auto outcome = m_useCase->save(*m_draft, m_token);
        if (outcome.error) {
            switch (*outcome.error) {
                case data::BenchmarkSaveError::EmptyName:
                    return errorMap(tr("Give the benchmark a name before saving."));
                case data::BenchmarkSaveError::UnknownTarget:
                    return errorMap(tr("That benchmark is no longer in the library."));
                case data::BenchmarkSaveError::StaleToken:
                case data::BenchmarkSaveError::Conflict:
                    return errorMap(tr("The file changed on disk. Refresh the library before saving."));
                case data::BenchmarkSaveError::WriteFailed:
                    return errorMap(tr("Saving failed. The stored benchmark is unchanged."));
            }
            return errorMap(tr("Saving failed."));
        }
        m_token = outcome.token;
        m_baseline = *m_draft;
        m_draftDirty = false;
        m_draftFromLibrary = true;
        emitChanged();
        return QVariantMap{{"ok", true}};
    }

    QVariantMap BenchmarkManagerViewModel::deleteBenchmark(const QString &id) {
        const domain::BenchmarkId target{id.toStdString()};
        std::optional<data::BenchmarkEditToken> token;
        if (m_token && m_token->id == target) {
            token = m_token;
        } else if (const auto &library = m_useCase->state().library) {
            for (const auto &entry: library->entries) {
                const auto *loaded = std::get_if<data::LoadedBenchmark>(&entry.content);
                if (loaded && loaded->benchmark.id == target) {
                    token = data::BenchmarkEditToken{target, entry.filename, entry.digest};
                    break;
                }
            }
        }
        if (!token) return errorMap(tr("That benchmark no longer exists in the library."));
        const auto outcome = m_useCase->deleteBenchmark(*token);
        if (outcome.error) {
            switch (*outcome.error) {
                case data::BenchmarkRemoveError::UnknownTarget:
                    return errorMap(tr("That benchmark no longer exists in the library."));
                case data::BenchmarkRemoveError::StaleToken:
                case data::BenchmarkRemoveError::Conflict:
                    return errorMap(tr("The file changed on disk. Refresh the library before deleting."));
                case data::BenchmarkRemoveError::WriteFailed:
                    return errorMap(tr("Deleting failed. The stored benchmark is unchanged."));
            }
            return errorMap(tr("Deleting failed."));
        }
        return QVariantMap{{"ok", true}};
    }

    void BenchmarkManagerViewModel::setBenchmarkName(const QString &name) {
        applyEdit([&](domain::BenchmarkEditor &e) { return e.renameBenchmark(name.toStdString()); });
    }

    void BenchmarkManagerViewModel::refresh() { m_useCase->refresh(); }

    QVariantMap BenchmarkManagerViewModel::addTier(const QString &name) {
        return applyEdit([&](domain::BenchmarkEditor &e) { return e.addTier(name.toStdString()); });
    }

    QVariantMap BenchmarkManagerViewModel::renameTier(const QString &id, const QString &name) {
        return applyEdit([&](domain::BenchmarkEditor &e) {
            return e.renameTier(domain::TierId{id.toStdString()}, name.toStdString());
        });
    }

    QVariantMap BenchmarkManagerViewModel::setTierColor(const QString &id, const QColor &color) {
        return applyEdit([&](domain::BenchmarkEditor &e) {
            return e.setTierColor(domain::TierId{id.toStdString()}, toDomainColor(color));
        });
    }

    QVariantMap BenchmarkManagerViewModel::reorderTier(const QString &id, int position) {
        return applyEdit([&](domain::BenchmarkEditor &e) {
            return e.reorderTier(domain::TierId{id.toStdString()},
                                 static_cast<std::size_t>(std::max(position, 0)));
        });
    }

    QVariantMap BenchmarkManagerViewModel::removeTier(const QString &id) {
        return applyEdit([&](domain::BenchmarkEditor &e) {
            return e.removeTier(domain::TierId{id.toStdString()});
        });
    }

    QVariantMap BenchmarkManagerViewModel::addUnplayedScenario(const QString &name) {
        return applyEdit([&](domain::BenchmarkEditor &e) {
            return e.addUnplayedScenario(name.toStdString());
        });
    }

    QVariantMap BenchmarkManagerViewModel::addKnownScenario(const QString &name, const QString &hash) {
        return applyEdit([&](domain::BenchmarkEditor &e) {
            return e.addKnownScenario(name.toStdString(), hash.toStdString());
        });
    }

    QVariantMap BenchmarkManagerViewModel::renameScenario(const QString &id, const QString &name) {
        return applyEdit([&](domain::BenchmarkEditor &e) {
            return e.renameScenario(domain::ScenarioEntryId{id.toStdString()}, name.toStdString());
        });
    }

    QVariantMap BenchmarkManagerViewModel::setScenarioHash(const QString &entryId, const QString &hash) {
        return applyEdit([&](domain::BenchmarkEditor &e) {
            return e.setScenarioHash(
                domain::ScenarioEntryId{entryId.toStdString()},
                hash.isEmpty() ? std::nullopt : std::optional{hash.toStdString()});
        });
    }

    QVariantMap BenchmarkManagerViewModel::removeScenario(const QString &id) {
        return applyEdit([&](domain::BenchmarkEditor &e) {
            return e.removeScenario(domain::ScenarioEntryId{id.toStdString()});
        });
    }

    QVariantMap BenchmarkManagerViewModel::setThreshold(const QString &entryId, const QString &tierId,
                                                        double score) {
        return applyEdit([&](domain::BenchmarkEditor &e) {
            return e.setThreshold(domain::ScenarioEntryId{entryId.toStdString()},
                                  domain::TierId{tierId.toStdString()}, score);
        });
    }

    QVariantMap BenchmarkManagerViewModel::clearThreshold(const QString &entryId, const QString &tierId) {
        return applyEdit([&](domain::BenchmarkEditor &e) {
            return e.clearThreshold(domain::ScenarioEntryId{entryId.toStdString()},
                                    domain::TierId{tierId.toStdString()});
        });
    }

    QVariantMap BenchmarkManagerViewModel::addCategory(const QString &name) {
        return applyEdit([&](domain::BenchmarkEditor &e) { return e.addCategory(name.toStdString()); });
    }

    QVariantMap BenchmarkManagerViewModel::renameGroup(const QString &id, const QString &name) {
        return applyEdit([&](domain::BenchmarkEditor &e) {
            return e.renameGroup(domain::GroupId{id.toStdString()}, name.toStdString());
        });
    }

    QVariantMap BenchmarkManagerViewModel::setGroupColor(const QString &id, const QColor &color) {
        return applyEdit([&](domain::BenchmarkEditor &e) {
            return e.setGroupColor(domain::GroupId{id.toStdString()}, toDomainColor(color));
        });
    }

    QVariantMap BenchmarkManagerViewModel::reorderCategory(const QString &id, int position) {
        return applyEdit([&](domain::BenchmarkEditor &e) {
            return e.reorderCategory(domain::GroupId{id.toStdString()},
                                     static_cast<std::size_t>(std::max(position, 0)));
        });
    }

    QVariantMap BenchmarkManagerViewModel::removeCategory(const QString &id) {
        return applyEdit([&](domain::BenchmarkEditor &e) {
            return e.removeCategory(domain::GroupId{id.toStdString()});
        });
    }

    QVariantMap BenchmarkManagerViewModel::addSubcategory(const QString &categoryId, const QString &name) {
        return applyEdit([&](domain::BenchmarkEditor &e) {
            return e.addSubcategory(domain::GroupId{categoryId.toStdString()}, name.toStdString());
        });
    }

    QVariantMap BenchmarkManagerViewModel::moveScenario(const QString &entryId, const QString &targetGroupId) {
        const domain::EditorGroupTarget target =
            targetGroupId.isEmpty()
                ? domain::EditorGroupTarget{std::monostate{}}
                : domain::EditorGroupTarget{domain::GroupId{targetGroupId.toStdString()}};
        return applyEdit([&](domain::BenchmarkEditor &e) {
            return e.moveScenario(domain::ScenarioEntryId{entryId.toStdString()}, target);
        });
    }
}
