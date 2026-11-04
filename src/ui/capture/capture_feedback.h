#pragma once

#include <QObject>
#include <QString>
#include <QTimer>

#include <optional>

namespace ksv::ui {
    // A failure stays until dismissed or replaced by another failure. A success or notice shown meanwhile
    // covers it for its usual lifetime, and then the failure shows again.
    class CaptureFeedback : public QObject {
        Q_OBJECT

    public:
        enum class Kind { None, Success, Notice, Failure };

        explicit CaptureFeedback(int expiryMs, QObject *parent = nullptr);

        [[nodiscard]] Kind kind() const;
        [[nodiscard]] QString text() const;

        void showSuccess(const QString &text);
        void showNotice(const QString &text);
        void showFailure(const QString &text);
        void dismiss();

    signals:
        void changed();

    private:
        void showTemporary(Kind kind, const QString &text);
        void endTemporary();
        void show(Kind kind, const QString &text);

        Kind m_kind = Kind::None;
        QString m_text;
        std::optional<QString> m_failure;
        QTimer m_expiry;
    };
}
