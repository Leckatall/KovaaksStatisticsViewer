#include <gtest/gtest.h>

#include <QImage>
#include <QPainter>

#include "components/series_painter.h"

namespace ksv::ui {
    TEST(SeriesPainterTest, DefaultStyleDoesNotDrawControlPointMarkers) {
        QImage image(30, 30, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);

        SeriesPainter::paint(painter, {{5.0, 15.0}, {25.0, 15.0}}, Qt::blue);

        EXPECT_EQ(image.pixelColor(5, 15), QColor(Qt::blue));
    }

    TEST(SeriesPainterTest, MarkerStyleDrawsControlPointMarkers) {
        QImage image(30, 30, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        SeriesPainter::Style style;
        style.showMarkers = true;
        style.markerColor = Qt::white;

        SeriesPainter::paint(painter, {{5.0, 15.0}, {25.0, 15.0}}, Qt::blue, style);

        EXPECT_EQ(image.pixelColor(5, 15), QColor(Qt::white));
    }

    TEST(SeriesPainterTest, MarkersDefaultToTheLineColourAndStandOffTheLine) {
        QImage image(40, 40, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        SeriesPainter::Style style;
        style.showMarkers = true;

        // A flat line at y=20; the marker is a disc around (5, 20) that reaches above the 3px line.
        SeriesPainter::paint(painter, {{5.0, 20.0}, {35.0, 20.0}}, Qt::blue, style);

        EXPECT_EQ(image.pixelColor(5, 20), QColor(Qt::blue));
        EXPECT_EQ(image.pixelColor(5, 17), QColor(Qt::blue)) << "no marker above the line";
        EXPECT_EQ(image.pixelColor(20, 17).alpha(), 0) << "the line itself is thinner than the marker";
    }

    TEST(SeriesPainterTest, ASinglePointIsDrawnAsAMarker) {
        QImage image(30, 30, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);

        // A lone sample has no curve; without a marker it would not be drawn at all.
        SeriesPainter::paint(painter, {{15.0, 15.0}}, Qt::blue);

        EXPECT_EQ(image.pixelColor(15, 15), QColor(Qt::blue));
    }
}
