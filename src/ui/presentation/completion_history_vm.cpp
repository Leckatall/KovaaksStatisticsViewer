#include "completion_history_vm.h"

#include <QColor>
#include <QtMath>

#include <array>
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
            ScoreAxis, AccuracyAxis, CountAxis, YAxisCount
        };

        ValueTransform runIndexTransform() {
            ValueTransform transform;
            transform.formatter = [](const qreal value) {
                return "#" + QString::number(qRound(value));
            };
            return transform;
        }

        const std::array<AxisDescriptor, YAxisCount> kYAxisMeta{{
            {ValueTransform::identity(), {.baseline = AxisModel::Baseline::Zero}},
            {ValueTransform::percentage(), {}},
            {ValueTransform::identity(), {.baseline = AxisModel::Baseline::Zero}},
        }};

        const std::array<ColumnMeta, CompletionHistoryViewModel::ColumnCount> kColumnMeta{{
            {"Run", QColor(), -1},
            {"Score", QColor("#009600"), ScoreAxis},
            {"Accuracy", QColor("cyan"), AccuracyAxis},
            {"Shots", QColor("orange"), CountAxis},
            {"Hits", QColor("#7CB342"), CountAxis},
            {"Misses", QColor("#E53935"), CountAxis},
        }};

        AxisModel axisForColumn(const QList<SeriesModel *> &series, const int column) {
            const int axis = kColumnMeta[column].yAxis;
            std::vector<const SeriesModel *> members;
            for (int member = CompletionHistoryViewModel::Score;
                 member < CompletionHistoryViewModel::ColumnCount; ++member) {
                if (kColumnMeta[member].yAxis == axis) members.push_back(series[member - CompletionHistoryViewModel::Score]);
            }
            const auto &descriptor = kYAxisMeta[axis];
            return axisForSeries(members, descriptor.options, descriptor.transform);
        }
    }

    CompletionHistoryViewModel::CompletionHistoryViewModel(
        std::shared_ptr<application::ICompletionHistoryUseCase> use_case, QObject *parent)
        : GraphViewModelBase(parent), m_use_case(std::move(use_case)) {
        for (int column = Score; column < ColumnCount; ++column) {
            auto *series_model = new SeriesModel(this);
            series_model->setId(QString::number(column));
            series_model->setName(QString::fromLatin1(kColumnMeta[column].name));
            series_model->setColor(kColumnMeta[column].color);
            series_model->setColumn(column);
            series_model->transform = kYAxisMeta[kColumnMeta[column].yAxis].transform;
            m_series.append(series_model);
        }
        refresh();
    }

    QList<SeriesModel *> CompletionHistoryViewModel::series(const QList<int> &columns) const {
        QList<SeriesModel *> result;
        result.reserve(columns.size());
        std::array<std::vector<int>, YAxisCount> membersByAxis;
        for (const int column: columns) {
            if (column < Score || column >= ColumnCount) continue;
            result.append(m_series[column - Score]);
            membersByAxis[kColumnMeta[column].yAxis].push_back(result.size() - 1);
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

    QVariantList CompletionHistoryViewModel::plottableColumns() const {
        QVariantList columns;
        for (int column = Score; column < ColumnCount; ++column) columns.append(column);
        return columns;
    }

    QVariantMap CompletionHistoryViewModel::axisBounds() const {
        QVariantMap bounds;
        bounds[QString::number(RunIndex)] = QPointF(m_x_axis.min(), m_x_axis.max());
        for (int column = Score; column < ColumnCount; ++column) {
            const AxisModel y_axis = axisForColumn(m_series, column);
            bounds[QString::number(column)] = QPointF(y_axis.min(), y_axis.max());
        }
        return bounds;
    }

    QList<qreal> CompletionHistoryViewModel::axisTicks(const int column) const {
        if (column == RunIndex) return m_x_axis.ticks();
        if (column < Score || column >= ColumnCount) return {};
        return axisForColumn(m_series, column).ticks();
    }

    QList<QPointF> CompletionHistoryViewModel::seriesPoints(const int column) const {
        if (column < Score || column >= ColumnCount) return {};
        return m_points[column];
    }

    void CompletionHistoryViewModel::refresh() {
        const application::CompletionHistory history = m_use_case->get_history();
        const QString title = QString::fromStdString(history.scenario_name);
        if (title != m_scenario_title) {
            m_scenario_title = title;
            emit scenarioTitleChanged();
        }

        m_run_count = static_cast<int>(history.rows.size());
        for (auto &points: m_points) points.clear();
        for (const auto &row: history.rows) {
            const qreal x = row.run_index;
            m_points[Score].append(QPointF(x, row.score));
            m_points[Accuracy].append(QPointF(x, row.accuracy));
            m_points[Shots].append(QPointF(x, row.shots));
            m_points[Hits].append(QPointF(x, row.hits));
            m_points[Misses].append(QPointF(x, row.misses));
        }
        for (int column = Score; column < ColumnCount; ++column) {
            SeriesModel *series = m_series[column - Score];
            series->points = m_points[column];
            series->yAxis.reset();
        }

        constexpr AxisModel::Options kRunAxis{
            .baseline = AxisModel::Baseline::Zero,
            .integral = true,
            .targetTicks = 10,
        };
        m_x_axis = AxisModel::forRange(m_run_count > 0 ? 1.0 : 0.0, m_run_count, kRunAxis)
                       .withDelegate(runIndexTransform());

        emit dataUpdated();
        emit boundsChanged();
    }
}
