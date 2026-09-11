#pragma once

#include <QtLogging>

namespace ksv {
    inline void install_debug_message_pattern() {
        qSetMessagePattern(QStringLiteral("%{file}:%{line} %{function} - %{message}"));
    }
}
