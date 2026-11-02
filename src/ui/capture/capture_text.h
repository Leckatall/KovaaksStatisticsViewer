#pragma once

#include <QFont>
#include <QString>
#include <QStringList>
#include <QTextOption>
#include <QtGlobal>

namespace ksv::ui {
    [[nodiscard]] qreal lineHeight(const QFont &font);

    [[nodiscard]] qreal measuredWidth(const QFont &font, const QString &text);

    [[nodiscard]] QStringList wrappedLines(const QString &text, const QFont &font, qreal width,
                                           QTextOption::WrapMode mode);
}
