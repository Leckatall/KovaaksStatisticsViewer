#include <gtest/gtest.h>

#include <QColor>
#include <QImage>
#include <QPainter>

#include "components/capture_preview_item.h"

namespace ksv::ui {
    namespace {
        QImage paintedAt(CapturePreviewItem &item, int width, int height) {
            item.setSize(QSizeF(width, height));
            QImage target(width, height, QImage::Format_ARGB32_Premultiplied);
            target.fill(Qt::transparent);
            QPainter painter(&target);
            item.paint(&painter);
            return target;
        }

        TEST(CapturePreviewItem, ThePaintedImageIsScaledToTheItemBoundsNotItsOwnSize) {
            QImage source(2, 1, QImage::Format_ARGB32);
            source.setPixelColor(0, 0, Qt::red);
            source.setPixelColor(1, 0, Qt::blue);
            CapturePreviewItem item;
            item.setImage(source);

            const QImage painted = paintedAt(item, 40, 20);

            EXPECT_EQ(painted.pixelColor(2, 10), QColor(Qt::red));
            EXPECT_EQ(painted.pixelColor(37, 10), QColor(Qt::blue));
            EXPECT_EQ(painted.pixelColor(2, 18), QColor(Qt::red));
        }

        TEST(CapturePreviewItem, WithoutAnImageNothingIsPainted) {
            CapturePreviewItem item;

            const QImage painted = paintedAt(item, 10, 10);

            EXPECT_EQ(painted.pixelColor(5, 5).alpha(), 0);
        }
    }
}
