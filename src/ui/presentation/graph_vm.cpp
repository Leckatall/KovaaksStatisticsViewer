//
// Created by Lecka on 30/07/2026.
//

#include "graph_vm.h"

#include <qurl.h>
#include <QDebug>
#include <algorithm>
#include <optional>
#include <utility>
#include <vector>


namespace ksv::presentation {
    namespace {
        ValueTransform transformFor(const application::AxisTransformKind kind) {
            switch (kind) {
                case application::AxisTransformKind::Identity: return ValueTransform::identity();
                case application::AxisTransformKind::Percentage: return ValueTransform::percentage();
            }
            return ValueTransform::identity();
        }

        AxisModel::Options axisOptionsFor(const application::AxisModelOptions &options) {
            return AxisModel::Options{
                options.baseline == application::AxisModelOptions::Baseline::Zero
                    ? AxisModel::Baseline::Zero
                    : AxisModel::Baseline::HugData,
                options.integral, options.targetTicks, options.fallbackSpan
            };
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

    QList<SeriesModel *> GraphViewModel::series(const QList<int> &columns) const {
        QList<SeriesModel *> result;
        result.reserve(columns.size());
        QHash<uint64_t, std::vector<int>> membersByAxis;
        for (SeriesModel *series: m_seriesById) {
            if (!columns.contains(series->column())) continue;
            result.append(series);
            if (series->yAxisId) membersByAxis[*series->yAxisId].push_back(result.size() - 1);
        }
        for (auto it = membersByAxis.constBegin(); it != membersByAxis.constEnd(); ++it) {
            const auto axisIt = m_axesById.constFind(it.key());
            if (axisIt == m_axesById.constEnd()) continue;
            std::vector<const SeriesModel *> members;
            members.reserve(it.value().size());
            for (const int index: it.value()) members.push_back(result[index]);
            const AxisModel yAxis = axisForSeries(members, axisOptionsFor(axisIt->options),
                                                  transformFor(axisIt->transformKind));
            for (const int index: it.value()) result[index]->yAxis = yAxis;
        }
        return result;
    }

    void GraphViewModel::recomputeBounds() {
        // Time: zero floor, integral steps (whole seconds)
        const AxisModel::Options timeOpts{AxisModel::Baseline::Zero, /*integral=*/true};

        const double hi = m_graphUseCase->getRunDuration();
        AxisModel newTimeAxis = hi > 0.0
                                    ? AxisModel::forRange(0.0, hi, timeOpts)
                                    : AxisModel::forRange(0.0, 60.0, timeOpts);
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
        const auto configs = m_graphUseCase->getSeriesConfigs();
        const auto axes = m_graphUseCase->getAxes();

        m_axesById.clear();
        for (const auto &axis: axes) m_axesById.insert(axis.id.value, axis);

        qDeleteAll(m_seriesById);
        m_seriesById.clear();
        for (const auto &config: configs) {
            const auto entry_id = QString::number(config.id.value);
            auto *series = new SeriesModel(this);
            series->setId(entry_id);
            series->setName(QString::fromStdString(config.presentation.name));
            series->setColor(QColor(config.presentation.lineStyle.color.red,
                                     config.presentation.lineStyle.color.green,
                                     config.presentation.lineStyle.color.blue,
                                     config.presentation.lineStyle.color.alpha));
            series->transform = transformFor(config.transformKind);
            series->yAxisId = config.yAxisId
                                  ? std::optional<uint64_t>{config.yAxisId->value}
                                  : std::nullopt;
            series->setColumn(entry_id.toInt());
            // TODO(2026-08-18): Remove once SeriesModel gains displayPosition/width support by replacing
            // this placeholder with assignments from config.presentation.
            // series->displayPosition = config.presentation.displayPosition;
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

        const auto configs = m_graphUseCase->getSeriesConfigs();
        m_allSeriesList.clear();
        m_enabledSeriesIds.clear();
        for (const auto &config: configs) {
            const auto entry_id = config.id.value;
            const QString id = QString::number(entry_id);
            const auto values = m_graphUseCase->getSeriesValues(config.id);
            QList<QPointF> points;
            if (values) {
                points.reserve(static_cast<int>(values->size()));
                for (const auto &[x, y]: *values) points.append(QPointF(x, y));
            }

            // fetchData() may run without a preceding fetchMetadata() (e.g. called directly), so
            // create any SeriesModel the id map doesn't already hold.
            SeriesModel *entrySeries = m_seriesById.value(id, nullptr);
            if (!entrySeries) {
                entrySeries = new SeriesModel(this);
                m_seriesById[id] = entrySeries;
            }
            entrySeries->points = points;
            entrySeries->yAxis = entrySeries->deriveYAxis();
            entrySeries->setColumn(static_cast<int>(entry_id));

            m_allSeriesList.append(entrySeries);
            m_enabledSeriesIds.append(id);
        }
        emit seriesConfigurationChanged();
        emit dataUpdated();
        recomputeBounds();
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
