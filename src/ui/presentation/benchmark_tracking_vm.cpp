#include "benchmark_tracking_vm.h"

#include <QCollator>
#include <QPointF>
#include <QVariantMap>

#include <algorithm>
#include <cmath>
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

        void collectGroupNames(const std::vector<domain::GroupProjection> &groups, QHash<QString, QString> &out) {
            for (const auto &group: groups) {
                out.insert(q(group.id.value), q(group.name));
                collectGroupNames(group.subgroups, out);
            }
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
                return QObject::tr("The selected benchmark is no longer available. Open the Benchmark Manager to repair it.");
            if (state == QLatin1String("incomplete"))
                return QObject::tr("This benchmark's setup is incomplete. Finish it in the Benchmark Manager to unlock ranks.");
            return QObject::tr("Tracking is up to date.");
        }
    }

    BenchmarkTrackingViewModel::BenchmarkTrackingViewModel(
        std::shared_ptr<application::IBenchmarkTrackingUseCase> useCase, QObject *parent)
        : QObject(parent), m_useCase(std::move(useCase)),
          m_rankHistory(new BenchmarkHistoryViewModel(BenchmarkHistoryViewModel::AverageRank, m_historyXAxis, this)),
          m_playtimeHistory(
              new BenchmarkHistoryViewModel(BenchmarkHistoryViewModel::RollingPlaytime, m_historyXAxis, this)),
          m_breakdown(new BenchmarkBreakdownModel(this)) {
        m_useCase->onChanged([this] { rebuild(); });
        rebuild();
    }

    void BenchmarkTrackingViewModel::selectBenchmark(const QString &id) {
        m_useCase->select(domain::BenchmarkId{id.toStdString()});
    }

    void BenchmarkTrackingViewModel::clearSelection() { m_useCase->clearSelection(); }

    void BenchmarkTrackingViewModel::setExpanded(const QString &nodeId, const bool expanded) {
        m_breakdown->setExpanded(nodeId, expanded);
    }

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

        std::vector<QString> tierNames;
        QHash<QString, QString> tierNameById;
        const auto indexTiers = [&](const std::vector<domain::Tier> &tiers) {
            for (const auto &tier: tiers) {
                tierNames.push_back(q(tier.name));
                tierNameById.insert(q(tier.id.value), q(tier.name));
            }
        };
        if (projection) indexTiers(projection->tiers);
        else if (definition) indexTiers(definition->tiers);

        const QString unavailableText = tr("Unavailable");
        if (trackable && projection) {
            m_attainedRank = projection->attainedRank
                                 ? tierNameById.value(q(projection->attainedRank->value), tr("Unranked"))
                                 : tr("Unranked");
            m_completedRank = projection->completedRank
                                  ? tierNameById.value(q(projection->completedRank->value), tr("None completed"))
                                  : tr("None completed");
            m_nextTier = projection->nextTier
                             ? tierNameById.value(q(projection->nextTier->value), tr("Highest tier attained"))
                             : tr("Highest tier attained");
            m_averageRank = projection->averageRank ? formatBenchmarkNumber(*projection->averageRank) : unavailableText;
        } else {
            m_attainedRank = unavailableText;
            m_completedRank = unavailableText;
            m_nextTier = unavailableText;
            m_averageRank = unavailableText;
        }
        m_totalPlaytime = projection ? formatDuration(projection->totalPlaytimeSeconds) : QString();
        m_scenariosAtNextTier = projection ? projection->scenariosAtNextTier : 0;
        m_scenarioCount = projection ? static_cast<int>(projection->scenarios.size()) : 0;

        m_blockers.clear();
        if (trackable && projection) {
            QHash<QString, QString> scenarioNames;
            for (const auto &scenario: projection->scenarios)
                scenarioNames.insert(q(scenario.entryId.value), q(scenario.name));
            QHash<QString, QString> groupNames;
            collectGroupNames(projection->categories, groupNames);

            for (const auto &blocker: projection->nextTierBlockers) {
                QVariantMap map;
                map[QStringLiteral("kind")] = blocker.entryId.value.empty() && blocker.group
                                                  ? QStringLiteral("group")
                                                  : QStringLiteral("scenario");
                map[QStringLiteral("entryId")] = q(blocker.entryId.value);
                map[QStringLiteral("scenarioName")] = scenarioNames.value(q(blocker.entryId.value),
                                                                          q(blocker.entryId.value));
                map[QStringLiteral("groupName")] =
                    blocker.group ? groupNames.value(q(blocker.group->value), q(blocker.group->value))
                                  : QString();
                map[QStringLiteral("currentText")] =
                    blocker.current ? formatBenchmarkNumber(*blocker.current) : tr("Unplayed");
                map[QStringLiteral("requiredText")] = formatBenchmarkNumber(blocker.required);
                m_blockers.push_back(map);
            }
        }

        const bool rankPresent = trackable && projection && !projection->averageRankHistory.empty();
        std::vector<qint64> unionMs;
        QList<QPointF> rankPoints;
        if (rankPresent)
            for (const auto &point: projection->averageRankHistory) {
                const qint64 milliseconds = utcMidnightMs(point.day);
                rankPoints.append(QPointF(static_cast<qreal>(milliseconds), point.averageRank));
                unionMs.push_back(milliseconds);
            }

        const bool playtimePresent = projection && !projection->rollingPlaytime.empty();
        QList<QPointF> playtimePoints;
        if (playtimePresent)
            for (const auto &[day, seconds]: projection->rollingPlaytime) {
                const qint64 milliseconds = utcMidnightMs(day);
                playtimePoints.append(QPointF(static_cast<qreal>(milliseconds), seconds));
                unionMs.push_back(milliseconds);
            }

        if (unionMs.empty()) {
            const QDateTime now = QDateTime::currentDateTimeUtc();
            std::ignore = m_historyXAxis.setRange(now, now);
        } else {
            const auto [lo, hi] = std::minmax_element(unionMs.begin(), unionMs.end());
            std::ignore = m_historyXAxis.setEpochMillisecondsRange(*lo, *hi);
        }
        m_rankHistory->update(rankPoints, rankPresent, tierNames);
        m_playtimeHistory->update(playtimePoints, playtimePresent, {});

        m_breakdown->reset(definition, projection);

        emit changed();
    }
}
