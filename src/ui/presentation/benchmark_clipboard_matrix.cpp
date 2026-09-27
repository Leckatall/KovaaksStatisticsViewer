#include "benchmark_clipboard_matrix.h"

#include <algorithm>

namespace ksv::presentation {
    BenchmarkClipboardMatrix BenchmarkClipboardMatrix::parse(const QString &text) {
        BenchmarkClipboardMatrix matrix;
        if (text.isEmpty()) return matrix;

        std::vector<QString> record;
        QString field;
        qsizetype i = 0;
        const qsizetype size = text.size();
        const auto endRecord = [&] {
            record.push_back(field);
            field.clear();
            matrix.rows.push_back(std::move(record));
            record.clear();
        };
        bool atFieldStart = true;
        // True when the text ended exactly at a line break, whose empty trailing record is routine.
        bool endedOnBreak = false;
        while (i < size) {
            const QChar ch = text.at(i);
            endedOnBreak = false;
            if (atFieldStart && ch == u'"') {
                // Spreadsheets quote a cell only when it holds a delimiter or an escaped quote, and
                // copy any other cell verbatim even if it starts with '"'. A leading quote is
                // therefore quoting only when it closes at the field's end around such content;
                // otherwise it is literal text, so one stray quote never shifts or rejects the block.
                QString quoted;
                bool needsQuoting = false;
                qsizetype j = i + 1;
                bool closed = false;
                while (j < size) {
                    const QChar c = text.at(j);
                    if (c == u'"') {
                        if (j + 1 < size && text.at(j + 1) == u'"') {
                            quoted.append(u'"');
                            needsQuoting = true;
                            j += 2;
                            continue;
                        }
                        ++j;
                        closed = true;
                        break;
                    }
                    if (c == u'\t' || c == u'\r' || c == u'\n') needsQuoting = true;
                    quoted.append(c);
                    ++j;
                }
                const bool endsField = j == size || text.at(j) == u'\t' || text.at(j) == u'\r' || text.at(j) == u'\n';
                if (closed && endsField && needsQuoting) {
                    field.append(quoted);
                    i = j;
                    atFieldStart = false;
                    continue;
                }
            }
            atFieldStart = false;
            if (ch == u'\t') {
                record.push_back(field);
                field.clear();
                atFieldStart = true;
                ++i;
            } else if (ch == u'\r' || ch == u'\n') {
                i += (ch == u'\r' && i + 1 < size && text.at(i + 1) == u'\n') ? 2 : 1;
                endRecord();
                atFieldStart = true;
                endedOnBreak = true;
            } else {
                field.append(ch);
                ++i;
            }
        }
        if (!endedOnBreak) endRecord();
        return matrix;
    }

    int BenchmarkClipboardMatrix::width() const {
        std::size_t widest = 0;
        for (const auto &row: rows) widest = std::max(widest, row.size());
        return static_cast<int>(widest);
    }
}
