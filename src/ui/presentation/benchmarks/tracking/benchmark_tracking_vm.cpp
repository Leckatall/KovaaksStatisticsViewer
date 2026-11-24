#include "presentation/benchmarks/tracking/benchmark_tracking_vm.h"

#include <QCollator>
#include <QPointF>
#include <QVariantMap>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <optional>
#include <tuple>
#include <utility>
#include <vector>

#include "presentation/graphs/common/axes/date_time_axis.h"
#include "presentation/benchmarks/benchmark_format.h"
#include "presentation/benchmarks/manager/benchmark_issue_text.h"
#include "presentation/graphs/common/axes/value_delegate.h"
#include "benchmarks/benchmark.h"
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

        // "(blocked by 2 groups and 1 scenario)": a group blocks as one requirement, and each
        // Uncategorized scenario is a requirement of its own.
        QString nextTierBlockerSummary(const std::vector<domain::RankBlocker> &blockers) {
            const auto groups = std::ranges::count_if(blockers, [](const auto &blocker) {
                return blocker.group.has_value();
            });
            const auto scenarios = static_cast<int>(blockers.size()) - static_cast<int>(groups);
            // The app ships no translations, so a bare "%n group(s)" would show its "(s)" verbatim.
            const QString groupPart =
                groups == 1 ? QObject::tr("1 group") : QObject::tr("%n groups", nullptr, static_cast<int>(groups));
            const QString scenarioPart =
                scenarios == 1 ? QObject::tr("1 scenario") : QObject::tr("%n scenarios", nullptr, scenarios);
            if (groups == 0) return QObject::tr("(blocked by %1)").arg(scenarioPart);
            if (scenarios == 0) return QObject::tr("(blocked by %1)").arg(groupPart);
            return QObject::tr("(blocked by %1 and %2)").arg(groupPart, scenarioPart);
        }

        QString classificationString(const application::BenchmarkChoiceClassification classification) {
            switch (classification) {
                case application::BenchmarkChoiceClassification::Trackable: return QStringLiteral("trackable");
                case application::BenchmarkChoiceClassification::Incomplete: return QStringLiteral("incomplete");
                case application::BenchmarkChoiceClassification::Invalid: return QStringLiteral("invalid");
                case application::BenchmarkChoiceClassification::Unsupported: return QStringLiteral("unsupported");
                case application::BenchmarkChoiceClassification::MigrationFailed:
                    return QStringLiteral("migration failed");
            }
            return QStringLiteral("invalid");
        }

        bool isLoadedClassification(const application::BenchmarkChoiceClassification classification) {
            return classification == application::BenchmarkChoiceClassification::Trackable ||
                   classification == application::BenchmarkChoiceClassification::Incomplete;
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
                std::optional<std::size_t> recency;
            };
            std::vector<Row> loaded;
            std::vector<Row> problems;
            for (const auto &choice: choices) {
                QVariantMap map;
                map[QStringLiteral("filename")] = q(choice.filename);
                map[QStringLiteral("id")] = choice.id ? q(choice.id->value) : QString();
                map[QStringLiteral("classification")] = classificationString(choice.classification);
                map[QStringLiteral("selectable")] = choice.selectable;
                map[QStringLiteral("problemText")] =
                    choice.migrationFailure ? migrationFailureText(*choice.migrationFailure) : QString();
                const QString displayName = q(choice.displayName);
                if (isLoadedClassification(choice.classification)) {
                    map[QStringLiteral("name")] = displayName;
                    loaded.push_back({map, displayName, q(choice.filename), choice.recency});
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
                if (a.recency.has_value() != b.recency.has_value()) return a.recency.has_value();
                if (a.recency && *a.recency != *b.recency) return *a.recency < *b.recency;
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
                case application::BenchmarkTrackingState::Ready:
                    return snapshot.benchmark &&
                           snapshot.benchmark->completeness().completeness == domain::Completeness::Trackable
                               ? QStringLiteral("trackable") : QStringLiteral("incomplete");
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

    QVariantMap BenchmarkTrackingViewModel::summaryCaptions() const {
        const BenchmarkSummaryCaptions captions;
        QVariantMap map;
        map[QStringLiteral("attainedRank")] = captions.attainedRank;
        map[QStringLiteral("completedRank")] = captions.completedRank;
        map[QStringLiteral("averageRank")] = captions.averageRank;
        map[QStringLiteral("totalPlaytime")] = captions.totalPlaytime;
        map[QStringLiteral("nextTier")] = captions.nextTier;
        return map;
    }

    BenchmarkTrackingViewModel::BenchmarkTrackingViewModel(
        std::shared_ptr<application::IBenchmarkTrackingUseCase> useCase, QObject *parent)
        : QObject(parent), m_useCase(std::move(useCase)),
          m_rankHistory(new BenchmarkHistoryViewModel(BenchmarkHistoryViewModel::AverageRank, this)),
          m_recentRankHistory(new BenchmarkHistoryViewModel(BenchmarkHistoryViewModel::RecentAverageRank, this)),
          m_playtimeHistory(new BenchmarkHistoryViewModel(BenchmarkHistoryViewModel::RollingPlaytime, this)),
          m_progressGrid(new BenchmarkProgressGridModel(this)) {
        m_useCase->onChanged([this] { rebuild(); });
        m_useCase->onPlaytimeChanged([this] { rebuildPlaytime(); });
        m_useCase->onRankChanged([this](const application::GraphKey graph) { rebuildRank(graph); });
        m_useCase->onPresentationChanged([this] { applyPresentation(); });
        applyPresentation();
        rebuild();
    }

    namespace {
        bool showsRunAxis(const application::GraphPresentation &presentation) {
            return presentation.xAxis == std::string(application::kRunXAxis);
        }
    }

    void BenchmarkTrackingViewModel::applyPresentation() {
        m_rankHistory->setPresentation(m_useCase->presentation(application::GraphKey::BenchmarkRank));
        m_recentRankHistory->setPresentation(m_useCase->presentation(application::GraphKey::BenchmarkRecentRank));
        m_playtimeHistory->setPresentation(m_useCase->presentation(application::GraphKey::BenchmarkPlaytime));
        if (!m_built) return;
        for (const auto graph: {application::GraphKey::BenchmarkRank, application::GraphKey::BenchmarkRecentRank}) {
            const auto &history = graph == application::GraphKey::BenchmarkRank ? *m_rankHistory : *m_recentRankHistory;
            if (showsRunAxis(m_useCase->presentation(graph)) != history.runAxisShown()) rebuildRank(graph);
        }
    }

    void BenchmarkTrackingViewModel::selectBenchmark(const QString &id) {
        m_useCase->select(domain::BenchmarkId{id.toStdString()});
    }

    void BenchmarkTrackingViewModel::clearSelection() { m_useCase->clearSelection(); }

    void BenchmarkTrackingViewModel::selectDefaultBenchmark() { m_useCase->selectDefault(); }

    void BenchmarkTrackingViewModel::rebuild() {
        const application::BenchmarkWorkspaceSnapshot &snapshot = m_useCase->snapshot();
        const domain::Benchmark *benchmark = snapshot.benchmark ? &*snapshot.benchmark : nullptr;

        m_selectorEntries = buildSelectorEntries(snapshot.choices);

        m_state = deriveState(snapshot);
        m_stateMessage = messageForState(m_state);
        m_summaryAvailable = m_state == QLatin1String("trackable");
        const bool trackable = m_summaryAvailable;

        m_selectedName = benchmark ? q(benchmark->definition().name) : q(snapshot.lastKnownSelectedDisplayName);

        m_completenessIssues.clear();
        if (benchmark)
            for (const auto &issue: benchmark->completeness().issues)
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
        if (benchmark) indexTiers(benchmark->tiers());

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
        const auto averageRank = benchmark ? benchmark->averageRank() : std::nullopt;
        const auto completedRank = benchmark ? benchmark->completedRank() : std::nullopt;
        if (trackable && benchmark) {
            resolveTier(benchmark->attainedRank(), tr("Unranked"), m_attainedRank, m_attainedRankColor);
            resolveTier(completedRank, tr("None completed"), m_completedRank, m_completedRankColor);
            resolveTier(benchmark->nextTier(), tr("Highest tier attained"), m_nextTier, m_nextTierColor);
            std::vector<QString> tierNames;
            tierNames.reserve(rankTiers.size());
            for (const auto &tier: rankTiers) tierNames.push_back(tier.name);
            m_averageRank = averageRank ? describeAverageRank(*averageRank, tierNames) : unavailableText;
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
        if (trackable && benchmark && averageRank)
            for (const double fill: rankChunkFills(*averageRank, benchmark->tiers().size()))
                m_averageRankFills.push_back(fill);
        if (trackable && benchmark && completedRank)
            for (std::size_t index = 0; index < benchmark->tiers().size(); ++index)
                if (benchmark->tiers()[index].id == *completedRank)
                    m_completedRankPosition = static_cast<double>(index + 1);

        m_totalPlaytime = benchmark ? formatDuration(benchmark->totalPlaytimeSeconds()) : QString();
        m_scenariosAtNextTier = benchmark ? benchmark->scenariosAtNextTier() : 0;
        m_hasNextTier = trackable && benchmark && benchmark->nextTier().has_value();
        m_nextTierBlockerCount = m_hasNextTier ? static_cast<int>(benchmark->nextTierBlockers().size()) : 0;
        m_nextTierBlockerText = m_hasNextTier ? nextTierBlockerSummary(benchmark->nextTierBlockers()) : QString();
        m_scenarioCount = benchmark ? static_cast<int>(benchmark->scenarios().size()) : 0;

        m_rankPresent = trackable && benchmark != nullptr;
        m_rankTiers = std::move(rankTiers);
        m_rankExtentMs.reset();
        m_recentRankExtentMs.reset();
        m_rankByDay.clear();
        m_recentRankByDay.clear();
        if (m_rankPresent) {
            m_rankByDay = domain::rankHistoryByDay(benchmark->averageRankHistory());
            m_recentRankByDay = domain::rankHistoryByDay(benchmark->recentAverageRankHistory());
            for (const auto &point: m_rankByDay) widenExtent(m_rankExtentMs, utcMidnightMs(point.day));
            for (const auto &point: m_recentRankByDay) widenExtent(m_recentRankExtentMs, utcMidnightMs(point.day));
        }

        const QList<QPointF> playtime = playtimePoints(snapshot);
        m_appliedUnionMs = unionExtent();
        for (const auto graph: {application::GraphKey::BenchmarkRank, application::GraphKey::BenchmarkRecentRank})
            rebuildRank(graph);
        applyPlaytimeAxis(snapshot);
        m_playtimeHistory->setAveragingWindowDays(snapshot.playtime.windowDays);
        m_playtimeHistory->update(playtimeContent(snapshot, playtime), benchmark != nullptr, {});

        m_progressGrid->reset(benchmark);
        m_built = true;

        emit changed();
    }

    QList<QPointF> BenchmarkTrackingViewModel::playtimePoints(const application::BenchmarkWorkspaceSnapshot &snapshot) {
        QList<QPointF> points;
        m_playtimeExtentMs.reset();
        if (!snapshot.benchmark) return points;
        for (const auto &[day, seconds]: snapshot.visiblePlaytime) {
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
        for (const auto &extent: {m_recentRankExtentMs, m_playtimeExtentMs}) {
            if (!extent) continue;
            widenExtent(combined, extent->first);
            widenExtent(combined, extent->second);
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

    void BenchmarkTrackingViewModel::applyRankAxis(DateTimeAxis &axis, const application::RankRangeView &view) const {
        if (!view.interval && view.anchoredEnd && m_appliedUnionMs) {
            const qint64 end = std::min(m_appliedUnionMs->second, utcMidnightMs(*view.anchoredEnd));
            std::ignore = axis.setEpochMillisecondsRange(m_appliedUnionMs->first, std::max(m_appliedUnionMs->first, end));
            return;
        }
        applyAxis(axis, view.interval);
    }

    void BenchmarkTrackingViewModel::applyPlaytimeAxis(const application::BenchmarkWorkspaceSnapshot &snapshot) {
        applyAxis(m_playtimeHistory->calendarAxis(), snapshot.playtime.interval);
    }

    BenchmarkHistoryViewModel::Content BenchmarkTrackingViewModel::playtimeContent(
        const application::BenchmarkWorkspaceSnapshot &snapshot, const QList<QPointF> &points) {
        BenchmarkHistoryViewModel::Content content;
        content.observations = points;
        content.historyAvailable = snapshot.playtime.fullExtent.has_value() || !points.isEmpty();
        return content;
    }

    void BenchmarkTrackingViewModel::updateRankHistory(BenchmarkHistoryViewModel &history, const DateTimeAxis &axis,
                                                       const std::span<const domain::AverageRankOfDay> full,
                                                       const application::RankRangeView &view) const {
        BenchmarkHistoryViewModel::Content content;
        if (m_rankPresent) {
            // An anchor with no contributing run is no history at all, even when the benchmark has a full rank history.
            content.historyAvailable = !full.empty() && !view.noRelevantHistory;
            for (const auto &point: full) content.fullHistoryPeak = std::max(content.fullHistoryPeak, point.averageRank);
        }
        if (!content.historyAvailable) {
            history.update(content, m_rankPresent, m_rankTiers);
            return;
        }

        const auto pointOf = [](const domain::AverageRankOfDay &point) {
            return QPointF(static_cast<qreal>(utcMidnightMs(point.day)), point.averageRank);
        };
        // Neither rank lapses between points: a personal best is never lost, and a recent average
        // only moves when the scenario is played again. So the level held after the last point is
        // drawn flat to the right-hand edge, and one reached before a range opens is carried in from
        // its left edge. Both are geometry only; the observations stay the series' actual points.
        std::optional<double> held;
        qreal rightEdge = 0.0;
        if (const auto &interval = view.interval) {
            const auto window = domain::rankHistoryWindow(full, interval->first, interval->last);
            for (const auto &point: window.points) content.observations.append(pointOf(point));
            const auto leftEdge = static_cast<qreal>(utcMidnightMs(interval->first));
            if (window.established) {
                held = window.established->averageRank;
                if (content.observations.isEmpty() || content.observations.first().x() > leftEdge)
                    content.leadingCarry = QPointF(leftEdge, *held);
            }
            rightEdge = static_cast<qreal>(utcMidnightMs(interval->last));
        } else {
            for (const auto &point: full) content.observations.append(pointOf(point));
            rightEdge = axis.max();
        }
        if (!content.observations.isEmpty()) held = content.observations.last().y();
        const qreal lastX = content.observations.isEmpty() ? -std::numeric_limits<qreal>::infinity()
                                                           : content.observations.last().x();
        if (held && lastX < rightEdge) content.trailingCarry = QPointF(rightEdge, *held);
        history.update(content, m_rankPresent, m_rankTiers);
    }

    void BenchmarkTrackingViewModel::updateRankHistoryOnRuns(BenchmarkHistoryViewModel &history,
                                                             const domain::Benchmark *benchmark, const bool recent,
                                                             const application::RankRangeView &view) const {
        BenchmarkHistoryViewModel::Content content;
        content.onRunAxis = true;
        std::span<const domain::AverageRankAfterRun> full;
        std::span<const std::chrono::sys_seconds> starts;
        if (m_rankPresent && benchmark) {
            full = recent ? benchmark->recentAverageRankHistory() : benchmark->averageRankHistory();
            starts = benchmark->memberRunStarts();
            content.historyAvailable = !full.empty() && !view.noRelevantHistory;
            for (const auto &point: full) content.fullHistoryPeak = std::max(content.fullHistoryPeak, point.averageRank);
        }

        const auto dayOf = [](const std::chrono::sys_seconds start) { return std::chrono::floor<std::chrono::days>(start); };
        std::optional<domain::RunSpan> span;
        std::optional<std::pair<qint64, qint64>> dayRange;
        if (content.historyAvailable) {
            if (const auto &interval = view.interval) {
                span = domain::runSpanOfDays(starts, interval->first, interval->last);
                dayRange = std::pair{utcMidnightMs(interval->first), utcMidnightMs(interval->last)};
            } else if (view.anchoredEnd) {
                span = domain::runSpanOfDays(starts, dayOf(starts.front()), *view.anchoredEnd);
            } else {
                span = domain::RunSpan{1, static_cast<int>(starts.size())};
            }
        }
        // A range with no member run spans nothing on this axis, whatever rank is held across it.
        if (!span) {
            std::ignore = history.runAxis().setRuns({}, 1, dayRange);
            history.update(content, m_rankPresent, m_rankTiers);
            return;
        }
        const auto visible = starts.subspan(static_cast<std::size_t>(span->first - 1),
                                            static_cast<std::size_t>(span->last - span->first + 1));
        if (!dayRange) dayRange = std::pair{utcMidnightMs(dayOf(visible.front())), utcMidnightMs(dayOf(visible.back()))};

        const auto window = domain::rankHistoryWindow(full, *span);
        for (const auto &point: window.points)
            content.observations.append(QPointF(static_cast<qreal>(point.benchmarkRun), point.averageRank));
        std::optional<double> held;
        if (window.established) {
            held = window.established->averageRank;
            if (content.observations.isEmpty() || content.observations.first().x() > span->first)
                content.leadingCarry = QPointF(static_cast<qreal>(span->first), *held);
        }
        if (!content.observations.isEmpty()) held = content.observations.last().y();
        const qreal lastX = content.observations.isEmpty() ? -std::numeric_limits<qreal>::infinity()
                                                           : content.observations.last().x();
        if (held && lastX < span->last) content.trailingCarry = QPointF(static_cast<qreal>(span->last), *held);

        std::vector<qint64> visibleMs;
        visibleMs.reserve(visible.size());
        for (const auto start: visible)
            visibleMs.push_back(std::chrono::duration_cast<std::chrono::milliseconds>(start.time_since_epoch()).count());
        std::ignore = history.runAxis().setRuns(std::move(visibleMs), span->first, dayRange);
        history.update(content, m_rankPresent, m_rankTiers);
    }

    void BenchmarkTrackingViewModel::rebuildRank(const application::GraphKey graph) {
        const application::BenchmarkWorkspaceSnapshot &snapshot = m_useCase->snapshot();
        const bool recent = graph == application::GraphKey::BenchmarkRecentRank;
        auto &history = recent ? *m_recentRankHistory : *m_rankHistory;
        const auto &view = recent ? snapshot.recentRank : snapshot.rank;
        if (showsRunAxis(m_useCase->presentation(graph))) {
            updateRankHistoryOnRuns(history, snapshot.benchmark ? &*snapshot.benchmark : nullptr, recent, view);
            return;
        }
        applyRankAxis(history.calendarAxis(), view);
        updateRankHistory(history, history.calendarAxis(), recent ? m_recentRankByDay : m_rankByDay, view);
    }

    void BenchmarkTrackingViewModel::rebuildPlaytime() {
        const application::BenchmarkWorkspaceSnapshot &snapshot = m_useCase->snapshot();
        const QList<QPointF> playtime = playtimePoints(snapshot);
        if (unionExtent() != m_appliedUnionMs) {
            // The rank axes, and the rank lines held flat to their edges, depend on the shared extent.
            rebuild();
            return;
        }
        applyPlaytimeAxis(snapshot);
        m_playtimeHistory->setAveragingWindowDays(snapshot.playtime.windowDays);
        m_playtimeHistory->update(playtimeContent(snapshot, playtime), snapshot.benchmark.has_value(), {});
    }
}
