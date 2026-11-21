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

namespace ksv::presentation {
    namespace {
        QString q(const std::string &value) { return QString::fromStdString(value); }

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
        QString personalBestDateText(const std::optional<domain::ScenarioEvaluation> &stats) {
            if (!stats) return QStringLiteral("—");
            return QDateTime::fromSecsSinceEpoch(stats->personalBestTime().time_since_epoch().count())
                .toString(QStringLiteral("yyyy-MM-dd"));
        }

        QString playCountText(const std::optional<domain::ScenarioEvaluation> &stats) {
            return stats ? QLocale().toString(stats->runCount()) : QStringLiteral("—");
        }

        // Where the PB sits on the ladder, in words: "Silver · 50% of the way to Gold (5,670 / 5,954)".
        QString progressDescription(const std::span<const domain::Tier> tiers,
                                    const domain::ScenarioBenchmarkEvaluation &scenario,
                                    const std::optional<double> score) {
            if (!score) return QObject::tr("Unplayed");
            // An Incomplete benchmark still reaches the grid with missing or out-of-order thresholds.
            // No tier position derived from such a ladder is trustworthy, and the hatched chunks
            // already show which thresholds are unusable.
            const auto &ladder = scenario.ladder();
            if (!ladder) return QObject::tr("%1 · thresholds incomplete").arg(formatBenchmarkNumber(*score));

            const auto reached = static_cast<std::size_t>(std::ranges::upper_bound(*ladder, *score) - ladder->begin());
            if (reached == ladder->size())
                return QObject::tr("%1 · top tier reached (%2 / %3)")
                    .arg(q(tiers.back().name), formatBenchmarkNumber(*score), formatBenchmarkNumber(ladder->back()));
            const QString from = reached == 0 ? QObject::tr("Unranked") : q(tiers[reached - 1].name);
            const double floor = reached == 0 ? 0.0 : (*ladder)[reached - 1];
            const double target = (*ladder)[reached];
            const double span = target - floor;
            const double fraction = span > 0.0 ? std::clamp((*score - floor) / span, 0.0, 1.0) : 0.0;
            const int percent = static_cast<int>(std::floor(fraction * 100.0));
            return QObject::tr("%1 · %2% of the way to %3 (%4 / %5)")
                .arg(from)
                .arg(percent)
                .arg(q(tiers[reached].name), formatBenchmarkNumber(*score), formatBenchmarkNumber(target));
        }

        struct TierStyle {
            QString name;
            QString textColor;
        };

        class GridBuilder {
        public:
            explicit GridBuilder(const domain::Benchmark &benchmark)
                : m_benchmark(benchmark), m_tiers(benchmark.tiers()) {
                for (const auto &tier: m_tiers) {
                    const QString id = q(tier.id.value);
                    content.tiers.push_back(ProgressGridTier{
                        id, q(tier.name), readableTextColor(tier.color).name(),
                        QColor(tier.color.red, tier.color.green, tier.color.blue).name()});
                    m_tierStyles.insert(id, TierStyle{q(tier.name), readableTextColor(tier.color).name()});
                }
                for (const auto &blocker: benchmark.nextTierBlockers())
                    m_blockerRequired.insert(q(blocker.entryId.value), blocker.required);
                if (const auto nextTier = benchmark.nextTier()) {
                    const TierStyle next = m_tierStyles.value(q(nextTier->value));
                    m_nextTierName = next.name;
                    m_nextTierColor = next.textColor;
                }
            }

            void build() {
                const auto uncategorized = m_benchmark.uncategorized();
                if (!uncategorized.empty()) {
                    for (const auto *scenario: uncategorized) addScenario(*scenario);
                    addGroup(ProgressGridGroupColumn::Category, 0, nullptr);
                }
                for (const auto *category: m_benchmark.categories()) {
                    const int start = rowCount();
                    for (const auto *scenario: category->scenarios()) addScenario(*scenario);
                    for (const auto *subcategory: category->subgroups()) {
                        const int subStart = rowCount();
                        for (const auto *scenario: subcategory->scenarios()) addScenario(*scenario);
                        if (rowCount() == subStart) continue;
                        addGroup(ProgressGridGroupColumn::Subcategory, subStart, subcategory);
                        content.hasSubcategories = true;
                    }
                    if (rowCount() != start) addGroup(ProgressGridGroupColumn::Category, start, category);
                }
                markSubcategorySpans();
            }

            ProgressGridContent content;

        private:
            [[nodiscard]] int rowCount() const { return static_cast<int>(content.rows.size()); }

            void addGroup(const ProgressGridGroupColumn column, const int start,
                          const domain::GroupEvaluation *evaluation) {
                ProgressGridGroup group;
                group.column = column;
                if (evaluation) {
                    group.id = q(evaluation->id().value);
                    group.name = q(evaluation->name());
                    group.color = toQColor(evaluation->color()).name();
                    addGroupRank(group, *evaluation);
                } else {
                    group.name = QObject::tr("Uncategorized");
                }
                group.start = start;
                group.span = rowCount() - start;
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

            void addGroupRank(ProgressGridGroup &cell, const domain::GroupEvaluation &group) const {
                if (cell.id.isEmpty() || m_benchmark.completeness().completeness != domain::Completeness::Trackable)
                    return;
                cell.rankText = QObject::tr("Unranked");
                if (const auto satisfied = group.satisfiedTier()) {
                    for (const auto &tier: m_tiers) {
                        if (tier.id != *satisfied) continue;
                        cell.rankText = q(tier.name);
                        cell.rankColor = readableTextColor(tier.color).name();
                        cell.rankBarColor = QColor(tier.color.red, tier.color.green, tier.color.blue).name();
                    }
                }
            }

            void addScenario(const domain::ScenarioBenchmarkEvaluation &scenario) {
                const QString entryId = q(scenario.entryId().value);
                const auto &stats = scenario.stats();
                const auto personalBest = stats ? std::optional(stats->personalBest()) : std::nullopt;
                const auto matchState = scenario.matchState();

                ProgressGridScenarioRow row;
                row.entryId = entryId;
                row.name = q(scenario.name());
                row.personalBestText = personalBest ? formatBenchmarkNumber(*personalBest) : QStringLiteral("—");
                row.personalBestDateText = personalBestDateText(stats);
                row.playCountText = playCountText(stats);
                row.hasPersonalBest = personalBest.has_value();
                row.hasPersonalBestDate = stats.has_value();
                row.hasPlayCount = stats.has_value();

                QString attainedText = QObject::tr("Unranked");
                QString attainedColor;
                if (const auto attained = scenario.attainedTier()) {
                    const auto found = m_tierStyles.constFind(q(attained->value));
                    if (found != m_tierStyles.constEnd()) {
                        attainedText = found->name;
                        attainedColor = found->textColor;
                    }
                }
                row.attainedTierText = attainedText;
                row.attainedTierColor = attainedColor;

                row.fills = scenarioChunkFills(m_tiers, scenario, personalBest);
                const bool topChunkFilled = !row.fills.empty() && row.fills.back() >= 1.0;

                row.recentPosition = stats ? std::optional(scenario.normalizedRecentRank()) : std::nullopt;
                if (stats) {
                    QString recentTier = QObject::tr("thresholds incomplete");
                    if (scenario.ladder()) {
                        recentTier = QObject::tr("Unranked");
                        const auto reached = std::min(
                            static_cast<std::size_t>(std::floor(*row.recentPosition)), m_tiers.size());
                        if (reached >= 1) recentTier = q(m_tiers[reached - 1].name);
                    }
                    row.recentDescription = QObject::tr("Recent average %1 · %2")
                                                .arg(formatBenchmarkNumber(stats->recentAverage()), recentTier);
                }

                // The next tier this scenario's PB has not reached, with the score it needs.
                QString nextTierText;
                QString nextTierColor;
                QString nextThresholdText = QStringLiteral("—");
                if (const auto next = scenario.nextThreshold()) {
                    const auto found = m_tierStyles.constFind(q(next->tierId.value));
                    if (found != m_tierStyles.constEnd()) {
                        nextTierText = found->name;
                        nextTierColor = found->textColor;
                    }
                    nextThresholdText = formatBenchmarkNumber(next->score);
                } else if (personalBest && topChunkFilled) {
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

                QString description = progressDescription(m_tiers, scenario, personalBest);
                if (!matchText.isEmpty()) description += QStringLiteral("\n") + matchText + QStringLiteral(": ") + matchHint;
                row.barDescription = description;

                // Edge states, each with its own look: an unplayed scenario, a mapping the user
                // has to fix (with its status), a PB above the top tier, and chunks whose
                // thresholds are unusable.
                row.missingChunks = missingThresholdChunks(m_tiers, scenario);
                // The score that reaches each tier, at the end of its chunk ("" when this
                // entry has no usable threshold for the tier).
                for (const auto &tier: m_tiers) {
                    const auto threshold = scenario.thresholdFor(tier.id);
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
                } else if (topChunkFilled) {
                    const auto top = scenario.thresholdFor(m_tiers.back().id);
                    if (top && *personalBest > *top) {
                        edgeState = ProgressGridEdgeState::AboveTop;
                        aboveTopText = QObject::tr("Above %1: %2 / %3")
                                           .arg(q(m_tiers.back().name), formatBenchmarkNumber(*personalBest),
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

            const domain::Benchmark &m_benchmark;
            const std::span<const domain::Tier> m_tiers;
            QHash<QString, TierStyle> m_tierStyles;
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

    std::vector<double> scenarioChunkFills(const std::span<const domain::Tier> tiers,
                                           const domain::ScenarioBenchmarkEvaluation &scenario,
                                           const std::optional<double> score) {
        std::vector<double> fills(tiers.size(), 0.0);
        if (!score || !std::isfinite(*score)) return fills;
        for (std::size_t index = 0; index < tiers.size(); ++index) {
            const auto upper = scenario.thresholdFor(tiers[index].id);
            const auto lower = index == 0 ? std::optional<double>{0.0} : scenario.thresholdFor(tiers[index - 1].id);
            if (!upper || !lower) continue;
            const double span = *upper - *lower;
            if (span > 0.0) fills[index] = std::clamp((*score - *lower) / span, 0.0, 1.0);
            else if (index == 0 && *upper == 0.0) fills[index] = 1.0; // a zero first threshold is met by any score
        }
        // A chunk the bar hatches never shows a fill measured from an unusable bound.
        const auto missing = missingThresholdChunks(tiers, scenario);
        for (std::size_t index = 0; index < fills.size(); ++index)
            if (missing[index]) fills[index] = 0.0;
        return fills;
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
            map[QStringLiteral("hasRecentMarker")] = row.recentPosition.has_value();
            map[QStringLiteral("recentPosition")] = row.recentPosition.value_or(0.0);
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

    void BenchmarkProgressGridModel::reset(const domain::Benchmark *benchmark) {
        m_content = {};
        if (benchmark) {
            GridBuilder builder(*benchmark);
            builder.build();
            m_content = std::move(builder.content);
        }
        m_tiers = toList(m_content.tiers);
        m_rows = toList(m_content.rows);
        m_groups = toList(m_content.groups);
        m_hasSubcategories = m_content.hasSubcategories;
        emit changed();
    }

    std::vector<bool> missingThresholdChunks(const std::span<const domain::Tier> tiers,
                                             const domain::ScenarioBenchmarkEvaluation &scenario) {
        const std::size_t count = tiers.size();
        std::vector<std::optional<double> > values(count);
        for (std::size_t index = 0; index < count; ++index) values[index] = scenario.thresholdFor(tiers[index].id);
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
