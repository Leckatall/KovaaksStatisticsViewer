//
// AxisPainter::measureLabelExtent tests. Lives here rather than tests/ui because
// QFontMetricsF needs a QGuiApplication (font database), which only this suite's
// main provides; tests/ui's main is a bare QCoreApplication.
//

#include <gtest/gtest.h>

#include <QFont>
#include <QFontMetricsF>

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

    // --- Label layout: never clipped at the canvas edges, never overlapping a neighbour. ---

    QString longDateLabel(const qreal v) { return QStringLiteral("Oct %1 00:00").arg(int(v)); }

    qreal textWidth(const QString &text) {
        QFont font;
        font.setPointSize(AxisPainter::Style{}.fontPointSize);
        return QFontMetricsF(font).horizontalAdvance(text);
    }

    TEST(AxisPainterLayoutTest, HorizontalLabelsAreFullyInsideTheCanvas) {
        // Plot nearly fills the canvas, so centring the first/last label on its tick would cross
        // the canvas edge (the old fixed 40px label box cut "Oct 9 00:00" to "ct 9 00:0").
        const QRectF bounds(0, 0, 600, 200);
        const QRectF plot(8, 10, 584, 170);
        const QList<qreal> ticks{9, 10, 11};

        const auto labels = AxisPainter::layoutLabels(plot, bounds, AxisPainter::Orientation::Horizontal,
                                                      9, 11, ticks, longDateLabel, AxisPainter::Style{});

        ASSERT_FALSE(labels.isEmpty());
        for (const auto &label: labels) {
            EXPECT_GE(label.rect.left(), bounds.left()) << label.text.toStdString();
            EXPECT_LE(label.rect.right(), bounds.right()) << label.text.toStdString();
            EXPECT_GE(label.rect.width(), textWidth(label.text)) << label.text.toStdString() << " is clipped";
        }
        EXPECT_EQ(labels.front().tickIndex, 0);
        EXPECT_EQ(labels.back().tickIndex, 2);
    }

    TEST(AxisPainterLayoutTest, CrowdedHorizontalLabelsAreThinnedNotOverlapped) {
        const QRectF bounds(0, 0, 300, 200);
        const QRectF plot(30, 10, 250, 170);
        QList<qreal> ticks;
        for (int day = 1; day <= 12; ++day) ticks.append(day);

        const auto labels = AxisPainter::layoutLabels(plot, bounds, AxisPainter::Orientation::Horizontal,
                                                      1, 12, ticks, longDateLabel, AxisPainter::Style{});

        ASSERT_GE(labels.size(), 2);
        EXPECT_LT(labels.size(), ticks.size()) << "twelve long labels cannot fit in 250px";
        for (int i = 1; i < labels.size(); ++i) {
            EXPECT_GT(labels[i].rect.left(), labels[i - 1].rect.right())
                << labels[i - 1].text.toStdString() << " overlaps " << labels[i].text.toStdString();
            EXPECT_NE(labels[i].text, labels[i - 1].text);
        }
        EXPECT_EQ(labels.front().tickIndex, 0);
    }

    TEST(AxisPainterLayoutTest, ShortVerticalAxisThinsItsLabels) {
        // A 1000x640 window leaves the per-run graph about 100px tall for ~10 Y ticks.
        const QRectF bounds(0, 0, 400, 130);
        const QRectF plot(40, 10, 340, 100);
        QList<qreal> ticks;
        for (int v = 60; v <= 105; v += 5) ticks.append(v);

        const auto labels = AxisPainter::layoutLabels(plot, bounds, AxisPainter::Orientation::Vertical,
                                                      60, 105, ticks, formatFixed, AxisPainter::Style{});

        ASSERT_GE(labels.size(), 2);
        for (int i = 1; i < labels.size(); ++i) {
            EXPECT_FALSE(labels[i].rect.intersects(labels[i - 1].rect))
                << labels[i - 1].text.toStdString() << " overlaps " << labels[i].text.toStdString();
        }
        for (const auto &label: labels) {
            EXPECT_GE(label.rect.top(), bounds.top());
            EXPECT_LE(label.rect.bottom(), bounds.bottom());
        }
    }

    TEST(AxisPainterLayoutTest, RoomyAxisLabelsEveryTick) {
        const QRectF bounds(0, 0, 800, 400);
        const QRectF plot(40, 10, 740, 370);
        const QList<qreal> ticks{0, 10, 20, 30, 40, 50, 60};

        EXPECT_EQ(AxisPainter::layoutLabels(plot, bounds, AxisPainter::Orientation::Horizontal, 0, 60, ticks,
                                            formatFixed, AxisPainter::Style{}).size(), ticks.size());
        EXPECT_EQ(AxisPainter::layoutLabels(plot, bounds, AxisPainter::Orientation::Vertical, 0, 60, ticks,
                                            formatFixed, AxisPainter::Style{}).size(), ticks.size());
    }

    TEST(AxisPainterLayoutTest, SingleTickIsLabelled) {
        const QRectF bounds(0, 0, 200, 100);
        const QRectF plot(30, 10, 150, 70);
        const auto labels = AxisPainter::layoutLabels(plot, bounds, AxisPainter::Orientation::Horizontal, 5, 5,
                                                      {5.0}, formatFixed, AxisPainter::Style{});
        ASSERT_EQ(labels.size(), 1);
        EXPECT_EQ(labels.front().text, "5");
    }
}
