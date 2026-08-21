---
type: fixed
area: User Interface
user: The Help > About dialog now shows the app name and version instead of a placeholder.
---
`AboutDialog.qml` bound `Qt.application.name`/`Qt.application.version` in place of the hardcoded
name label and the "not wired up yet" placeholder. No new C++ wiring needed: `KSV_VERSION`
(`CMakeLists.txt`) is already set on `QGuiApplication`/`QCoreApplication` in `main.cpp` before the
QML engine starts, and `Qt.application.version` reads that global directly.
