#include "case_fold.h"

#include <QString>

namespace ksv::qt_data {
    std::string foldCase(std::string_view utf8) {
        return QString::fromUtf8(utf8.data(), static_cast<qsizetype>(utf8.size())).toCaseFolded().toStdString();
    }
}
