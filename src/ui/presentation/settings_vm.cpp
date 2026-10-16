//
// Created by Lecka on 03/08/2026.
//

#include "settings_vm.h"
#include "series_expression_editor_model.h"

#include <chrono>
#include <concepts>
#include <limits>
#include <ranges>
#include <variant>

#include <QDate>
#include <QRegularExpression>

#include "app/contracts/expression_dsl.h"

namespace ksv::presentation {
    namespace {
        std::optional<application::Expression> decodeTransportExpression(const QString &text) {
            if (text.isEmpty()) return application::Expression{};
            const auto decoded = application::decodeExpressionDsl(text.toStdString());
            if (!decoded || !*decoded) return std::nullopt;
            return *decoded;
        }

        QVariantMap mutationMap(const application::MutationResult &result) {
            QVariantMap map;
            map["succeeded"] = result.succeeded();
            map["requiresReload"] = result.requiresReload;
            map["failure"] = result.failure ? QVariant(static_cast<int>(*result.failure)) : QVariant();
            map["createdId"] = result.createdId ? QVariant(QString::number(result.createdId->value)) : QVariant();
            map["createdAxisId"] = result.createdAxisId
                                     ? QVariant(QString::number(result.createdAxisId->value))
                                     : QVariant();
            QVariantList errors;
            for (const auto &error: result.errors)
                errors.append(QVariantMap{{"code", static_cast<int>(error.code)}, {"path", QString::fromStdString(error.path)}});
            map["validationErrors"] = errors;
            return map;
        }

        QVariantMap invalidMutationMap() {
            return {{"succeeded", false}, {"failure", "invalidRequest"}, {"requiresReload", false},
                    {"createdId", QVariant()}, {"createdAxisId", QVariant()}, {"validationErrors", QVariantList{}}};
        }

        std::optional<uint64_t> parseId(const QString &text) {
            bool ok = false;
            const auto value = text.toULongLong(&ok);
            return ok ? std::optional{value} : std::nullopt;
        }

        application::RgbaColor rgba(const QColor &color) {
            return {static_cast<uint8_t>(color.red()), static_cast<uint8_t>(color.green()),
                    static_cast<uint8_t>(color.blue()), static_cast<uint8_t>(color.alpha())};
        }

        QString issueCodeName(const application::GraphConfigurationIssueCode code) {
            using enum application::GraphConfigurationIssueCode;
            switch (code) {
                case NotApplicable: return "notApplicable";
                case MissingValue: return "missingValue";
                case InvalidDate: return "invalidDate";
                case ReversedDateRange: return "reversedDateRange";
                case NonPositiveCount: return "nonPositiveCount";
                case CountTooLarge: return "countTooLarge";
                case NonFiniteNumber: return "nonFiniteNumber";
                case LineWidthOutOfRange: return "lineWidthOutOfRange";
                case AxisBoundOutOfRange: return "axisBoundOutOfRange";
                case FixedBoundsNotIncreasing: return "fixedBoundsNotIncreasing";
                case UnknownSeries: return "unknownSeries";
                case UnknownAxis: return "unknownAxis";
                case InvalidRunSeries: return "invalidRunSeries";
            }
            return "invalid";
        }

        // Raw input is never part of a candidate, so its issue has no GraphConfigurationIssueCode.
        constexpr auto kUnparsedInputCode = "unparsedInput";

        QString graphName(const application::GraphKey key) {
            return QString::fromUtf8(application::graphKeyName(key).data(),
                                     static_cast<qsizetype>(application::graphKeyName(key).size()));
        }

        QVariantMap issueMap(const application::GraphKey graph, const QString &group, const QString &target,
                             const QString &field, const QString &code) {
            return {{"graph", graphName(graph)}, {"group", group}, {"target", target}, {"field", field},
                    {"code", code}};
        }

        QVariantMap quickPatchResult(const bool accepted, QVariantList issues = {}) {
            return {{"accepted", accepted}, {"issues", std::move(issues)}};
        }

        QString connectionName(const application::ConnectionMode mode) {
            switch (mode) {
                case application::ConnectionMode::Smooth: return "smooth";
                case application::ConnectionMode::Straight: return "straight";
                case application::ConnectionMode::PointsOnly: return "pointsOnly";
            }
            return "smooth";
        }

        std::optional<application::ConnectionMode> connectionFromName(const QString &name) {
            if (name == "smooth") return application::ConnectionMode::Smooth;
            if (name == "straight") return application::ConnectionMode::Straight;
            if (name == "pointsOnly") return application::ConnectionMode::PointsOnly;
            return std::nullopt;
        }

        QString isoDate(const std::chrono::year_month_day &date) {
            return QDate(static_cast<int>(date.year()), static_cast<int>(static_cast<unsigned>(date.month())),
                         static_cast<int>(static_cast<unsigned>(date.day()))).toString(Qt::ISODate);
        }

        // Either the whole number, or the issue code that explains why there is none.
        struct ParsedCount {
            std::optional<int> value;
            QString failure;
        };

        ParsedCount parseCount(const QVariant &input) {
            if (input.metaType().id() != QMetaType::QString) {
                bool ok = false;
                const int value = input.toInt(&ok);
                if (ok) return {value, {}};
                return {std::nullopt, kUnparsedInputCode};
            }
            const auto text = input.toString().trimmed();
            static const QRegularExpression wholeNumber(R"(^[+-]?\d+$)");
            if (!wholeNumber.match(text).hasMatch()) return {std::nullopt, kUnparsedInputCode};
            bool ok = false;
            const qlonglong value = text.toLongLong(&ok);
            const bool negative = text.startsWith(QLatin1Char('-'));
            if (!ok || value > std::numeric_limits<int>::max() || value < std::numeric_limits<int>::min())
                return {std::nullopt, issueCodeName(negative ? application::GraphConfigurationIssueCode::NonPositiveCount
                                                             : application::GraphConfigurationIssueCode::CountTooLarge)};
            return {static_cast<int>(value), {}};
        }
    }

    SettingsViewModel::SettingsViewModel(
        std::shared_ptr<application::ISettingsService> settings_service,
        std::shared_ptr<application::IProfileService> profile_service,
        std::shared_ptr<application::IGraphConfigurationUseCase> configuration,
        QObject *parent) : QObject(parent),
                           m_settings_service(std::move(settings_service)),
                           m_profile_service(std::move(profile_service)),
                           m_configuration(std::move(configuration)),
                           m_kovaaks_dir([&] {
                               const auto dirs = m_settings_service->getKovaaksDirs();
                               return dirs.empty() ? QUrl{} : QUrl::fromLocalFile(QString::fromStdString(dirs.front()));
                           }()) {
        const QPointer<SettingsViewModel> self(this);
        m_profile_service->onProfileChanged([self] { if (self) emit self->profileLoadedChanged(); });
        m_configuration->onChanged([self](const application::GraphConfigurationChange &change) {
            if (!self) return;
            if (!change.graphs.empty()) emit self->quickSettingsChanged();
            if (self->isVisitOpen()) return;
            if (change.runSeries || change.runAxisLabel) emit self->seriesConfigurationChanged();
        });
    }

    void SettingsViewModel::setKovaaksDir(const QUrl &dir) {
        if (dir == m_kovaaks_dir) return;
        auto dirs = m_settings_service->getKovaaksDirs();
        const auto path = dir.toLocalFile().toStdString();
        if (dirs.empty()) dirs.push_back(path);
        else dirs.front() = path;
        m_settings_service->setKovaaksDirs(dirs);
        m_kovaaks_dir = dir;
        emit kovaaksDirChanged();
    }

    void SettingsViewModel::setProfilePath(const QUrl &path) {
        if (path == getProfilePath()) return;
        // Setting path triggers ProfileService to repoint the store and reload.
        m_settings_service->setProfilePath(path.toLocalFile().toStdString());
        emit profilePathChanged();
    }

    // ---- Visit lifecycle ----------------------------------------------------------------------------

    void SettingsViewModel::beginVisit() {
        if (m_working) return;
        auto seed = m_configuration->editorSeed();
        m_baseline = seed.configuration;
        m_working = std::move(seed.configuration);
        emit visitOpenChanged();
        emit seriesConfigurationChanged();
        refreshPendingChanges();
    }

    QVariantMap SettingsViewModel::save() {
        if (!m_working) return {{"accepted", true}, {"issues", QVariantList{}}};
        std::vector<Issue> issues;
        for (const auto &key: m_rawInput | std::views::keys) issues.push_back({key, kUnparsedInputCode});
        if (issues.empty()) {
            for (const auto &issue: m_configuration->save(*m_working).issues)
                issues.push_back({{issue.graph, issue.group, issue.target, issue.field}, issueCodeName(issue.code)});
        }
        const bool accepted = issues.empty();
        if (accepted) m_baseline = *m_working;
        setIssues(std::move(issues));
        refreshPendingChanges();
        return {{"accepted", accepted}, {"issues", this->issues()}};
    }

    void SettingsViewModel::revertChanges() {
        if (!m_working) return;
        m_working = m_baseline;
        const bool hadRawInput = !m_rawInput.empty();
        m_rawInput.clear();
        if (hadRawInput) emit rawInputsChanged();
        setIssues({});
        emit seriesConfigurationChanged();
        refreshPendingChanges();
    }

    void SettingsViewModel::endVisit() {
        if (!m_working) return;
        m_working.reset();
        const bool hadRawInput = !m_rawInput.empty();
        m_rawInput.clear();
        if (hadRawInput) emit rawInputsChanged();
        setIssues({});
        emit visitOpenChanged();
        emit seriesConfigurationChanged();
        refreshPendingChanges();
    }

    bool SettingsViewModel::hasPendingChanges() const {
        return m_working && (!m_rawInput.empty() || *m_working != m_baseline);
    }

    void SettingsViewModel::refreshPendingChanges() {
        const bool pending = hasPendingChanges();
        if (pending == m_pendingChanges) return;
        m_pendingChanges = pending;
        emit pendingChangesChanged();
    }

    QString SettingsViewModel::selectedGraph() const { return graphName(m_selectedGraph); }

    void SettingsViewModel::setSelectedGraph(const QString &graph) {
        const auto key = application::graphKeyFromName(graph.toStdString());
        if (!key || *key == m_selectedGraph) return;
        m_selectedGraph = *key;
        emit selectedGraphChanged();
    }

    QVariantList SettingsViewModel::issues() const {
        QVariantList result;
        for (const auto &[key, code]: m_issues) {
            result.append(issueMap(key.graph, QString::fromStdString(key.group), QString::fromStdString(key.target),
                                   QString::fromStdString(key.field), code));
        }
        return result;
    }

    void SettingsViewModel::setIssues(std::vector<Issue> issues) {
        if (issues.empty() && m_issues.empty()) return;
        m_issues = std::move(issues);
        emit issuesChanged();
    }

    // ---- Unparsed input -----------------------------------------------------------------------------

    std::optional<SettingsViewModel::FieldKey> SettingsViewModel::fieldKey(
        const QString &graph, const QString &group, const QString &target, const QString &field) {
        const auto key = application::graphKeyFromName(graph.toStdString());
        if (!key) return std::nullopt;
        return FieldKey{*key, group.toStdString(), target.toStdString(), field.toStdString()};
    }

    void SettingsViewModel::setRawInput(const QString &graph, const QString &group, const QString &target,
                                        const QString &field, const QString &text) {
        const auto key = fieldKey(graph, group, target, field);
        if (!m_working || !key) return;
        const auto [it, inserted] = m_rawInput.try_emplace(*key, text);
        if (!inserted) {
            if (it->second == text) return;
            it->second = text;
        }
        emit rawInputsChanged();
        refreshPendingChanges();
    }

    void SettingsViewModel::clearRawInput(const QString &graph, const QString &group, const QString &target,
                                          const QString &field) {
        const auto key = fieldKey(graph, group, target, field);
        if (!key || m_rawInput.erase(*key) == 0) return;
        emit rawInputsChanged();
        auto issues = m_issues;
        std::erase_if(issues, [&](const Issue &issue) { return issue.key == *key && issue.code == kUnparsedInputCode; });
        if (issues.size() != m_issues.size()) setIssues(std::move(issues));
        refreshPendingChanges();
    }

    QVariant SettingsViewModel::rawInput(const QString &graph, const QString &group, const QString &target,
                                         const QString &field) const {
        const auto key = fieldKey(graph, group, target, field);
        if (!key) return {};
        const auto it = m_rawInput.find(*key);
        return it == m_rawInput.end() ? QVariant() : QVariant(it->second);
    }

    QVariantList SettingsViewModel::rawInputs() const {
        QVariantList result;
        for (const auto &[key, text]: m_rawInput) {
            result.append(QVariantMap{
                {"graph", graphName(key.graph)},
                {"group", QString::fromStdString(key.group)},
                {"target", QString::fromStdString(key.target)},
                {"field", QString::fromStdString(key.field)},
                {"text", text}
            });
        }
        return result;
    }

    // ---- Quick settings ----------------------------------------------------------------------------

    QVariantMap SettingsViewModel::quickSettings() const {
        const auto committed = m_configuration->current().configuration;
        QVariantMap result;
        for (const auto key: application::kGraphKeys) {
            const auto &descriptor = application::graphDescriptor(key);
            const auto &preferences = committed->preferences(key);
            QVariantMap entry{
                {"hasRange", descriptor.dateRange},
                {"hasLatestRuns", descriptor.latestRuns},
                {"hasWindow", descriptor.averagingWindow},
                {"rangeKind", "allHistory"},
                {"rangeCount", 0},
                {"rangeStart", QString()},
                {"rangeEnd", QString()},
                {"window", preferences.averagingWindowDays.value_or(0)},
                {"connection", connectionName(preferences.appearance.connection)},
                {"markers", preferences.appearance.lineModeMarkers}
            };
            if (preferences.range) {
                std::visit([&entry](const auto &range) {
                    using Range = std::decay_t<decltype(range)>;
                    if constexpr (std::same_as<Range, application::RecentDays>) {
                        entry["rangeKind"] = "recentDays";
                        entry["rangeCount"] = range.days;
                    } else if constexpr (std::same_as<Range, application::LatestRuns>) {
                        entry["rangeKind"] = "latestRuns";
                        entry["rangeCount"] = range.count;
                    } else if constexpr (std::same_as<Range, application::CustomDates>) {
                        entry["rangeKind"] = "customDates";
                        entry["rangeStart"] = isoDate(range.start);
                        entry["rangeEnd"] = isoDate(range.end);
                    }
                }, preferences.range->range);
            }
            result.insert(graphName(key), entry);
        }
        return result;
    }

    QVariantMap SettingsViewModel::applyQuickPatch(const QString &graph, const QVariantMap &patch) {
        const auto key = application::graphKeyFromName(graph.toStdString());
        if (!key) return quickPatchResult(false);

        application::GraphQuickPatch request;
        QVariantList issues;
        const auto parse = [&](const QVariant &input, const QString &field) -> std::optional<int> {
            const auto parsed = parseCount(input);
            if (!parsed.value) issues.append(issueMap(*key, "dataRange", {}, field, parsed.failure));
            return parsed.value;
        };

        if (patch.contains("rangeKind")) {
            const auto kind = patch.value("rangeKind").toString();
            const auto committed = m_configuration->current().configuration;
            const auto &stored = committed->preferences(*key).range;
            const bool anchor = stored && stored->anchorToLatestRun;
            if (kind == "allHistory") {
                request.range = application::RangePreference{application::AllHistory{}, anchor};
            } else if (kind == "recentDays") {
                if (const auto days = parse(patch.value("count"), "recentDays"))
                    request.range = application::RangePreference{application::RecentDays{*days}, anchor};
            } else if (kind == "latestRuns") {
                if (const auto count = parse(patch.value("count"), "latestRuns"))
                    request.range = application::RangePreference{application::LatestRuns{*count}, anchor};
            } else {
                return quickPatchResult(false);
            }
        }
        if (patch.contains("window")) request.averagingWindowDays = parse(patch.value("window"), "averagingWindow");
        if (patch.contains("connection")) {
            request.connection = connectionFromName(patch.value("connection").toString());
            if (!request.connection) return quickPatchResult(false);
        }
        if (patch.contains("markers")) request.lineModeMarkers = patch.value("markers").toBool();

        if (!issues.isEmpty()) return quickPatchResult(false, std::move(issues));

        const auto result = m_configuration->applyQuickPatch(*key, request);
        for (const auto &issue: result.issues)
            issues.append(issueMap(issue.graph, QString::fromStdString(issue.group),
                                   QString::fromStdString(issue.target), QString::fromStdString(issue.field),
                                   issueCodeName(issue.code)));
        return quickPatchResult(result.accepted(), std::move(issues));
    }

    // ---- Graph Lines --------------------------------------------------------------------------------

    std::shared_ptr<const application::GraphConfiguration> SettingsViewModel::shownConfiguration() const {
        // During a visit this aliases the working copy without owning it.
        if (m_working) return {std::shared_ptr<void>(), &*m_working};
        return m_configuration->current().configuration;
    }

    QVariantList SettingsViewModel::getAllSeriesConfigs() const {
        QVariantList result;
        const auto shown = shownConfiguration();
        for (const auto &config: shown->runSeries.series) {
            result.append(QVariantMap{
                {"id", QString::number(config.id.value)},
                {"name", QString::fromStdString(config.presentation.name)},
                {"color", QColor(config.presentation.lineStyle.color.red, config.presentation.lineStyle.color.green,
                                  config.presentation.lineStyle.color.blue, config.presentation.lineStyle.color.alpha)},
                {"width", config.presentation.lineStyle.width},
                {"enabled", config.presentation.enabled},
                {"displayPosition", config.presentation.displayPosition},
                {"isPrimitive", config.isPrimitive()},
                {"expression", QString::fromStdString(application::encodeExpressionDsl(config.expression))},
                {"yAxisId", config.yAxisId ? QString::number(config.yAxisId->value) : QString()},
            });
        }
        return result;
    }

    QVariantList SettingsViewModel::getAllAxes() const {
        QVariantList result;
        const auto shown = shownConfiguration();
        for (const auto &axis: shown->runSeries.axes) {
            result.append(QVariantMap{
                {"id", QString::number(axis.id.value)},
                {"name", QString::fromStdString(axis.name)}
            });
        }
        return result;
    }

    QString SettingsViewModel::runAxisLabelSeriesId() const {
        const auto label = shownConfiguration()->runAxisLabelSeriesId;
        return label ? QString::number(label->value) : QString();
    }

    QVariantMap SettingsViewModel::editRunSeries(
        const std::function<application::MutationResult(application::RunSeriesDocument &)> &edit) {
        if (!m_working) return invalidMutationMap();
        const auto result = edit(m_working->runSeries);
        if (result.succeeded()) {
            emit seriesConfigurationChanged();
            refreshPendingChanges();
        }
        return mutationMap(result);
    }

    QVariantMap SettingsViewModel::setSeriesEnabled(const QString &id, const bool enabled) {
        const auto value = parseId(id);
        if (!value) return invalidMutationMap();
        return editRunSeries([&](application::RunSeriesDocument &document) {
            return application::updateSeries(document, {
                .id = {*value}, .presentation = application::UpdatedSeriesPresentation{.enabled = enabled}
            });
        });
    }

    QVariantMap SettingsViewModel::createComputedSeries(const QString &name, const QColor &color, const double width,
                                                         const bool enabled, const QString &expression) {
        const auto parsed = decodeTransportExpression(expression);
        if (!parsed) return invalidMutationMap();
        return editRunSeries([&](application::RunSeriesDocument &document) {
            return application::createComputedSeries(document, {
                {name.toStdString(), {rgba(color), width}, enabled}, *parsed
            });
        });
    }

    QVariantMap SettingsViewModel::updateComputedSeries(const QString &id, const QString &name, const QColor &color,
                                                         const double width, const bool enabled,
                                                         const QString &expression) {
        const auto value = parseId(id);
        if (!value) return invalidMutationMap();
        const auto parsed = decodeTransportExpression(expression);
        if (!parsed) return invalidMutationMap();
        application::UpdateSeriesRequest request;
        request.id = {*value};
        request.presentation = application::UpdatedSeriesPresentation{
            name.toStdString(), application::LineStyle{rgba(color), width}, enabled
        };
        request.expression = *parsed;
        return editRunSeries([&](application::RunSeriesDocument &document) {
            return application::updateSeries(document, request);
        });
    }

    QVariantMap SettingsViewModel::removeComputedSeries(const QString &id) {
        const auto value = parseId(id);
        if (!value) return invalidMutationMap();
        return editRunSeries([&](application::RunSeriesDocument &document) {
            return application::removeSeries(document, {*value});
        });
    }

    QVariantMap SettingsViewModel::reorderSeries(const QString &id, const int displayPosition) {
        const auto value = parseId(id);
        if (!value || displayPosition < 0) return invalidMutationMap();
        return editRunSeries([&](application::RunSeriesDocument &document) {
            return application::reorderSeries(document, {*value}, static_cast<uint32_t>(displayPosition));
        });
    }

    SeriesExpressionEditorModel *SettingsViewModel::beginExpressionEdit(const QString &id) {
        auto *editor = new SeriesExpressionEditorModel();
        const auto value = parseId(id);
        if (!value) return editor;
        const auto shown = shownConfiguration();
        for (const auto &config: shown->runSeries.series) {
            if (config.id.value == *value) {
                editor->loadFrom(config.expression);
                break;
            }
        }
        return editor;
    }

    QVariantMap SettingsViewModel::createAxis(const QString &name) {
        return editRunSeries([&](application::RunSeriesDocument &document) {
            return application::createAxis(document, {name.toStdString()});
        });
    }

    QVariantMap SettingsViewModel::updateSeriesAxis(const QString &id, const QString &axisId) {
        const auto value = parseId(id);
        if (!value) return invalidMutationMap();
        application::UpdateSeriesRequest request;
        request.id = {*value};
        if (axisId.isEmpty()) {
            request.yAxisId = std::optional<application::AxisId>{std::nullopt};
        } else {
            const auto axisValue = parseId(axisId);
            if (!axisValue) return invalidMutationMap();
            request.yAxisId = std::optional<application::AxisId>{application::AxisId{*axisValue}};
        }
        return editRunSeries([&](application::RunSeriesDocument &document) {
            return application::updateSeries(document, request);
        });
    }

    void SettingsViewModel::setRunAxisLabelSeriesId(const QString &id) {
        if (!m_working) return;
        std::optional<application::SeriesId> label;
        if (!id.isEmpty()) {
            const auto value = parseId(id);
            if (!value) return;
            label = application::SeriesId{*value};
        }
        if (label == m_working->runAxisLabelSeriesId) return;
        m_working->runAxisLabelSeriesId = label;
        emit seriesConfigurationChanged();
        refreshPendingChanges();
    }
}
