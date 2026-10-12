#include "benchmark_progress_grid_model.h"

#include <QColor>
#include <QHash>
#include <QVariantMap>

#include <algorithm>
#include <cmath>

#include "benchmark_format.h"
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

        // "5,226.4", plus the sample count when fewer than five runs make up the average.
        QString recentText(const domain::ScenarioProjection &scenario) {
            if (!scenario.recentAverage) return QStringLiteral("—");
            QString text = formatBenchmarkNumber(*scenario.recentAverage);
            if (scenario.recentSampleCount >= 1 && scenario.recentSampleCount <= 4)
                text += QObject::tr(" (%1)").arg(scenario.recentSampleCount);
            return text;
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
                    QVariantMap map;
                    map[QStringLiteral("id")] = id;
                    map[QStringLiteral("name")] = q(tier.name);
                    map[QStringLiteral("color")] = readableTextColor(tier.color).name();
                    map[QStringLiteral("barColor")] = QColor(tier.color.red, tier.color.green, tier.color.blue).name();
                    tiers.push_back(map);
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
                    addGroup(QStringLiteral("category"), QString(), QObject::tr("Uncategorized"), QString(), start);
                }
                for (const auto &category: m_definition.categories) {
                    const int start = rowCount();
                    for (const auto &entry: category.scenarios) addScenario(entry);
                    for (const auto &subcategory: category.subcategories) {
                        const int subStart = rowCount();
                        for (const auto &entry: subcategory.scenarios) addScenario(entry);
                        if (rowCount() == subStart) continue;
                        addGroup(QStringLiteral("subcategory"), q(subcategory.id.value), q(subcategory.name),
                                 toQColor(subcategory.color).name(), subStart);
                        hasSubcategories = true;
                    }
                    if (rowCount() == start) continue;
                    addGroup(QStringLiteral("category"), q(category.id.value), q(category.name),
                             toQColor(category.color).name(), start);
                }
            }

            QVariantList tiers;
            QVariantList rows;
            QVariantList groups;
            bool hasSubcategories = false;

        private:
            [[nodiscard]] int rowCount() const { return static_cast<int>(rows.size()); }

            void addGroup(const QString &column, const QString &id, const QString &name, const QString &color,
                          const int start) {
                QVariantMap map;
                map[QStringLiteral("column")] = column;
                map[QStringLiteral("id")] = id;
                map[QStringLiteral("name")] = name;
                map[QStringLiteral("color")] = color;
                map[QStringLiteral("start")] = start;
                map[QStringLiteral("span")] = rowCount() - start;
                addGroupRank(map, id);
                groups.push_back(map);
            }

            // The group's rank chip: the highest tier the group satisfies (the old breakdown's
            // "highest Silver"), or Unranked. Only a trackable benchmark ranks groups, and
            // Uncategorized is never ranked as a group, so both get no chip (empty rankText).
            void addGroupRank(QVariantMap &map, const QString &id) const {
                QString text;
                QString color;
                QString barColor;
                const domain::GroupProjection *group = findGroup(m_projection.categories, id);
                if (m_projection.completeness == domain::Completeness::Trackable && group) {
                    text = QObject::tr("Unranked");
                    if (group->satisfiedTier) {
                        for (const auto &tier: m_projection.tiers) {
                            if (tier.id != *group->satisfiedTier) continue;
                            text = q(tier.name);
                            color = readableTextColor(tier.color).name();
                            barColor = QColor(tier.color.red, tier.color.green, tier.color.blue).name();
                        }
                    }
                }
                map[QStringLiteral("rankText")] = text;
                map[QStringLiteral("rankColor")] = color;
                map[QStringLiteral("rankBarColor")] = barColor;
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

                QVariantMap map;
                map[QStringLiteral("entryId")] = entryId;
                map[QStringLiteral("name")] = projected ? q(projected->name) : q(entry.name);
                map[QStringLiteral("personalBestText")] =
                    personalBest ? formatBenchmarkNumber(*personalBest) : QStringLiteral("—");
                map[QStringLiteral("recentText")] = projected ? recentText(*projected) : QStringLiteral("—");

                QString attainedText = QObject::tr("Unranked");
                QString attainedColor;
                if (projected && projected->attainedTier) {
                    const auto found = m_tierStyles.constFind(q(projected->attainedTier->value));
                    if (found != m_tierStyles.constEnd()) {
                        attainedText = found->name;
                        attainedColor = found->textColor;
                    }
                }
                map[QStringLiteral("attainedTierText")] = attainedText;
                map[QStringLiteral("attainedTierColor")] = attainedColor;

                const std::vector<double> fills = scenarioChunkFills(m_projection.tiers, entry.thresholds, personalBest);
                QVariantList fillList;
                for (const double fill: fills) fillList.push_back(fill);
                map[QStringLiteral("fills")] = fillList;

                // The recent average on the same chunk scale as the PB fill (-1: no marker).
                const auto recent = projected ? projected->recentAverage : std::nullopt;
                const auto recentPosition = tierScalePosition(m_projection.tiers, entry.thresholds, recent);
                map[QStringLiteral("recentPosition")] = recentPosition ? *recentPosition : -1.0;
                QString recentTier = QObject::tr("Unranked");
                if (recentPosition) {
                    const auto reached = std::min(
                        static_cast<std::size_t>(std::floor(
                            domain::normalizedTierValue(m_projection.tiers, entry.thresholds, recent))),
                        m_projection.tiers.size());
                    if (reached >= 1) recentTier = q(m_projection.tiers[reached - 1].name);
                }
                map[QStringLiteral("recentDescription")] =
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
                map[QStringLiteral("nextTierText")] = nextTierText;
                map[QStringLiteral("nextTierColor")] = nextTierColor;
                map[QStringLiteral("nextThresholdText")] = nextThresholdText;

                const QString matchText = matchStateLabel(matchState);
                const QString matchHint = matchStateHint(matchState);
                map[QStringLiteral("matchStateText")] = matchText;
                map[QStringLiteral("matchStateHint")] = matchHint;

                QString description = progressDescription(m_projection.tiers, entry.thresholds, personalBest);
                if (!matchText.isEmpty()) description += QStringLiteral("\n") + matchText + QStringLiteral(": ") + matchHint;
                map[QStringLiteral("barDescription")] = description;

                // Edge states, each with its own look: an unplayed scenario, a mapping the user
                // has to fix (with its status), a PB above the top tier, and chunks whose
                // thresholds are unusable.
                QVariantList missingList;
                for (const bool missing: missingThresholdChunks(m_projection.tiers, entry.thresholds))
                    missingList.push_back(missing);
                map[QStringLiteral("missingChunks")] = missingList;
                // The score that reaches each tier, for the start of its chunk ("" when this
                // entry has no usable threshold for the tier).
                QVariantList thresholdList;
                for (const auto &tier: m_projection.tiers) {
                    const auto threshold = thresholdFor(entry.thresholds, tier.id);
                    thresholdList.push_back(threshold ? formatBenchmarkNumber(*threshold) : QString());
                }
                map[QStringLiteral("thresholdTexts")] = thresholdList;
                QString edgeState;
                QString statusText;
                QString aboveTopText;
                const bool mappingNeedsAction = matchState == domain::ScenarioMatchState::Unresolved ||
                                                matchState == domain::ScenarioMatchState::Ambiguous ||
                                                matchState == domain::ScenarioMatchState::AutoMappable;
                if (mappingNeedsAction) {
                    edgeState = QStringLiteral("unmatched");
                    statusText = matchText;
                } else if (!personalBest) {
                    // A linked scenario without runs is unplayed too; it keeps its reason.
                    edgeState = QStringLiteral("unplayed");
                    statusText = matchState == domain::ScenarioMatchState::MappedUnavailable
                                     ? QObject::tr("Unplayed · no runs of the linked scenario")
                                     : QObject::tr("Unplayed");
                } else if (!m_projection.tiers.empty()) {
                    const auto top = thresholdFor(entry.thresholds, m_projection.tiers.back().id);
                    if (top && !fills.empty() && fills.back() >= 1.0 && *personalBest > *top) {
                        edgeState = QStringLiteral("aboveTop");
                        aboveTopText = QObject::tr("Above %1: %2 / %3")
                                           .arg(q(m_projection.tiers.back().name), formatBenchmarkNumber(*personalBest),
                                                formatBenchmarkNumber(*top));
                    }
                }
                map[QStringLiteral("edgeState")] = edgeState;
                map[QStringLiteral("statusText")] = statusText;
                map[QStringLiteral("aboveTopText")] = aboveTopText;

                // A row whose scenario keeps the benchmark from the next tier.
                const auto required = m_blockerRequired.constFind(entryId);
                const bool blocker = required != m_blockerRequired.constEnd() && !m_nextTierName.isEmpty();
                map[QStringLiteral("blocker")] = blocker;
                map[QStringLiteral("blockerColor")] = blocker ? m_nextTierColor : QString();
                map[QStringLiteral("blockerText")] =
                    blocker ? QObject::tr("Blocks %1 (needs %2)").arg(m_nextTierName, formatBenchmarkNumber(*required))
                            : QString();

                rows.push_back(map);
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
        if (!std::isfinite(rank) || tierCount == 0) return fills;
        const auto top = static_cast<double>(tierCount);
        for (std::size_t index = 0; index < tierCount; ++index) {
            const auto tier = static_cast<double>(index + 1); // the rank that enters this chunk
            if (rank < tier) continue;
            fills[index] = index + 1 == tierCount || rank >= top
                               ? 1.0
                               : std::max(kEnteredChunkFill, std::min(1.0, rank - tier));
        }
        return fills;
    }

    std::vector<double> scenarioChunkFills(const std::vector<domain::Tier> &tiers,
                                           const std::vector<domain::Threshold> &thresholds,
                                           const std::optional<double> score) {
        std::vector<double> fills(tiers.size(), 0.0);
        if (!score || !std::isfinite(*score)) return fills;
        for (std::size_t index = 0; index < tiers.size(); ++index) {
            const auto lower = thresholdFor(thresholds, tiers[index].id);
            if (!lower || *score < *lower) continue;
            if (index + 1 == tiers.size()) {
                fills[index] = 1.0;
                continue;
            }
            const auto upper = thresholdFor(thresholds, tiers[index + 1].id);
            if (!upper) continue;
            const double span = *upper - *lower;
            if (span > 0.0) fills[index] = std::max(kEnteredChunkFill, std::min(1.0, (*score - *lower) / span));
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
        if (!score || !std::isfinite(*score) || tiers.empty() || !domain::thresholdLadder(tiers, thresholds))
            return std::nullopt;
        // The normalized value counts tiers reached; the bar starts at the first tier's threshold
        // and its last chunk is the top tier, full once reached.
        const double rank = domain::normalizedTierValue(tiers, thresholds, score);
        const auto top = static_cast<double>(tiers.size());
        return rank >= top ? top : std::max(0.0, rank - 1.0);
    }

    BenchmarkProgressGridModel::BenchmarkProgressGridModel(QObject *parent) : QObject(parent) {}

    void BenchmarkProgressGridModel::reset(const domain::Benchmark *definition,
                                      const domain::BenchmarkProjection *projection) {
        m_tiers.clear();
        m_rows.clear();
        m_groups.clear();
        m_hasSubcategories = false;
        if (definition && projection) {
            GridBuilder builder(*definition, *projection);
            builder.build();
            m_tiers = builder.tiers;
            m_rows = builder.rows;
            m_groups = builder.groups;
            m_hasSubcategories = builder.hasSubcategories;
        }
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
            missing[index] = !usable(index) || (index + 1 < count && !usable(index + 1));
        return missing;
    }
}
