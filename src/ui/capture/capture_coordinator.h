#pragma once

#include <QImage>
#include <QObject>
#include <QSize>
#include <QSizeF>
#include <QString>

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>

#include "app/contracts/i_image_output_use_case.h"
#include "capture/capture_render.h"
#include "capture/capture_render_queue.h"
#include "capture/capture_snapshot.h"

namespace ksv::ui {
    class CaptureFeedback;

    // Called only after the action has been admitted.
    using CaptureSource = std::function<std::optional<CaptureContent>()>;

    class CaptureCoordinator : public QObject {
        Q_OBJECT

    public:
        enum class PreviewState { Rendering, Ready, Invalid, Failed };

        CaptureCoordinator(std::shared_ptr<application::IImageOutputUseCase> output, CaptureRenderFunction render,
                           CaptureFeedback *feedback, QObject *parent = nullptr);

        void quickCopy(CaptureTarget target, const CaptureSource &source);
        void openPreview(CaptureTarget target, const CaptureSource &source);
        void closePreview();

        void setPercent(int percent);
        void setIncludeSummary(bool include);
        void setKeepAspect(bool keep);
        void commitDimension(Qt::Orientation orientation, const QString &text);
        void retryRender();

        void copyPreview();
        bool beginExport();
        void cancelExport();
        void confirmExport(std::filesystem::path destination);

        [[nodiscard]] bool previewOpen() const;
        [[nodiscard]] PreviewState previewState() const;
        [[nodiscard]] QString previewTargetName() const;
        [[nodiscard]] QImage previewImage() const;
        [[nodiscard]] QSize outputSize() const;
        [[nodiscard]] bool hasReference() const;
        [[nodiscard]] int percent() const;
        [[nodiscard]] bool includeSummary() const;
        [[nodiscard]] bool summaryAvailable() const;
        [[nodiscard]] bool isGraph() const;
        [[nodiscard]] bool keepAspect() const;
        [[nodiscard]] QString widthText() const;
        [[nodiscard]] QString heightText() const;
        [[nodiscard]] QString widthError() const;
        [[nodiscard]] QString heightError() const;
        [[nodiscard]] QString previewError() const;
        [[nodiscard]] bool canOutput() const;
        [[nodiscard]] bool optionsLocked() const;
        [[nodiscard]] QString suggestedFileName() const;

    signals:
        void previewChanged();
        void previewClosed();

    private:
        struct PreviewSession {
            quint64 id = 0;
            CaptureTarget target = CaptureTarget::Run;
            std::shared_ptr<const CaptureContent> content;
            CaptureOptions options;
            quint64 generation = 1;
            PreviewState state = PreviewState::Rendering;
            std::optional<CaptureRendered> rendered;
            // Reported by the first outcome, because measuring it needs the worker.
            std::optional<QSizeF> reference;
            bool keepAspect = true;
            // The entered text, while it is not a usable size. No render runs while either is set.
            std::optional<QString> malformedWidth;
            std::optional<QString> malformedHeight;
            QString renderError;
            std::optional<Qt::Orientation> lastCommitted;
            // Unlike a malformed field, this does not block rendering: a lower resolution can cure it.
            std::optional<Qt::Orientation> rejectedField;
            std::optional<application::RasterImage> pinned;

            [[nodiscard]] std::optional<QSizeF> logical() const {
                return options.logical ? options.logical : reference;
            }

            [[nodiscard]] std::optional<QString> &malformed(Qt::Orientation orientation) {
                return orientation == Qt::Horizontal ? malformedWidth : malformedHeight;
            }
        };

        void handleOutputCompleted(const application::ImageOutputResult &result);
        void showBusyNotice();
        void showNoContent(CaptureTarget target);
        void submitPreviewRender();
        void startNewGeneration();
        void renderCurrentChoices();
        void discardRender();
        [[nodiscard]] bool optionsEditable() const;
        [[nodiscard]] QString dimensionText(Qt::Orientation orientation) const;
        [[nodiscard]] QString dimensionError(Qt::Orientation orientation) const;
        void installPreviewOutcome(const CaptureRenderRequest &request, CaptureRenderOutcome outcome);

        std::shared_ptr<application::IImageOutputUseCase> m_output;
        CaptureFeedback *m_feedback;
        CaptureRenderQueue m_queue;
        // Set from admission until the copy's result, so a repeat in the gap after publishing stays silent.
        std::optional<CaptureTarget> m_preparing;
        // Held here and not on a session, because a save outlives its session.
        std::optional<std::uint64_t> m_admittedOperation;
        std::optional<PreviewSession> m_session;
        quint64 m_lastSessionId = 0;
    };
}
