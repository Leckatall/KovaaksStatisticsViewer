#include "presentation/benchmarks/tracking/benchmark_progress_grid_model.h"

#include <QColor>
#include <QDateTime>
#include <QHash>
#include <QLocale>
#include <QVariantMap>

#include <algorithm>
#include <cmath>
#include <utility>

#include "presentation/benchmarks/benchmark_format.h"
#include "benchmarks/benchmark_evaluator.h"
#include "benchmarks/benchmark_ids.h"
#include "benchmarks/benchmark_resolution.h"

namespace ksv::presentation {
    namespace {
        QString q(const std::string &value) { return QString::fromStdString(value); }

        // The tier's threshold when exactly one finite, non-negative score is set for it, the same
        // admission rule as domain::thresholdLadder.
        std::optional<double> thresholdFor(const std::vector<domain::Threshold> &thresholds,
                                           const domain::TierId &tier) {
            std::optional<double> found;
            for (const auto &threshold: thresholds) {
                if (threshold.tierId != tier) continue;
                if (found) return std::nullopt;
                found = threshold.score;
            }
            if (found && (!std::isfinite(*found) || *found < 0.0)) return std::nullopt;
            return found;
        }

        // A normally resolved scenario reports nothing; the user only sees match states they may
        // need to act on, in their own terms rather than the resolver's.
        QString matchStateLabel(const domain::ScenarioMatchState state) {
            switch (state) {
                case domain::ScenarioMatchState::Resolved:
                    return {};
                case domain::ScenarioMatchState::MappedUnavailable:
                    return QObject::tr("No runs of linked scenario");
                case domain::ScenarioMatchState::Unresolved:
                    return QObject::tr("Not found in your runs");
                case domain::ScenarioMatchState::Ambiguous:
                    return QObject::tr("Several scenarios match");
                case domain::ScenarioMatchState::AutoMappable:
                    return QObject::tr("Linking to your runs…");
            }
            return {};
        }

        QString matchStateHint(const domain::ScenarioMatchState state) {
            switch (state) {
                case domain::ScenarioMatchState::Resolved:
                    return {};
                case domain::ScenarioMatchState::MappedUnavailable:
                    return QObject::tr("This entry is linked to a scenario that has no runs in your profile. "
                                       "Play it in KovaaK's, or link the entry to another scenario in the "
                                       "Benchmark Manager.");
                case domain::ScenarioMatchState::Unresolved:
                    return QObject::tr("No scenario in your runs has this name. Play it in KovaaK's, or link "
                                       "the entry to a scenario in the Benchmark Manager.");
                case domain::ScenarioMatchState::Ambiguous:
                    return QObject::tr("More than one scenario in your runs has this name. Choose the right "
                                       "one in the Benchmark Manager.");
                case domain::ScenarioMatchState::AutoMappable:
                    return QObject::tr("A matching scenario was found in your runs and is being linked "
                                       "automatically.");
            }
            return {};
        }

        // The local day the PB was set, "2026-08-14".
        QString personalBestDateText(const domain::ScenarioProjection &scenario) {
            if (!scenario.personalBestTime) return QStringLiteral("—");
            return QDateTime::fromSecsSinceEpoch(scenario.personalBestTime->time_since_epoch().count())
                .toString(QStringLiteral("yyyy-MM-dd"));
        }

        QString playCountText(const domain::ScenarioProjection &scenario) {
            return scenario.runCount > 0 ? QLocale().toString(scenario.runCount) : QStringLiteral("—");
        }

        // Where the PB sits on the ladder, in words: "Silver · 50% of the way to Gold (5,670 / 5,954)".
        QString progressDescription(const std::vector<domain::Tier> &tiers,
                                    const std::vector<domain::Threshold> &thresholds,
                                    const std::optional<double> score) {
            if (!score) return QObject::tr("Unplayed");
            // The first tier the score has not reached, climbing only while the ladder is usable.
            std::size_t index = 0;
            for (; index < tiers.size(); ++index) {
                const auto threshold = thresholdFor(thresholds, tiers[index].id);
                if (!threshold || *score < *threshold) break;
                if (index > 0) {
                    const auto previous = thresholdFor(thresholds, tiers[index - 1].id);
                    if (previous && *threshold <= *previous) break;
                }
            }
            if (index == tiers.size()) {
                if (tiers.empty()) return formatBenchmarkNumber(*score);
                const auto top = thresholdFor(thresholds, tiers.back().id);
                return QObject::tr("%1 · top tier reached (%2 / %3)")
                    .arg(q(tiers.back().name), formatBenchmarkNumber(*score),
                         top ? formatBenchmarkNumber(*top) : QStringLiteral("—"));
            }
            const QString from = index == 0 ? QObject::tr("Unranked") : q(tiers[index - 1].name);
            const auto target = thresholdFor(thresholds, tiers[index].id);
            if (!target) return QObject::tr("%1 · no threshold set for %2").arg(from, q(tiers[index].name));
            const auto floor = index == 0 ? std::optional<double>{0.0} : thresholdFor(thresholds, tiers[index - 1].id);
            if (index > 0 && floor && *target <= *floor)
                return QObject::tr("%1 · the %2 threshold is not above %3's")
                    .arg(from, q(tiers[index].name), q(tiers[index - 1].name));
            const double span = floor ? *target - *floor : 0.0;
            const double fraction = span > 0.0 ? std::clamp((*score - *floor) / span, 0.0, 1.0) : 0.0;
            const int percent = static_cast<int>(std::floor(fraction * 100.0));
            return QObject::tr("%1 · %2% of the way to %3 (%4 / %5)")
                .arg(from)
                .arg(percent)
                .arg(q(tiers[index].name), formatBenchmarkNumber(*score), formatBenchmarkNumber(*target));
        }

        struct TierStyle {
            QString name;
            QString textColor;
        };

        // Builds the grid's lists in one pass over the definition, joined with the projection.
        class GridBuilder {
        public:
            GridBuilder(const domain::Benchmark &definition, const domain::BenchmarkProjection &projection)
                : m_definition(definition), m_projection(projection) {
                for (const auto &tier: projection.tiers) {
                    const QString id = q(tier.id.value);
                    content.tiers.push_back(ProgressGridTier{
                        id, q(tier.name), readableTextColor(tier.color).name(),
                        QColor(tier.color.red, tier.color.green, tier.color.blue).name()});
                    m_tierStyles.insert(id, TierStyle{q(tier.name), readableTextColor(tier.color).name()});
                }
                for (const auto &scenario: projection.scenarios) m_scenarios.insert(q(scenario.entryId.value), &scenario);
                for (const auto &blocker: projection.nextTierBlockers)
                    m_blockerRequired.insert(q(blocker.entryId.value), blocker.required);
                if (projection.nextTier) {
                    const TierStyle next = m_tierStyles.value(q(projection.nextTier->value));
                    m_nextTierName = next.name;
                    m_nextTierColor = next.textColor;
                }
            }

            void build() {
                if (!m_definition.uncategorized.empty()) {
                    const int start = rowCount();
                    for (const auto &entry: m_definition.uncategorized) addScenario(entry);
                    addGroup(ProgressGridGroupColumn::Category, QString(), QObject::tr("Uncategorized"), QString(),
                             start);
                }
                for (const auto &category: m_definition.categories) {
                    const int start = rowCount();
                    for (const auto &entry: category.scenarios) addScenario(entry);
                    for (const auto &subcategory: category.subcategories) {
                        const int subStart = rowCount();
                        for (const auto &entry: subcategory.scenarios) addScenario(entry);
                        if (rowCount() == subStart) continue;
                        addGroup(ProgressGridGroupColumn::Subcategory, q(subcategory.id.value), q(subcategory.name),
                                 toQColor(subcategory.color).name(), subStart);
                        content.hasSubcategories = true;
                    }
                    if (rowCount() == start) continue;
                    addGroup(ProgressGridGroupColumn::Category, q(category.id.value), q(category.name),
                             toQColor(category.color).name(), start);
                }
                markSubcategorySpans();
            }

            ProgressGridContent content;

        private:
            [[nodiscard]] int rowCount() const { return static_cast<int>(content.rows.size()); }

            void addGroup(const ProgressGridGroupColumn column, const QString &id, const QString &name,
                          const QString &color, const int start) {
                ProgressGridGroup group;
                group.column = column;
                group.id = id;
                group.name = name;
                group.color = color;
                group.start = start;
                group.span = rowCount() - start;
                addGroupRank(group);
                content.groups.push_back(std::move(group));
            }

            // A category cell widens across the Subcategory column when none of its rows has a
            // subcategory cell.
            void markSubcategorySpans() {
                const auto startsInside = [this](const int start, const int end) {
                    return std::ranges::any_of(content.groups, [&](const ProgressGridGroup &other) {
                        return other.column == ProgressGridGroupColumn::Subcategory && other.start >= start &&
                               other.start < end;
                    });
                };
                for (auto &group: content.groups)
                    group.spansSubcategory = group.column == ProgressGridGroupColumn::Category &&
                                             !startsInside(group.start, group.start + group.span);
            }

            // The group's rank chip: the highest tier the group satisfies (the old breakdown's
            // "highest Silver"), or Unranked. Only a trackable benchmark ranks groups, and
            // Uncategorized is never ranked as a group, so both get no chip (empty rankText).
            void addGroupRank(ProgressGridGroup &cell) const {
                const domain::GroupProjection *group = findGroup(m_projection.categories, cell.id);
                if (m_projection.completeness == domain::Completeness::Trackable && group) {
                    cell.rankText = QObject::tr("Unranked");
                    if (group->satisfiedTier) {
                        for (const auto &tier: m_projection.tiers) {
                            if (tier.id != *group->satisfiedTier) continue;
                            cell.rankText = q(tier.name);
                            cell.rankColor = readableTextColor(tier.color).name();
                            cell.rankBarColor = QColor(tier.color.red, tier.color.green, tier.color.blue).name();
                        }
                    }
                }
            }

            static const domain::GroupProjection *findGroup(const std::vector<domain::GroupProjection> &groups,
                                                            const QString &id) {
                if (id.isEmpty()) return nullptr;
                for (const auto &group: groups) {
                    if (q(group.id.value) == id) return &group;
                    if (const auto *found = findGroup(group.subgroups, id)) return found;
                }
                return nullptr;
            }

            void addScenario(const domain::ScenarioEntry &entry) {
                const QString entryId = q(entry.id.value);
                const domain::ScenarioProjection *projected = m_scenarios.value(entryId, nullptr);
                const std::optional<double> personalBest = projected ? projected->personalBest : std::nullopt;
                const auto matchState = projected ? projected->matchState : domain::ScenarioMatchState::Unresolved;

                ProgressGridScenarioRow row;
                row.entryId = entryId;
                row.name = projected ? q(projected->name) : q(entry.name);
                row.personalBestText = personalBest ? formatBenchmarkNumber(*personalBest) : QStringLiteral("—");
                row.personalBestDateText = projected ? personalBestDateText(*projected) : QStringLiteral("—");
                row.playCountText = projected ? playCountText(*projected) : QStringLiteral("—");
                row.hasPersonalBest = personalBest.has_value();
                row.hasPersonalBestDate = projected && projected->personalBestTime.has_value();
                row.hasPlayCount = projected && projected->runCount > 0;

                QString attainedText = QObject::tr("Unranked");
                QString attainedColor;
                if (projected && projected->attainedTier) {
                    const auto found = m_tierStyles.constFind(q(projected->attainedTier->value));
                    if (found != m_tierStyles.constEnd()) {
                        attainedText = found->name;
                        attainedColor = found->textColor;
                    }
                }
                row.attainedTierText = attainedText;
                row.attainedTierColor = attainedColor;

                const std::vector<double> fills = scenarioChunkFills(m_projection.tiers, entry.thresholds, personalBest);
                row.fills = fills;

                const auto recent = projected ? projected->recentAverage : std::nullopt;
                const auto recentPosition = tierScalePosition(m_projection.tiers, entry.thresholds, recent);
                row.recentPosition = recentPosition;
                QString recentTier = QObject::tr("Unranked");
                if (recentPosition) {
                    const auto reached = std::min(
                        static_cast<std::size_t>(std::floor(
                            domain::normalizedTierValue(m_projection.tiers, entry.thresholds, recent))),
                        m_projection.tiers.size());
                    if (reached >= 1) recentTier = q(m_projection.tiers[reached - 1].name);
                }
                row.recentDescription =
                    recentPosition ? QObject::tr("Recent average %1 · %2").arg(formatBenchmarkNumber(*recent), recentTier)
                                   : QString();

                // The next tier this scenario's PB has not reached, with the score it needs.
                QString nextTierText;
                QString nextTierColor;
                QString nextThresholdText = QStringLiteral("—");
                if (projected && projected->nextThreshold) {
                    const auto found = m_tierStyles.constFind(q(projected->nextThreshold->tierId.value));
                    if (found != m_tierStyles.constEnd()) {
                        nextTierText = found->name;
                        nextTierColor = found->textColor;
                    }
                    nextThresholdText = formatBenchmarkNumber(projected->nextThreshold->score);
                } else if (personalBest && !fills.empty() && fills.back() >= 1.0) {
                    nextThresholdText = QObject::tr("Highest tier attained");
                }
                row.nextTierText = nextTierText;
                row.nextTierColor = nextTierColor;
                row.nextThresholdText = nextThresholdText;
                row.nextText = nextTierText.isEmpty() ? nextThresholdText : nextTierText + QLatin1Char(' ') + nextThresholdText;

                const QString matchText = matchStateLabel(matchState);
                const QString matchHint = matchStateHint(matchState);
                row.matchStateText = matchText;
                row.matchStateHint = matchHint;

                QString description = progressDescription(m_projection.tiers, entry.thresholds, personalBest);
                if (!matchText.isEmpty()) description += QStringLiteral("\n") + matchText + QStringLiteral(": ") + matchHint;
                row.barDescription = description;

                // Edge states, each with its own look: an unplayed scenario, a mapping the user
                // has to fix (with its status), a PB above the top tier, and chunks whose
                // thresholds are unusable.
                row.missingChunks = missingThresholdChunks(m_projection.tiers, entry.thresholds);
                // The score that reaches each tier, at the end of its chunk ("" when this
                // entry has no usable threshold for the tier).
                for (const auto &tier: m_projection.tiers) {
                    const auto threshold = thresholdFor(entry.thresholds, tier.id);
                    row.thresholdTexts.push_back(threshold ? formatBenchmarkNumber(*threshold) : QString());
                }
                ProgressGridEdgeState edgeState = ProgressGridEdgeState::None;
                QString statusText;
                QString aboveTopText;
                const bool mappingNeedsAction = matchState == domain::ScenarioMatchState::Unresolved ||
                                                matchState == domain::ScenarioMatchState::Ambiguous ||
                                                matchState == domain::ScenarioMatchState::AutoMappable;
                if (mappingNeedsAction) {
                    edgeState = ProgressGridEdgeState::Unmatched;
                    statusText = matchText;
                } else if (!personalBest) {
                    // A linked scenario without runs is unplayed too; it keeps its reason.
                    edgeState = ProgressGridEdgeState::Unplayed;
                    statusText = matchState == domain::ScenarioMatchState::MappedUnavailable
                                     ? QObject::tr("Unplayed · no runs of the linked scenario")
                                     : QObject::tr("Unplayed");
                } else if (!m_projection.tiers.empty()) {
                    const auto top = thresholdFor(entry.thresholds, m_projection.tiers.back().id);
                    if (top && !fills.empty() && fills.back() >= 1.0 && *personalBest > *top) {
                        edgeState = ProgressGridEdgeState::AboveTop;
                        aboveTopText = QObject::tr("Above %1: %2 / %3")
                                           .arg(q(m_projection.tiers.back().name), formatBenchmarkNumber(*personalBest),
                                                formatBenchmarkNumber(*top));
                    }
                }
                row.edgeState = edgeState;
                row.statusText = statusText;
                row.aboveTopText = aboveTopText;

                // A row whose scenario keeps the benchmark from the next tier.
                const auto required = m_blockerRequired.constFind(entryId);
                row.blocker = required != m_blockerRequired.constEnd() && !m_nextTierName.isEmpty();
                row.blockerColor = row.blocker ? m_nextTierColor : QString();
                row.blockerText =
                    row.blocker ? QObject::tr("Blocks %1 (needs %2)").arg(m_nextTierName, formatBenchmarkNumber(*required))
                                : QString();

                content.rows.push_back(std::move(row));
            }

            const domain::Benchmark &m_definition;
            const domain::BenchmarkProjection &m_projection;
            QHash<QString, TierStyle> m_tierStyles;
            QHash<QString, const domain::ScenarioProjection *> m_scenarios;
            QHash<QString, double> m_blockerRequired;
            QString m_nextTierName;
            QString m_nextTierColor;
        };
    }

    std::vector<double> rankChunkFills(const double rank, const std::size_t tierCount) {
        std::vector<double> fills(tierCount, 0.0);
        if (!std::isfinite(rank)) return fills;
        for (std::size_t index = 0; index < tierCount; ++index)
            fills[index] = std::clamp(rank - static_cast<double>(index), 0.0, 1.0);
        return fills;
    }

    std::vector<double> scenarioChunkFills(const std::vector<domain::Tier> &tiers,
                                           const std::vector<domain::Threshold> &thresholds,
                                           const std::optional<double> score) {
        std::vector<double> fills(tiers.size(), 0.0);
        if (!score || !std::isfinite(*score)) return fills;
        for (std::size_t index = 0; index < tiers.size(); ++index) {
            const auto upper = thresholdFor(thresholds, tiers[index].id);
            const auto lower = index == 0 ? std::optional<double>{0.0} : thresholdFor(thresholds, tiers[index - 1].id);
            if (!upper || !lower) continue;
            const double span = *upper - *lower;
            if (span > 0.0) fills[index] = std::clamp((*score - *lower) / span, 0.0, 1.0);
            else if (index == 0 && *upper == 0.0) fills[index] = 1.0; // a zero first threshold is met by any score
        }
        // A chunk the bar hatches never shows a fill measured from an unusable bound.
        const auto missing = missingThresholdChunks(tiers, thresholds);
        for (std::size_t index = 0; index < fills.size(); ++index)
            if (missing[index]) fills[index] = 0.0;
        return fills;
    }

    std::optional<double> tierScalePosition(const std::vector<domain::Tier> &tiers,
                                            const std::vector<domain::Threshold> &thresholds,
                                            const std::optional<double> score) {
        if (!score || !std::isfinite(*score) || !domain::thresholdLadder(tiers, thresholds)) return std::nullopt;
        return domain::normalizedTierValue(tiers, thresholds, score);
    }

    namespace {
        QVariantMap toMap(const ProgressGridTier &tier) {
            QVariantMap map;
            map[QStringLiteral("id")] = tier.id;
            map[QStringLiteral("name")] = tier.name;
            map[QStringLiteral("color")] = tier.color;
            map[QStringLiteral("barColor")] = tier.barColor;
            return map;
        }

        QString edgeStateKey(const ProgressGridEdgeState state) {
            switch (state) {
                case ProgressGridEdgeState::None:
                    return {};
                case ProgressGridEdgeState::Unplayed:
                    return QStringLiteral("unplayed");
                case ProgressGridEdgeState::Unmatched:
                    return QStringLiteral("unmatched");
                case ProgressGridEdgeState::AboveTop:
                    return QStringLiteral("aboveTop");
            }
            return {};
        }

        QVariantMap toMap(const ProgressGridScenarioRow &row) {
            QVariantMap map;
            map[QStringLiteral("entryId")] = row.entryId;
            map[QStringLiteral("name")] = row.name;
            map[QStringLiteral("personalBestText")] = row.personalBestText;
            map[QStringLiteral("personalBestDateText")] = row.personalBestDateText;
            map[QStringLiteral("playCountText")] = row.playCountText;
            map[QStringLiteral("hasPersonalBest")] = row.hasPersonalBest;
            map[QStringLiteral("attainedTierText")] = row.attainedTierText;
            map[QStringLiteral("attainedTierColor")] = row.attainedTierColor;
            QVariantList fills;
            for (const double fill: row.fills) fills.push_back(fill);
            map[QStringLiteral("fills")] = fills;
            // -1 tells QML there is no recent-average marker.
            map[QStringLiteral("recentPosition")] = row.recentPosition ? *row.recentPosition : -1.0;
            map[QStringLiteral("recentDescription")] = row.recentDescription;
            map[QStringLiteral("nextTierText")] = row.nextTierText;
            map[QStringLiteral("nextTierColor")] = row.nextTierColor;
            map[QStringLiteral("nextThresholdText")] = row.nextThresholdText;
            map[QStringLiteral("nextText")] = row.nextText;
            map[QStringLiteral("matchStateText")] = row.matchStateText;
            map[QStringLiteral("matchStateHint")] = row.matchStateHint;
            map[QStringLiteral("barDescription")] = row.barDescription;
            QVariantList missing;
            for (const bool chunk: row.missingChunks) missing.push_back(chunk);
            map[QStringLiteral("missingChunks")] = missing;
            QVariantList thresholds;
            for (const QString &text: row.thresholdTexts) thresholds.push_back(text);
            map[QStringLiteral("thresholdTexts")] = thresholds;
            map[QStringLiteral("edgeState")] = edgeStateKey(row.edgeState);
            map[QStringLiteral("statusText")] = row.statusText;
            map[QStringLiteral("aboveTopText")] = row.aboveTopText;
            map[QStringLiteral("blocker")] = row.blocker;
            map[QStringLiteral("blockerColor")] = row.blockerColor;
            map[QStringLiteral("blockerText")] = row.blockerText;
            return map;
        }

        QVariantMap toMap(const ProgressGridGroup &group) {
            QVariantMap map;
            map[QStringLiteral("column")] = group.column == ProgressGridGroupColumn::Category
                                                ? QStringLiteral("category")
                                                : QStringLiteral("subcategory");
            map[QStringLiteral("id")] = group.id;
            map[QStringLiteral("name")] = group.name;
            map[QStringLiteral("color")] = group.color;
            map[QStringLiteral("start")] = group.start;
            map[QStringLiteral("span")] = group.span;
            map[QStringLiteral("spansSubcategory")] = group.spansSubcategory;
            map[QStringLiteral("rankText")] = group.rankText;
            map[QStringLiteral("rankColor")] = group.rankColor;
            map[QStringLiteral("rankBarColor")] = group.rankBarColor;
            return map;
        }

        template<typename T>
        QVariantList toList(const std::vector<T> &items) {
            QVariantList list;
            list.reserve(static_cast<qsizetype>(items.size()));
            for (const T &item: items) list.push_back(toMap(item));
            return list;
        }
    }

    BenchmarkProgressGridModel::BenchmarkProgressGridModel(QObject *parent) : QObject(parent) {}

    QVariantMap BenchmarkProgressGridModel::headers() const {
        const ProgressGridHeadings &headings = m_content.headings;
        QVariantMap map;
        map[QStringLiteral("category")] = headings.category;
        map[QStringLiteral("subcategory")] = headings.subcategory;
        map[QStringLiteral("scenario")] = headings.scenario;
        map[QStringLiteral("progress")] = headings.progress;
        map[QStringLiteral("pbDate")] = headings.pbDate;
        map[QStringLiteral("plays")] = headings.plays;
        map[QStringLiteral("next")] = headings.next;
        return map;
    }

    void BenchmarkProgressGridModel::reset(const domain::Benchmark *definition,
                                      const domain::BenchmarkProjection *projection) {
        m_content = {};
        if (definition && projection) {
            GridBuilder builder(*definition, *projection);
            builder.build();
            m_content = std::move(builder.content);
        }
        m_tiers = toList(m_content.tiers);
        m_rows = toList(m_content.rows);
        m_groups = toList(m_content.groups);
        m_hasSubcategories = m_content.hasSubcategories;
        emit changed();
    }

    std::vector<bool> missingThresholdChunks(const std::vector<domain::Tier> &tiers,
                                             const std::vector<domain::Threshold> &thresholds) {
        const std::size_t count = tiers.size();
        std::vector<std::optional<double> > values(count);
        for (std::size_t index = 0; index < count; ++index) values[index] = thresholdFor(thresholds, tiers[index].id);
        // A threshold out of order with a neighbour: which of the two is wrong is unknowable, so
        // neither bounds a chunk the bar may fill.
        std::vector<bool> outOfOrder(count, false);
        for (std::size_t index = 0; index < count; ++index) {
            if (!values[index]) continue;
            const bool belowPrevious = index > 0 && values[index - 1] && *values[index] <= *values[index - 1];
            const bool aboveNext = index + 1 < count && values[index + 1] && *values[index + 1] <= *values[index];
            outOfOrder[index] = belowPrevious || aboveNext;
        }
        const auto usable = [&](const std::size_t index) { return values[index] && !outOfOrder[index]; };
        std::vector<bool> missing(count, false);
        for (std::size_t index = 0; index < count; ++index)
            missing[index] = !usable(index) || (index > 0 && !usable(index - 1));
        return missing;
    }
}
