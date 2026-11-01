#pragma once

#include <filesystem>
#include <functional>
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
    };
}
