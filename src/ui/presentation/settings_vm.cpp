//
// Created by Lecka on 03/08/2026.
//

#include "settings_vm.h"
#include "series_expression_editor_model.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <concepts>
#include <functional>
#include <limits>
#include <ranges>
#include <variant>

#include <QDate>
#include <QDateTime>
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

        // Group separators are rejected so that a whole string either is a number in the locale or is not.
        QLocale parsingLocale(QLocale locale) {
            locale.setNumberOptions(locale.numberOptions() | QLocale::RejectGroupSeparator);
            return locale;
        }

        QLocale displayLocale(QLocale locale) {
            locale.setNumberOptions(locale.numberOptions() | QLocale::OmitGroupSeparator);
            return locale;
        }

        std::optional<int> parseWholeNumber(const QString &text, const QLocale &locale) {
            bool ok = false;
            const qlonglong value = parsingLocale(locale).toLongLong(text.trimmed(), &ok);
            if (!ok || value > std::numeric_limits<int>::max() || value < std::numeric_limits<int>::min())
                return std::nullopt;
            return static_cast<int>(value);
        }

        std::optional<double> parseDecimal(const QString &text, const QLocale &locale) {
            bool ok = false;
            const double value = parsingLocale(locale).toDouble(text.trimmed(), &ok);
            if (!ok || !std::isfinite(value)) return std::nullopt;
            return value;
        }

        QString formatWhole(const int value, const QLocale &locale) {
            return displayLocale(locale).toString(value);
        }

        QString formatDecimal(const double value, const QLocale &locale) {
            return displayLocale(locale).toString(value, 'g', 15);
        }

        // The locale's short date order with a four-digit year, so entry and display agree.
        QString dateFormatFor(const QLocale &locale) {
            static const QRegularExpression shortYear(QStringLiteral("(?<!y)y{1,2}(?!y)"));
            return locale.dateFormat(QLocale::ShortFormat).replace(shortYear, QStringLiteral("yyyy"));
        }

        std::chrono::year_month_day toCalendarDate(const QDate &date) {
            return {std::chrono::year{date.year()}, std::chrono::month{static_cast<unsigned>(date.month())},
                    std::chrono::day{static_cast<unsigned>(date.day())}};
        }

        QString formatDate(const std::chrono::year_month_day &date, const QLocale &locale) {
            const QDate value(static_cast<int>(date.year()), static_cast<int>(static_cast<unsigned>(date.month())),
                              static_cast<int>(static_cast<unsigned>(date.day())));
            return value.isValid() ? locale.toString(value, dateFormatFor(locale)) : QString();
        }

        // The locale's own order, or ISO year-month-day in any locale; the whole text must be the date.
        std::optional<std::chrono::year_month_day> parseDate(const QString &text, const QLocale &locale) {
            const auto trimmed = text.trimmed();
            auto date = QDate::fromString(trimmed, Qt::ISODate);
            if (!date.isValid()) date = locale.toDate(trimmed, dateFormatFor(locale));
            if (!date.isValid()) return std::nullopt;
            return toCalendarDate(date);
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
            if (!change.graphs.empty()) {
                emit self->quickSettingsChanged();
                if (!self->isVisitOpen()) emit self->graphDraftsChanged();
            }
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
        emit graphDraftsChanged();
        revalidateDraft();
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
        revalidateDraft();
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
        emit graphDraftsChanged();
        revalidateDraft();
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
        emit graphDraftsChanged();
        revalidateDraft();
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
        emit graphDraftsChanged();
        revalidateDraft();
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
        emit graphDraftsChanged();
        revalidateDraft();
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

    // ---- Graph preferences --------------------------------------------------------------------------

    QVariantMap SettingsViewModel::graphDrafts() const {
        QVariantMap result;
        for (const auto key: application::kGraphKeys) result.insert(graphName(key), draftEntry(key));
        return result;
    }

    QVariantMap SettingsViewModel::graphDraft(const QString &graph) const {
        const auto key = application::graphKeyFromName(graph.toStdString());
        return key ? draftEntry(*key) : QVariantMap{};
    }

    QVariantList SettingsViewModel::draftIssues() const {
        QVariantList result;
        for (const auto &[key, code]: m_draftIssues) {
            result.append(issueMap(key.graph, QString::fromStdString(key.group), QString::fromStdString(key.target),
                                   QString::fromStdString(key.field), code));
        }
        return result;
    }

    QVariantMap SettingsViewModel::graphLimits() const {
        return {
            {"maxRecentDays", application::kMaximumRangeDays},
            {"maxLatestRuns", application::kMaximumLatestRuns},
            {"maxAveragingWindow", application::kMaximumAveragingWindowDays},
            {"minWidth", application::kMinimumLineWidth},
            {"maxWidth", application::kMaximumLineWidth},
            {"maxBound", application::kMaximumAxisBoundMagnitude},
            {"earliestDate", isoDate(application::kEarliestCalendarDate)},
            {"latestDate", isoDate(application::kLatestCalendarDate)},
            {"earliestDateText", formatDate(application::kEarliestCalendarDate, m_locale)},
            {"latestDateText", formatDate(application::kLatestCalendarDate, m_locale)},
            {"dateFormat", dateFormatFor(m_locale)}
        };
    }

    void SettingsViewModel::setLocale(const QLocale &locale) {
        m_locale = locale;
        emit graphDraftsChanged();
    }

    QString SettingsViewModel::rawOr(const FieldKey &key, const QString &shown) const {
        const auto it = m_rawInput.find(key);
        return it == m_rawInput.end() ? shown : it->second;
    }

    QVariantMap SettingsViewModel::draftEntry(const application::GraphKey key) const {
        using namespace application;
        const auto &descriptor = graphDescriptor(key);
        const auto shown = shownConfiguration();
        const auto &preferences = shown->preferences(key);
        const auto keyed = [&](const std::string &group, const std::string &target, const std::string &field) {
            return FieldKey{key, group, target, field};
        };

        QVariantMap entry{
            {"graph", graphName(key)},
            {"hasRange", descriptor.dateRange},
            {"hasLatestRuns", descriptor.latestRuns},
            {"hasWindow", descriptor.averagingWindow},
            {"rangeKind", "allHistory"},
            {"rangeCount", 0},
            {"rangeCountText", QString()},
            {"rangeStart", QString()},
            {"rangeEnd", QString()},
            {"rangeStartText", QString()},
            {"rangeEndText", QString()},
            {"rangeStartFormatted", QString()},
            {"rangeEndFormatted", QString()},
            {"anchor", preferences.range && preferences.range->anchorToLatestRun},
            {"window", preferences.averagingWindowDays.value_or(0)},
            {"windowText", QString()},
            {"connection", connectionName(preferences.appearance.connection)},
            {"markers", preferences.appearance.lineModeMarkers}
        };

        if (preferences.range) {
            std::visit([&](const auto &range) {
                using Range = std::decay_t<decltype(range)>;
                if constexpr (std::same_as<Range, RecentDays>) {
                    entry["rangeKind"] = "recentDays";
                    entry["rangeCount"] = range.days;
                    entry["rangeCountText"] = rawOr(keyed("dataRange", "", "recentDays"), formatWhole(range.days, m_locale));
                } else if constexpr (std::same_as<Range, LatestRuns>) {
                    entry["rangeKind"] = "latestRuns";
                    entry["rangeCount"] = range.count;
                    entry["rangeCountText"] = rawOr(keyed("dataRange", "", "latestRuns"), formatWhole(range.count, m_locale));
                } else if constexpr (std::same_as<Range, CustomDates>) {
                    entry["rangeKind"] = "customDates";
                    entry["rangeStart"] = isoDate(range.start);
                    entry["rangeEnd"] = isoDate(range.end);
                    entry["rangeStartFormatted"] = formatDate(range.start, m_locale);
                    entry["rangeEndFormatted"] = formatDate(range.end, m_locale);
                    entry["rangeStartText"] = rawOr(keyed("dataRange", "", "startDate"), formatDate(range.start, m_locale));
                    entry["rangeEndText"] = rawOr(keyed("dataRange", "", "endDate"), formatDate(range.end, m_locale));
                }
            }, preferences.range->range);
        }
        if (preferences.averagingWindowDays)
            entry["windowText"] = rawOr(keyed("dataRange", "", "averagingWindow"),
                                        formatWhole(*preferences.averagingWindowDays, m_locale));

        QVariantList series;
        for (const auto &described: descriptor.series) {
            const auto seriesKey = std::string(described.key);
            const auto found = preferences.seriesStyles.find(seriesKey);
            const SeriesStyleOverride *override = found == preferences.seriesStyles.end() ? nullptr : &found->second;
            const auto defaults = defaultSeriesStyle(key, described.key);
            const auto color = override && override->color ? override->color : defaults.color;
            const double width = override && override->width ? *override->width : defaults.width;
            series.append(QVariantMap{
                {"key", QString::fromStdString(seriesKey)},
                {"axisKey", QString::fromUtf8(described.axisKey.data(), static_cast<qsizetype>(described.axisKey.size()))},
                {"color", color ? QVariant(QColor(color->red, color->green, color->blue, color->alpha)) : QVariant()},
                {"colorOverridden", override && override->color.has_value()},
                {"width", width},
                {"widthText", rawOr(keyed("seriesStyle", seriesKey, "width"), formatDecimal(width, m_locale))},
                {"widthOverridden", override && override->width.has_value()},
                {"defaultWidth", defaults.width}
            });
        }
        entry["series"] = series;

        QVariantList axes;
        for (const auto &described: descriptor.axes) {
            const auto axisKey = std::string(described.key);
            const auto found = preferences.axisPolicies.find(axisKey);
            const AxisPolicy &policy = found == preferences.axisPolicies.end() ? described.defaultPolicy : found->second;
            QString mode = "fit";
            QString minText;
            QString maxText;
            if (std::holds_alternative<StartAtZero>(policy)) {
                mode = "zero";
            } else if (const auto *bounds = std::get_if<FixedBounds>(&policy)) {
                mode = "fixed";
                minText = formatDecimal(bounds->minimum, m_locale);
                maxText = formatDecimal(bounds->maximum, m_locale);
            }
            QStringList sharing;
            for (const auto &member: descriptor.series)
                if (member.axisKey == described.key)
                    sharing.append(QString::fromUtf8(member.key.data(), static_cast<qsizetype>(member.key.size())));
            axes.append(QVariantMap{
                {"key", QString::fromStdString(axisKey)},
                {"mode", mode},
                {"minText", rawOr(keyed("axes", axisKey, "fixedMin"), minText)},
                {"maxText", rawOr(keyed("axes", axisKey, "fixedMax"), maxText)},
                {"seriesKeys", sharing}
            });
        }
        entry["axes"] = axes;
        return entry;
    }

    application::GraphPreferences *SettingsViewModel::draftPreferences(const QString &graph,
                                                                       application::GraphKey &key) {
        const auto parsed = application::graphKeyFromName(graph.toStdString());
        if (!m_working || !parsed) return nullptr;
        key = *parsed;
        return &m_working->preferences(key);
    }

    void SettingsViewModel::draftChanged() {
        emit graphDraftsChanged();
        revalidateDraft();
        // The list of the last rejected Save is only worth showing while something it named is still wrong.
        if (m_draftIssues.empty()) setIssues({});
        refreshPendingChanges();
    }

    void SettingsViewModel::setDraftIssues(std::vector<Issue> issues) {
        if (issues == m_draftIssues) return;
        m_draftIssues = std::move(issues);
        emit draftIssuesChanged();
    }

    void SettingsViewModel::revalidateDraft() {
        std::vector<Issue> live;
        if (m_working) {
            for (const auto &key: m_rawInput | std::views::keys) live.push_back({key, kUnparsedInputCode});
            for (const auto &issue: application::validateGraphConfiguration(*m_working)) {
                const FieldKey key{issue.graph, issue.group, issue.target, issue.field};
                if (!m_rawInput.contains(key)) live.push_back({key, issueCodeName(issue.code)});
            }
        }
        setDraftIssues(std::move(live));
    }

    bool SettingsViewModel::dropRawInput(const application::GraphKey graph, const std::string &group,
                                         const std::string &target,
                                         const std::initializer_list<const char *> fields) {
        bool dropped = false;
        for (const char *field: fields) dropped |= m_rawInput.erase({graph, group, target, field}) > 0;
        if (dropped) emit rawInputsChanged();
        return dropped;
    }

    bool SettingsViewModel::finishFieldEdit(const FieldKey &key, const QString &text, const bool parsed,
                                            const bool changed) {
        bool rawChanged = false;
        if (parsed) {
            rawChanged = m_rawInput.erase(key) > 0;
        } else {
            const auto [it, inserted] = m_rawInput.try_emplace(key, text);
            rawChanged = inserted || it->second != text;
            it->second = text;
        }
        if (rawChanged) emit rawInputsChanged();
        if (changed || rawChanged) draftChanged();
        return parsed;
    }

    bool SettingsViewModel::setGraphField(const QString &graph, const QString &group, const QString &target,
                                          const QString &field, const QString &text) {
        using namespace application;
        GraphKey key{};
        auto *preferences = draftPreferences(graph, key);
        if (!preferences) return false;
        const auto &descriptor = graphDescriptor(key);
        const FieldKey location{key, group.toStdString(), target.toStdString(), field.toStdString()};

        const auto editWhole = [&](const std::function<bool(int)> &store) {
            const auto value = parseWholeNumber(text, m_locale);
            return finishFieldEdit(location, text, value.has_value(), value && store(*value));
        };

        if (group == "dataRange" && target.isEmpty()) {
            if (field == "recentDays" && preferences->range && std::holds_alternative<RecentDays>(preferences->range->range))
                return editWhole([&](const int value) {
                    auto &days = std::get<RecentDays>(preferences->range->range).days;
                    return std::exchange(days, value) != value;
                });
            if (field == "latestRuns" && descriptor.latestRuns && preferences->range &&
                std::holds_alternative<LatestRuns>(preferences->range->range))
                return editWhole([&](const int value) {
                    auto &count = std::get<LatestRuns>(preferences->range->range).count;
                    return std::exchange(count, value) != value;
                });
            if (field == "averagingWindow" && descriptor.averagingWindow && preferences->averagingWindowDays)
                return editWhole([&](const int value) {
                    return std::exchange(*preferences->averagingWindowDays, value) != value;
                });
            if ((field == "startDate" || field == "endDate") && preferences->range &&
                std::holds_alternative<CustomDates>(preferences->range->range)) {
                const auto date = parseDate(text, m_locale);
                bool changed = false;
                if (date) {
                    auto &custom = std::get<CustomDates>(preferences->range->range);
                    auto &slot = field == "startDate" ? custom.start : custom.end;
                    changed = slot != *date;
                    slot = *date;
                }
                return finishFieldEdit(location, text, date.has_value(), changed);
            }
            return false;
        }

        if (group == "seriesStyle" && field == "width") {
            if (std::ranges::none_of(descriptor.series, [&](const auto &s) { return s.key == location.target; }))
                return false;
            const auto value = parseDecimal(text, m_locale);
            bool changed = false;
            if (value) {
                auto &style = preferences->seriesStyles[location.target];
                const auto before = style.width;
                if (*value == defaultSeriesStyle(key, location.target).width) style.width.reset();
                else style.width = *value;
                changed = before != style.width;
                if (!style.color && !style.width) preferences->seriesStyles.erase(location.target);
            }
            return finishFieldEdit(location, text, value.has_value(), changed);
        }

        if (group == "axes" && (field == "fixedMin" || field == "fixedMax")) {
            const auto policy = preferences->axisPolicies.find(location.target);
            if (policy == preferences->axisPolicies.end() || !std::holds_alternative<FixedBounds>(policy->second))
                return false;
            const auto value = parseDecimal(text, m_locale);
            bool changed = false;
            if (value) {
                auto &bounds = std::get<FixedBounds>(policy->second);
                auto &slot = field == "fixedMin" ? bounds.minimum : bounds.maximum;
                changed = slot != *value;
                slot = *value;
            }
            return finishFieldEdit(location, text, value.has_value(), changed);
        }
        return false;
    }

    bool SettingsViewModel::setGraphRangeKind(const QString &graph, const QString &kind) {
        using namespace application;
        constexpr int kDefaultRecentDays = 30;
        constexpr int kDefaultLatestRuns = 50;
        constexpr int kDefaultCustomSpanDays = 30;

        GraphKey key{};
        auto *preferences = draftPreferences(graph, key);
        if (!preferences || !preferences->range) return false;
        auto &range = preferences->range->range;

        GraphRange next;
        if (kind == "allHistory") {
            next = AllHistory{};
        } else if (kind == "recentDays") {
            next = std::holds_alternative<RecentDays>(range) ? range : GraphRange{RecentDays{kDefaultRecentDays}};
        } else if (kind == "latestRuns") {
            if (!graphDescriptor(key).latestRuns) return false;
            next = std::holds_alternative<LatestRuns>(range) ? range : GraphRange{LatestRuns{kDefaultLatestRuns}};
        } else if (kind == "customDates") {
            if (std::holds_alternative<CustomDates>(range)) {
                next = range;
            } else {
                // Only a seed to edit from; the app's day source is not reachable from here.
                const auto today = QDateTime::currentDateTimeUtc().date();
                next = CustomDates{toCalendarDate(today.addDays(1 - kDefaultCustomSpanDays)), toCalendarDate(today)};
            }
        } else {
            return false;
        }

        if (next == range) return true;
        range = next;
        dropRawInput(key, "dataRange", "", {"recentDays", "latestRuns", "startDate", "endDate"});
        draftChanged();
        return true;
    }

    bool SettingsViewModel::setGraphAnchor(const QString &graph, const bool anchorToLatestRun) {
        application::GraphKey key{};
        auto *preferences = draftPreferences(graph, key);
        if (!preferences || !preferences->range) return false;
        if (std::exchange(preferences->range->anchorToLatestRun, anchorToLatestRun) != anchorToLatestRun) draftChanged();
        return true;
    }

    bool SettingsViewModel::setGraphConnection(const QString &graph, const QString &connection) {
        application::GraphKey key{};
        auto *preferences = draftPreferences(graph, key);
        const auto mode = connectionFromName(connection);
        if (!preferences || !mode) return false;
        if (std::exchange(preferences->appearance.connection, *mode) != *mode) draftChanged();
        return true;
    }

    bool SettingsViewModel::setGraphMarkers(const QString &graph, const bool markers) {
        application::GraphKey key{};
        auto *preferences = draftPreferences(graph, key);
        if (!preferences) return false;
        if (std::exchange(preferences->appearance.lineModeMarkers, markers) != markers) draftChanged();
        return true;
    }

    bool SettingsViewModel::setSeriesColor(const QString &graph, const QString &series, const QColor &color) {
        using namespace application;
        GraphKey key{};
        auto *preferences = draftPreferences(graph, key);
        const auto name = series.toStdString();
        if (!preferences || !color.isValid() ||
            std::ranges::none_of(graphDescriptor(key).series, [&](const auto &s) { return s.key == name; }))
            return false;
        auto &style = preferences->seriesStyles[name];
        const auto before = style.color;
        const auto chosen = rgba(color);
        if (chosen == defaultSeriesStyle(key, name).color) style.color.reset();
        else style.color = chosen;
        const bool changed = before != style.color;
        if (!style.color && !style.width) preferences->seriesStyles.erase(name);
        if (changed) draftChanged();
        return true;
    }

    bool SettingsViewModel::clearSeriesOverride(const QString &graph, const QString &series, const QString &field) {
        using namespace application;
        GraphKey key{};
        auto *preferences = draftPreferences(graph, key);
        const auto name = series.toStdString();
        if (!preferences || (field != "color" && field != "width") ||
            std::ranges::none_of(graphDescriptor(key).series, [&](const auto &s) { return s.key == name; }))
            return false;
        bool changed = false;
        if (const auto found = preferences->seriesStyles.find(name); found != preferences->seriesStyles.end()) {
            auto &style = found->second;
            changed = (field == "color" ? style.color.has_value() : style.width.has_value());
            if (field == "color") style.color.reset();
            else style.width.reset();
            if (!style.color && !style.width) preferences->seriesStyles.erase(found);
        }
        const bool rawDropped = field == "width" && dropRawInput(key, "seriesStyle", name, {"width"});
        if (changed || rawDropped) draftChanged();
        return true;
    }

    bool SettingsViewModel::setAxisMode(const QString &graph, const QString &axis, const QString &mode) {
        using namespace application;
        GraphKey key{};
        auto *preferences = draftPreferences(graph, key);
        const auto name = axis.toStdString();
        const auto &axes = graphDescriptor(key).axes;
        const auto described = preferences ? std::ranges::find(axes, name, &GraphAxisDescriptor::key) : axes.end();
        if (!preferences || described == axes.end()) return false;

        auto &policy = preferences->axisPolicies[name];
        AxisPolicy next;
        if (mode == "fit") next = FitVisibleData{};
        else if (mode == "zero") next = StartAtZero{};
        else if (mode == "fixed") next = std::holds_alternative<FixedBounds>(policy) ? policy : AxisPolicy{FixedBounds{}};
        else return false;

        if (next == policy) return true;
        policy = next;
        if (mode != "fixed") dropRawInput(key, "axes", name, {"fixedMin", "fixedMax"});
        draftChanged();
        return true;
    }

    bool SettingsViewModel::resetGraph(const QString &graph) {
        application::GraphKey key{};
        auto *preferences = draftPreferences(graph, key);
        if (!preferences) return false;
        bool rawDropped = false;
        for (auto it = m_rawInput.begin(); it != m_rawInput.end();) {
            if (it->first.graph == key && it->first.group != "graphLines") {
                it = m_rawInput.erase(it);
                rawDropped = true;
            } else {
                ++it;
            }
        }
        if (rawDropped) emit rawInputsChanged();
        const auto defaults = application::defaultGraphPreferences(key);
        const bool changed = *preferences != defaults;
        *preferences = defaults;
        if (changed || rawDropped) draftChanged();
        return true;
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
