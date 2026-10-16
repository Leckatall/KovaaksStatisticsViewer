#include "benchmark_tracking_vm.h"

#include <QCollator>
#include <QPointF>
#include <QVariantMap>

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <tuple>
#include <utility>
#include <vector>

#include "date_time_axis.h"
#include "benchmark_format.h"
#include "benchmark_issue_text.h"
#include "value_transform.h"
#include "benchmarks/benchmark.h"
#include "benchmarks/benchmark_projection.h"
#include "benchmarks/benchmark_validation.h"
#include "benchmarks/rank_history.h"

namespace ksv::presentation {
    namespace {
        QString q(const std::string &value) { return QString::fromStdString(value); }

        QString formatDuration(const double seconds) {
            const long long total = std::llround(seconds);
            if (total < 60) return QObject::tr("<1 min");
            if (total < 3600) return QObject::tr("%1 min").arg(total / 60);
            return QObject::tr("%1h %2m").arg(total / 3600).arg(total % 3600 / 60);
        }

        QString classificationString(const application::BenchmarkChoiceClassification classification) {
            switch (classification) {
                case application::BenchmarkChoiceClassification::Trackable: return QStringLiteral("trackable");
                case application::BenchmarkChoiceClassification::Incomplete: return QStringLiteral("incomplete");
                case application::BenchmarkChoiceClassification::Invalid: return QStringLiteral("invalid");
                case application::BenchmarkChoiceClassification::Unsupported: return QStringLiteral("unsupported");
            }
            return QStringLiteral("invalid");
        }

        bool isLoadedClassification(const application::BenchmarkChoiceClassification classification) {
            return classification == application::BenchmarkChoiceClassification::Trackable ||
                   classification == application::BenchmarkChoiceClassification::Incomplete;
        }

        qint64 utcMidnightMs(const std::chrono::sys_days day) {
            return utcDateTimeForEpochDay(static_cast<long long>(day.time_since_epoch().count())).toMSecsSinceEpoch();
        }

        void widenExtent(std::optional<std::pair<qint64, qint64>> &extent, const qint64 milliseconds) {
            if (!extent) extent = std::pair<qint64, qint64>{milliseconds, milliseconds};
            else extent = std::pair<qint64, qint64>{std::min(extent->first, milliseconds),
                                                    std::max(extent->second, milliseconds)};
        }

        QVariantList buildSelectorEntries(const std::vector<application::BenchmarkChoice> &choices) {
            struct Row {
                QVariantMap map;
                QString nameKey;
                QString filename;
            };
            std::vector<Row> loaded;
            std::vector<Row> problems;
            for (const auto &choice: choices) {
                QVariantMap map;
                map[QStringLiteral("filename")] = q(choice.filename);
                map[QStringLiteral("id")] = choice.id ? q(choice.id->value) : QString();
                map[QStringLiteral("classification")] = classificationString(choice.classification);
                map[QStringLiteral("selectable")] = choice.selectable;
                const QString displayName = q(choice.displayName);
                if (isLoadedClassification(choice.classification)) {
                    map[QStringLiteral("name")] = displayName;
                    loaded.push_back({map, displayName, q(choice.filename)});
                } else {
                    const QString shown = displayName.isEmpty() ? q(choice.filename) : displayName;
                    map[QStringLiteral("name")] = shown;
                    problems.push_back({map, shown, q(choice.filename)});
                }
            }

            QCollator collator;
            collator.setCaseSensitivity(Qt::CaseInsensitive);
            collator.setNumericMode(false);
            std::sort(loaded.begin(), loaded.end(), [&](const Row &a, const Row &b) {
                const int byName = collator.compare(a.nameKey, b.nameKey);
                if (byName != 0) return byName < 0;
                return a.filename < b.filename;
            });
            std::sort(problems.begin(), problems.end(),
                      [](const Row &a, const Row &b) { return a.filename < b.filename; });

            QVariantList entries;
            entries.reserve(static_cast<qsizetype>(loaded.size() + problems.size()));
            for (const auto &row: loaded) entries.push_back(row.map);
            for (const auto &row: problems) entries.push_back(row.map);
            return entries;
        }

        QString deriveState(const application::BenchmarkWorkspaceSnapshot &snapshot) {
            switch (snapshot.availability) {
                case application::BenchmarkTrackingState::NoSelection: {
                    const bool anySelectable = std::any_of(
                        snapshot.choices.begin(), snapshot.choices.end(),
                        [](const application::BenchmarkChoice &choice) { return choice.selectable; });
                    return anySelectable ? QStringLiteral("noSelection") : QStringLiteral("emptyLibrary");
                }
                case application::BenchmarkTrackingState::Unavailable:
                    return QStringLiteral("unavailable");
                case application::BenchmarkTrackingState::Ready: {
                    domain::Completeness completeness = domain::Completeness::Incomplete;
                    if (snapshot.projection) completeness = snapshot.projection->completeness;
                    else if (snapshot.selectedLoaded)
                        completeness = snapshot.selectedLoaded->completeness.completeness;
                    return completeness == domain::Completeness::Incomplete ? QStringLiteral("incomplete")
                                                                           : QStringLiteral("trackable");
                }
            }
            return QStringLiteral("emptyLibrary");
        }

        QString messageForState(const QString &state) {
            if (state == QLatin1String("emptyLibrary"))
                return QObject::tr("No benchmarks are installed yet. Create or import one to start tracking.");
            if (state == QLatin1String("noSelection"))
                return QObject::tr("Select a benchmark to see your tracking status.");
            if (state == QLatin1String("unavailable"))
                return QObject::tr("The selected benchmark is no longer available. Its file may have been deleted or "
                                   "may have a problem.");
            if (state == QLatin1String("incomplete"))
                return QObject::tr("This benchmark's setup is incomplete. Finish it in the Benchmark Manager to unlock ranks.");
            return QObject::tr("Tracking is up to date.");
        }
    }

    BenchmarkTrackingViewModel::BenchmarkTrackingViewModel(
        std::shared_ptr<application::IBenchmarkTrackingUseCase> useCase, QObject *parent)
        : QObject(parent), m_useCase(std::move(useCase)),
          // Both history metrics are one point per UTC day, so the axis never ticks in hours.
          m_historyXAxis(DateTimeAxis::Options{.wholeDays = true}),
          m_playtimeXAxis(DateTimeAxis::Options{.wholeDays = true}),
          m_rankHistory(new BenchmarkHistoryViewModel(BenchmarkHistoryViewModel::AverageRank, m_historyXAxis, this)),
          m_playtimeHistory(
              new BenchmarkHistoryViewModel(BenchmarkHistoryViewModel::RollingPlaytime, m_playtimeXAxis, this)),
          m_progressGrid(new BenchmarkProgressGridModel(this)) {
        m_useCase->onChanged([this] { rebuild(); });
        m_useCase->onPlaytimeChanged([this] { rebuildPlaytime(); });
        m_useCase->onRankChanged([this] { rebuildRank(); });
        m_useCase->onPresentationChanged([this] { applyPresentation(); });
        applyPresentation();
        rebuild();
    }

    void BenchmarkTrackingViewModel::applyPresentation() {
        m_rankHistory->setPresentation(m_useCase->presentation(application::GraphKey::BenchmarkRank));
        m_playtimeHistory->setPresentation(m_useCase->presentation(application::GraphKey::BenchmarkPlaytime));
    }

    void BenchmarkTrackingViewModel::selectBenchmark(const QString &id) {
        m_useCase->select(domain::BenchmarkId{id.toStdString()});
    }

    void BenchmarkTrackingViewModel::clearSelection() { m_useCase->clearSelection(); }

    void BenchmarkTrackingViewModel::selectDefaultBenchmark() { m_useCase->selectDefault(); }

    void BenchmarkTrackingViewModel::rebuild() {
        const application::BenchmarkWorkspaceSnapshot &snapshot = m_useCase->snapshot();
        const domain::BenchmarkProjection *projection =
            snapshot.projection ? &*snapshot.projection : nullptr;
        const domain::Benchmark *definition =
            snapshot.selectedLoaded ? &snapshot.selectedLoaded->benchmark : nullptr;

        m_selectorEntries = buildSelectorEntries(snapshot.choices);

        m_state = deriveState(snapshot);
        m_stateMessage = messageForState(m_state);
        m_summaryAvailable = m_state == QLatin1String("trackable");
        const bool trackable = m_summaryAvailable;

        m_selectedName = definition ? q(definition->name) : q(snapshot.lastKnownSelectedDisplayName);

        m_completenessIssues.clear();
        if (snapshot.selectedLoaded)
            for (const auto &issue: snapshot.selectedLoaded->completeness.issues)
                m_completenessIssues << benchmarkIssueText(issue.code);

        std::vector<RankTier> rankTiers;
        QHash<QString, QString> tierNameById;
        QHash<QString, QString> tierColorById;
        const auto indexTiers = [&](const std::vector<domain::Tier> &tiers) {
            for (const auto &tier: tiers) {
                const QColor readable = readableTextColor(tier.color);
                rankTiers.push_back({q(tier.name), readable});
                tierNameById.insert(q(tier.id.value), q(tier.name));
                tierColorById.insert(q(tier.id.value), readable.name());
            }
        };
        if (projection) indexTiers(projection->tiers);
        else if (definition) indexTiers(definition->tiers);

        // Sets `name`/`color` to the tier's, or to `fallback` and no colour when the id is absent.
        const auto resolveTier = [&](const std::optional<domain::TierId> &id, const QString &fallback,
                                     QString &name, QString &color) {
            name = fallback;
            color.clear();
            if (!id) return;
            const QString key = q(id->value);
            if (!tierNameById.contains(key)) return;
            name = tierNameById.value(key);
            color = tierColorById.value(key);
        };

        const QString unavailableText = tr("Unavailable");
        if (trackable && projection) {
            resolveTier(projection->attainedRank, tr("Unranked"), m_attainedRank, m_attainedRankColor);
            resolveTier(projection->completedRank, tr("None completed"), m_completedRank, m_completedRankColor);
            resolveTier(projection->nextTier, tr("Highest tier attained"), m_nextTier, m_nextTierColor);
            std::vector<QString> tierNames;
            tierNames.reserve(rankTiers.size());
            for (const auto &tier: rankTiers) tierNames.push_back(tier.name);
            m_averageRank = projection->averageRank ? describeAverageRank(*projection->averageRank, tierNames)
                                                    : unavailableText;
        } else {
            m_attainedRank = unavailableText;
            m_completedRank = unavailableText;
            m_nextTier = unavailableText;
            m_averageRank = unavailableText;
            m_attainedRankColor.clear();
            m_completedRankColor.clear();
            m_nextTierColor.clear();
        }
        // The summary's average-rank bar: averageRank is the mean normalized rank, so exactly k
        // (tier k attained) ends at tier k's chunk's right edge; the completed rank ticks its chunk's end.
        m_averageRankFills.clear();
        m_completedRankPosition = -1.0;
        if (trackable && projection && projection->averageRank)
            for (const double fill: rankChunkFills(*projection->averageRank, projection->tiers.size()))
                m_averageRankFills.push_back(fill);
        if (trackable && projection && projection->completedRank)
            for (std::size_t index = 0; index < projection->tiers.size(); ++index)
                if (projection->tiers[index].id == *projection->completedRank)
                    m_completedRankPosition = static_cast<double>(index + 1);

        m_totalPlaytime = projection ? formatDuration(projection->totalPlaytimeSeconds) : QString();
        m_scenariosAtNextTier = projection ? projection->scenariosAtNextTier : 0;
        m_hasNextTier = trackable && projection && projection->nextTier.has_value();
        m_nextTierBlockerCount = m_hasNextTier ? static_cast<int>(projection->nextTierBlockers.size()) : 0;
        m_scenarioCount = projection ? static_cast<int>(projection->scenarios.size()) : 0;

        m_rankPresent = trackable && projection != nullptr;
        m_rankTiers = std::move(rankTiers);
        m_rankExtentMs.reset();
        if (m_rankPresent)
            for (const auto &point: projection->averageRankHistory) widenExtent(m_rankExtentMs, utcMidnightMs(point.day));

        const QList<QPointF> playtime = playtimePoints(snapshot);
        m_appliedUnionMs = unionExtent();
        applyRankAxis(snapshot);
        applyPlaytimeAxis(snapshot);
        updateRankHistory(snapshot);
        m_playtimeHistory->setAveragingWindowDays(snapshot.playtime.windowDays);
        m_playtimeHistory->update(playtimeContent(snapshot, playtime), projection != nullptr, {});

        m_progressGrid->reset(definition, projection);

        emit changed();
    }

    QList<QPointF> BenchmarkTrackingViewModel::playtimePoints(const application::BenchmarkWorkspaceSnapshot &snapshot) {
        QList<QPointF> points;
        m_playtimeExtentMs.reset();
        if (!snapshot.projection) return points;
        for (const auto &[day, seconds]: snapshot.projection->rollingPlaytime) {
            const qint64 milliseconds = utcMidnightMs(day);
            points.append(QPointF(static_cast<qreal>(milliseconds), seconds));
            widenExtent(m_playtimeExtentMs, milliseconds);
        }
        // A narrowed range shows fewer samples, but the default extent stays that of the whole series.
        if (const auto &full = snapshot.playtime.fullExtent) {
            m_playtimeExtentMs.reset();
            widenExtent(m_playtimeExtentMs, utcMidnightMs(full->first));
            widenExtent(m_playtimeExtentMs, utcMidnightMs(full->last));
        }
        return points;
    }

    std::optional<BenchmarkTrackingViewModel::ExtentMs> BenchmarkTrackingViewModel::unionExtent() const {
        std::optional<ExtentMs> combined = m_rankExtentMs;
        if (m_playtimeExtentMs) {
            widenExtent(combined, m_playtimeExtentMs->first);
            widenExtent(combined, m_playtimeExtentMs->second);
        }
        return combined;
    }

    void BenchmarkTrackingViewModel::applyAxis(DateTimeAxis &axis,
                                               const std::optional<application::DayInterval> &interval) const {
        if (interval) {
            const qint64 first = utcMidnightMs(interval->first);
            std::ignore = axis.setEpochMillisecondsRange(first, std::max(first, utcMidnightMs(interval->last)));
        } else if (m_appliedUnionMs) {
            std::ignore = axis.setEpochMillisecondsRange(m_appliedUnionMs->first, m_appliedUnionMs->second);
        } else {
            std::ignore = axis.setRange(QDateTime::currentDateTimeUtc(), QDateTime::currentDateTimeUtc());
        }
    }

    void BenchmarkTrackingViewModel::applyRankAxis(const application::BenchmarkWorkspaceSnapshot &snapshot) {
        applyAxis(m_historyXAxis, snapshot.rank.interval);
    }

    void BenchmarkTrackingViewModel::applyPlaytimeAxis(const application::BenchmarkWorkspaceSnapshot &snapshot) {
        applyAxis(m_playtimeXAxis, snapshot.playtime.interval);
    }

    BenchmarkHistoryViewModel::Content BenchmarkTrackingViewModel::playtimeContent(
        const application::BenchmarkWorkspaceSnapshot &snapshot, const QList<QPointF> &points) {
        BenchmarkHistoryViewModel::Content content;
        content.observations = points;
        content.historyAvailable = snapshot.playtime.fullExtent.has_value() || !points.isEmpty();
        return content;
    }

    void BenchmarkTrackingViewModel::updateRankHistory(const application::BenchmarkWorkspaceSnapshot &snapshot) {
        BenchmarkHistoryViewModel::Content content;
        const domain::BenchmarkProjection *projection = snapshot.projection ? &*snapshot.projection : nullptr;
        if (m_rankPresent && projection) {
            content.historyAvailable = !projection->averageRankHistory.empty();
            for (const auto &point: projection->averageRankHistory)
                content.fullHistoryPeak = std::max(content.fullHistoryPeak, point.averageRank);
        }
        if (!content.historyAvailable || snapshot.rank.noRelevantHistory) {
            m_rankHistory->update(content, m_rankPresent, m_rankTiers);
            return;
        }

        const auto pointOf = [](const domain::AverageRankPoint &point) {
            return QPointF(static_cast<qreal>(utcMidnightMs(point.day)), point.averageRank);
        };
        const auto &full = projection->averageRankHistory;
        // A personal-best rank never lapses: the level held after the last improvement is drawn flat
        // to the right-hand edge, and one reached before a range opens is carried in from its left
        // edge. Both are geometry only; the observations stay the actual improvements.
        std::optional<double> held;
        qreal rightEdge = 0.0;
        if (const auto &interval = snapshot.rank.interval) {
            const auto window = domain::rankHistoryWindow(full, interval->first, interval->last);
            for (const auto &point: window.improvements) content.observations.append(pointOf(point));
            const auto leftEdge = static_cast<qreal>(utcMidnightMs(interval->first));
            if (window.established) {
                held = window.established->averageRank;
                if (content.observations.isEmpty() || content.observations.first().x() > leftEdge)
                    content.leadingCarry = QPointF(leftEdge, *held);
            }
            rightEdge = static_cast<qreal>(utcMidnightMs(interval->last));
        } else {
            for (const auto &point: full) content.observations.append(pointOf(point));
            rightEdge = m_historyXAxis.max();
        }
        if (!content.observations.isEmpty()) held = content.observations.last().y();
        const qreal lastX = content.observations.isEmpty() ? -std::numeric_limits<qreal>::infinity()
                                                           : content.observations.last().x();
        if (held && lastX < rightEdge) content.trailingCarry = QPointF(rightEdge, *held);
        m_rankHistory->update(content, m_rankPresent, m_rankTiers);
    }

    void BenchmarkTrackingViewModel::rebuildRank() {
        const application::BenchmarkWorkspaceSnapshot &snapshot = m_useCase->snapshot();
        applyRankAxis(snapshot);
        updateRankHistory(snapshot);
    }

    void BenchmarkTrackingViewModel::rebuildPlaytime() {
        const application::BenchmarkWorkspaceSnapshot &snapshot = m_useCase->snapshot();
        const QList<QPointF> playtime = playtimePoints(snapshot);
        if (unionExtent() != m_appliedUnionMs) {
            // The rank axis, and the rank line held flat to its edge, depend on the shared extent.
            rebuild();
            return;
        }
        applyPlaytimeAxis(snapshot);
        m_playtimeHistory->setAveragingWindowDays(snapshot.playtime.windowDays);
        m_playtimeHistory->update(playtimeContent(snapshot, playtime), snapshot.projection.has_value(), {});
    }
}
