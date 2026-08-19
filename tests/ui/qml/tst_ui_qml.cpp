//
// Entry point for the QML component tests (tst_*.qml in this directory).
// These drive real QML item trees (menu wiring, button clicks, checkbox
// bindings) via Qt Quick Test, as opposed to the gtest suite in
// tests/ui/*.cpp which only covers the plain-C++ view-model logic.
//

#include <QtQuickTest/quicktest.h>
#include <QtQml>
#include <QSettings>
#include <QTemporaryDir>

#include "components/graph_canvas.h"
#include "presentation/completion_history_vm.h"
#include "usecases/i_session_controller.h"

class UiQmlTestSetup : public QObject {
    Q_OBJECT

public slots:
    void qmlEngineAvailable(QQmlEngine *) {
        // QML `Settings {}` items (VisualSettingsManager.qml) need these to construct at
        // all; without them QSettings fails to initialize and every Settings-backed
        // property silently behaves as a plain, non-persisted local property instead.
        QCoreApplication::setOrganizationName("Lecka");
        QCoreApplication::setApplicationName("KovaaksStatsViewer");
        static QTemporaryDir settings_dir;
        if (settings_dir.isValid()) {
            QSettings::setDefaultFormat(QSettings::IniFormat);
            QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings_dir.path());
            QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, settings_dir.path());
        }

        qRegisterMetaType<ksv::application::ISessionController *>();
        qmlRegisterUncreatableType<ksv::presentation::CompletionHistoryViewModel>(
            "KovaaksStatsViewer", 1, 0, "CompletionHistoryViewModel", "Created in C++");
        qmlRegisterType<ksv::ui::GraphCanvas>("KovaaksStatsViewer", 1, 0, "GraphCanvas");
    }
};

QUICK_TEST_MAIN_WITH_SETUP(ui_qml_tests, UiQmlTestSetup)

#include "tst_ui_qml.moc"
