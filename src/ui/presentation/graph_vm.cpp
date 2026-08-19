//
// Created by Lecka on 30/07/2026.
//

#include "graph_vm.h"

#include <qurl.h>
#include <QDebug>
#include <algorithm>
#include <array>
#include <optional>
#include <utility>
#include <vector>


namespace ksv::presentation {
    namespace {
        struct AxisDescriptor {
            ValueTransform transform;
            AxisModel::Options options;
        };

        // Only series that can't derive a sensible axis on their own get grouped here; everything
        // else (Score, Shots, Kills, Dmg, and any user computed series) falls through to
        // SeriesModel::deriveYAxis()'s per-series default via GraphCanvas::yAxisFor().
        enum YAxis { AccuracyAxis, ScoreFamilyAxis, YAxisCount };

        const std::array<AxisDescriptor, YAxisCount> kYAxisMeta{
            {
                {ValueTransform::percentage(), {}},
                {ValueTransform::identity(), {}},
            }
        };

        // column == SeriesId::value for the built-in series (see fetchMetadata()/fetchData()).
        std::optional<YAxis> yAxisFor(const int column) {
            switch (column) {
                // case 2: return AccuracyAxis; // Accuracy
                case 7: case 8: case 9: return ScoreFamilyAxis; // Score Total / Expected Final Score / (5s)
                default: return std::nullopt;
            }
        }

        ValueTransform secondsDelegate() {
            ValueTransform t;
            t.formatter = [](const qreal v) { return QString::number(qRound(v)) + "s"; };
            return t;
        }
    }

    GraphViewModel::GraphViewModel(std::shared_ptr<application::IGraphUseCase> graphUseCase,
                                   QObject *parent) : GraphViewModelBase(parent),
                                                      m_graphUseCase(std::move(graphUseCase)) {
        fetchMetadata();
        recomputeBounds();
        m_graphUseCase->onSeriesConfigChanged([this] { fetchMetadata(); });
    }

    void GraphViewModel::setData(QList<QMap<int, qreal> > data) {
        m_data = std::move(data);
        emit dataUpdated();
        recomputeBounds();
    }

    QList<SeriesModel *> GraphViewModel::series(const QList<int> &columns) const {
        QList<SeriesModel *> result;
        result.reserve(columns.size());
        std::array<std::vector<int>, YAxisCount> membersByAxis;
        for (SeriesModel *series: m_seriesById) {
            if (!columns.contains(series->column())) continue;
            result.append(series);
            if (const auto axis = yAxisFor(series->column())) membersByAxis[*axis].push_back(result.size() - 1);
        }
        for (int axis = 0; axis < YAxisCount; ++axis) {
            const auto &indices = membersByAxis[axis];
            if (indices.empty()) continue;
            std::vector<const SeriesModel *> members;
            members.reserve(indices.size());
            for (const int index: indices) members.push_back(result[index]);
            const auto &descriptor = kYAxisMeta[axis];
            const AxisModel yAxis = axisForSeries(members, descriptor.options, descriptor.transform);
            for (const int index: indices) result[index]->yAxis = yAxis;
        }
        return result;
    }

    QVariantMap GraphViewModel::axisBounds() const {
        return {{QString::number(kTimeColumn), QPointF(m_timeAxis.min(), m_timeAxis.max())}};
    }

    QList<qreal> GraphViewModel::axisTicks(const int column) const {
        return column == kTimeColumn ? m_timeAxis.ticks() : QList<qreal>{};
    }

    QList<QPointF> GraphViewModel::seriesPoints(const int column) const {
        QList<QPointF> points;
        points.reserve(m_data.size());
        for (const auto &row: m_data) points.append(QPointF(row[kTimeColumn], row[column]));
        return points;
    }

    void GraphViewModel::recomputeBounds() {
        // Time: zero floor, integral steps (whole seconds)
        const AxisModel::Options timeOpts{AxisModel::Baseline::Zero, /*integral=*/true};

        AxisModel newTimeAxis;
        if (m_data.isEmpty()) {
            newTimeAxis = AxisModel::forRange(0.0, 60.0, timeOpts);
        } else {
            qreal hi = m_data.front()[kTimeColumn];
            for (const auto &row: m_data) hi = std::max(hi, row[kTimeColumn]);
            newTimeAxis = AxisModel::forRange(0.0, hi, timeOpts);
        }
        newTimeAxis = newTimeAxis.withDelegate(secondsDelegate());

        if (qFuzzyCompare(1.0 + m_timeAxis.min(), 1.0 + newTimeAxis.min()) &&
            qFuzzyCompare(1.0 + m_timeAxis.max(), 1.0 + newTimeAxis.max()))
            return;

        m_timeAxis = newTimeAxis;

        emit boundsChanged();
    }

    void GraphViewModel::fetchLatestData() {
        m_graphUseCase->load_latest_perf();
        // fetchData() called through signal
    }

    void GraphViewModel::fetchMetadata() {
        auto series_list = m_graphUseCase->get_resolved_graph().series;
        qDeleteAll(m_seriesById);
        m_seriesById.clear();
        for (const auto &entry: series_list) {
            if (!entry.config.presentation.enabled) continue;
            const auto entry_id = QString::number(entry.config.id.value);
            auto *series = new SeriesModel(this);
            series->setId(entry_id);
            series->setName(QString::fromStdString(entry.config.presentation.name));
            series->setColor(QColor(entry.config.presentation.lineStyle.color.red,
                                     entry.config.presentation.lineStyle.color.green,
                                     entry.config.presentation.lineStyle.color.blue,
                                     entry.config.presentation.lineStyle.color.alpha));
            series->transform = entry_id == "2" ? ValueTransform::percentage() : ValueTransform::identity();
            series->setColumn(entry_id.toInt());
            // TODO(18/08/26): transform and yAxisId should be stored as part of seriesConfig
            // series->transform = entry.config.presentation.lineStyle.transform;
            // TODO(18/08/26): displayPosition and width support in SeriesModel
            // series->displayPosition = entry.config.presentation.displayPosition;
            m_seriesById[entry_id] = series;
        }
        fetchData();
    }

    void GraphViewModel::fetchData() {
        const QString newTitle = QString::fromStdString(m_graphUseCase->get_run_label());
        if (newTitle != m_scenarioTitle) {
            m_scenarioTitle = newTitle;
            emit scenarioTitleChanged(); // Seems redundant
        }

        const auto resolved = m_graphUseCase->get_resolved_graph();
        if (!resolved.series.empty() || !resolved.times.empty()) {
            m_allSeriesList.clear();
            m_enabledSeriesIds.clear();
            QList<QMap<int, qreal> > rows(int(resolved.times.size()));
            for (int i = 0; i < rows.size(); ++i) rows[i][kTimeColumn] = resolved.times[static_cast<size_t>(i)];

            for (const auto &entry: resolved.series) {
                const auto &presentation = entry.config.presentation;
                const auto &entry_id = entry.config.id.value;
                const QString id = QString::number(entry_id);
                const auto values = entry.values.value_or({});
                QList<QPointF> points;
                points.reserve(resolved.times.size());
                for (int i = 0; i < rows.size(); ++i) {
                    points.append(QPointF(resolved.times[i], values[i]));
                }

                // fetchMetadata() only creates entries for enabled series; this preserves the
                // original QMap<QString,SeriesModel>::operator[] auto-vivification behavior for
                // any id fetchData() sees that fetchMetadata() didn't (e.g. a disabled series
                // still present in resolved.series).
                SeriesModel *entrySeries = m_seriesById.value(id, nullptr);
                if (!entrySeries) {
                    entrySeries = new SeriesModel(this);
                    m_seriesById[id] = entrySeries;
                }
                entrySeries->points = points;
                entrySeries->setColumn(static_cast<int>(entry_id));

                m_allSeriesList.append(entrySeries);
                m_enabledSeriesIds.append(id);
            }
            emit seriesConfigurationChanged();
            setData(std::move(rows));
            return;
        }

        // TODO(2026-08-19): Deprecated, unwanted fallback. Dead in production — App::App() always
        // wires GraphUseCase with a SeriesConfigStore, so get_resolved_graph() above is never empty
        // there. This branch (and the ColumnId->SeriesId table below it) exists only because ~15
        // tests in graph_vm_test.cpp still drive GraphViewModel via the legacy GraphSeries/
        // get_series() path instead of resolved_graph_to_return. Once those tests migrate to the
        // SeriesConfig-backed path, delete this whole branch, get_series(), GraphSeries, and this
        // file's use of application::ColumnId.
        const application::GraphSeries seriesData = m_graphUseCase->get_series();

        QList<QMap<int, qreal> > rows(int(seriesData.times.size()));
        for (int i = 0; i < int(seriesData.times.size()); ++i) rows[i][kTimeColumn] = seriesData.times[i];

        // Maps the legacy fixed ColumnId set onto the real built-in SeriesIds from
        // defaultSeriesConfigs(). Hits (SeriesId 4) has no ColumnId and is skipped — it was never
        // part of this legacy set.
        static constexpr std::array<std::pair<application::ColumnId, int>, 8> kLegacyColumnToSeriesId{
            {
                {application::ColumnId::Score, 1}, {application::ColumnId::Accuracy, 2},
                {application::ColumnId::Shots, 3}, {application::ColumnId::Kills, 5},
                {application::ColumnId::Dmg, 6}, {application::ColumnId::ScoreTotal, 7},
                {application::ColumnId::ExpectedFinalScore, 8}, {application::ColumnId::ExpectedFinalScoreRecent, 9},
            }
        };
        for (const auto &[legacyColumn, seriesId]: kLegacyColumnToSeriesId) {
            const auto it = seriesData.columns.find(legacyColumn);
            if (it == seriesData.columns.end()) continue;
            const auto &values = it->second;
            for (int i = 0; i < rows.size() && i < int(values.size()); ++i) rows[i][seriesId] = values[i];
        }

        setData(std::move(rows));
    }

    void GraphViewModel::fetchData(const QString &scenario_id) {
        if (scenario_id.isEmpty()) {
            qWarning() << "GraphViewModel::fetchData(scenario_id) called with an empty id; ignoring";
            return;
        }
        m_graphUseCase->load_perf(QUrl(scenario_id).toLocalFile().toStdString());
        // fetchData() called through signal
    }

    QString GraphViewModel::seriesIdForColumn(const int column) const {
        const auto id = QString::number(column);
        return m_seriesById.contains(id) ? id : QString();
    }

    int GraphViewModel::columnForSeriesId(const QString &id) const {
        const auto it = m_seriesById.find(id);
        return it != m_seriesById.end() ? (*it)->column() : -1;
    }

}
