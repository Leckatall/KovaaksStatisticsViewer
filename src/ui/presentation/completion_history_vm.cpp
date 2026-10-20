#include "completion_history_vm.h"

#include "series_style.h"

#include <QColor>
#include <QtMath>

#include <algorithm>
#include <array>
#include <tuple>
#include <utility>
#include <vector>

namespace ksv::presentation {
    namespace {
        struct ColumnMeta {
            const char *name;
            // The series key graphDescriptor(ScenarioHistory) and the committed overrides use.
            std::string_view key;
            int yAxis;
        };

        struct AxisDescriptor {
            ValueTransform transform;
            ValueAxis::Options options;
            const char *name;
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

        constexpr std::array<std::string_view, YAxisCount> kAxisKeys{"score", "accuracy", "counts"};

        const std::array<AxisDescriptor, YAxisCount> kYAxisMeta{{
            {ValueTransform::identity(), {}, "Score"},
            {ValueTransform::percentage(), {}, "Accuracy"},
            {ValueTransform::identity(), {}, "Shots, hits and misses"},
        }};

        const std::array<ColumnMeta, CompletionHistoryViewModel::ColumnCount> kColumnMeta{{
            {"Run", "", -1},
            {"Score", "score", ScoreAxis},
            {"Accuracy", "accuracy", AccuracyAxis},
            {"Shots", "shots", CountAxis},
            {"Hits", "hits", CountAxis},
            {"Misses", "misses", CountAxis},
            {"Score average (10 runs)", "scoreAverage", ScoreAxis},
        }};

    }

    CompletionHistoryViewModel::CompletionHistoryViewModel(
        std::shared_ptr<application::ICompletionHistoryUseCase> use_case, QObject *parent)
        : GraphViewModelBase(parent), m_use_case(std::move(use_case)),
          // The axis spans exactly the first to the last visible run, so it never labels a run that
          // does not exist (no "#0", nothing past the last shown run). A lone run is padded by half a
          // run on each side, which keeps its own tick and no neighbour's.
          m_xAxis({.baseline = ValueAxis::Baseline::HugData, .integral = true, .targetTicks = 10,
                   .fallbackSpan = 0.5, .niceBounds = false},
                  runIndexTransform()) {
        for (int axis = 0; axis < YAxisCount; ++axis)
            m_axisPolicies[axis] = axisPolicyFor({}, application::GraphKey::ScenarioHistory, kAxisKeys[axis]);
        for (int column = Score; column < ColumnCount; ++column) {
            auto *series_model = new SeriesModel(this);
            series_model->setId(QString::number(column));
            series_model->setName(QString::fromLatin1(kColumnMeta[column].name));
            series_model->setColumn(column);
            series_model->transform = kYAxisMeta[kColumnMeta[column].yAxis].transform;
            // Lets the canvas tell which lines share the labelled axis (e.g. Shots/Hits/Misses).
            series_model->yAxisId = static_cast<uint64_t>(kColumnMeta[column].yAxis);
            series_model->yAxisName = QString::fromLatin1(kYAxisMeta[kColumnMeta[column].yAxis].name);
            m_series.append(series_model);
        }
        m_use_case->onPresentationChanged([this] { applyPresentation(); });
        applyPresentation();
        refresh();
    }

    void CompletionHistoryViewModel::applyPresentation() {
        const auto presentation = m_use_case->presentation();
        setAppearance(presentation.appearance);
        for (int column = Score; column < ColumnCount; ++column)
            applySeriesStyle(*m_series[column - Score], presentation, kColumnMeta[column].key);
        setAxisPolicies(presentation.axisPolicies);
        emit styleChanged();
    }

    QList<SeriesModel *> CompletionHistoryViewModel::series(const QList<int> &columns) const {
        QList<SeriesModel *> result;
        result.reserve(columns.size());
        std::array<std::vector<int>, YAxisCount> membersByAxis;
        // Plottable column order (Score, Accuracy, Shots, Hits, Misses, ScoreAverage), whatever order was asked for.
        for (int column = Score; column < ColumnCount; ++column) {
            if (!columns.contains(column)) continue;
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
            const ValueAxis yAxis = axisForPolicy(members, m_axisPolicies[axis], descriptor.options, descriptor.transform);
            for (const int index: indices) result[index]->yAxis = yAxis;
        }
        return result;
    }

    void CompletionHistoryViewModel::refresh() {
        const application::CompletionHistory history = m_use_case->get_history();
        const QString title = QString::fromStdString(history.scenario_name);
        if (title != m_scenario_title) {
            m_scenario_title = title;
            emit scenarioTitleChanged();
        }

        m_run_count = history.full_count;
        m_visible_runs.clear();
        m_visible_runs.reserve(history.rows.size());
        for (auto &points: m_points) points.clear();
        for (const auto &row: history.rows) {
            m_visible_runs.push_back(row.run_index);
            const qreal x = row.run_index;
            m_points[Score].append(QPointF(x, row.score));
            m_points[Accuracy].append(QPointF(x, row.accuracy));
            m_points[Shots].append(QPointF(x, row.shots));
            m_points[Hits].append(QPointF(x, row.hits));
            m_points[Misses].append(QPointF(x, row.misses));
            m_points[ScoreAverage].append(QPointF(x, row.score_average));
        }
        for (int column = Score; column < ColumnCount; ++column) {
            SeriesModel *series = m_series[column - Score];
            series->points = m_points[column];
            series->yAxis.reset();
        }

        if (history.rows.empty()) std::ignore = m_xAxis.setRange(1.0, std::max(1, m_run_count));
        else std::ignore = m_xAxis.setRange(history.rows.front().run_index, history.rows.back().run_index);

        emit dataUpdated();
        emit boundsChanged();
        refreshSelection();
    }

    void CompletionHistoryViewModel::setAxisPolicies(const AxisPolicyMap &policies) {
        std::array<application::AxisPolicy, YAxisCount> next;
        for (int axis = 0; axis < YAxisCount; ++axis)
            next[axis] = axisPolicyFor(policies, application::GraphKey::ScenarioHistory, kAxisKeys[axis]);
        if (next == m_axisPolicies) return;
        m_axisPolicies = std::move(next);
        emit boundsChanged();
    }

    void CompletionHistoryViewModel::selectRun(const int runIndex) {
        if (!std::ranges::binary_search(m_visible_runs, runIndex)) return;
        m_use_case->select_run(runIndex);
    }

    void CompletionHistoryViewModel::refreshSelection() {
        const int index = m_use_case->selected_run_index().value_or(-1);
        const bool visible = index > 0 && std::ranges::binary_search(m_visible_runs, index);
        const bool indexChanged = index != m_selected_run_index;
        const bool visibleChanged = visible != m_selected_run_visible;
        m_selected_run_index = index;
        m_selected_run_visible = visible;
        if (indexChanged) emit selectedRunIndexChanged();
        if (visibleChanged) emit selectedRunVisibleChanged();
    }
}
