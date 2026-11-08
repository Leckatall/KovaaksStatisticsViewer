#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstdint>
#include <filesystem>
#include <optional>

#include <gtest/gtest.h>

#include <QBuffer>
#include <QByteArray>
#include <QColor>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QStringList>
#include <QTemporaryDir>

#include "app/contracts/raster_image.h"
#include "qt_data/qt_png_image_writer.h"

namespace ksv::qt_data {
    using application::ImageOutputStatus;

    namespace {
        constexpr int kWidth = 64;
        constexpr int kHeight = 32;

        std::uint32_t pixelAt(int x, int y) {
            return 0xff000000u | (static_cast<std::uint32_t>(x * 4) << 16) |
                   (static_cast<std::uint32_t>(y * 8) << 8) | static_cast<std::uint32_t>(x + y);
        }

        std::optional<application::RasterImage> distinctImage() {
            auto buffer = application::RasterBuffer::allocate(kWidth, kHeight);
            if (!buffer) return std::nullopt;
            for (int y = 0; y < kHeight; ++y)
                for (int x = 0; x < kWidth; ++x) buffer->pixels()[y * kWidth + x] = pixelAt(x, y);
            return std::move(*buffer).freeze();
        }

        std::filesystem::path inDir(const QTemporaryDir &dir, const QString &name) {
            return std::filesystem::path(dir.filePath(name).toStdWString());
        }

        void expectDecodesAsDistinctImage(const QString &path) {
            const QImage decoded(path);
            ASSERT_FALSE(decoded.isNull());
            EXPECT_EQ(decoded.width(), kWidth);
            EXPECT_EQ(decoded.height(), kHeight);
            EXPECT_FALSE(decoded.hasAlphaChannel());
            for (const auto [x, y] : {std::pair{0, 0}, {63, 0}, {0, 31}, {63, 31}, {17, 9}, {40, 22}})
                EXPECT_EQ(decoded.pixelColor(x, y), QColor::fromRgb(pixelAt(x, y))) << x << "," << y;
        }

        class HeldOpen {
        public:
            explicit HeldOpen(const QString &path)
                : m_handle(CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()), GENERIC_READ, FILE_SHARE_READ,
                                       nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr)) {}
            ~HeldOpen() { close(); }
            HeldOpen(const HeldOpen &) = delete;
            HeldOpen &operator=(const HeldOpen &) = delete;

            [[nodiscard]] bool isOpen() const { return m_handle != INVALID_HANDLE_VALUE; }
            void close() {
                if (isOpen()) CloseHandle(m_handle);
                m_handle = INVALID_HANDLE_VALUE;
            }

        private:
            HANDLE m_handle;
        };
    }

    TEST(QtPngImageWriter, WritesARealDecodablePng) {
        const QTemporaryDir dir;
        ASSERT_TRUE(dir.isValid());
        const auto image = distinctImage();
        ASSERT_TRUE(image);
        const auto destination = inDir(dir, "out.png");

        ASSERT_EQ(QtPngImageWriter().write(*image, destination), ImageOutputStatus::Succeeded);

        expectDecodesAsDistinctImage(dir.filePath("out.png"));
    }

    TEST(QtPngImageWriter, WritesNoTextMetadata) {
        const QTemporaryDir dir;
        ASSERT_TRUE(dir.isValid());
        const auto image = distinctImage();
        ASSERT_TRUE(image);

        ASSERT_EQ(QtPngImageWriter().write(*image, inDir(dir, "out.png")), ImageOutputStatus::Succeeded);

        EXPECT_TRUE(QImage(dir.filePath("out.png")).textKeys().isEmpty());
    }

    TEST(QtPngImageWriter, ReplacesAnExistingFileCompletely) {
        const QTemporaryDir dir;
        ASSERT_TRUE(dir.isValid());
        const auto image = distinctImage();
        ASSERT_TRUE(image);
        {
            QFile old(dir.filePath("out.png"));
            ASSERT_TRUE(old.open(QIODevice::WriteOnly));
            old.write(QByteArray(100000, 'x'));
        }

        ASSERT_EQ(QtPngImageWriter().write(*image, inDir(dir, "out.png")), ImageOutputStatus::Succeeded);

        expectDecodesAsDistinctImage(dir.filePath("out.png"));
    }

    TEST(QtPngImageWriter, ACommitFailureLeavesTheDestinationIntact) {
        const QTemporaryDir dir;
        ASSERT_TRUE(dir.isValid());
        const auto image = distinctImage();
        ASSERT_TRUE(image);
        const QByteArray known("known destination bytes");
        const QString path = dir.filePath("out.png");
        {
            QFile old(path);
            ASSERT_TRUE(old.open(QIODevice::WriteOnly));
            old.write(known);
        }

        // FILE_SHARE_READ without FILE_SHARE_DELETE makes replacement fail whatever share mode Qt uses.
        HeldOpen held(path);
        ASSERT_TRUE(held.isOpen());
        const auto status = QtPngImageWriter().write(*image, inDir(dir, "out.png"));
        held.close();

        EXPECT_EQ(status, ImageOutputStatus::CommitFailed);
        QFile after(path);
        ASSERT_TRUE(after.open(QIODevice::ReadOnly));
        EXPECT_EQ(after.readAll(), known);
        EXPECT_EQ(QDir(dir.path()).entryList(QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot),
                  QStringList{"out.png"});
    }

    TEST(QtPngImageWriter, AMissingDirectoryIsDestinationUnavailable) {
        const QTemporaryDir dir;
        ASSERT_TRUE(dir.isValid());
        const auto image = distinctImage();
        ASSERT_TRUE(image);

        const auto status = QtPngImageWriter().write(*image, inDir(dir, "missing/out.png"));

        EXPECT_EQ(status, ImageOutputStatus::DestinationUnavailable);
        EXPECT_TRUE(QDir(dir.path()).isEmpty());
    }

    TEST(QtPngImageWriter, EncodeReportsAnUnwritableDevice) {
        const auto image = distinctImage();
        ASSERT_TRUE(image);

        QBuffer readOnly;
        ASSERT_TRUE(readOnly.open(QIODevice::ReadOnly));
        EXPECT_FALSE(QtPngImageWriter::encode(*image, readOnly));

        QBuffer writable;
        ASSERT_TRUE(writable.open(QIODevice::WriteOnly));
        ASSERT_TRUE(QtPngImageWriter::encode(*image, writable));
        const QImage decoded = QImage::fromData(writable.data(), "PNG");
        EXPECT_EQ(decoded.size(), QSize(kWidth, kHeight));
    }
}
