#ifndef KOVAAKSSTATSVIEWER_BENCHMARK_CLIPBOARD_ADAPTER_H
#define KOVAAKSSTATSVIEWER_BENCHMARK_CLIPBOARD_ADAPTER_H

#include <QObject>
#include <QString>

namespace ksv::presentation {
    // Reads the system clipboard only when the user invokes paste; everything downstream takes
    // supplied text so tests never touch the real clipboard.
    class BenchmarkClipboardAdapter : public QObject {
        Q_OBJECT

    public:
        explicit BenchmarkClipboardAdapter(QObject *parent = nullptr) : QObject(parent) {}

        Q_INVOKABLE QString text() const;
    };
}

#endif // KOVAAKSSTATSVIEWER_BENCHMARK_CLIPBOARD_ADAPTER_H
