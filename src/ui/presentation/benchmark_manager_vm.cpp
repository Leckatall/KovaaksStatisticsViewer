#include "benchmark_manager_vm.h"

#include <algorithm>
#include <QDateTime>
#include <QLocale>
#include <QTimeZone>
#include <QUuid>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <ranges>
#include <utility>
#include <variant>

#include "benchmark_cell_input.h"
#include "benchmark_clipboard_matrix.h"
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
                    return QObject::tr("A category cannot hold both scenarios and subcategories. Move its "
                                       "scenarios into a subcategory or to Uncategorized first.");
            }
            return {};
        }

        // An empty id targets Uncategorized.
        domain::EditorGroupTarget toGroupTarget(const QString &groupId) {
            if (groupId.isEmpty()) return std::monostate{};
            return domain::GroupId{groupId.toStdString()};
        }

        std::optional<std::size_t> tierPosition(const domain::Benchmark &draft, const domain::TierId &id) {
            const auto found = std::ranges::find_if(draft.tiers, [&](const domain::Tier &t) { return t.id == id; });
            if (found == draft.tiers.end()) return std::nullopt;
            return static_cast<std::size_t>(found - draft.tiers.begin());
        }

        // Only table elements (tiers, groups, scenarios) are focusable from a validation issue;
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

        using IssueCodes = std::vector<domain::BenchmarkIssueCode>;

        // Validation issues grouped by the element each addresses. Benchmark-wide issues have no
        // table location and stay in the dialog's issue list only.
        struct IssueIndex {
            std::unordered_map<std::string, IssueCodes> byTier;
            std::unordered_map<std::string, IssueCodes> byGroup;
            std::unordered_map<std::string, IssueCodes> byEntry;
            std::map<std::pair<std::string, std::string>, IssueCodes> byCell;

            template<class Map, class Key>
            static IssueCodes lookup(const Map &map, const Key &key) {
                const auto found = map.find(key);
                return found == map.end() ? IssueCodes{} : found->second;
            }
        };

        QStringList issueTexts(const IssueCodes &codes) {
            QStringList texts;
            for (const auto code: codes) texts.push_back(benchmarkIssueText(code));
            return texts;
        }

        IssueIndex indexIssues(const domain::CompletenessResult &completeness) {
            IssueIndex index;
            for (const auto &issue: completeness.issues) {
                std::visit([&](const auto &target) {
                    using T = std::decay_t<decltype(target)>;
                    if constexpr (std::is_same_v<T, domain::TierId>) index.byTier[target.value].push_back(issue.code);
                    else if constexpr (std::is_same_v<T, domain::GroupId>) index.byGroup[target.value].push_back(issue.code);
                    else if constexpr (std::is_same_v<T, domain::ScenarioEntryId>) {
                        if (issue.tierId) index.byCell[{target.value, issue.tierId->value}].push_back(issue.code);
                        else index.byEntry[target.value].push_back(issue.code);
                    }
                }, issue.target);
            }
            return index;
        }

        BenchmarkTableGroupSpan groupSpan(const auto &group, int start, const IssueIndex &issues) {
            return {idString(group.id), QString::fromStdString(group.name), toQColor(group.color), start, 0,
                    issueTexts(IssueIndex::lookup(issues.byGroup, group.id.value))};
        }

        BenchmarkTableGroupSpan noGroupAt(std::size_t row, const QString &name = {}) {
            return {{}, name, QColor(), static_cast<int>(row), 0, {}};
        }

        QString inputIssueText(CellParseOutcome outcome) {
            return outcome == CellParseOutcome::NonFinite
                       ? QObject::tr("Not a finite number. Saving stores this threshold as missing.")
                       : QObject::tr("Not a number. Saving stores this threshold as missing.");
        }

        BenchmarkTableProjection buildTableProjection(
            const domain::Benchmark &draft, const IssueIndex &issues,
            const std::vector<domain::ScenarioResolution> &resolutions,
            const std::map<BenchmarkCellKey, BenchmarkCellRecord> &inputs) {
            std::unordered_map<std::string, const domain::ScenarioResolution *> byEntry;
            for (const auto &resolution: resolutions) byEntry.emplace(resolution.entryId.value, &resolution);
            const QLocale locale;

            BenchmarkTableProjection projection;
            for (const auto &tier: draft.tiers)
                projection.tiers.push_back({idString(tier.id), QString::fromStdString(tier.name), toQColor(tier.color),
                                            issueTexts(IssueIndex::lookup(issues.byTier, tier.id.value))});

            const auto appendRow = [&](const domain::ScenarioEntry &entry, const BenchmarkTableGroupSpan &category,
                                       const BenchmarkTableGroupSpan &subcategory) {
                BenchmarkTableRow row;
                row.entryId = idString(entry.id);
                row.name = QString::fromStdString(entry.name);
                row.scenarioIssues = issueTexts(IssueIndex::lookup(issues.byEntry, entry.id.value));
                row.category = category;
                row.subcategory = subcategory;
                const auto resolution = byEntry.find(entry.id.value);
                row.mappingState = matchStateText(resolution == byEntry.end()
                                                      ? domain::ScenarioMatchState::Unresolved
                                                      : resolution->second->state);
                if (resolution != byEntry.end()) row.mappingCandidates = candidateList(resolution->second->candidates);
                for (const auto &tier: draft.tiers) {
                    BenchmarkTableCell cell;
                    const auto threshold = std::ranges::find_if(
                        entry.thresholds, [&](const domain::Threshold &t) { return t.tierId == tier.id; });
                    if (threshold != entry.thresholds.end()) {
                        cell.hasValue = true;
                        cell.displayText = BenchmarkCellInput::displayText(threshold->score, locale);
                        cell.editText = BenchmarkCellInput::editText(threshold->score, locale);
                    }
                    auto codes = IssueIndex::lookup(issues.byCell, std::pair{entry.id.value, tier.id.value});
                    const auto input = inputs.find({entry.id, tier.id});
                    if (input != inputs.end()) {
                        // The retained text explains the cell better than the derived "missing"
                        // label, which it replaces.
                        cell.displayText = cell.editText = input->second.rawText;
                        cell.inputState = input->second.outcome == CellParseOutcome::NonFinite
                                              ? QStringLiteral("nonFinite")
                                              : QStringLiteral("invalid");
                        std::erase(codes, domain::BenchmarkIssueCode::MissingThreshold);
                    }
                    cell.issues = issueTexts(codes);
                    if (input != inputs.end()) cell.issues.prepend(inputIssueText(input->second.outcome));
                    row.cells.push_back(std::move(cell));
                }
                projection.rows.push_back(std::move(row));
            };

            // Span lengths are only known once a group's rows are placed, so they are patched in.
            const auto closeSpan = [&](std::size_t first, BenchmarkTableGroupSpan BenchmarkTableRow::*member) {
                const int length = static_cast<int>(projection.rows.size() - first);
                for (auto i = first; i < projection.rows.size(); ++i) (projection.rows[i].*member).spanLength = length;
            };

            for (const auto &category: draft.categories) {
                const auto categoryFirst = projection.rows.size();
                const auto categorySpan = groupSpan(category, static_cast<int>(categoryFirst), issues);
                for (const auto &entry: category.scenarios)
                    appendRow(entry, categorySpan, noGroupAt(projection.rows.size()));
                for (const auto &sub: category.subcategories) {
                    const auto subFirst = projection.rows.size();
                    const auto subSpan = groupSpan(sub, static_cast<int>(subFirst), issues);
                    for (const auto &entry: sub.scenarios) appendRow(entry, categorySpan, subSpan);
                    closeSpan(subFirst, &BenchmarkTableRow::subcategory);
                }
                closeSpan(categoryFirst, &BenchmarkTableRow::category);
            }
            const auto uncategorizedFirst = projection.rows.size();
            const auto uncategorized = noGroupAt(uncategorizedFirst, QObject::tr("Uncategorized"));
            for (const auto &entry: draft.uncategorized)
                appendRow(entry, uncategorized, noGroupAt(projection.rows.size()));
            closeSpan(uncategorizedFirst, &BenchmarkTableRow::category);
            return projection;
        }

        QVariantList buildGroups(const domain::Benchmark &draft, const IssueIndex &issues) {
            QVariantList groups;
            const auto describe = [&](const auto &group, const QString &kind, const QString &parentId, int count) {
                return QVariantMap{
                    {"id", idString(group.id)},
                    {"kind", kind},
                    {"parentId", parentId},
                    {"name", QString::fromStdString(group.name)},
                    {"color", toQColor(group.color)},
                    {"scenarioCount", count},
                    {"issues", issueTexts(IssueIndex::lookup(issues.byGroup, group.id.value))},
                };
            };
            for (const auto &category: draft.categories) {
                int count = static_cast<int>(category.scenarios.size());
                for (const auto &sub: category.subcategories) count += static_cast<int>(sub.scenarios.size());
                groups.push_back(describe(category, QStringLiteral("category"), QString(), count));
                for (const auto &sub: category.subcategories)
                    groups.push_back(describe(sub, QStringLiteral("subcategory"), idString(category.id),
                                              static_cast<int>(sub.scenarios.size())));
            }
            return groups;
        }

        // Scenario entries in the table's row order: categories in order (direct scenarios, then
        // each subcategory), Uncategorized last.
        std::vector<domain::ScenarioEntryId> visibleEntryOrder(const domain::Benchmark &draft) {
            std::vector<domain::ScenarioEntryId> order;
            const auto append = [&](const std::vector<domain::ScenarioEntry> &entries) {
                for (const auto &entry: entries) order.push_back(entry.id);
            };
            for (const auto &category: draft.categories) {
                append(category.scenarios);
                for (const auto &sub: category.subcategories) append(sub.scenarios);
            }
            append(draft.uncategorized);
            return order;
        }

        // Commits one cell's text: a finite score becomes typed, blank clears, anything else is kept
        // verbatim as retained input with no typed threshold.
        domain::BenchmarkEditResult applyThresholdText(domain::BenchmarkEditor &editor,
                                                       std::map<BenchmarkCellKey, BenchmarkCellRecord> &inputs,
                                                       const BenchmarkCellKey &key, const QString &text) {
            const auto parsed = BenchmarkCellInput::parse(text, QLocale());
            if (parsed.outcome == CellParseOutcome::Finite) {
                const auto result = editor.setThreshold(key.entryId, key.tierId, parsed.value);
                if (result.ok()) inputs.erase(key);
                return result;
            }
            const auto result = editor.clearThreshold(key.entryId, key.tierId);
            if (!result.ok()) return result;
            if (parsed.outcome == CellParseOutcome::Missing) inputs.erase(key);
            else inputs[key] = BenchmarkCellRecord{text, parsed.outcome};
            return result;
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
          }),
          m_tableModel(new BenchmarkTableModel(this)), m_clipboard(new BenchmarkClipboardAdapter(this)) {
        // A publication can change what the unchanged local draft resolves to; the draft, its input
        // and history stay as they are. Resolution is kept out of adaptState() so a local edit,
        // which resolves in recomputeAfterLocalEdit(), still resolves once.
        m_useCase->onChanged([this] {
            if (m_draft) m_draftResolutions = m_useCase->resolve(*m_draft);
            emitChanged();
        });
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
                    {"tierId", issue.tierId ? idString(*issue.tierId) : QString()},
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
        QVariantList groups;
        BenchmarkTableProjection projection;
        if (m_draft) {
            const auto issues = indexIssues(m_draftCompleteness);
            groups = buildGroups(*m_draft, issues);
            projection = buildTableProjection(*m_draft, issues, m_draftResolutions, m_cellInputs);
        }

        QVariantList scenarioCatalogue;
        for (const auto &scenario: state.scenarioCatalogue)
            scenarioCatalogue.push_back(QVariantMap{
                {"name", QString::fromStdString(scenario.name)},
                {"hash", QString::fromStdString(scenario.hash)},
            });

        QVariantList libraryEntries = buildLibraryEntries(state.library);

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
        m_groups = std::move(groups);
        m_tableModel->setProjection(std::move(projection));
        m_scenarioCatalogue = std::move(scenarioCatalogue);
        m_libraryEntries = std::move(libraryEntries);
    }

    void BenchmarkManagerViewModel::adoptSeed(application::BenchmarkEditorSeed seed, bool fromLibrary,
                                              bool dirty) {
        m_draft = std::move(seed.benchmark);
        m_token = std::move(seed.token);
        resetSession();
        m_baseline = dirty ? std::nullopt : std::optional<domain::Benchmark>(*m_draft);
        m_draftFromLibrary = fromLibrary;
        m_draftDirty = dirty;
        m_draftCompleteness = domain::validateBenchmark(*m_draft);
        m_draftResolutions = m_useCase->resolve(*m_draft);
        emitChanged();
    }

    void BenchmarkManagerViewModel::clearWorkingCopy() {
        m_draft.reset();
        resetSession();
        m_baseline.reset();
        m_token.reset();
        m_draftCompleteness = {};
        m_draftResolutions.clear();
        m_draftDirty = false;
        m_draftFromLibrary = false;
        emitChanged();
    }

    void BenchmarkManagerViewModel::resetSession() {
        m_cellInputs.clear();
        m_history.clear();
        if (m_selectedEntryIds.isEmpty() && m_currentCell.isEmpty()) return;
        m_selectedEntryIds.clear();
        m_currentCell.clear();
        emit selectionChanged();
    }

    QVariantMap BenchmarkManagerViewModel::applyEdit(
        const std::function<domain::BenchmarkEditResult(domain::BenchmarkEditor &)> &op) {
        return applySessionEdit([&](domain::BenchmarkEditor &editor, CellInputs &) { return op(editor); });
    }

    QVariantMap BenchmarkManagerViewModel::applySessionEdit(const SessionEdit &op) {
        if (!m_draft) return errorMap(tr("No benchmark is open."));
        // Edited on copies, so a rejected multi-step edit leaves the session exactly as it was.
        domain::Benchmark candidate = *m_draft;
        CellInputs inputs = m_cellInputs;
        domain::BenchmarkEditor editor{candidate, m_idFactory};
        const auto result = op(editor, inputs);
        if (result.error) return errorMap(editErrorText(*result.error));
        if (candidate != *m_draft || inputs != m_cellInputs) {
            m_history.push_back({std::exchange(*m_draft, std::move(candidate)),
                                 std::exchange(m_cellInputs, std::move(inputs)), m_selectedEntryIds, m_currentCell});
            recomputeAfterLocalEdit();
        }
        QVariantMap map{{"ok", true}};
        if (result.createdTier) map["createdId"] = idString(*result.createdTier);
        else if (result.createdGroup) map["createdId"] = idString(*result.createdGroup);
        else if (result.createdEntry) map["createdId"] = idString(*result.createdEntry);
        return map;
    }

    void BenchmarkManagerViewModel::pruneCellInputs() {
        if (m_cellInputs.empty()) return;
        const auto entries = visibleEntryOrder(*m_draft);
        std::erase_if(m_cellInputs, [&](const auto &input) {
            const auto &key = input.first;
            return !tierPosition(*m_draft, key.tierId) || std::ranges::find(entries, key.entryId) == entries.end();
        });
    }

    void BenchmarkManagerViewModel::recomputeAfterLocalEdit() {
        // The model still holds the outgoing projection, so it knows where the focused row sat.
        const int oldRow = m_tableModel->rowForEntry(m_currentCell.value(QStringLiteral("entryId")).toString());
        restoreSelection(m_selectedEntryIds, m_currentCell, oldRow);
        pruneCellInputs();
        m_draftCompleteness = domain::validateBenchmark(*m_draft);
        m_draftDirty = !m_baseline || *m_draft != *m_baseline || !m_cellInputs.empty();
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
        // Retained input is saved as a missing threshold. The candidate stays isolated until the
        // outcome is known: a publication may re-enter during save(), and a failure must leave the
        // visible session, its input and its history exactly as they were.
        domain::Benchmark candidate = *m_draft;
        {
            domain::BenchmarkEditor normalizer{candidate, m_idFactory};
            for (const auto &key: m_cellInputs | std::views::keys) normalizer.clearThreshold(key.entryId, key.tierId);
        }
        const auto outcome = m_useCase->save(candidate, m_token);
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
        m_draft = candidate;
        m_baseline = std::move(candidate);
        m_cellInputs.clear();
        m_history.clear();
        m_draftFromLibrary = true;
        recomputeAfterLocalEdit();
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

    QVariantMap BenchmarkManagerViewModel::removeScenarios(const QStringList &entryIds) {
        QStringList unique = entryIds;
        unique.removeDuplicates();
        return applyEdit([&](domain::BenchmarkEditor &e) {
            for (const auto &id: unique)
                if (auto result = e.removeScenario(domain::ScenarioEntryId{id.toStdString()}); result.error)
                    return result;
            return domain::BenchmarkEditResult{};
        });
    }

    QVariantMap BenchmarkManagerViewModel::setThreshold(const QString &entryId, const QString &tierId,
                                                        double score) {
        const BenchmarkCellKey key{domain::ScenarioEntryId{entryId.toStdString()}, domain::TierId{tierId.toStdString()}};
        return applySessionEdit([&](domain::BenchmarkEditor &e, CellInputs &inputs) {
            const auto result = e.setThreshold(key.entryId, key.tierId, score);
            if (result.ok()) inputs.erase(key);
            return result;
        });
    }

    QVariantMap BenchmarkManagerViewModel::clearThreshold(const QString &entryId, const QString &tierId) {
        const BenchmarkCellKey key{domain::ScenarioEntryId{entryId.toStdString()}, domain::TierId{tierId.toStdString()}};
        return applySessionEdit([&](domain::BenchmarkEditor &e, CellInputs &inputs) {
            const auto result = e.clearThreshold(key.entryId, key.tierId);
            if (result.ok()) inputs.erase(key);
            return result;
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
        return applyEdit([&](domain::BenchmarkEditor &e) {
            return e.moveScenario(domain::ScenarioEntryId{entryId.toStdString()}, toGroupTarget(targetGroupId));
        });
    }

    void BenchmarkManagerViewModel::setSelectedEntryIds(const QStringList &ids) {
        if (ids == m_selectedEntryIds) return;
        m_selectedEntryIds = ids;
        emit selectionChanged();
    }

    void BenchmarkManagerViewModel::setCurrentCell(const QVariantMap &cell) {
        if (cell == m_currentCell) return;
        m_currentCell = cell;
        emit selectionChanged();
    }

    void BenchmarkManagerViewModel::restoreSelection(const QStringList &ids, const QVariantMap &cell,
                                                     int fallbackRow) {
        const auto order = visibleEntryOrder(*m_draft);
        const auto exists = [&](const QString &id) {
            return std::ranges::find(order, domain::ScenarioEntryId{id.toStdString()}) != order.end();
        };
        QStringList surviving;
        for (const auto &id: ids)
            if (exists(id)) surviving.push_back(id);
        QVariantMap current = cell;
        const auto entryId = cell.value(QStringLiteral("entryId")).toString();
        if (!entryId.isEmpty() && !exists(entryId)) {
            // Fall back to the row now nearest the lost anchor, keeping its column.
            current.remove(QStringLiteral("entryId"));
            if (!order.empty()) {
                const auto row = std::min(static_cast<std::size_t>(std::max(fallbackRow, 0)), order.size() - 1);
                current.insert(QStringLiteral("entryId"), QString::fromStdString(order[row].value));
            }
        }
        if (order.empty()) current.clear();
        const auto tierId = current.value(QStringLiteral("tierId")).toString();
        if (!tierId.isEmpty() && !tierPosition(*m_draft, domain::TierId{tierId.toStdString()})) {
            current.remove(QStringLiteral("tierId"));
            current.insert(QStringLiteral("columnKind"), static_cast<int>(BenchmarkColumnKind::Scenario));
        }
        if (surviving == m_selectedEntryIds && current == m_currentCell) return;
        m_selectedEntryIds = std::move(surviving);
        m_currentCell = std::move(current);
        emit selectionChanged();
    }

    QVariantList BenchmarkManagerViewModel::normalizationIssues() const {
        QVariantList issues;
        if (!m_draft || m_cellInputs.empty()) return issues;
        const auto order = visibleEntryOrder(*m_draft);
        const auto rowOf = [&](const domain::ScenarioEntryId &id) { return std::ranges::find(order, id) - order.begin(); };
        std::vector<std::pair<BenchmarkCellKey, BenchmarkCellRecord>> inputs(m_cellInputs.begin(), m_cellInputs.end());
        std::ranges::stable_sort(inputs, {}, [&](const auto &input) { return rowOf(input.first.entryId); });
        for (const auto &[key, record]: inputs)
            issues.push_back(QVariantMap{
                {"entryId", QString::fromStdString(key.entryId.value)},
                {"tierId", QString::fromStdString(key.tierId.value)},
                {"text", record.rawText},
                {"message", inputIssueText(record.outcome)},
            });
        return issues;
    }

    QVariantMap BenchmarkManagerViewModel::editThresholdText(const QString &entryId, const QString &tierId,
                                                             const QString &text) {
        const BenchmarkCellKey key{domain::ScenarioEntryId{entryId.toStdString()},
                                   domain::TierId{tierId.toStdString()}};
        return applySessionEdit([&](domain::BenchmarkEditor &e, CellInputs &inputs) {
            return applyThresholdText(e, inputs, key, text);
        });
    }

    QVariantMap BenchmarkManagerViewModel::pasteText(const QVariantMap &destination, const QString &text) {
        if (!m_draft) return errorMap(tr("No benchmark is open."));
        const QString kind = destination.value(QStringLiteral("kind")).toString();
        if (kind == QStringLiteral("category") || kind == QStringLiteral("subcategory"))
            return errorMap(tr("Paste into scenario, threshold or rank-name cells. Assign selected rows to a "
                               "group instead of pasting into it."));
        const auto matrix = BenchmarkClipboardMatrix::parse(text);
        if (matrix.rows.empty()) return QVariantMap{{"ok", true}};

        // Placement is resolved against the table as it is now, by position, never by name.
        const auto rows = visibleEntryOrder(*m_draft);
        const auto rowIndex = [&](const QString &id) -> std::optional<std::size_t> {
            const auto found = std::ranges::find(rows, domain::ScenarioEntryId{id.toStdString()});
            if (found == rows.end()) return std::nullopt;
            return static_cast<std::size_t>(found - rows.begin());
        };
        const auto tierIndex = [&](const QString &id) { return tierPosition(*m_draft, domain::TierId{id.toStdString()}); };
        const QString entryId = destination.value(QStringLiteral("entryId")).toString();
        const QString tierId = destination.value(QStringLiteral("tierId")).toString();
        const auto gone = [&] { return errorMap(tr("That paste destination no longer exists.")); };

        bool header = false;
        bool namesFirst = false;
        std::size_t startRow = 0;
        std::size_t startTier = 0;
        if (kind == QStringLiteral("scenario")) {
            const auto row = rowIndex(entryId);
            if (!row) return gone();
            startRow = *row;
            namesFirst = true;
        } else if (kind == QStringLiteral("threshold")) {
            const auto row = rowIndex(entryId);
            const auto tier = tierIndex(tierId);
            if (!row || !tier) return gone();
            startRow = *row;
            startTier = *tier;
        } else if (kind == QStringLiteral("append")) {
            startRow = rows.size();
            namesFirst = true;
        } else if (kind == QStringLiteral("rankHeader")) {
            header = true;
            if (tierId.isEmpty()) {
                startTier = m_draft->tiers.size();
            } else {
                const auto tier = tierIndex(tierId);
                if (!tier) return gone();
                startTier = *tier;
            }
            if (matrix.rows.size() > 1)
                return errorMap(tr("Rank names paste into the single header row. Copy one row of names."));
        } else {
            return errorMap(tr("Choose a scenario, threshold or rank-name cell to paste into."));
        }

        const auto width = static_cast<std::size_t>(matrix.width());
        const std::size_t thresholdWidth = header ? width : width - (namesFirst ? 1 : 0);
        return applySessionEdit([&](domain::BenchmarkEditor &editor, CellInputs &inputs) {
            std::vector<domain::ScenarioEntryId> rowIds = rows;
            if (!header && startRow + matrix.rows.size() > rows.size()) {
                const auto created = editor.appendUnnamedScenarios(startRow + matrix.rows.size() - rows.size());
                rowIds.insert(rowIds.end(), created.createdEntries.begin(), created.createdEntries.end());
            }
            std::vector<domain::TierId> tierIds;
            for (const auto &tier: m_draft->tiers) tierIds.push_back(tier.id);
            while (tierIds.size() < startTier + thresholdWidth) tierIds.push_back(*editor.addTier({}).createdTier);

            if (header) {
                for (std::size_t column = 0; column < matrix.rows.front().size(); ++column)
                    editor.renameTier(tierIds[startTier + column], matrix.rows.front()[column].toStdString());
            } else {
                for (std::size_t r = 0; r < matrix.rows.size(); ++r) {
                    const auto &fields = matrix.rows[r];
                    const auto &entry = rowIds[startRow + r];
                    for (std::size_t column = 0; column < fields.size(); ++column) {
                        if (namesFirst && column == 0) {
                            editor.renameScenario(entry, fields[column].toStdString());
                            continue;
                        }
                        const auto tier = startTier + column - (namesFirst ? 1 : 0);
                        applyThresholdText(editor, inputs, {entry, tierIds[tier]}, fields[column]);
                    }
                }
            }
            return domain::BenchmarkEditResult{};
        });
    }

    QVariantMap BenchmarkManagerViewModel::assignScenarios(const QStringList &entryIds,
                                                           const QString &targetGroupId) {
        if (!m_draft) return errorMap(tr("No benchmark is open."));
        // The generic MixedContent text advises relocating a category's own scenarios, which is the
        // wrong remedy when the rows being assigned are what would mix it.
        if (std::ranges::any_of(m_draft->categories, [&](const domain::Category &category) {
                return category.id.value == targetGroupId.toStdString() && !category.subcategories.empty();
            }))
            return errorMap(tr("That category is divided into subcategories. Assign the rows to one of its "
                               "subcategories instead."));
        // Selection arrives in click order; the batch keeps the rows' current visible order.
        std::unordered_map<std::string, std::size_t> visibleIndex;
        for (const auto &id: visibleEntryOrder(*m_draft)) visibleIndex.emplace(id.value, visibleIndex.size());
        std::vector<domain::ScenarioEntryId> ordered;
        for (const auto &id: entryIds) ordered.push_back(domain::ScenarioEntryId{id.toStdString()});
        std::ranges::stable_sort(ordered, {}, [&](const domain::ScenarioEntryId &id) {
            const auto found = visibleIndex.find(id.value);
            return found == visibleIndex.end() ? visibleIndex.size() : found->second;
        });
        return applyEdit([&](domain::BenchmarkEditor &e) {
            return e.assignScenarios(ordered, toGroupTarget(targetGroupId));
        });
    }

    QVariantMap BenchmarkManagerViewModel::addSubcategoryRelocating(const QString &categoryId, const QString &name,
                                                                    const QString &relocation) {
        domain::DirectScenarioRelocation choice{domain::RelocateDirectToUncategorized{}};
        if (relocation == QStringLiteral("newSubcategory")) choice = domain::RelocateDirectToNewSubcategory{};
        else if (relocation != QStringLiteral("uncategorized"))
            return errorMap(tr("Choose where the category's scenarios should go."));
        return applyEdit([&](domain::BenchmarkEditor &e) {
            return e.addSubcategory(domain::GroupId{categoryId.toStdString()}, name.toStdString(), choice);
        });
    }

    QVariantMap BenchmarkManagerViewModel::reorderSubcategory(const QString &id, int position) {
        return applyEdit([&](domain::BenchmarkEditor &e) {
            return e.reorderSubcategory(domain::GroupId{id.toStdString()},
                                        static_cast<std::size_t>(std::max(position, 0)));
        });
    }

    QVariantMap BenchmarkManagerViewModel::reorderScenario(const QString &entryId, int position) {
        return applyEdit([&](domain::BenchmarkEditor &e) {
            return e.reorderScenario(domain::ScenarioEntryId{entryId.toStdString()},
                                     static_cast<std::size_t>(std::max(position, 0)));
        });
    }

    QVariantMap BenchmarkManagerViewModel::undo() {
        if (!m_draft || m_history.empty()) return errorMap(tr("There is nothing to undo."));
        auto previous = std::move(m_history.back());
        m_history.pop_back();
        // A step recorded without a focused cell keeps the present focus where it survives.
        const QVariantMap anchor = previous.currentCell.isEmpty() ? m_currentCell : previous.currentCell;
        const int fallbackRow = m_tableModel->rowForEntry(anchor.value(QStringLiteral("entryId")).toString());
        m_draft = std::move(previous.draft);
        m_cellInputs = std::move(previous.cellInputs);
        restoreSelection(previous.selectedEntryIds, anchor, fallbackRow);
        recomputeAfterLocalEdit();
        return QVariantMap{{"ok", true}};
    }
}
