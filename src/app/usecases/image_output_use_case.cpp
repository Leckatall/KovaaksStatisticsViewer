#include "app/usecases/image_output_use_case.h"

#include <utility>

#include <QMetaObject>

namespace ksv::application {
    ImageOutputUseCase::ImageOutputUseCase(std::shared_ptr<data::IClipboardImageWriter> clipboard,
                                           std::shared_ptr<data::IPngImageWriter> png)
        : m_clipboard(std::move(clipboard)), m_png(std::move(png)) {
        m_savePool.setMaxThreadCount(1);
    }

    ImageOutputUseCase::~ImageOutputUseCase() {
        m_savePool.waitForDone();
    }

    ImageOutputAdmission ImageOutputUseCase::copyImage(RasterImage image, std::string targetLabel) {
        if (m_active) return {};
        m_active = ImageOutputKind::Copy;
        const std::uint64_t operationId = m_nextOperationId++;

        ImageOutputStatus status = ImageOutputStatus::Unexpected;
        try {
            status = m_clipboard->publish(image);
        } catch (...) {
        }

        postCompletion({operationId, ImageOutputKind::Copy, std::move(targetLabel), status});
        return {true, operationId};
    }

    ImageOutputAdmission ImageOutputUseCase::savePng(RasterImage image, std::filesystem::path destination,
                                                     std::string targetLabel) {
        if (m_active) return {};
        const std::uint64_t operationId = m_nextOperationId++;

        m_savePool.start([this, operationId, image = std::move(image), destination = std::move(destination),
                          targetLabel = std::move(targetLabel)]() mutable {
            ImageOutputStatus status = ImageOutputStatus::Unexpected;
            try {
                // Scoped so the pixels are freed before the result can report the save as done.
                const RasterImage pinned = std::move(image);
                status = m_png->write(pinned, destination);
            } catch (...) {
            }
            postCompletion({operationId, ImageOutputKind::SavePng, std::move(targetLabel), status});
        });
        // Set after start() so a failed start cannot leave admission closed. Delivery runs on this
        // thread, so it cannot clear the flag first.
        m_active = ImageOutputKind::SavePng;
        return {true, operationId};
    }

    void ImageOutputUseCase::onCompleted(std::function<void(const ImageOutputResult &)> handler) {
        m_handlers.push_back(std::move(handler));
    }

    std::optional<ImageOutputKind> ImageOutputUseCase::activeOutput() const {
        return m_active;
    }

    void ImageOutputUseCase::postCompletion(ImageOutputResult result) {
        QMetaObject::invokeMethod(
                &m_completionContext,
                [this, result = std::move(result)] { deliverCompletion(result); },
                Qt::QueuedConnection);
    }

    void ImageOutputUseCase::deliverCompletion(const ImageOutputResult &result) {
        m_active.reset();
        // A snapshot, because a handler may register another handler.
        const auto handlers = m_handlers;
        for (const auto &handler : handlers) handler(result);
    }
}
