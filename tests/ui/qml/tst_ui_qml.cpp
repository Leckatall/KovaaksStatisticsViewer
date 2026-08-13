//
// Entry point for the QML component tests (tst_*.qml in this directory).
// These drive real QML item trees (menu wiring, button clicks, checkbox
// bindings) via Qt Quick Test, as opposed to the gtest suite in
// tests/ui/*.cpp which only covers the plain-C++ view-model logic.
//

#include <QtQuickTest/quicktest.h>
#include <QtQml>

#include "components/graph_canvas.h"
#include "presentation/completion_history_vm.h"
#include "usecases/i_session_controller.h"

class UiQmlTestSetup : public QObject {
    Q_OBJECT

public slots:
    void qmlEngineAvailable(QQmlEngine *) {
        qRegisterMetaType<ksv::application::ISessionController *>();
        qmlRegisterUncreatableType<ksv::presentation::CompletionHistoryViewModel>(
            "KovaaksStatsViewer", 1, 0, "CompletionHistoryViewModel", "Created in C++");
        qmlRegisterType<ksv::ui::GraphCanvas>("KovaaksStatsViewer", 1, 0, "GraphCanvas");
    }
};

QUICK_TEST_MAIN_WITH_SETUP(ui_qml_tests, UiQmlTestSetup)

#include "tst_ui_qml.moc"
