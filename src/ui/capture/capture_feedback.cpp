#include "capture/capture_feedback.h"

namespace ksv::ui {
    CaptureFeedback::CaptureFeedback(int expiryMs, QObject *parent) : QObject(parent) {
        m_expiry.setSingleShot(true);
        m_expiry.setInterval(expiryMs);
        connect(&m_expiry, &QTimer::timeout, this, &CaptureFeedback::dismiss);
    }

    CaptureFeedback::Kind CaptureFeedback::kind() const {
        return m_kind;
    }

    QString CaptureFeedback::text() const {
        return m_text;
    }

    void CaptureFeedback::showSuccess(const QString &text) {
        show(Kind::Success, text);
        m_expiry.start();
    }

    void CaptureFeedback::showNotice(const QString &text) {
        if (m_kind == Kind::Failure) return;
        show(Kind::Notice, text);
        m_expiry.start();
    }

    void CaptureFeedback::showFailure(const QString &text) {
        show(Kind::Failure, text);
    }

    void CaptureFeedback::dismiss() {
        show(Kind::None, {});
    }

    void CaptureFeedback::show(Kind kind, const QString &text) {
        m_expiry.stop();
        m_kind = kind;
        m_text = text;
        emit changed();
    }
}
