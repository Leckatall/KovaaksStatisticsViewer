#pragma once

#include <QJsonArray>
#include <QMutex>
#include <QString>
#include <QtLogging>
#include <deque>

namespace ksv::driver {
    // Process-wide ring buffer of Qt log messages (QML errors, binding warnings,
    // qWarning()s), so the driver can hand an agent exactly what the app logged
    // while a command ran. Installing it chains to the previous handler, so the
    // messages still reach stderr / app.log.
    class MessageLog {
    public:
        struct Entry {
            qint64 seq = 0;
            QtMsgType type = QtDebugMsg;
            QString message;
            QString file;
            int line = 0;
        };

        static MessageLog &instance();
        static void install();

        void append(QtMsgType type, const QMessageLogContext &context, const QString &message);

        // The sequence number of the newest entry (0 when empty).
        [[nodiscard]] qint64 lastSeq() const;
        // Entries newer than `afterSeq`, optionally only warnings and worse.
        [[nodiscard]] QJsonArray since(qint64 afterSeq, bool warningsOnly, int limit) const;
        void clear();

    private:
        static constexpr std::size_t kCapacity = 1000;
        mutable QMutex m_mutex;
        std::deque<Entry> m_entries;
        qint64 m_nextSeq = 1;
    };
}
