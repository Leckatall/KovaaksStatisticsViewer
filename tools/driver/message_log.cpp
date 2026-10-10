#include "message_log.h"

#include <QJsonObject>
#include <QMutexLocker>

namespace ksv::driver {
    namespace {
        QtMessageHandler g_previousHandler = nullptr;

        void handler(const QtMsgType type, const QMessageLogContext &context, const QString &message) {
            MessageLog::instance().append(type, context, message);
            if (g_previousHandler) g_previousHandler(type, context, message);
        }

        QString typeName(const QtMsgType type) {
            switch (type) {
                case QtDebugMsg: return QStringLiteral("debug");
                case QtInfoMsg: return QStringLiteral("info");
                case QtWarningMsg: return QStringLiteral("warning");
                case QtCriticalMsg: return QStringLiteral("critical");
                case QtFatalMsg: return QStringLiteral("fatal");
            }
            return QStringLiteral("unknown");
        }
    }

    MessageLog &MessageLog::instance() {
        static MessageLog log;
        return log;
    }

    void MessageLog::install() {
        g_previousHandler = qInstallMessageHandler(handler);
    }

    void MessageLog::append(const QtMsgType type, const QMessageLogContext &context, const QString &message) {
        QMutexLocker lock(&m_mutex);
        m_entries.push_back({m_nextSeq++, type, message,
                             context.file ? QString::fromUtf8(context.file) : QString(), context.line});
        while (m_entries.size() > kCapacity) m_entries.pop_front();
    }

    qint64 MessageLog::lastSeq() const {
        QMutexLocker lock(&m_mutex);
        return m_nextSeq - 1;
    }

    QJsonArray MessageLog::since(const qint64 afterSeq, const bool warningsOnly, const int limit) const {
        QMutexLocker lock(&m_mutex);
        QJsonArray result;
        for (const auto &entry: m_entries) {
            if (entry.seq <= afterSeq) continue;
            if (warningsOnly && (entry.type == QtDebugMsg || entry.type == QtInfoMsg)) continue;
            QJsonObject object{
                {"seq", entry.seq},
                {"type", typeName(entry.type)},
                {"message", entry.message},
            };
            if (!entry.file.isEmpty()) object.insert("source", QStringLiteral("%1:%2").arg(entry.file).arg(entry.line));
            result.append(object);
        }
        // Keep the newest entries when over the limit.
        while (limit > 0 && result.size() > limit) result.removeFirst();
        return result;
    }

    void MessageLog::clear() {
        QMutexLocker lock(&m_mutex);
        m_entries.clear();
    }
}
