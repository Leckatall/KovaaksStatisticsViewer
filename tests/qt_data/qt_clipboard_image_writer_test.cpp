#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <QImage>

#include "app/contracts/raster_image.h"
#include "qt_data/qt_clipboard_image_writer.h"

namespace ksv::qt_data {
    using application::ImageOutputStatus;

    namespace {
        struct RecordingBackend {
            bool owns = true;
            std::vector<QImage> published;
            std::vector<std::string> log;

            QtClipboardImageWriter::Backend backend() {
                return {[this](const QImage &image) {
                            published.push_back(image);
                            log.push_back("publish");
                        },
                        [this] {
                            log.push_back("ownsClipboard");
                            return owns;
                        }};
            }
        };

        std::optional<application::RasterImage> smallImage() {
            auto buffer = application::RasterBuffer::allocate(5, 3);
            if (!buffer) return std::nullopt;
            for (int i = 0; i < 15; ++i) buffer->pixels()[i] = 0xff000000u | static_cast<std::uint32_t>(i * 9);
            return std::move(*buffer).freeze();
        }
    }

    TEST(QtClipboardImageWriter, PublishesTheRenderedPixelsOpaque) {
        const auto raster = smallImage();
        ASSERT_TRUE(raster);
        RecordingBackend recording;
        QtClipboardImageWriter writer(recording.backend());

        writer.publish(*raster);

        ASSERT_EQ(recording.published.size(), 1u);
        const QImage &seen = recording.published.front();
        EXPECT_EQ(seen.format(), QImage::Format_RGB32);
        EXPECT_FALSE(seen.hasAlphaChannel());
        EXPECT_EQ(seen.width(), 5);
        EXPECT_EQ(seen.height(), 3);
        EXPECT_EQ(reinterpret_cast<const std::uint32_t *>(seen.constBits()), raster->pixels());
    }

    TEST(QtClipboardImageWriter, SucceedsOnlyAfterOwnershipIsConfirmed) {
        const auto raster = smallImage();
        ASSERT_TRUE(raster);
        RecordingBackend recording;
        QtClipboardImageWriter writer(recording.backend());

        const auto status = writer.publish(*raster);

        EXPECT_EQ(status, ImageOutputStatus::Succeeded);
        EXPECT_EQ(recording.log, (std::vector<std::string>{"publish", "ownsClipboard"}));
    }

    TEST(QtClipboardImageWriter, UnconfirmedOwnershipIsAFailureWithoutRetry) {
        const auto raster = smallImage();
        ASSERT_TRUE(raster);
        RecordingBackend recording;
        recording.owns = false;
        QtClipboardImageWriter writer(recording.backend());

        const auto status = writer.publish(*raster);

        EXPECT_EQ(recording.published.size(), 1u);
        EXPECT_EQ(recording.log, (std::vector<std::string>{"publish", "ownsClipboard"}));
        EXPECT_EQ(status, ImageOutputStatus::ClipboardNotConfirmed);
    }
}
