#include "benchmark_clipboard_adapter.h"

#include <QClipboard>
#include <QGuiApplication>

namespace ksv::presentation {
    QString BenchmarkClipboardAdapter::text() const {
        const auto *clipboard = QGuiApplication::clipboard();
        return clipboard ? clipboard->text() : QString();
    }
}
