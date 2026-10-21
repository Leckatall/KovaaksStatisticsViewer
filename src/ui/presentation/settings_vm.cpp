//
// Created by Lecka on 03/08/2026.
//

#include "settings_vm.h"
#include "axis_policy.h"
#include "series_expression_editor_model.h"
#include "series_style.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <concepts>
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

        QString qstring(const std::string_view text) {
            return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
        }

        QString graphName(const application::GraphKey key) { return qstring(application::graphKeyName(key)); }

        QVariantMap issueMap(const application::GraphKey graph, const QString &group, const QString &target,
                             const QString &field, const QString &code) {
            return {{"graph", graphName(graph)}, {"group", group}, {"target", target}, {"field", field},
                    {"code", code}};
        }

        QVariantMap quickPatchResult(const bool accepted, QVariantList issues = {}) {
            return {{"accepted", accepted}, {"issues", std::move(issues)}};
        }

        std::optional<application::ConnectionMode> connectionFromName(const QString &name) {
            return application::connectionModeFromName(name.toStdString());
        }

        QDate toQDate(const std::chrono::year_month_day &date) {
            return {static_cast<int>(date.year()), static_cast<int>(static_cast<unsigned>(date.month())),
                    static_cast<int>(static_cast<unsigned>(date.day()))};
        }

        QString isoDate(const std::chrono::year_month_day &date) { return toQDate(date).toString(Qt::ISODate); }

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
            const auto value = toQDate(date);
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

        // A text that is only digits (of any script) with an optional sign is a whole number too large for
        // an int; telling it from garbage keeps the out-of-range issue codes distinct.
        bool isDigitsOnly(const QString &text) {
            const auto digits = (text.startsWith(QLatin1Char('-')) || text.startsWith(QLatin1Char('+'))) ? QStringView(text).mid(1)
                                                                                                           : QStringView(text);
            return !digits.isEmpty() && std::ranges::all_of(digits, [](const QChar c) { return c.digitValue() >= 0; });
        }

        ParsedCount parseCount(const QVariant &input, const QLocale &locale) {
            if (input.metaType().id() != QMetaType::QString) {
                bool ok = false;
                const int value = input.toInt(&ok);
                if (ok) return {value, {}};
                return {std::nullopt, kUnparsedInputCode};
            }
            const auto text = input.toString().trimmed();
            if (const auto value = parseWholeNumber(text, locale)) return {value, {}};
            if (!isDigitsOnly(text)) return {std::nullopt, kUnparsedInputCode};
            const bool negative = text.startsWith(QLatin1Char('-'));
            return {std::nullopt, issueCodeName(negative ? application::GraphConfigurationIssueCode::NonPositiveCount
                                                         : application::GraphConfigurationIssueCode::CountTooLarge)};
        }

        // A series left with no override field is dropped from the map. Returns whether the override moved.
        template<typename Edit>
        bool editSeriesOverride(application::GraphPreferences &preferences, const std::string &series, Edit edit) {
            auto &style = preferences.seriesStyles[series];
            const auto before = style;
            edit(style);
            const bool changed = style != before;
            if (style == application::SeriesStyleOverride{}) preferences.seriesStyles.erase(series);
            return changed;
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
            if (change.anyGraph()) {
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
        m_baseline = *m_configuration->current().configuration;
        replaceWorkingCopy(m_baseline);
    }

    QVariantMap SettingsViewModel::save() {
        if (!m_working) return {{"accepted", true}, {"issues", QVariantList{}}};
        std::vector<Issue> issues;
        for (const auto &key: m_rawInput | std::views::keys) issues.push_back({key, kUnparsedInputCode});
        if (issues.empty()) {
            for (const auto &issue: m_configuration->save(*m_working).issues) issues.push_back(toIssue(issue));
        }
        const bool accepted = issues.empty();
        if (accepted) m_baseline = *m_working;
        setIssues(std::move(issues));
        revalidateDraft();
        refreshPendingChanges();
        return {{"accepted", accepted}, {"issues", this->issues()}};
    }

    void SettingsViewModel::revertChanges() {
        if (m_working) replaceWorkingCopy(m_baseline);
    }

    void SettingsViewModel::endVisit() {
        if (m_working) replaceWorkingCopy(std::nullopt);
    }

    void SettingsViewModel::replaceWorkingCopy(std::optional<application::GraphConfiguration> working) {
        const bool visitChanged = working.has_value() != m_working.has_value();
        m_working = std::move(working);
        if (!m_rawInput.empty()) {
            m_rawInput.clear();
            emit rawInputsChanged();
        }
        setIssues({});
        if (visitChanged) emit visitOpenChanged();
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

    QVariantList SettingsViewModel::issues() const { return issueList(m_issues); }

    SettingsViewModel::Issue SettingsViewModel::toIssue(const application::GraphConfigurationIssue &issue) {
        return {{issue.graph, issue.group, issue.target, issue.field}, issueCodeName(issue.code)};
    }

    QVariantList SettingsViewModel::issueList(const std::vector<Issue> &issues) {
        QVariantList result;
        for (const auto &[key, code]: issues) {
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
        for (const auto key: application::kGraphKeys)
            result.insert(graphName(key), preferencesEntry(key, committed->preferences(key), false));
        return result;
    }

    QVariantMap SettingsViewModel::applyQuickPatch(const QString &graph, const QVariantMap &patch) {
        const auto key = application::graphKeyFromName(graph.toStdString());
        if (!key) return quickPatchResult(false);

        application::GraphQuickPatch request;
        QVariantList issues;
        const auto parse = [&](const QVariant &input, const QString &field) -> std::optional<int> {
            const auto parsed = parseCount(input, m_locale);
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
        std::vector<Issue> rejected;
        for (const auto &issue: result.issues) rejected.push_back(toIssue(issue));
        return quickPatchResult(result.accepted(), issueList(rejected));
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

    QVariantList SettingsViewModel::draftIssues() const { return issueList(m_draftIssues); }

    int SettingsViewModel::maximumHiddenLeadingPoints() {
        return static_cast<int>(application::kMaximumHiddenLeadingPoints);
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
        emit quickSettingsChanged();
        emit graphDraftsChanged();
    }

    QString SettingsViewModel::rawOr(const FieldKey &key, const QString &shown) const {
        const auto it = m_rawInput.find(key);
        return it == m_rawInput.end() ? shown : it->second;
    }

    QVariantMap SettingsViewModel::preferencesEntry(const application::GraphKey key,
                                                    const application::GraphPreferences &preferences,
                                                    const bool withRawInput) const {
        using namespace application;
        const auto &descriptor = graphDescriptor(key);
        const auto text = [&](const char *field, const QString &shown) {
            return withRawInput ? rawOr({key, "dataRange", "", field}, shown) : shown;
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
            {"windowText", preferences.averagingWindowDays
                               ? text("averagingWindow", formatWhole(*preferences.averagingWindowDays, m_locale))
                               : QString()},
            {"connection", qstring(connectionModeName(preferences.appearance.connection))},
            {"markers", preferences.appearance.lineModeMarkers}
        };

        if (preferences.range) {
            std::visit([&](const auto &range) {
                using Range = std::decay_t<decltype(range)>;
                if constexpr (std::same_as<Range, RecentDays>) {
                    entry["rangeKind"] = "recentDays";
                    entry["rangeCount"] = range.days;
                    entry["rangeCountText"] = text("recentDays", formatWhole(range.days, m_locale));
                } else if constexpr (std::same_as<Range, LatestRuns>) {
                    entry["rangeKind"] = "latestRuns";
                    entry["rangeCount"] = range.count;
                    entry["rangeCountText"] = text("latestRuns", formatWhole(range.count, m_locale));
                } else if constexpr (std::same_as<Range, CustomDates>) {
                    entry["rangeKind"] = "customDates";
                    entry["rangeStart"] = isoDate(range.start);
                    entry["rangeEnd"] = isoDate(range.end);
                    entry["rangeStartFormatted"] = formatDate(range.start, m_locale);
                    entry["rangeEndFormatted"] = formatDate(range.end, m_locale);
                    entry["rangeStartText"] = text("startDate", formatDate(range.start, m_locale));
                    entry["rangeEndText"] = text("endDate", formatDate(range.end, m_locale));
                }
            }, preferences.range->range);
        }
        return entry;
    }

    QVariantMap SettingsViewModel::draftEntry(const application::GraphKey key) const {
        using namespace application;
        const auto &descriptor = graphDescriptor(key);
        const auto shown = shownConfiguration();
        const auto &preferences = shown->preferences(key);
        auto entry = preferencesEntry(key, preferences, true);

        QVariantList series;
        for (const auto &described: descriptor.series) {
            const auto seriesKey = std::string(described.key);
            const auto found = preferences.seriesStyles.find(seriesKey);
            const SeriesStyleOverride *override = found == preferences.seriesStyles.end() ? nullptr : &found->second;
            const auto style = resolvedSeriesStyle(key, preferences.seriesStyles, described.key);
            series.append(QVariantMap{
                {"key", QString::fromStdString(seriesKey)},
                {"axisKey", qstring(described.axisKey)},
                {"color", style.color ? QVariant(toQColor(*style.color)) : QVariant()},
                {"colorOverridden", override && override->color.has_value()},
                {"width", style.width},
                {"widthText", rawOr({key, "seriesStyle", seriesKey, "width"}, formatDecimal(style.width, m_locale))},
                {"widthOverridden", override && override->width.has_value()},
                {"defaultWidth", defaultSeriesStyle(key, described.key).width}
            });
        }
        entry["series"] = series;

        QVariantList axes;
        for (const auto &described: descriptor.axes) {
            const auto axisKey = std::string(described.key);
            const auto policy = axisPolicyFor(preferences.axisPolicies, key, described.key);
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
                    sharing.append(qstring(member.key));
            axes.append(QVariantMap{
                {"key", QString::fromStdString(axisKey)},
                {"mode", mode},
                {"minText", rawOr({key, "axes", axisKey, "fixedMin"}, minText)},
                {"maxText", rawOr({key, "axes", axisKey, "fixedMax"}, maxText)},
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
                auto converted = toIssue(issue);
                if (!m_rawInput.contains(converted.key)) live.push_back(std::move(converted));
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

        // Stores a parsed value in `slot`; text that did not parse leaves it alone.
        const auto edit = [&](const auto &parsed, auto &slot) {
            const bool changed = parsed && std::exchange(slot, *parsed) != *parsed;
            return finishFieldEdit(location, text, parsed.has_value(), changed);
        };
        auto &range = preferences->range;

        if (group == "dataRange" && target.isEmpty()) {
            if (field == "recentDays" && range && std::holds_alternative<RecentDays>(range->range))
                return edit(parseWholeNumber(text, m_locale), std::get<RecentDays>(range->range).days);
            if (field == "latestRuns" && descriptor.latestRuns && range && std::holds_alternative<LatestRuns>(range->range))
                return edit(parseWholeNumber(text, m_locale), std::get<LatestRuns>(range->range).count);
            if (field == "averagingWindow" && descriptor.averagingWindow && preferences->averagingWindowDays)
                return edit(parseWholeNumber(text, m_locale), *preferences->averagingWindowDays);
            if ((field == "startDate" || field == "endDate") && range && std::holds_alternative<CustomDates>(range->range)) {
                auto &custom = std::get<CustomDates>(range->range);
                return edit(parseDate(text, m_locale), field == "startDate" ? custom.start : custom.end);
            }
            return false;
        }

        if (group == "seriesStyle" && field == "width") {
            if (!offersSeries(descriptor, location.target)) return false;
            const auto value = parseDecimal(text, m_locale);
            const bool changed = value && editSeriesOverride(*preferences, location.target, [&](SeriesStyleOverride &style) {
                style.width = *value == defaultSeriesStyle(key, location.target).width ? std::nullopt : value;
            });
            return finishFieldEdit(location, text, value.has_value(), changed);
        }

        if (group == "axes" && (field == "fixedMin" || field == "fixedMax")) {
            const auto policy = preferences->axisPolicies.find(location.target);
            if (policy == preferences->axisPolicies.end() || !std::holds_alternative<FixedBounds>(policy->second))
                return false;
            auto &bounds = std::get<FixedBounds>(policy->second);
            return edit(parseDecimal(text, m_locale), field == "fixedMin" ? bounds.minimum : bounds.maximum);
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
        if (!preferences || !color.isValid() || !offersSeries(graphDescriptor(key), name)) return false;
        const auto chosen = rgba(color);
        if (editSeriesOverride(*preferences, name, [&](SeriesStyleOverride &style) {
            style.color = chosen == defaultSeriesStyle(key, name).color ? std::nullopt : std::optional{chosen};
        }))
            draftChanged();
        return true;
    }

    bool SettingsViewModel::clearSeriesOverride(const QString &graph, const QString &series, const QString &field) {
        using namespace application;
        GraphKey key{};
        auto *preferences = draftPreferences(graph, key);
        const auto name = series.toStdString();
        if (!preferences || (field != "color" && field != "width") || !offersSeries(graphDescriptor(key), name))
            return false;
        const bool changed = editSeriesOverride(*preferences, name, [&](SeriesStyleOverride &style) {
            if (field == "color") style.color.reset();
            else style.width.reset();
        });
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
                {"color", toQColor(config.presentation.lineStyle.color)},
                {"width", config.presentation.lineStyle.width},
                {"enabled", config.presentation.enabled},
                {"displayPosition", config.presentation.displayPosition},
                {"hiddenLeadingPoints", config.presentation.hiddenLeadingPoints},
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

    QVariantMap SettingsViewModel::setSeriesHiddenLeadingPoints(const QString &id, const int count) {
        const auto value = parseId(id);
        if (!value || count < 0) return invalidMutationMap();
        return editRunSeries([&](application::RunSeriesDocument &document) {
            return application::updateSeries(document, {
                .id = {*value},
                .presentation = application::UpdatedSeriesPresentation{
                    .hiddenLeadingPoints = static_cast<uint32_t>(count)
                }
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
