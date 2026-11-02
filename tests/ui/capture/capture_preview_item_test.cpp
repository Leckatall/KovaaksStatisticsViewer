#include <gtest/gtest.h>

#include <QImage>
#include <QSignalSpy>

#include "components/capture_preview_item.h"

namespace ksv::ui {
    namespace {
        TEST(CapturePreviewItem, TheImagePropertyHoldsWhatItWasGiven) {
            CapturePreviewItem item;
            QSignalSpy changed(&item, &CapturePreviewItem::imageChanged);
            QImage image(4, 4, QImage::Format_ARGB32);
            image.fill(Qt::red);

            item.setImage(image);
            EXPECT_EQ(item.image(), image);
            EXPECT_EQ(changed.count(), 1);

            item.setImage(QImage());
            EXPECT_TRUE(item.image().isNull());
            EXPECT_EQ(changed.count(), 2);
        }
    }
}
