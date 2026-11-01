#include <memory>
#include <type_traits>

#include <gtest/gtest.h>

#include "app/contracts/i_image_output_use_case.h"
#include "app/contracts/raster_image.h"
#include "app/usecases/image_output_use_case.h"
#include "fake_image_output.h"

namespace ksv::application {
    static_assert(std::is_copy_constructible_v<RasterImage>);
    static_assert(!std::is_default_constructible_v<RasterImage>);
    static_assert(!std::is_copy_constructible_v<RasterBuffer>);

    TEST(ImageOutputContract, SurfaceCompiles) {
        auto clipboard = std::make_shared<tests_support::FakeClipboardImageWriter>();
        auto png = std::make_shared<tests_support::FakePngImageWriter>();
        ImageOutputUseCase useCase(clipboard, png);

        useCase.onCompleted([](const ImageOutputResult &) {});
    }
}
