#ifndef KOVAAKSSTATSVIEWER_BENCHMARK_CLIPBOARD_MATRIX_H
#define KOVAAKSSTATSVIEWER_BENCHMARK_CLIPBOARD_MATRIX_H

#include <QString>
#include <vector>

namespace ksv::presentation {
    // Position-preserving decode of tab/newline clipboard text. Rows may be ragged: a row holds
    // exactly the fields supplied, including trailing empty ones, and is never padded.
    struct BenchmarkClipboardMatrix {
        std::vector<std::vector<QString>> rows;

        static BenchmarkClipboardMatrix parse(const QString &text);
        [[nodiscard]] int width() const;
    };
}

#endif // KOVAAKSSTATSVIEWER_BENCHMARK_CLIPBOARD_MATRIX_H
