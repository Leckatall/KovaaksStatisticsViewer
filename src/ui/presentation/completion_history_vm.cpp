#include "completion_history_vm.h"

#include <QColor>
#include <QtMath>

#include <array>
#include <utility>

namespace ksv::presentation {
    namespace {
        struct ColumnMeta {
            const char *name;
            const char *key;
            QColor color;
            ValueTransform transform;
            AxisModel::Options y_axis_options;
        };

        ValueTransform runIndexTransform() {
            ValueTransform transform;
            transform.formatter = [](const qreal value) {
                return "#" + QString::number(qRound(value));
            };
            return transform;
        }

        const std::array<ColumnMeta, CompletionHistoryViewModel::ColumnCount> kColumnMeta{{
            {"Run", "run", QColor(), runIndexTransform(), {}},
            {"Score", "score", QColor("#009600"), ValueTransform::identity(),
             {.baseline = AxisModel::Baseline::Zero}},
            {"Accuracy", "accuracy", QColor("cyan"), ValueTransform::percentage(), {}},
            {"Shots", "shots", QColor("orange"), ValueTransform::identity(),
             {.baseline = AxisModel::Baseline::Zero}},
            {"Hits", "hits", QColor("#7CB342"), ValueTransform::identity(),
             {.baseline = AxisModel::Baseline::Zero}},
            {"Misses", "misses", QColor("#E53935"), ValueTransform::identity(),
             {.baseline = AxisModel::Baseline::Zero}},
        }};
    }

    CompletionHistoryViewModel::CompletionHistoryViewModel(
        std::shared_ptr<application::ICompletionHistoryUseCase> use_case, QObject *parent)
        : GraphViewModelBase(parent), m_use_case(std::move(use_case)) {
        for (int column = Score; column < ColumnCount; ++column) {
            SeriesModel series_model;
            series_model.name = columnName(column);
            series_model.color = kColumnMeta[column].color;
            series_model.transform = kColumnMeta[column].transform;
            series_model.yAxisOptions = kColumnMeta[column].y_axis_options;
            m_series.append(std::move(series_model));
        }
        refresh();
    }

    QList<SeriesModel> CompletionHistoryViewModel::series(const QList<int> &columns) const {
        QList<SeriesModel> result;
        result.reserve(columns.size());
        for (const int column: columns) {
            if (column >= Score && column < ColumnCount) result.append(m_series[column - Score]);
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
            const AxisModel y_axis = m_series[column - Score].deriveYAxis();
            bounds[QString::number(column)] = QPointF(y_axis.min(), y_axis.max());
        }
        return bounds;
    }

    QList<qreal> CompletionHistoryViewModel::axisTicks(const int column) const {
        if (column == RunIndex) return m_x_axis.ticks();
        if (column < Score || column >= ColumnCount) return {};
        return m_series[column - Score].yAxis->ticks();
    }

    QList<QPointF> CompletionHistoryViewModel::seriesPoints(const int column) const {
        if (column < Score || column >= ColumnCount) return {};
        return m_points[column];
    }

    QString CompletionHistoryViewModel::columnName(const int column) const {
        if (column < 0 || column >= ColumnCount) return {};
        return QString::fromLatin1(kColumnMeta[column].name);
    }

    QColor CompletionHistoryViewModel::columnColor(const int column) const {
        if (column < 0 || column >= ColumnCount) return {};
        return kColumnMeta[column].color;
    }

    QString CompletionHistoryViewModel::columnKey(const int column) const {
        if (column < 0 || column >= ColumnCount) return {};
        return QString::fromLatin1(kColumnMeta[column].key);
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
            m_series[column - Score].setData(m_points[column]);
        }

        constexpr AxisModel::Options kRunAxis{
            .baseline = AxisModel::Baseline::Zero,
            .integral = true,
            .targetTicks = 10,
        };
        m_x_axis = AxisModel::forRange(m_run_count > 0 ? 1.0 : 0.0, m_run_count, kRunAxis)
                       .withDelegate(kColumnMeta[RunIndex].transform);

        emit dataUpdated();
        emit boundsChanged();
    }
}
