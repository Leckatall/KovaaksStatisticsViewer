//
// Created by Lecka on 03/08/2026.
//

#ifndef KOVAAKSSTATSVIEWER_SETTINGS_VM_H
#define KOVAAKSSTATSVIEWER_SETTINGS_VM_H

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <QtCore>
#include <QColor>

#include "data/interfaces/i_profile_service.h"
#include "data/interfaces/i_settings_service.h"
#include "app/contracts/i_graph_configuration_use_case.h"
#include "app/contracts/run_series_editing.h"

namespace ksv::presentation {
    class SeriesExpressionEditorModel;

    // Owns the Settings window's one working copy of the whole graph configuration for a visit,
    // with its baseline, unparsed input and the issues of the last rejected Save. Nothing here reaches
    // the dashboard until save() hands the complete working copy to IGraphConfigurationUseCase.
    //
    // Fields are addressed from QML as (graph, group, target, field) strings, the same location
    // GraphConfigurationIssue uses, so an issue and the raw text it concerns share one key.
    class SettingsViewModel : public QObject {
        Q_OBJECT
        Q_PROPERTY(QUrl kovaaksDir READ getKovaaksDir WRITE setKovaaksDir NOTIFY kovaaksDirChanged)
        Q_PROPERTY(bool kovaaksDirSet READ isKovaaksDirSet NOTIFY kovaaksDirChanged)
        Q_PROPERTY(QUrl profilePath READ getProfilePath WRITE setProfilePath NOTIFY profilePathChanged)
        Q_PROPERTY(bool profileLoaded READ isProfileLoaded NOTIFY profileLoadedChanged)
        Q_PROPERTY(QVariantList allSeriesConfigs READ getAllSeriesConfigs NOTIFY seriesConfigurationChanged)
        Q_PROPERTY(QVariantList allAxes READ getAllAxes NOTIFY seriesConfigurationChanged)
        Q_PROPERTY(QString runAxisLabelSeriesId READ runAxisLabelSeriesId NOTIFY seriesConfigurationChanged)
        Q_PROPERTY(bool visitOpen READ isVisitOpen NOTIFY visitOpenChanged)
        Q_PROPERTY(bool pendingChanges READ hasPendingChanges NOTIFY pendingChangesChanged)
        Q_PROPERTY(QString selectedGraph READ selectedGraph WRITE setSelectedGraph NOTIFY selectedGraphChanged)
        Q_PROPERTY(QVariantList issues READ issues NOTIFY issuesChanged)
        Q_PROPERTY(QVariantList rawInputs READ rawInputs NOTIFY rawInputsChanged)
        Q_PROPERTY(QVariantMap quickSettings READ quickSettings NOTIFY quickSettingsChanged)
        Q_PROPERTY(QVariantMap graphDrafts READ graphDrafts NOTIFY graphDraftsChanged)
        Q_PROPERTY(QVariantList draftIssues READ draftIssues NOTIFY draftIssuesChanged)
        Q_PROPERTY(QVariantMap graphLimits READ graphLimits CONSTANT)

    public:
        explicit SettingsViewModel(std::shared_ptr<application::ISettingsService> settings_service,
                                   std::shared_ptr<application::IProfileService> profile_service,
                                   std::shared_ptr<application::IGraphConfigurationUseCase> configuration,
                                   QObject *parent = nullptr);

        Q_INVOKABLE [[nodiscard]] QUrl getKovaaksDir() const { return m_kovaaks_dir; }
        [[nodiscard]] bool isKovaaksDirSet() const { return m_settings_service->isKovaaksDirSet(); }

        Q_INVOKABLE void setKovaaksDir(const QUrl &dir);

        Q_INVOKABLE [[nodiscard]] QUrl getProfilePath() const {
            return QUrl::fromLocalFile(m_settings_service->getProfilePath().data());
        }

        Q_INVOKABLE void setProfilePath(const QUrl &path);

        [[nodiscard]] bool isProfileLoaded() const { return m_profile_service->isProfileLoaded(); }

        // ---- Visit lifecycle ----------------------------------------------------------------------
        // Seeds the working copy; does nothing while a visit is already open.
        Q_INVOKABLE void beginVisit();
        // Returns {accepted, issues}. Acceptance keeps the visit open with the saved copy as baseline.
        Q_INVOKABLE QVariantMap save();
        // Back to the baseline: the seed, or the last accepted save.
        Q_INVOKABLE void revertChanges();
        // Releases the working copy without applying it.
        Q_INVOKABLE void endVisit();

        [[nodiscard]] bool isVisitOpen() const { return m_working.has_value(); }
        // Includes unparsed input, so closing cannot silently drop it.
        [[nodiscard]] bool hasPendingChanges() const;

        [[nodiscard]] QString selectedGraph() const;
        void setSelectedGraph(const QString &graph);

        [[nodiscard]] QVariantList issues() const;

        // ---- Unparsed input ------------------------------------------------------------------------
        // Text a control could not turn into a value; it blocks save() until cleared.
        Q_INVOKABLE void setRawInput(const QString &graph, const QString &group, const QString &target,
                                     const QString &field, const QString &text);
        Q_INVOKABLE void clearRawInput(const QString &graph, const QString &group, const QString &target,
                                       const QString &field);
        // An invalid QVariant when the field holds no unparsed text.
        Q_INVOKABLE [[nodiscard]] QVariant rawInput(const QString &graph, const QString &group,
                                                    const QString &target, const QString &field) const;
        [[nodiscard]] QVariantList rawInputs() const;

        // ---- Quick settings: committed state, independent of any visit ----------------------------
        // Keyed by graph name; each entry carries only what the gear popup shows for that graph.
        [[nodiscard]] QVariantMap quickSettings() const;
        // Returns {accepted, issues}. Count fields may be numbers or text; text that is not a whole
        // number is reported without reaching the use case.
        Q_INVOKABLE QVariantMap applyQuickPatch(const QString &graph, const QVariantMap &patch);

        // ---- Graph preferences: the working copy during a visit, the committed ones otherwise ------------
        // Keyed by graph name; each entry says which controls the graph offers and holds the text each
        // one shows (unparsed input verbatim, else the value in the app locale). See graphDraft().
        [[nodiscard]] QVariantMap graphDrafts() const;
        Q_INVOKABLE [[nodiscard]] QVariantMap graphDraft(const QString &graph) const;
        // What is wrong with the working copy now, in the shape of issues(): unparsed text plus the
        // semantic validation of the candidate. Save() stays the authority; this only places errors.
        [[nodiscard]] QVariantList draftIssues() const;
        // The stated input limits and the date format fields expect.
        [[nodiscard]] QVariantMap graphLimits() const;

        // Every edit below changes only the working copy; it returns false when nothing was applied
        // (no visit, unknown graph or control, or text that does not parse completely).
        // Text is parsed whole in the app locale; text that does not parse is kept as unparsed input at
        // its field and the working copy keeps its previous value.
        // Fields: dataRange/{recentDays,latestRuns,startDate,endDate,averagingWindow},
        // seriesStyle/<series>/width, axes/<axis>/{fixedMin,fixedMax}.
        Q_INVOKABLE bool setGraphField(const QString &graph, const QString &group, const QString &target,
                                       const QString &field, const QString &text);
        // allHistory, recentDays, latestRuns or customDates; the anchor flag is kept, and unparsed text
        // of the range fields the new kind hides is dropped.
        Q_INVOKABLE bool setGraphRangeKind(const QString &graph, const QString &kind);
        Q_INVOKABLE bool setGraphAnchor(const QString &graph, bool anchorToLatestRun);
        Q_INVOKABLE bool setGraphConnection(const QString &graph, const QString &connection);
        Q_INVOKABLE bool setGraphMarkers(const QString &graph, bool markers);
        Q_INVOKABLE bool setSeriesColor(const QString &graph, const QString &series, const QColor &color);
        // field is "color" or "width"; the series falls back to the graph's default for it.
        Q_INVOKABLE bool clearSeriesOverride(const QString &graph, const QString &series, const QString &field);
        // fit, zero or fixed; unparsed text of the bound fields is dropped unless the mode is fixed.
        Q_INVOKABLE bool setAxisMode(const QString &graph, const QString &axis, const QString &mode);
        // Stages the graph's presentation defaults only; Graph Lines and other graphs are untouched.
        Q_INVOKABLE bool resetGraph(const QString &graph);

        // The locale that numbers and dates are read and shown in; the system's unless a test sets it.
        void setLocale(const QLocale &locale);

        // ---- Graph Lines: the working copy during a visit, the committed configuration otherwise -----
        Q_INVOKABLE [[nodiscard]] QVariantList getAllSeriesConfigs() const;
        Q_INVOKABLE [[nodiscard]] QVariantList getAllAxes() const;
        [[nodiscard]] QString runAxisLabelSeriesId() const;

        // Every edit below changes only the working copy and is refused outside a visit.
        Q_INVOKABLE QVariantMap setSeriesEnabled(const QString &id, bool enabled);
        Q_INVOKABLE QVariantMap createComputedSeries(const QString &name, const QColor &color, double width,
                                                      bool enabled, const QString &expression);
        Q_INVOKABLE QVariantMap updateComputedSeries(const QString &id, const QString &name, const QColor &color,
                                                      double width, bool enabled, const QString &expression);
        Q_INVOKABLE QVariantMap removeComputedSeries(const QString &id);
        Q_INVOKABLE QVariantMap reorderSeries(const QString &id, int displayPosition);
        Q_INVOKABLE SeriesExpressionEditorModel *beginExpressionEdit(const QString &id);
        Q_INVOKABLE QVariantMap createAxis(const QString &name);
        Q_INVOKABLE QVariantMap updateSeriesAxis(const QString &id, const QString &axisId);
        // An empty id clears the choice, so the run graph labels its first visible series.
        Q_INVOKABLE void setRunAxisLabelSeriesId(const QString &id);

    signals:
        void kovaaksDirChanged();
        void profilePathChanged();
        void profileLoadedChanged();
        void seriesConfigurationChanged();
        void visitOpenChanged();
        void pendingChangesChanged();
        void selectedGraphChanged();
        void issuesChanged();
        void rawInputsChanged();
        void quickSettingsChanged();
        void graphDraftsChanged();
        void draftIssuesChanged();

    private:
        struct FieldKey {
            application::GraphKey graph;
            std::string group;
            std::string target;
            std::string field;

            auto operator<=>(const FieldKey &) const = default;
        };

        struct Issue {
            FieldKey key;
            QString code;

            bool operator==(const Issue &) const = default;
        };

        [[nodiscard]] std::shared_ptr<const application::GraphConfiguration> shownConfiguration() const;
        [[nodiscard]] static std::optional<FieldKey> fieldKey(const QString &graph, const QString &group,
                                                              const QString &target, const QString &field);
        QVariantMap editRunSeries(const std::function<application::MutationResult(application::RunSeriesDocument &)> &edit);
        // Installs `working` (no visit when empty) with no unparsed input and no Save issues.
        void replaceWorkingCopy(std::optional<application::GraphConfiguration> working);
        [[nodiscard]] static Issue toIssue(const application::GraphConfigurationIssue &issue);
        [[nodiscard]] static QVariantList issueList(const std::vector<Issue> &issues);
        void setIssues(std::vector<Issue> issues);
        // Emits pendingChangesChanged when the dirty state flipped.
        void refreshPendingChanges();
        // After any edit of the working copy's graph preferences or unparsed text.
        void draftChanged();
        void setDraftIssues(std::vector<Issue> issues);
        [[nodiscard]] QString rawOr(const FieldKey &key, const QString &shown) const;
        [[nodiscard]] application::GraphPreferences *draftPreferences(const QString &graph,
                                                                      application::GraphKey &key);
        // The keys the gear popup and the settings page share; `withRawInput` shows unparsed text in
        // place of a field's value.
        [[nodiscard]] QVariantMap preferencesEntry(application::GraphKey key,
                                                   const application::GraphPreferences &preferences,
                                                   bool withRawInput) const;
        [[nodiscard]] QVariantMap draftEntry(application::GraphKey key) const;
        // True when any text was dropped.
        bool dropRawInput(application::GraphKey graph, const std::string &group, const std::string &target,
                          std::initializer_list<const char *> fields);
        // Records the outcome of editing `key` with `text`: unparsed text is kept, parsed text replaces it.
        // `changed` says whether the working copy's value moved. Returns `parsed`.
        bool finishFieldEdit(const FieldKey &key, const QString &text, bool parsed, bool changed);
        void revalidateDraft();

        std::shared_ptr<application::ISettingsService> m_settings_service;
        std::shared_ptr<application::IProfileService> m_profile_service;
        std::shared_ptr<application::IGraphConfigurationUseCase> m_configuration;
        QUrl m_kovaaks_dir;

        std::optional<application::GraphConfiguration> m_working;
        application::GraphConfiguration m_baseline;
        std::map<FieldKey, QString> m_rawInput;
        std::vector<Issue> m_issues;
        std::vector<Issue> m_draftIssues;
        QLocale m_locale;
        application::GraphKey m_selectedGraph = application::GraphKey::Run;
        bool m_pendingChanges = false;
    };
}

#endif //KOVAAKSSTATSVIEWER_SETTINGS_VM_H
