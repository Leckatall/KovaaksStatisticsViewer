#include "capture/capture_coordinator.h"

#include <QPointer>

#include <algorithm>
#include <utility>
#include <variant>

#include "capture/capture_feedback.h"
#include "capture/capture_sizing.h"

namespace ksv::ui {
    namespace {
        QString failureCause(application::ImageOutputStatus status) {
            using application::ImageOutputStatus;
            switch (status) {
                case ImageOutputStatus::Succeeded:
                case ImageOutputStatus::Unexpected:
                    break;
                case ImageOutputStatus::ClipboardNotConfirmed:
                    return CaptureCoordinator::tr("the clipboard could not be confirmed");
                case ImageOutputStatus::DestinationUnavailable:
                    return CaptureCoordinator::tr("the destination could not be opened");
                case ImageOutputStatus::EncodeFailed:
                    return CaptureCoordinator::tr("the image could not be encoded");
                case ImageOutputStatus::CommitFailed:
                    return CaptureCoordinator::tr("the file could not be replaced");
            }
            return CaptureCoordinator::tr("an unexpected error occurred");
        }

        std::optional<int> parsePixels(const QString &text) {
            if (!std::all_of(text.begin(), text.end(), [](QChar c) { return c.isDigit(); })) return std::nullopt;
            bool ok = false;
            const int pixels = text.toInt(&ok);
            if (!ok || pixels <= 0) return std::nullopt;
            return pixels;
        }

        QString malformedDimensionText() {
            return CaptureCoordinator::tr("Enter a positive whole number of pixels");
        }
    }

    CaptureCoordinator::CaptureCoordinator(std::shared_ptr<application::IImageOutputUseCase> output,
                                           CaptureRenderFunction render, CaptureFeedback *feedback, QObject *parent)
        : QObject(parent), m_output(std::move(output)), m_feedback(feedback), m_queue(std::move(render)) {
        // The use case offers no removal and can outlive this coordinator.
        m_output->onCompleted([self = QPointer(this)](const application::ImageOutputResult &result) {
            if (self) self->handleOutputCompleted(result);
        });
    }

    void CaptureCoordinator::quickCopy(CaptureTarget target, const CaptureSource &source) {
        if (m_preparing == target) return;
        if (m_preparing || m_session || m_output->activeOutput()) {
            showBusyNotice();
            return;
        }

        std::optional<CaptureContent> content = source();
        if (!content) {
            showNoContent(target);
            return;
        }

        m_preparing = target;
        CaptureRenderRequest request;
        request.content = std::make_shared<const CaptureContent>(std::move(*content));
        m_queue.submit(std::move(request), this, [this, target](CaptureRenderRequest, CaptureRenderOutcome outcome) {
            auto *rendered = std::get_if<CaptureRendered>(&outcome);
            if (!rendered) {
                m_preparing.reset();
                const auto *rejected = std::get_if<CaptureRejected>(&outcome);
                m_feedback->showFailure(tr("Could not copy %1: %2")
                                                .arg(captureTargetName(target),
                                                     rejected ? captureRejectionText(rejected->rejection)
                                                              : tr("the image could not be rendered")));
                return;
            }
            const application::ImageOutputAdmission admission =
                    m_output->copyImage(std::move(rendered->raster), captureTargetName(target).toStdString());
            if (admission.accepted) {
                m_admittedOperation = admission.operationId;
            } else {
                m_preparing.reset();
                showBusyNotice();
            }
        });
    }

    void CaptureCoordinator::showBusyNotice() {
        const std::optional<application::ImageOutputKind> active = m_output->activeOutput();
        if (active == application::ImageOutputKind::SavePng) {
            m_feedback->showNotice(tr("Another image is still saving"));
        } else if (m_session && !active) {
            m_feedback->showNotice(tr("Close the image preview first"));
        } else {
            m_feedback->showNotice(tr("Another image is still being copied"));
        }
    }

    void CaptureCoordinator::showNoContent(CaptureTarget target) {
        m_feedback->showFailure(tr("%1 has nothing to capture").arg(captureTargetName(target)));
    }

    void CaptureCoordinator::openPreview(CaptureTarget target, const CaptureSource &source) {
        if (m_preparing) {
            showBusyNotice();
            return;
        }

        std::optional<CaptureContent> content = source();
        if (!content) {
            showNoContent(target);
            return;
        }

        PreviewSession session;
        session.id = ++m_lastSessionId;
        session.target = target;
        session.content = std::make_shared<const CaptureContent>(std::move(*content));
        m_session = std::move(session);
        submitPreviewRender();
        emit previewChanged();
    }

    void CaptureCoordinator::submitPreviewRender() {
        m_queue.submit({m_session->id, m_session->generation, m_session->content, m_session->options}, this,
                       [this](CaptureRenderRequest request, CaptureRenderOutcome outcome) {
                           installPreviewOutcome(request, std::move(outcome));
                       });
    }

    void CaptureCoordinator::installPreviewOutcome(const CaptureRenderRequest &request, CaptureRenderOutcome outcome) {
        if (!m_session || request.session != m_session->id || request.generation != m_session->generation) return;

        if (auto *rendered = std::get_if<CaptureRendered>(&outcome)) {
            if (!m_session->reference) m_session->reference = rendered->logical;
            m_session->rendered = std::move(*rendered);
            m_session->state = PreviewState::Ready;
        } else if (const auto *rejected = std::get_if<CaptureRejected>(&outcome)) {
            if (!m_session->reference && !rejected->logical.isEmpty()) m_session->reference = rejected->logical;
            m_session->rendered.reset();
            m_session->state = PreviewState::Invalid;
            m_session->renderError = captureRejectionText(rejected->rejection);
            if (request.options.logical) m_session->rejectedField = m_session->lastCommitted;
        } else {
            m_session->rendered.reset();
            m_session->state = PreviewState::Failed;
            m_session->renderError = tr("The image could not be rendered");
        }
        emit previewChanged();
    }

    void CaptureCoordinator::discardRender() {
        ++m_session->generation;
        m_session->renderError.clear();
        m_session->rejectedField.reset();
    }

    void CaptureCoordinator::closePreview() {
        if (!m_session) return;
        m_session.reset();
        emit previewClosed();
        emit previewChanged();
    }

    void CaptureCoordinator::startNewGeneration() {
        discardRender();
        m_session->state = PreviewState::Rendering;
        submitPreviewRender();
        emit previewChanged();
    }

    void CaptureCoordinator::renderCurrentChoices() {
        if (m_session->malformedWidth || m_session->malformedHeight) {
            emit previewChanged();
        } else {
            startNewGeneration();
        }
    }

    bool CaptureCoordinator::optionsEditable() const {
        return m_session && !m_session->pinned;
    }

    void CaptureCoordinator::setPercent(int percent) {
        if (!optionsEditable()) return;
        m_session->options.percent = percent;
        renderCurrentChoices();
    }

    void CaptureCoordinator::setIncludeSummary(bool include) {
        if (!optionsEditable()) return;
        m_session->options.includeSummary = include;
        renderCurrentChoices();
    }

    void CaptureCoordinator::setKeepAspect(bool keep) {
        if (!optionsEditable()) return;
        m_session->keepAspect = keep;
        emit previewChanged();
    }

    void CaptureCoordinator::commitDimension(Qt::Orientation orientation, const QString &text) {
        if (!optionsEditable() || !isGraph() || !m_session->reference) return;

        const std::optional<int> pixels = parsePixels(text);
        if (!pixels) {
            m_session->malformed(orientation) = text;
            discardRender();
            m_session->rendered.reset();
            m_session->state = PreviewState::Invalid;
            emit previewChanged();
            return;
        }

        m_session->malformed(orientation).reset();
        m_session->lastCommitted = orientation;
        m_session->options.logical = applyCustomDimension(*m_session->logical(), orientation, *pixels,
                                                          m_session->options.percent, m_session->keepAspect);
        renderCurrentChoices();
    }

    void CaptureCoordinator::retryRender() {
        if (optionsEditable()) renderCurrentChoices();
    }

    void CaptureCoordinator::copyPreview() {
        if (!canOutput()) return;
        const application::ImageOutputAdmission admission = m_output->copyImage(
                m_session->rendered->raster, captureTargetName(m_session->target).toStdString());
        if (admission.accepted) {
            m_admittedOperation = admission.operationId;
        } else {
            showBusyNotice();
        }
    }

    bool CaptureCoordinator::beginExport() {
        if (!canOutput()) return false;
        if (m_output->activeOutput()) {
            showBusyNotice();
            return false;
        }
        m_session->pinned = m_session->rendered->raster;
        emit previewChanged();
        return true;
    }

    void CaptureCoordinator::cancelExport() {
        if (!optionsLocked()) return;
        m_session->pinned.reset();
        emit previewChanged();
    }

    void CaptureCoordinator::confirmExport(std::filesystem::path destination) {
        if (!optionsLocked()) return;
        const QString target = captureTargetName(m_session->target);
        const application::ImageOutputAdmission admission =
                m_output->savePng(std::move(*m_session->pinned), std::move(destination), target.toStdString());
        closePreview();
        if (admission.accepted) {
            m_admittedOperation = admission.operationId;
        } else {
            m_feedback->showFailure(tr("Could not export %1: another image is still in progress").arg(target));
        }
    }

    bool CaptureCoordinator::previewOpen() const {
        return m_session.has_value();
    }

    CaptureCoordinator::PreviewState CaptureCoordinator::previewState() const {
        return m_session ? m_session->state : PreviewState::Rendering;
    }

    QString CaptureCoordinator::previewTargetName() const {
        return m_session ? captureTargetName(m_session->target) : QString();
    }

    QImage CaptureCoordinator::previewImage() const {
        return m_session && m_session->rendered ? m_session->rendered->thumbnail : QImage();
    }

    QSize CaptureCoordinator::outputSize() const {
        return m_session && m_session->rendered ? m_session->rendered->pixels : QSize();
    }

    int CaptureCoordinator::percent() const {
        return m_session ? m_session->options.percent : CaptureOptions{}.percent;
    }

    bool CaptureCoordinator::includeSummary() const {
        return m_session ? m_session->options.includeSummary : CaptureOptions{}.includeSummary;
    }

    bool CaptureCoordinator::summaryAvailable() const {
        if (!m_session) return false;
        const auto *table = std::get_if<TableCaptureSnapshot>(m_session->content.get());
        return table && table->summary.has_value();
    }

    bool CaptureCoordinator::isGraph() const {
        return m_session && std::holds_alternative<GraphCaptureSnapshot>(*m_session->content);
    }

    bool CaptureCoordinator::keepAspect() const {
        return !m_session || m_session->keepAspect;
    }

    QString CaptureCoordinator::dimensionText(Qt::Orientation orientation) const {
        if (!m_session) return {};
        const bool horizontal = orientation == Qt::Horizontal;
        const std::optional<QString> &malformed = horizontal ? m_session->malformedWidth : m_session->malformedHeight;
        if (malformed) return *malformed;

        const std::optional<QSizeF> logical = m_session->logical();
        if (!logical) return {};
        const std::optional<QSize> pixels = captureOutputPixels(*logical, m_session->options.percent);
        if (!pixels) return {};
        return QString::number(horizontal ? pixels->width() : pixels->height());
    }

    QString CaptureCoordinator::widthText() const {
        return dimensionText(Qt::Horizontal);
    }

    QString CaptureCoordinator::heightText() const {
        return dimensionText(Qt::Vertical);
    }

    QString CaptureCoordinator::dimensionError(Qt::Orientation orientation) const {
        if (!m_session) return {};
        const std::optional<QString> &malformed =
                orientation == Qt::Horizontal ? m_session->malformedWidth : m_session->malformedHeight;
        if (malformed) return malformedDimensionText();
        return m_session->rejectedField == orientation ? m_session->renderError : QString();
    }

    QString CaptureCoordinator::widthError() const {
        return dimensionError(Qt::Horizontal);
    }

    QString CaptureCoordinator::heightError() const {
        return dimensionError(Qt::Vertical);
    }

    QString CaptureCoordinator::previewError() const {
        if (!m_session) return {};
        if (m_session->malformedWidth || m_session->malformedHeight) return malformedDimensionText();
        return m_session->renderError;
    }

    bool CaptureCoordinator::canOutput() const {
        return m_session && m_session->state == PreviewState::Ready;
    }

    bool CaptureCoordinator::optionsLocked() const {
        return m_session && m_session->pinned.has_value();
    }

    QString CaptureCoordinator::suggestedFileName() const {
        return {};
    }

    void CaptureCoordinator::handleOutputCompleted(const application::ImageOutputResult &result) {
        if (result.operationId != m_admittedOperation) return;
        m_admittedOperation.reset();
        m_preparing.reset();
        const QString target = QString::fromStdString(result.targetLabel);
        const bool copy = result.kind == application::ImageOutputKind::Copy;
        if (result.status == application::ImageOutputStatus::Succeeded) {
            m_feedback->showSuccess((copy ? tr("%1 copied to the clipboard") : tr("%1 exported as PNG")).arg(target));
        } else {
            m_feedback->showFailure((copy ? tr("Could not copy %1: %2") : tr("Could not export %1: %2"))
                                            .arg(target, failureCause(result.status)));
        }
    }
}
