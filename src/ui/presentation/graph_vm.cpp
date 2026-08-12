//
// Created by Lecka on 30/07/2026.
//

#include "graph_vm.h"

#include <qurl.h>
#include <QDebug>
#include <algorithm>
#include <utility>


namespace ksv::presentation {
    namespace {
        struct ColumnMeta {
            const char *name;
            const char *key;
            QColor color;
            ValueTransform transform;
        };

        ValueTransform secondsDelegate() {
            ValueTransform t;
            t.formatter = [](const qreal v) { return QString::number(qRound(v)) + "s"; };
            return t;
        }

        // Time column only carries X delegate; others are drawn as series
        const std::array<ColumnMeta, GraphViewModel::ColumnCount> kColumnMeta{{
            {"Time", "time", QColor(), secondsDelegate()},
            {"Score", "score", QColor("#009600"), ValueTransform::identity()},
            {"Accuracy", "accuracy", QColor("cyan"), ValueTransform::percentage()},
            {"Shots", "shots", QColor("orange"), ValueTransform::identity()},
            {"Kills", "kills", QColor("red"), ValueTransform::identity()},
            {"Dmg", "dmg", QColor("yellow"), ValueTransform::identity()},
            {"Score Total", "scoreTotal", QColor("purple"), ValueTransform::identity()},
            {"Expected Final Score", "expectedFinalScore", QColor("magenta"), ValueTransform::identity()},
            {"Expected Final Score (5s)", "expectedFinalScoreRecent", QColor("deepskyblue"), ValueTransform::identity()},
        }};
    }

    GraphViewModel::GraphViewModel(std::shared_ptr<application::IGraphUseCase> graphUseCase,
                                   QObject *parent) : GraphViewModelBase(parent),
                                                      m_graphUseCase(std::move(graphUseCase)) {
        for (int c = Score; c < ColumnCount; ++c) {
            SeriesModel series;
            series.name = GraphViewModel::columnName(c);
            series.color = kColumnMeta[c].color;
            series.transform = kColumnMeta[c].transform;
            m_series.append(std::move(series));
        }
        recomputeBounds();
    }

    void GraphViewModel::setData(QList<QMap<Column, qreal>> data) {
        m_data = std::move(data);
        emit dataUpdated();
        recomputeBounds();
    }

    QVariantList GraphViewModel::plottableColumns() const {
        QVariantList columns;
        for (int c = Score; c < ColumnCount; ++c) columns.append(c);
        return columns;
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
        return QString::fromLatin1(kColumnMeta[column].key);
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
        std::pair<qreal, qreal> rawColumnRange(const QList<QMap<GraphViewModel::Column, qreal>> &data,
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
        newAxes[Time] = newAxes[Time].withDelegate(kColumnMeta[Time].transform);

        // Rebuild series on every call (change-gated skip below only guards deprecated m_axes path)
        for (int c = Score; c < ColumnCount; ++c) {
            m_series[c - Score].setData(seriesPoints(static_cast<Column>(c)));
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

    void GraphViewModel::fetchData() {
        const QString newTitle = QString::fromStdString(m_graphUseCase->get_run_label());
        if (newTitle != m_scenarioTitle) {
            m_scenarioTitle = newTitle;
            emit scenarioTitleChanged();
        }

        const application::GraphSeries seriesData = m_graphUseCase->get_series();

        QList<QMap<Column, qreal>> rows(int(seriesData.times.size()));
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
}
