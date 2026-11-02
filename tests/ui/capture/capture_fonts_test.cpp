#include <gtest/gtest.h>

#include <QFont>
#include <QFontMetricsF>
#include <QGuiApplication>

#include <thread>

#include "capture/capture_fonts.h"
#include "capture/capture_snapshot.h"

namespace ksv::ui {
    namespace {
        CaptureFontDescription applicationDescription() {
            const QFont app = QGuiApplication::font();
            CaptureFontDescription description;
            description.families = app.families().isEmpty() ? QStringList{app.family()} : app.families();
            description.pixelSize = 14;
            return description;
        }
    }

    TEST(CaptureFonts, BuildsAndMeasuresOffTheGuiThread) {
        const CaptureFontDescription description = applicationDescription();
        const QString sample = QStringLiteral("Off-thread measurement 0123456789");
        qreal worker = 0.0;
        std::thread thread([&] {
            worker = QFontMetricsF(captureFont(description, CaptureTextRole::Body)).horizontalAdvance(sample);
        });
        thread.join();
        const qreal main = QFontMetricsF(captureFont(description, CaptureTextRole::Body)).horizontalAdvance(sample);
        EXPECT_GT(main, 0.0);
        EXPECT_DOUBLE_EQ(worker, main);
    }

    // A resolution change must not rewrap text, which holds only while measured advances scale linearly with size.
    TEST(CaptureFonts, MetricsAreLinearWhenScaled) {
        CaptureFontDescription single = applicationDescription();
        CaptureFontDescription doubled = single;
        doubled.pixelSize = single.pixelSize * 2;
        const QString sample = QStringLiteral("The quick brown fox jumps over the lazy dog 0123456789 Aim Trainer");

        const QFontMetricsF singleMetrics(captureFont(single, CaptureTextRole::Body));
        const QFontMetricsF doubledMetrics(captureFont(doubled, CaptureTextRole::Body));
        const qreal advance = singleMetrics.horizontalAdvance(sample);

        ASSERT_GT(advance, 0.0);
        EXPECT_NEAR(doubledMetrics.horizontalAdvance(sample), 2.0 * advance, 0.01 * 2.0 * advance);
    }
}
