#include "capture/capture_text.h"

#include <QFontMetricsF>
#include <QTextLayout>

#include <cmath>

namespace ksv::ui {
    qreal lineHeight(const QFont &font) {
        return QFontMetricsF(font).height();
    }

    qreal measuredWidth(const QFont &font, const QString &text) {
        return std::ceil(QFontMetricsF(font).horizontalAdvance(text));
    }

    QStringList wrappedLines(const QString &text, const QFont &font, const qreal width,
                             const QTextOption::WrapMode mode) {
        // QTextLayout yields one blank line for empty text, which would reserve room for nothing.
        if (text.isEmpty()) return {};

        QTextOption option;
        option.setWrapMode(mode);
        QTextLayout layout(text, font);
        layout.setTextOption(option);

        QStringList lines;
        layout.beginLayout();
        for (QTextLine line = layout.createLine(); line.isValid(); line = layout.createLine()) {
            line.setLineWidth(width);
            lines.append(text.mid(line.textStart(), line.textLength()).trimmed());
        }
        layout.endLayout();
        return lines;
    }
}
