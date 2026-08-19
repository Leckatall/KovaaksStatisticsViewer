//
// Created by Lecka on 30/07/2026.
//

#include "graph_vm.h"
#include "series_expression_qml.h"

#include <qurl.h>
#include <QDebug>
#include <QSet>
#include <algorithm>
#include <utility>
#include <vector>


namespace ksv::presentation {
    namespace {
        struct ColumnMeta {
            const char *name;
            QColor color;
            int yAxis;
        };

        struct AxisDescriptor {
            ValueTransform transform;
            AxisModel::Options options;
        };

        enum YAxis {
            ScoreAxis, AccuracyAxis, ShotsAxis, KillsAxis, DmgAxis, ScoreFamilyAxis, YAxisCount
        };

        ValueTransform secondsDelegate() {
            ValueTransform t;
            t.formatter = [](const qreal v) { return QString::number(qRound(v)) + "s"; };
            return t;
        }

        const std::array<AxisDescriptor, YAxisCount> kYAxisMeta{
            {
                {ValueTransform::identity(), {}},
                {ValueTransform::percentage(), {}},
                {ValueTransform::identity(), {}},
                {ValueTransform::identity(), {}},
                {ValueTransform::identity(), {}},
                {ValueTransform::identity(), {}},
            }
        };

        const std::array<ColumnMeta, GraphViewModel::ColumnCount> kColumnMeta{
            {
                {"Time", QColor(), -1},
                {"Score", QColor("#009600"), ScoreAxis},
                {"Accuracy", QColor("cyan"), AccuracyAxis},
                {"Shots", QColor("orange"), ShotsAxis},
                {"Kills", QColor("red"), KillsAxis},
                {"Dmg", QColor("yellow"), DmgAxis},
                {"Score Total", QColor("purple"), ScoreFamilyAxis},
                {"Expected Final Score", QColor("magenta"), ScoreFamilyAxis},
                {"Expected Final Score (5s)", QColor("deepskyblue"), ScoreFamilyAxis},
            }
        };

        static_assert(static_cast<int>(application::ColumnId::Time) == GraphViewModel::Time);
        static_assert(static_cast<int>(application::ColumnId::Score) == GraphViewModel::Score);
        static_assert(static_cast<int>(application::ColumnId::Accuracy) == GraphViewModel::Accuracy);
        static_assert(static_cast<int>(application::ColumnId::Shots) == GraphViewModel::Shots);
        static_assert(static_cast<int>(application::ColumnId::Kills) == GraphViewModel::Kills);
        static_assert(static_cast<int>(application::ColumnId::Dmg) == GraphViewModel::Dmg);
        static_assert(static_cast<int>(application::ColumnId::ScoreTotal) == GraphViewModel::ScoreTotal);
        static_assert(static_cast<int>(application::ColumnId::ExpectedFinalScore) == GraphViewModel::ExpectedFinalScore)
        ;
        static_assert(
            static_cast<int>(application::ColumnId::ExpectedFinalScoreRecent) ==
            GraphViewModel::ExpectedFinalScoreRecent);

    }

    GraphViewModel::GraphViewModel(std::shared_ptr<application::IGraphUseCase> graphUseCase,
                                   QObject *parent) : GraphViewModelBase(parent),
                                                      m_graphUseCase(std::move(graphUseCase)) {
        m_enabledColumns = allColumns();
        // for (int c = Score; c < ColumnCount; ++c) {
        //     SeriesModel series;
        //     series.name = GraphViewModel::columnName(c);
        //     series.color = kColumnMeta[c].color;
        //     series.transform = kYAxisMeta[kColumnMeta[c].yAxis].transform;
        //     m_series.append(std::move(series));
        // }
        fetchMetadata();
        recomputeBounds();
        m_graphUseCase->onSeriesConfigChanged([this] { fetchMetadata(); });
    }

    void GraphViewModel::setData(QList<QMap<Column, qreal> > data) {
        m_data = std::move(data);
        emit dataUpdated();
        recomputeBounds();
    }

    QList<SeriesModel> GraphViewModel::series(const QList<int> &columns) const {
        QList<SeriesModel> result;
        result.reserve(columns.size());
        std::array<std::vector<int>, YAxisCount> membersByAxis;
        // for (const int column: columns) {
        //     if (column < Score || column >= ColumnCount) continue;
        //     result.append(m_series[column - Score]);
        //     result.back().column = column;
        //     membersByAxis[kColumnMeta[column].yAxis].push_back(result.size() - 1);
        // }
        for (const SeriesModel &series: m_seriesById) {
            if (!columns.contains(columnForSeriesId(series.id))) continue;
            result.append(series);
            membersByAxis[kColumnMeta[series.column].yAxis].push_back(result.size() - 1);
        }
        for (int axis = 0; axis < YAxisCount; ++axis) {
            const auto &indices = membersByAxis[axis];
            if (indices.empty()) continue;
            std::vector<const SeriesModel *> members;
            members.reserve(indices.size());
            for (const int index: indices) members.push_back(&result[index]);
            const auto &descriptor = kYAxisMeta[axis];
            const AxisModel yAxis = axisForSeries(members, descriptor.options, descriptor.transform);
            for (const int index: indices) result[index].yAxis = yAxis;
        }
        return result;
    }

    QVariantList GraphViewModel::plottableColumns() const {
        return allColumns();
    }

    QVariantList GraphViewModel::allColumns() const {
        QVariantList columns;
        columns.reserve(static_cast<qsizetype>(application::kPlottableColumnIds.size()));
        for (const auto column: application::kPlottableColumnIds) {
            columns.append(static_cast<int>(column));
        }
        return columns;
    }

    void GraphViewModel::setEnabledColumns(const std::vector<application::ColumnId> &columns) {
        QSet<int> requested;
        for (const auto column: columns) {
            if (application::isPlottableGraphColumn(column)) requested.insert(static_cast<int>(column));
        }

        QVariantList normalized;
        normalized.reserve(static_cast<qsizetype>(application::kPlottableColumnIds.size()));
        for (const auto column: application::kPlottableColumnIds) {
            const auto value = static_cast<int>(column);
            if (requested.contains(value)) normalized.append(value);
        }
        if (m_enabledColumns == normalized) return;
        m_enabledColumns = std::move(normalized);
        emit enabledColumnsChanged();
    }

    QVariantMap GraphViewModel::axisBounds() const {
        QVariantMap map;
        for (int c = 0; c < ColumnCount; ++c) {
            map[QString::number(c)] = QPointF(m_axes[c].min(), m_axes[c].max());
        }
        return map;
    }

    QList<qreal> GraphViewModel::axisTicks(const int column) const {
        if (column < 0 || column >= ColumnCount) return {};
        return m_axes[column].ticks();
    }

    QString GraphViewModel::columnName(const int column) const {
        if (column < 0 || column >= ColumnCount) return {};
        return QString::fromLatin1(kColumnMeta[column].name);
    }

    QColor GraphViewModel::columnColor(const int column) const {
        if (column < 0 || column >= ColumnCount) return {};
        return kColumnMeta[column].color;
    }

    QString GraphViewModel::columnKey(const int column) const {
        if (column < 0 || column >= ColumnCount) return {};
        const auto key = application::graphColumnKey(static_cast<application::ColumnId>(column));
        return QString::fromLatin1(key.data(), static_cast<qsizetype>(key.size()));
    }

    int GraphViewModel::columnYAxis(const int column) const {
        if (column < 0 || column >= ColumnCount) return -1;
        return kColumnMeta[column].yAxis;
    }

    QList<QPointF> GraphViewModel::seriesPoints(const int column) const {
        if (column < 0 || column >= ColumnCount) return {};
        const auto col = static_cast<Column>(column);
        QList<QPointF> points;
        points.reserve(m_data.size());
        for (const auto &row: m_data) points.append(QPointF(row[Time], row[col]));
        return points;
    }

    namespace {
        std::pair<qreal, qreal> rawColumnRange(const QList<QMap<GraphViewModel::Column, qreal> > &data,
                                               const GraphViewModel::Column column) {
            qreal lo = data.front()[column];
            qreal hi = lo;
            for (const auto &row: data) {
                lo = std::min(lo, row[column]);
                hi = std::max(hi, row[column]);
            }
            return {lo, hi};
        }
    }

    void GraphViewModel::recomputeBounds() {
        // Time: zero floor, integral steps (whole seconds)
        const AxisModel::Options timeOpts{AxisModel::Baseline::Zero, /*integral=*/true};

        std::array<AxisModel, ColumnCount> newAxes{};
        if (m_data.isEmpty()) {
            newAxes[Time] = AxisModel::forRange(0.0, 60.0, timeOpts);
            for (int c = Score; c < ColumnCount; ++c) newAxes[c] = AxisModel::forRange(0.0, 1.0);
        } else {
            const auto [xlo, xhi] = rawColumnRange(m_data, Time);
            newAxes[Time] = AxisModel::forRange(0.0, xhi, timeOpts);
            for (int c = Score; c < ColumnCount; ++c) {
                const auto [lo, hi] = rawColumnRange(m_data, static_cast<Column>(c));
                newAxes[c] = AxisModel::forRange(lo, hi);
            }
        }
        newAxes[Time] = newAxes[Time].withDelegate(secondsDelegate());

        if (!m_data.isEmpty()) {
            qreal lo = 0.0;
            qreal hi = 0.0;
            bool initialized = false;
            for (int c = ScoreTotal; c <= ExpectedFinalScoreRecent; ++c) {
                const auto range = rawColumnRange(m_data, static_cast<Column>(c));
                if (!initialized) {
                    lo = range.first;
                    hi = range.second;
                    initialized = true;
                } else {
                    lo = std::min(lo, range.first);
                    hi = std::max(hi, range.second);
                }
            }
            const AxisModel scoreFamilyAxis = AxisModel::forRange(lo, hi);
            for (int c = ScoreTotal; c <= ExpectedFinalScoreRecent; ++c) newAxes[c] = scoreFamilyAxis;
        }

        bool changed = false;
        for (int c = 0; c < ColumnCount; ++c) {
            if (!qFuzzyCompare(1.0 + m_axes[c].min(), 1.0 + newAxes[c].min()) ||
                !qFuzzyCompare(1.0 + m_axes[c].max(), 1.0 + newAxes[c].max())) {
                changed = true;
                break;
            }
        }

        if (!changed) return;

        m_axes = newAxes;

        emit boundsChanged();
    }

    void GraphViewModel::fetchLatestData() {
        m_graphUseCase->load_latest_perf();
        // fetchData() called through signal
    }

    void GraphViewModel::fetchMetadata() {
        auto series_list = m_graphUseCase->get_resolved_graph().series;
        m_seriesById.clear();
        for (const auto &entry: series_list) {
            if (!entry.config.presentation.enabled) continue;
            m_seriesById[QString::number(entry.config.id.value)] = SeriesModel{
                .id = QString::number(entry.config.id.value),
                .name = QString::fromStdString(entry.config.presentation.name),
                .color = QColor(entry.config.presentation.lineStyle.color.red,
                                 entry.config.presentation.lineStyle.color.green,
                                 entry.config.presentation.lineStyle.color.blue,
                                 entry.config.presentation.lineStyle.color.alpha),
                // TODO(18/08/26): transform and yAxisId should be stored as part of seriesConfig
                // .transform = entry.config.presentation.lineStyle.transform,
                // TODO(18/08/26): displayPosition and width support in SeriesModel
                // .displayPosition = entry.config.presentation.displayPosition,
            };
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
            m_allSeries.clear();
            m_enabledSeriesIds.clear();
            m_legacyColumnIds.clear();
            QVariantList transitionalEnabledColumns;
            QList<QMap<Column, qreal> > rows(int(resolved.times.size()));
            for (int i = 0; i < rows.size(); ++i) rows[i][Time] = resolved.times[static_cast<size_t>(i)];

            for (const auto &entry: resolved.series) {
                const auto &presentation = entry.config.presentation;
                if (!presentation.enabled) continue;
                const auto &entry_id = entry.config.id.value;
                const QString id = QString::number(entry_id);
                const auto values = entry.values.value_or({});
                QList<QPointF> points;
                points.reserve(resolved.times.size());
                for (int i = 0; i < rows.size(); ++i) {
                    points.append(QPointF(resolved.times[i], values[i]));
                }
                m_seriesById[id].points = points;

                int column = -1;
                if (entry_id <= 3) column = static_cast<int>(entry_id);
                else if (entry_id <= 9) column = static_cast<int>(entry_id - 1);
                m_seriesById[id].column = column;

                QVariantMap series{
                    {"id", id},
                    {"name", QString::fromStdString(presentation.name)},
                    {"color", QColor(presentation.lineStyle.color.red, presentation.lineStyle.color.green,
                                      presentation.lineStyle.color.blue, presentation.lineStyle.color.alpha)},
                    {"enabled", presentation.enabled},
                    {"displayPosition", presentation.displayPosition},
                };
                m_allSeries.append(series);
                m_enabledSeriesIds.append(id);

                if (column >= Score && column < ColumnCount) m_legacyColumnIds[column] = id;
                if (presentation.enabled && column >= Score && column < ColumnCount)
                    transitionalEnabledColumns.append(column);
            }
            if (m_enabledColumns != transitionalEnabledColumns) {
                m_enabledColumns = std::move(transitionalEnabledColumns);
                emit enabledColumnsChanged();
            }
            emit seriesConfigurationChanged();
            setData(std::move(rows));
            return;
        }

        const application::GraphSeries seriesData = m_graphUseCase->get_series();

        QList<QMap<Column, qreal> > rows(int(seriesData.times.size()));
        for (int i = 0; i < int(seriesData.times.size()); ++i) rows[i][Time] = seriesData.times[i];

        for (int c = Score; c < ColumnCount; ++c) {
            const auto it = seriesData.columns.find(static_cast<application::ColumnId>(c));
            if (it == seriesData.columns.end()) continue;
            const auto &values = it->second;
            for (int i = 0; i < rows.size() && i < int(values.size()); ++i) rows[i][static_cast<Column>(c)] = values[i];
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
        return m_legacyColumnIds.value(column);
    }

    int GraphViewModel::columnForSeriesId(const QString &id) const {
        const auto it = m_seriesById.find(id);
        return it != m_seriesById.end() ? it->column : -1;
    }

    QVariantMap GraphViewModel::setSeriesEnabled(const QString &id, const bool enabled) {
        if (id.startsWith("computed:")) {
            const auto result = m_graphUseCase->setSeriesEnabled(application::SeriesId{id.toULongLong()},enabled);
            if (result.succeeded()) {
                fetchData();
                for (auto it = m_legacyColumnIds.cbegin(); it != m_legacyColumnIds.cend(); ++it)
                    if (it.value() == id) {
                        auto columns = m_enabledColumns;
                        columns.removeAll(it.key());
                        if (enabled) columns.append(it.key());
                        if (columns != m_enabledColumns) {
                            m_enabledColumns = columns;
                            emit enabledColumnsChanged();
                        }
                        break;
                    }
            }
            return mutationMap(result);
        }

        return invalidMutationMap();
    }

    QVariantMap GraphViewModel::updateBasePresentation(const QString &id, const QColor &color, const double width) {
        // const std::optional<application::PrimitiveMetric> metric = id.startsWith("base:")
        //                                                                ? metricFromString(id.mid(5))
        //                                                                : std::optional<application::PrimitiveMetric>{};
        // if (!metric) return invalidMutationMap();
        // return mutationMap(m_graphUseCase->updateBasePresentation({
        //     *metric, true,
        //     {
        //         {
        //             static_cast<uint8_t>(color.red()), static_cast<uint8_t>(color.green()),
        //             static_cast<uint8_t>(color.blue()), static_cast<uint8_t>(color.alpha())
        //         },
        //         width
        //     }
        // }));
        return {};
    }

    QVariantMap GraphViewModel::createComputedSeries(const QString &name, const QColor &color, const double width,
                                                     const bool enabled, const QVariantMap &expression) {
        const auto parsed = parseExpression(expression);
        if (!parsed) return invalidMutationMap();
        return mutationMap(m_graphUseCase->createComputed({
            {
                name.toStdString(),
                {
                    {
                        static_cast<uint8_t>(color.red()), static_cast<uint8_t>(color.green()),
                        static_cast<uint8_t>(color.blue()), static_cast<uint8_t>(color.alpha())
                    },
                    width
                },
                enabled
            },
            *parsed
        }));
    }

    QVariantMap GraphViewModel::updateComputedSeries(const QString &id, const QString &name, const QColor &color,
                                                     const double width, const bool enabled,
                                                     const QVariantMap &expression) {
        const auto parsed = parseExpression(expression);
        bool ok = false;
        const auto numericId = id.startsWith("computed:") ? id.mid(9).toULongLong(&ok) : 0;
        if (!parsed || !ok || numericId == 0) return invalidMutationMap();
        application::UpdateComputedSeriesRequest request;
        request.id.value = numericId;
        request.presentation.name = name.toStdString();
        request.presentation.lineStyle = {
            {
                static_cast<uint8_t>(color.red()), static_cast<uint8_t>(color.green()),
                static_cast<uint8_t>(color.blue()), static_cast<uint8_t>(color.alpha())
            },
            width
        };
        request.presentation.enabled = enabled;
        request.expression = *parsed;
        return mutationMap(m_graphUseCase->updateComputed(request));
    }

    QVariantMap GraphViewModel::removeComputedSeries(const QString &id) {
        bool ok = false;
        const auto numericId = id.startsWith("computed:") ? id.mid(9).toULongLong(&ok) : 0;
        return ok && numericId ? mutationMap(m_graphUseCase->removeComputed({numericId})) : invalidMutationMap();
    }

    QVariantMap GraphViewModel::moveSeries(const QString &id, const int displayPosition) {
        // if (displayPosition < 0) return invalidMutationMap();
        // if (id.startsWith("computed:")) return mutationMap(
        //     m_graphUseCase->moveSeries(application::SeriesId{id.mid(9).toULongLong()},
        //                                static_cast<uint32_t>(displayPosition)));
        // const std::optional<application::PrimitiveMetric> metric = id.startsWith("base:")
        //                                                                ? metricFromString(id.mid(5))
        //                                                                : std::optional<application::PrimitiveMetric>{};
        // return metric
        //            ? mutationMap(m_graphUseCase->moveSeries(*metric, static_cast<uint32_t>(displayPosition)))
        //            : invalidMutationMap();
        return{};
    }
}
