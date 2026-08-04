//
// Entry point for the QML component tests (tst_*.qml in this directory).
// These drive real QML item trees (menu wiring, button clicks, checkbox
// bindings) via Qt Quick Test, as opposed to the gtest suite in
// tests/ui/*.cpp which only covers the plain-C++ view-model logic.
//

#include <QtQuickTest/quicktest.h>

QUICK_TEST_MAIN(ui_qml_tests)
