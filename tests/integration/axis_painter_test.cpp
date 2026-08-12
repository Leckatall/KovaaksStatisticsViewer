//
// AxisPainter::measureLabelExtent tests. Lives here rather than tests/ui because
// QFontMetricsF needs a QGuiApplication (font database), which only this suite's
// main provides; tests/ui's main is a bare QCoreApplication.
//

#include <gtest/gtest.h>

#include "axis_painter.h"

using ksv::ui::AxisPainter;

namespace {
    QString formatFixed(const qreal v) { return QString::number(v, 'f', 0); }

    TEST(AxisPainterTest, VerticalExtentGrowsWithWiderLabelText) {
        const qreal narrow = AxisPainter::measureLabelExtent(
            AxisPainter::Orientation::Vertical, {0.0, 1.0}, formatFixed);
        const qreal wide = AxisPainter::measureLabelExtent(
            AxisPainter::Orientation::Vertical, {0.0, 123456.0}, formatFixed);

        EXPECT_GT(wide, narrow);
    }

    TEST(AxisPainterTest, VerticalExtentIsWidestLabelNotLastLabel) {
        const qreal extent = AxisPainter::measureLabelExtent(
            AxisPainter::Orientation::Vertical, {123456.0, 1.0}, formatFixed);
        const qreal wideAlone = AxisPainter::measureLabelExtent(
            AxisPainter::Orientation::Vertical, {123456.0}, formatFixed);

        EXPECT_DOUBLE_EQ(extent, wideAlone);
    }

    TEST(AxisPainterTest, HorizontalExtentIgnoresLabelTextWidth) {
        const qreal shortLabel = AxisPainter::measureLabelExtent(
            AxisPainter::Orientation::Horizontal, {0.0}, formatFixed);
        const qreal longLabel = AxisPainter::measureLabelExtent(
            AxisPainter::Orientation::Horizontal, {123456.0}, formatFixed);

        EXPECT_DOUBLE_EQ(shortLabel, longLabel);
    }

    TEST(AxisPainterTest, EmptyTicksGiveZeroVerticalExtent) {
        EXPECT_DOUBLE_EQ(
            AxisPainter::measureLabelExtent(AxisPainter::Orientation::Vertical, {}, formatFixed), 0.0);
    }
}
