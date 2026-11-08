#include "capture/capture_feedback.h"

namespace ksv::ui {
    CaptureFeedback::CaptureFeedback(int expiryMs, QObject *parent) : QObject(parent) {
        m_expiry.setSingleShot(true);
        m_expiry.setInterval(expiryMs);
        connect(&m_expiry, &QTimer::timeout, this, &CaptureFeedback::endTemporary);
    }

    CaptureFeedback::Kind CaptureFeedback::kind() const {
        return m_kind;
    }

    QString CaptureFeedback::text() const {
        return m_text;
    }

    void CaptureFeedback::showSuccess(const QString &text) {
        showTemporary(Kind::Success, text);
    }

    void CaptureFeedback::showNotice(const QString &text) {
        showTemporary(Kind::Notice, text);
    }

    void CaptureFeedback::showFailure(const QString &text) {
        m_failure = text;
        show(Kind::Failure, text);
    }

    void CaptureFeedback::dismiss() {
        if (m_kind == Kind::Failure) m_failure.reset();
        endTemporary();
    }

    void CaptureFeedback::showTemporary(Kind kind, const QString &text) {
        show(kind, text);
        m_expiry.start();
    }

    void CaptureFeedback::endTemporary() {
        if (m_failure) {
            show(Kind::Failure, *m_failure);
        } else {
            show(Kind::None, {});
        }
    }

    void CaptureFeedback::show(Kind kind, const QString &text) {
        m_expiry.stop();
        m_kind = kind;
        m_text = text;
        emit changed();
    }
}
