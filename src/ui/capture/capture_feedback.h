#pragma once

#include <QObject>
#include <QString>
#include <QTimer>

namespace ksv::ui {
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
        void show(Kind kind, const QString &text);

        Kind m_kind = Kind::None;
        QString m_text;
        QTimer m_expiry;
    };
}
