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
        };

        [[nodiscard]] std::shared_ptr<const application::GraphConfiguration> shownConfiguration() const;
        [[nodiscard]] static std::optional<FieldKey> fieldKey(const QString &graph, const QString &group,
                                                              const QString &target, const QString &field);
        QVariantMap editRunSeries(const std::function<application::MutationResult(application::RunSeriesDocument &)> &edit);
        void setIssues(std::vector<Issue> issues);
        // Emits pendingChangesChanged when the dirty state flipped.
        void refreshPendingChanges();

        std::shared_ptr<application::ISettingsService> m_settings_service;
        std::shared_ptr<application::IProfileService> m_profile_service;
        std::shared_ptr<application::IGraphConfigurationUseCase> m_configuration;
        QUrl m_kovaaks_dir;

        std::optional<application::GraphConfiguration> m_working;
        application::GraphConfiguration m_baseline;
        std::map<FieldKey, QString> m_rawInput;
        std::vector<Issue> m_issues;
        application::GraphKey m_selectedGraph = application::GraphKey::Run;
        bool m_pendingChanges = false;
    };
}

#endif //KOVAAKSSTATSVIEWER_SETTINGS_VM_H
