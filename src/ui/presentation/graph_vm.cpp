//
// Created by Lecka on 30/07/2026.
//

#include "graph_vm.h"

#include <qurl.h>
#include <QDebug>
#include <QSet>
#include <algorithm>
#include <utility>
#include <vector>
#include <optional>
#include <cmath>
#include <type_traits>
#include <initializer_list>
#include <QVariantMap>


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

        const std::array<AxisDescriptor, YAxisCount> kYAxisMeta{{
            {ValueTransform::identity(), {}},
            {ValueTransform::percentage(), {}},
            {ValueTransform::identity(), {}},
            {ValueTransform::identity(), {}},
            {ValueTransform::identity(), {}},
            {ValueTransform::identity(), {}},
        }};

        const std::array<ColumnMeta, GraphViewModel::ColumnCount> kColumnMeta{{
            {"Time", QColor(), -1},
            {"Score", QColor("#009600"), ScoreAxis},
            {"Accuracy", QColor("cyan"), AccuracyAxis},
            {"Shots", QColor("orange"), ShotsAxis},
            {"Kills", QColor("red"), KillsAxis},
            {"Dmg", QColor("yellow"), DmgAxis},
            {"Score Total", QColor("purple"), ScoreFamilyAxis},
            {"Expected Final Score", QColor("magenta"), ScoreFamilyAxis},
            {"Expected Final Score (5s)", QColor("deepskyblue"), ScoreFamilyAxis},
        }};

        static_assert(static_cast<int>(application::ColumnId::Time) == GraphViewModel::Time);
        static_assert(static_cast<int>(application::ColumnId::Score) == GraphViewModel::Score);
        static_assert(static_cast<int>(application::ColumnId::Accuracy) == GraphViewModel::Accuracy);
        static_assert(static_cast<int>(application::ColumnId::Shots) == GraphViewModel::Shots);
        static_assert(static_cast<int>(application::ColumnId::Kills) == GraphViewModel::Kills);
        static_assert(static_cast<int>(application::ColumnId::Dmg) == GraphViewModel::Dmg);
        static_assert(static_cast<int>(application::ColumnId::ScoreTotal) == GraphViewModel::ScoreTotal);
        static_assert(static_cast<int>(application::ColumnId::ExpectedFinalScore) == GraphViewModel::ExpectedFinalScore);
        static_assert(static_cast<int>(application::ColumnId::ExpectedFinalScoreRecent) == GraphViewModel::ExpectedFinalScoreRecent);

        std::optional<application::PrimitiveMetric> metricFromString(const QString &value) {
            if (value == "score") return application::PrimitiveMetric::Score;
            if (value == "shots") return application::PrimitiveMetric::Shots;
            if (value == "hits") return application::PrimitiveMetric::Hits;
            if (value == "kills") return application::PrimitiveMetric::Kills;
            if (value == "dmg") return application::PrimitiveMetric::Dmg;
            return std::nullopt;
        }

        bool exactKeys(const QVariantMap &map, std::initializer_list<const char *> expected) {
            if (map.size() != static_cast<int>(expected.size())) return false;
            for (const auto *key : expected) if (!map.contains(key)) return false;
            return true;
        }

        std::optional<application::Expression> parseExpression(const QVariantMap &map) {
            const auto kind = map.value("kind").toString();
            if (kind == "primitive") {
                if (!exactKeys(map, {"kind", "primitiveMetric"})) return std::nullopt;
                const auto metric = metricFromString(map.value("primitiveMetric").toString());
                if (!metric) return std::nullopt;
                return std::optional<application::Expression>{application::primitive(*metric)};
            }
            if (kind == "constant") {
                if (!exactKeys(map, {"kind", "value"})) return std::nullopt;
                bool ok = false; const auto value = map.value("value").toDouble(&ok);
                if (!ok || !std::isfinite(value)) return std::nullopt;
                return std::optional<application::Expression>{application::numericConstant(value)};
            }
            if (kind == "runningSum" || kind == "projectedFinalValue" || kind == "projectRateToFinal" || kind == "rollingMean") {
                if (!exactKeys(map, kind == "rollingMean" ? std::initializer_list<const char *>{"kind", "input", "window"} : std::initializer_list<const char *>{"kind", "input"})) return std::nullopt;
                const auto input = parseExpression(map.value("input").toMap());
                if (!input) return std::nullopt;
                if (kind == "runningSum") return application::runningSum(*input);
                if (kind == "projectedFinalValue") return application::projectedFinalValue(*input);
                if (kind == "projectRateToFinal") return application::projectRateToFinal(*input);
                bool ok = false; const auto window = map.value("window").toUInt(&ok);
                if (!ok || window == 0) return std::nullopt;
                return std::optional<application::Expression>{application::rollingMean(*input, window)};
            }
            if (kind == "add" || kind == "subtract" || kind == "multiply" || kind == "divide") {
                if (!exactKeys(map, {"kind", "left", "right"})) return std::nullopt;
                const auto left = parseExpression(map.value("left").toMap());
                const auto right = parseExpression(map.value("right").toMap());
                if (!left || !right) return std::nullopt;
                if (kind == "add") return application::add(*left, *right);
                if (kind == "subtract") return application::subtract(*left, *right);
                if (kind == "multiply") return application::multiply(*left, *right);
                return application::divide(*left, *right);
            }
            if (kind == "averageAcrossRuns") {
                if (!exactKeys(map, {"kind", "input", "selection"})) return std::nullopt;
                const auto input = parseExpression(map.value("input").toMap());
                const auto selection = map.value("selection").toMap();
                if (!input || selection.size() != 2 || !selection.contains("kind")) return std::nullopt;
                bool ok = false;
                if (selection.value("kind").toString() == "recentRuns" && selection.contains("count")) {
                    const auto count = selection.value("count").toUInt(&ok);
                    if (ok && count > 0) return application::averageAcrossRuns(*input, application::RecentRuns{count});
                } else if (selection.value("kind").toString() == "topPercentile" && selection.contains("percent")) {
                    const auto percent = selection.value("percent").toDouble(&ok);
                    if (ok && std::isfinite(percent)) return application::averageAcrossRuns(*input, application::TopPercentile{percent});
                }
            }
            return std::nullopt;
        }

        QVariantMap expressionMap(const application::Expression &expression) {
            if (!expression) return {};
            return std::visit([](const auto &node) -> QVariantMap {
                using Node = std::decay_t<decltype(node)>;
                if constexpr (std::is_same_v<Node, application::PrimitiveReference>) {
                    const auto names = std::array{"score", "shots", "hits", "kills", "dmg"};
                    return {{"kind", "primitive"}, {"primitiveMetric", names[static_cast<int>(node.metric)]}};
                } else if constexpr (std::is_same_v<Node, application::NumericConstant>) return {{"kind", "constant"}, {"value", node.value}};
                else if constexpr (std::is_same_v<Node, application::RunningSum>) return {{"kind", "runningSum"}, {"input", expressionMap(node.input)}};
                else if constexpr (std::is_same_v<Node, application::ProjectedFinalValue>) return {{"kind", "projectedFinalValue"}, {"input", expressionMap(node.input)}};
                else if constexpr (std::is_same_v<Node, application::ProjectRateToFinal>) return {{"kind", "projectRateToFinal"}, {"input", expressionMap(node.input)}};
                else if constexpr (std::is_same_v<Node, application::RollingMean>) return {{"kind", "rollingMean"}, {"input", expressionMap(node.input)}, {"window", node.window}};
                else if constexpr (std::is_same_v<Node, application::Add>) return {{"kind", "add"}, {"left", expressionMap(node.left)}, {"right", expressionMap(node.right)}};
                else if constexpr (std::is_same_v<Node, application::Subtract>) return {{"kind", "subtract"}, {"left", expressionMap(node.left)}, {"right", expressionMap(node.right)}};
                else if constexpr (std::is_same_v<Node, application::Multiply>) return {{"kind", "multiply"}, {"left", expressionMap(node.left)}, {"right", expressionMap(node.right)}};
                else if constexpr (std::is_same_v<Node, application::Divide>) return {{"kind", "divide"}, {"left", expressionMap(node.left)}, {"right", expressionMap(node.right)}};
                else if constexpr (std::is_same_v<Node, application::AverageAcrossRuns>) {
                    QVariantMap selection;
                    std::visit([&](const auto &value) {
                        using Selection = std::decay_t<decltype(value)>;
                        if constexpr (std::is_same_v<Selection, application::RecentRuns>) selection = {{"kind", "recentRuns"}, {"count", value.count}};
                        else selection = {{"kind", "topPercentile"}, {"percent", value.percent}};
                    }, node.selection);
                    return {{"kind", "averageAcrossRuns"}, {"input", expressionMap(node.input)}, {"selection", selection}};
                }
                else return QVariantMap{};
            }, expression->value());
        }

        QVariantMap mutationMap(const application::MutationResult &result) {
            QVariantMap map;
            map["succeeded"] = result.succeeded();
            map["requiresReload"] = result.requiresReload;
            map["failure"] = result.failure ? QVariant(static_cast<int>(*result.failure)) : QVariant();
            map["createdId"] = result.createdId ? QVariant(QString::number(result.createdId->value)) : QVariant();
            QVariantList errors;
            for (const auto &error : result.errors) errors.append(QVariantMap{{"code", static_cast<int>(error.code)}, {"path", QString::fromStdString(error.path)}});
            map["validationErrors"] = errors;
            return map;
        }

        QVariantMap invalidMutationMap() {
            return {{"succeeded", false}, {"failure", "invalidRequest"}, {"requiresReload", false},
                    {"createdId", QVariant()}, {"validationErrors", QVariantList{}}};
        }
    }

    GraphViewModel::GraphViewModel(std::shared_ptr<application::IGraphUseCase> graphUseCase,
                                   QObject *parent) : GraphViewModelBase(parent),
                                                      m_graphUseCase(std::move(graphUseCase)) {
        m_enabledColumns = allColumns();
        for (int c = Score; c < ColumnCount; ++c) {
            SeriesModel series;
            series.name = GraphViewModel::columnName(c);
            series.color = kColumnMeta[c].color;
            series.transform = kYAxisMeta[kColumnMeta[c].yAxis].transform;
            m_series.append(std::move(series));
        }
        recomputeBounds();
        m_graphUseCase->onSeriesConfigChanged([this] { fetchData(); });
    }

    void GraphViewModel::setData(QList<QMap<Column, qreal>> data) {
        m_data = std::move(data);
        emit dataUpdated();
        recomputeBounds();
    }

    QList<SeriesModel> GraphViewModel::series(const QList<int> &columns) const {
        QList<SeriesModel> result;
        result.reserve(columns.size());
        std::array<std::vector<int>, YAxisCount> membersByAxis;
        for (const int column: columns) {
            if (column < Score || column >= ColumnCount) continue;
            result.append(m_series[column - Score]);
            result.back().column = column;
            membersByAxis[kColumnMeta[column].yAxis].push_back(result.size() - 1);
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
        newAxes[Time] = newAxes[Time].withDelegate(secondsDelegate());

        for (int c = Score; c < ColumnCount; ++c) {
            auto &series = m_series[c - Score];
            series.points = seriesPoints(c);
            series.yAxis.reset();
        }

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

    void GraphViewModel::fetchData() {
        const QString newTitle = QString::fromStdString(m_graphUseCase->get_run_label());
        if (newTitle != m_scenarioTitle) {
            m_scenarioTitle = newTitle;
            emit scenarioTitleChanged();
        }

        const auto resolved = m_graphUseCase->get_resolved_graph();
        if (!resolved.series.empty() || !resolved.times.empty()) {
            m_allSeries.clear(); m_enabledSeriesIds.clear(); m_legacyColumnIds.clear();
            QVariantList transitionalEnabledColumns;
            QList<QMap<Column, qreal>> rows(int(resolved.times.size()));
            for (int i = 0; i < rows.size(); ++i) rows[i][Time] = resolved.times[static_cast<size_t>(i)];
            for (const auto &entry : resolved.series) {
                const auto &presentation = application::seriesPresentation(entry.config);
                const bool computed = std::holds_alternative<application::ComputedSeriesConfig>(entry.config);
                QString id;
                if (computed) {
                    id = "computed:" + QString::number(std::get<application::ComputedSeriesConfig>(entry.config).id.value);
                } else {
                    const auto metric = std::get<application::BaseSeriesConfig>(entry.config).metric;
                    static constexpr std::array tags{"score", "shots", "hits", "kills", "dmg"};
                    id = "base:" + QString::fromLatin1(tags[static_cast<int>(metric)]);
                }
                QVariantMap series{{"id", id}, {"kind", computed ? "computed" : "base"},
                                   {"name", QString::fromStdString(presentation.name)}, {"enabled", presentation.enabled},
                                   {"displayPosition", static_cast<qulonglong>(presentation.displayPosition)},
                                   {"color", QColor(presentation.lineStyle.color.red, presentation.lineStyle.color.green,
                                                      presentation.lineStyle.color.blue, presentation.lineStyle.color.alpha)},
                                   {"width", presentation.lineStyle.width}};
                if (const auto *base = std::get_if<application::BaseSeriesConfig>(&entry.config)) series["metric"] = static_cast<int>(base->metric);
                else series["expression"] = expressionMap(std::get<application::ComputedSeriesConfig>(entry.config).expression);
                m_allSeries.append(series);
                if (presentation.enabled) m_enabledSeriesIds.append(id);
                int column = -1;
                if (const auto *base = std::get_if<application::BaseSeriesConfig>(&entry.config)) {
                    switch (base->metric) {
                        case application::PrimitiveMetric::Score: column = Score; break;
                        case application::PrimitiveMetric::Shots: column = Shots; break;
                        case application::PrimitiveMetric::Kills: column = Kills; break;
                        case application::PrimitiveMetric::Dmg: column = Dmg; break;
                        case application::PrimitiveMetric::Hits: break;
                    }
                } else {
                    const auto idValue = std::get<application::ComputedSeriesConfig>(entry.config).id.value;
                    if (idValue == 1) column = Accuracy;
                    else if (idValue == 2) column = ScoreTotal;
                    else if (idValue == 3) column = ExpectedFinalScore;
                    else if (idValue == 4) column = ExpectedFinalScoreRecent;
                }
                if (column >= Score && column < ColumnCount) m_legacyColumnIds[column] = id;
                if (presentation.enabled && column >= Score && column < ColumnCount) transitionalEnabledColumns.append(column);
                if (column >= Score && column < ColumnCount && entry.values) {
                    for (int i = 0; i < rows.size() && i < static_cast<int>(entry.values->size()); ++i)
                        rows[i][static_cast<Column>(column)] = static_cast<qreal>((*entry.values)[static_cast<size_t>(i)]);
                }
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

    QString GraphViewModel::seriesIdForColumn(const int column) const {
        for (const auto &value : m_allSeries) {
            const auto map = value.toMap();
            if (m_legacyColumnIds.value(column) == map.value("id").toString()) return map.value("id").toString();
        }
        return column >= 0 && column < ColumnCount ? "base:" + columnKey(column) : QString{};
    }

    QVariantMap GraphViewModel::setSeriesEnabled(const QString &id, const bool enabled) {
        if (id.startsWith("computed:")) {
            const auto result = m_graphUseCase->setSeriesEnabled(application::ComputedSeriesId{id.mid(9).toULongLong()}, enabled);
            if (result.succeeded()) {
                fetchData();
                for (auto it = m_legacyColumnIds.cbegin(); it != m_legacyColumnIds.cend(); ++it)
                    if (it.value() == id) {
                        auto columns = m_enabledColumns;
                        columns.removeAll(it.key());
                        if (enabled) columns.append(it.key());
                        if (columns != m_enabledColumns) { m_enabledColumns = columns; emit enabledColumnsChanged(); }
                        break;
                    }
            }
            return mutationMap(result);
        }
        if (id.startsWith("base:")) {
            const auto metric = metricFromString(id.mid(5));
            if (metric) {
                const auto result = m_graphUseCase->setSeriesEnabled(*metric, enabled);
                if (result.succeeded()) {
                    fetchData();
                    for (auto it = m_legacyColumnIds.cbegin(); it != m_legacyColumnIds.cend(); ++it)
                        if (it.value() == id) {
                            auto columns = m_enabledColumns;
                            columns.removeAll(it.key());
                            if (enabled) columns.append(it.key());
                            if (columns != m_enabledColumns) { m_enabledColumns = columns; emit enabledColumnsChanged(); }
                            break;
                        }
                }
                return mutationMap(result);
            }
        }
        return invalidMutationMap();
    }

    QVariantMap GraphViewModel::updateBasePresentation(const QString &id, const QColor &color, const double width) {
        const std::optional<application::PrimitiveMetric> metric = id.startsWith("base:")
            ? metricFromString(id.mid(5)) : std::optional<application::PrimitiveMetric>{};
        if (!metric) return invalidMutationMap();
        return mutationMap(m_graphUseCase->updateBasePresentation({*metric, true,
            {{static_cast<uint8_t>(color.red()), static_cast<uint8_t>(color.green()), static_cast<uint8_t>(color.blue()), static_cast<uint8_t>(color.alpha())}, width}}));
    }

    QVariantMap GraphViewModel::createComputedSeries(const QString &name, const QColor &color, const double width,
                                                      const bool enabled, const QVariantMap &expression) {
        const auto parsed = parseExpression(expression);
        if (!parsed) return invalidMutationMap();
        return mutationMap(m_graphUseCase->createComputed({{name.toStdString(),
            {{static_cast<uint8_t>(color.red()), static_cast<uint8_t>(color.green()), static_cast<uint8_t>(color.blue()), static_cast<uint8_t>(color.alpha())}, width}, enabled}, *parsed}));
    }

    QVariantMap GraphViewModel::updateComputedSeries(const QString &id, const QString &name, const QColor &color,
                                                      const double width, const bool enabled, const QVariantMap &expression) {
        const auto parsed = parseExpression(expression);
        bool ok = false; const auto numericId = id.startsWith("computed:") ? id.mid(9).toULongLong(&ok) : 0;
        if (!parsed || !ok || numericId == 0) return invalidMutationMap();
        application::UpdateComputedSeriesRequest request;
        request.id.value = numericId;
        request.presentation.name = name.toStdString();
        request.presentation.lineStyle = {{static_cast<uint8_t>(color.red()), static_cast<uint8_t>(color.green()),
                                           static_cast<uint8_t>(color.blue()), static_cast<uint8_t>(color.alpha())}, width};
        request.presentation.enabled = enabled;
        request.expression = *parsed;
        return mutationMap(m_graphUseCase->updateComputed(request));
    }

    QVariantMap GraphViewModel::removeComputedSeries(const QString &id) {
        bool ok = false; const auto numericId = id.startsWith("computed:") ? id.mid(9).toULongLong(&ok) : 0;
        return ok && numericId ? mutationMap(m_graphUseCase->removeComputed({numericId})) : invalidMutationMap();
    }

    QVariantMap GraphViewModel::moveSeries(const QString &id, const int displayPosition) {
        if (displayPosition < 0) return invalidMutationMap();
        if (id.startsWith("computed:")) return mutationMap(m_graphUseCase->moveSeries(application::ComputedSeriesId{id.mid(9).toULongLong()}, static_cast<uint32_t>(displayPosition)));
        const std::optional<application::PrimitiveMetric> metric = id.startsWith("base:")
            ? metricFromString(id.mid(5)) : std::optional<application::PrimitiveMetric>{};
        return metric ? mutationMap(m_graphUseCase->moveSeries(*metric, static_cast<uint32_t>(displayPosition))) : invalidMutationMap();
    }
}
