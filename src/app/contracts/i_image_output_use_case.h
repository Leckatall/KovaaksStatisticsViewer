#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <string>

#include "app/contracts/image_output.h"
#include "app/contracts/raster_image.h"

namespace ksv::application {
    class IImageOutputUseCase {
    public:
        virtual ~IImageOutputUseCase() = default;

        virtual ImageOutputAdmission copyImage(RasterImage image, std::string targetLabel) = 0;
        virtual ImageOutputAdmission savePng(RasterImage image, std::filesystem::path destination,
                                             std::string targetLabel) = 0;

        virtual void onCompleted(std::function<void(const ImageOutputResult &)> handler) = 0;

        // The admitted operation that has not yet delivered its result. Admission rejects while set.
        [[nodiscard]] virtual std::optional<ImageOutputKind> activeOutput() const = 0;
    };
}
