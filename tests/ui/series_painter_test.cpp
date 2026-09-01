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

        SeriesPainter::paint(painter, {{5.0, 15.0}, {25.0, 15.0}}, Qt::blue, style);

        EXPECT_EQ(image.pixelColor(5, 15), QColor(Qt::white));
    }
}
