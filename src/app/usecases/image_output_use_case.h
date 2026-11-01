#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

#include <QObject>
#include <QThreadPool>

#include "app/contracts/i_image_output_use_case.h"
#include "data/interfaces/i_clipboard_image_writer.h"
#include "data/interfaces/i_png_image_writer.h"

namespace ksv::application {
    class ImageOutputUseCase final : public IImageOutputUseCase {
    public:
        ImageOutputUseCase(std::shared_ptr<data::IClipboardImageWriter> clipboard,
                           std::shared_ptr<data::IPngImageWriter> png);
        ~ImageOutputUseCase() override;

        ImageOutputAdmission copyImage(RasterImage image, std::string targetLabel) override;
        ImageOutputAdmission savePng(RasterImage image, std::filesystem::path destination,
                                     std::string targetLabel) override;
        void onCompleted(std::function<void(const ImageOutputResult &)> handler) override;

    private:
        void postCompletion(ImageOutputResult result);
        void deliverCompletion(const ImageOutputResult &result);

        std::shared_ptr<data::IClipboardImageWriter> m_clipboard;
        std::shared_ptr<data::IPngImageWriter> m_png;
        std::vector<std::function<void(const ImageOutputResult &)>> m_handlers;
        bool m_busy = false;
        std::uint64_t m_nextOperationId = 1;
        // Completions are posted to this object, so one still queued dies with the use case unreported.
        QObject m_completionContext;
        // Last member, and drained in the destructor body: a running task uses m_png and the context.
        QThreadPool m_savePool;
    };
}
