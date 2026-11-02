#include <gtest/gtest.h>

#include <QSizeF>

#include <cmath>
#include <limits>

#include "capture/capture_sizing.h"

namespace ksv::ui {
    TEST(CaptureSizing, TwoHundredPercentDoublesOneHundred) {
        for (const QSizeF logical : {QSizeF(600, 300), QSizeF(333.5, 187.25), QSizeF(1000.4, 77.5), QSizeF(1, 1)}) {
            const auto at100 = captureOutputPixels(logical, 100);
            const auto at200 = captureOutputPixels(logical, 200);
            ASSERT_TRUE(at100.has_value());
            ASSERT_TRUE(at200.has_value());
            EXPECT_EQ(at100->width(), static_cast<int>(std::lround(logical.width())));
            EXPECT_EQ(at100->height(), static_cast<int>(std::lround(logical.height())));
            EXPECT_EQ(at200->width(), static_cast<int>(std::lround(logical.width() * 2.0)));
            EXPECT_EQ(at200->height(), static_cast<int>(std::lround(logical.height() * 2.0)));
        }
        for (const QSizeF logical : {QSizeF(600, 300), QSizeF(801, 123)}) {
            const auto at100 = captureOutputPixels(logical, 100);
            const auto at200 = captureOutputPixels(logical, 200);
            ASSERT_TRUE(at100.has_value());
            ASSERT_TRUE(at200.has_value());
            EXPECT_EQ(at200->width(), 2 * at100->width());
            EXPECT_EQ(at200->height(), 2 * at100->height());
        }
    }

    TEST(CaptureSizing, ACustomEditBecomesLogicalAtTheCurrentResolution) {
        QSizeF logical(600, 300);
        logical = applyCustomDimension(logical, Qt::Horizontal, 1800, 200, false);
        EXPECT_DOUBLE_EQ(logical.width(), 900.0);
        EXPECT_DOUBLE_EQ(logical.height(), 300.0);

        logical = applyCustomDimension(logical, Qt::Vertical, 1000, 200, false);
        EXPECT_DOUBLE_EQ(logical.width(), 900.0);
        EXPECT_DOUBLE_EQ(logical.height(), 500.0);

        const auto pixels = captureOutputPixels(logical, 300);
        ASSERT_TRUE(pixels.has_value());
        EXPECT_EQ(*pixels, QSize(2700, 1500));
    }

    TEST(CaptureSizing, OverflowingProductsAreNotPixels) {
        EXPECT_FALSE(captureOutputPixels(QSizeF(2'000'000'000.0, 10), 200).has_value());
        EXPECT_FALSE(captureOutputPixels(QSizeF(std::numeric_limits<double>::infinity(), 10), 100).has_value());
        EXPECT_FALSE(captureOutputPixels(QSizeF(std::nan(""), 10), 100).has_value());
        EXPECT_FALSE(captureOutputPixels(QSizeF(0, 10), 100).has_value());
        EXPECT_FALSE(captureOutputPixels(QSizeF(-5, 10), 100).has_value());
        EXPECT_FALSE(captureOutputPixels(QSizeF(10, 10), 0).has_value());
    }

    TEST(CaptureSizing, KeepAspectScalesTheOtherEdge) {
        const QSizeF result = applyCustomDimension(QSizeF(800, 400), Qt::Horizontal, 1200, 100, true);
        EXPECT_DOUBLE_EQ(result.width(), 1200.0);
        EXPECT_DOUBLE_EQ(result.height(), 600.0);
    }
}
