#pragma once

#include <QFont>
#include <QImage>
#include <QObject>
#include <QString>
#include <QUrl>
#include <qqmlintegration.h>

#include <memory>
#include <optional>

#include "app/contracts/i_image_output_use_case.h"
#include "capture/capture_coordinator.h"
#include "capture/capture_feedback.h"
#include "capture/capture_render.h"

namespace ksv::presentation {
    class BenchmarkTrackingViewModel;
    class CompletionHistoryViewModel;
    class GraphViewModel;
    class PlaytimeGraphViewModel;

    struct CaptureLiveViewModels {
        GraphViewModel *graph = nullptr;
        PlaytimeGraphViewModel *playtime = nullptr;
        CompletionHistoryViewModel *history = nullptr;
        BenchmarkTrackingViewModel *tracking = nullptr;
    };

    class CaptureViewModel final : public QObject {
        Q_OBJECT
        QML_ELEMENT
        QML_UNCREATABLE("Created in C++")
        Q_PROPERTY(bool previewOpen READ previewOpen NOTIFY previewOpenChanged)
        Q_PROPERTY(QString previewState READ previewState NOTIFY previewStateChanged)
        Q_PROPERTY(QString previewTargetName READ previewTargetName NOTIFY previewTargetNameChanged)
        Q_PROPERTY(QImage previewImage READ previewImage NOTIFY previewImageChanged)
        Q_PROPERTY(int outputWidth READ outputWidth NOTIFY outputWidthChanged)
        Q_PROPERTY(int outputHeight READ outputHeight NOTIFY outputHeightChanged)
        Q_PROPERTY(int percent READ percent NOTIFY percentChanged)
        Q_PROPERTY(bool includeSummary READ includeSummary NOTIFY includeSummaryChanged)
        Q_PROPERTY(bool summaryAvailable READ summaryAvailable NOTIFY summaryAvailableChanged)
        Q_PROPERTY(bool isGraph READ isGraph NOTIFY isGraphChanged)
        Q_PROPERTY(bool keepAspect READ keepAspect NOTIFY keepAspectChanged)
        Q_PROPERTY(QString widthText READ widthText NOTIFY widthTextChanged)
        Q_PROPERTY(QString heightText READ heightText NOTIFY heightTextChanged)
        Q_PROPERTY(QString widthError READ widthError NOTIFY widthErrorChanged)
        Q_PROPERTY(QString heightError READ heightError NOTIFY heightErrorChanged)
        Q_PROPERTY(QString previewError READ previewError NOTIFY previewErrorChanged)
        Q_PROPERTY(bool canOutput READ canOutput NOTIFY canOutputChanged)
        Q_PROPERTY(bool optionsLocked READ optionsLocked NOTIFY optionsLockedChanged)
        Q_PROPERTY(QString suggestedFileName READ suggestedFileName NOTIFY suggestedFileNameChanged)
        Q_PROPERTY(QString feedbackKind READ feedbackKind NOTIFY feedbackKindChanged)
        Q_PROPERTY(QString feedbackText READ feedbackText NOTIFY feedbackTextChanged)

    public:
        CaptureViewModel(std::shared_ptr<application::IImageOutputUseCase> output, CaptureLiveViewModels live,
                         ui::CaptureRenderFunction render, QObject *parent = nullptr);

        [[nodiscard]] bool previewOpen() const;
        [[nodiscard]] QString previewState() const;
        [[nodiscard]] QString previewTargetName() const;
        [[nodiscard]] QImage previewImage() const;
        [[nodiscard]] int outputWidth() const;
        [[nodiscard]] int outputHeight() const;
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
        [[nodiscard]] QString feedbackKind() const;
        [[nodiscard]] QString feedbackText() const;

        Q_INVOKABLE void useTheme(QObject *theme, const QFont &font);
        Q_INVOKABLE void quickCopy(const QString &key, QObject *canvas);
        Q_INVOKABLE void openPreview(const QString &key, QObject *canvas);
        Q_INVOKABLE void closePreview();
        Q_INVOKABLE void setPercent(int percent);
        Q_INVOKABLE void setIncludeSummary(bool include);
        Q_INVOKABLE void setKeepAspect(bool keep);
        Q_INVOKABLE void commitWidth(const QString &text);
        Q_INVOKABLE void commitHeight(const QString &text);
        Q_INVOKABLE void retryRender();
        Q_INVOKABLE void copyPreview();
        Q_INVOKABLE bool beginExport();
        Q_INVOKABLE void cancelExport();
        Q_INVOKABLE void confirmExport(const QUrl &destination);
        Q_INVOKABLE void dismissFeedback();

    signals:
        void previewOpenChanged();
        void previewStateChanged();
        void previewTargetNameChanged();
        void previewImageChanged();
        void outputWidthChanged();
        void outputHeightChanged();
        void percentChanged();
        void includeSummaryChanged();
        void summaryAvailableChanged();
        void isGraphChanged();
        void keepAspectChanged();
        void widthTextChanged();
        void heightTextChanged();
        void widthErrorChanged();
        void heightErrorChanged();
        void previewErrorChanged();
        void canOutputChanged();
        void optionsLockedChanged();
        void suggestedFileNameChanged();
        void feedbackKindChanged();
        void feedbackTextChanged();
        void previewClosed();

    private:
        struct Resolved {
            ui::CaptureTarget target;
            ui::CaptureSource source;
        };

        [[nodiscard]] std::optional<Resolved> resolve(const QString &key, QObject *canvas) const;

        std::optional<ui::CaptureAppearance> m_appearance;
        CaptureLiveViewModels m_live;
        ui::CaptureFeedback *m_feedback;
        ui::CaptureCoordinator *m_coordinator;
    };
}
