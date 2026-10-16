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

    TEST(SeriesPainterTest, CarriedVerticesExtendTheLineButAreNeverMarked) {
        QImage image(60, 30, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        SeriesPainter::Style style;
        style.showMarkers = true;
        style.markerColor = Qt::white;

        SeriesPainter::paint(painter, {{30.0, 15.0}}, {.leading = QPointF(5.0, 15.0), .trailing = QPointF(55.0, 15.0)},
                             Qt::blue, style);

        EXPECT_EQ(image.pixelColor(30, 15), QColor(Qt::white)) << "the observation keeps its marker";
        EXPECT_EQ(image.pixelColor(15, 15), QColor(Qt::blue)) << "the line runs to the leading carry";
        EXPECT_EQ(image.pixelColor(50, 15), QColor(Qt::blue)) << "the line runs to the trailing carry";
        EXPECT_NE(image.pixelColor(5, 15), QColor(Qt::white));
        EXPECT_NE(image.pixelColor(55, 15), QColor(Qt::white));
    }

    TEST(SeriesPainterTest, CarryWithoutObservationsDrawsALineAndNoMarker) {
        QImage image(60, 30, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        SeriesPainter::Style style;
        style.showMarkers = true;
        style.markerColor = Qt::white;

        SeriesPainter::paint(painter, {}, {.leading = QPointF(5.0, 15.0), .trailing = QPointF(55.0, 15.0)}, Qt::blue,
                             style);

        EXPECT_EQ(image.pixelColor(30, 15), QColor(Qt::blue));
        EXPECT_EQ(image.pixelColor(5, 12).alpha(), 0) << "no marker at the carried boundary";
    }

    // --- Connection modes, widths, clipping. ---

    namespace {
        QImage blankImage(const int width = 60, const int height = 30) {
            QImage image(width, height, QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            return image;
        }

        SeriesPainter::Style markedStyle(const SeriesPainter::Connection connection) {
            SeriesPainter::Style style;
            style.connection = connection;
            style.showMarkers = true;
            style.markerColor = Qt::white;
            return style;
        }
    }

    TEST(SeriesPainterTest, StraightConnectsConsecutiveObservationsWhereSmoothBulges) {
        // A steep drop followed by a plateau: the spline flattens into the plateau, the chord does not.
        const QVector<QPointF> points{{5.0, 28.0}, {15.0, 2.0}, {55.0, 2.0}};
        auto straight = blankImage();
        auto smooth = blankImage();
        {
            QPainter painter(&straight);
            SeriesPainter::Style style;
            style.connection = SeriesPainter::Connection::Straight;
            SeriesPainter::paint(painter, points, Qt::blue, style);
        }
        {
            QPainter painter(&smooth);
            SeriesPainter::paint(painter, points, Qt::blue);
        }

        EXPECT_GT(straight.pixelColor(10, 16).alpha(), 128) << "the steep 3px chord still covers y=16.5 at x=10.5";
        EXPECT_LT(smooth.pixelColor(10, 16).alpha(), 64) << "the monotone curve has already levelled off there";
    }

    TEST(SeriesPainterTest, StraightRunsThroughCarriedVertices) {
        auto image = blankImage();
        QPainter painter(&image);
        SeriesPainter::Style style;
        style.connection = SeriesPainter::Connection::Straight;

        SeriesPainter::paint(painter, {{30.0, 15.0}, {40.0, 5.0}}, {.leading = QPointF(5.0, 15.0)}, Qt::blue, style);

        EXPECT_GT(image.pixelColor(15, 15).alpha(), 200) << "flat carried segment into the first observation";
        EXPECT_GT(image.pixelColor(35, 10).alpha(), 128) << "straight segment between the observations";
    }

    TEST(SeriesPainterTest, PointsOnlyDrawsMarkersWithoutAnyConnectionOrCarry) {
        auto image = blankImage();
        QPainter painter(&image);
        SeriesPainter::Style style;
        style.connection = SeriesPainter::Connection::None;
        style.markerColor = Qt::white;

        SeriesPainter::paint(painter, {{10.0, 15.0}, {50.0, 15.0}},
                             {.leading = QPointF(2.0, 15.0), .trailing = QPointF(58.0, 15.0)}, Qt::blue, style);

        EXPECT_EQ(image.pixelColor(10, 15), QColor(Qt::white)) << "markers need no showMarkers in Points only";
        EXPECT_EQ(image.pixelColor(50, 15), QColor(Qt::white));
        EXPECT_EQ(image.pixelColor(30, 15).alpha(), 0) << "no data connection";
        EXPECT_EQ(image.pixelColor(3, 15).alpha(), 0) << "carried vertices are not drawn";
        EXPECT_EQ(image.pixelColor(57, 15).alpha(), 0);
    }

    TEST(SeriesPainterTest, PointsOnlyKeepsEveryMarkerOfADenseSeries) {
        auto image = blankImage(400, 30);
        QPainter painter(&image);
        SeriesPainter::Style style;
        style.connection = SeriesPainter::Connection::None;
        style.markerRadius = 0.8;
        QVector<QPointF> points;
        for (int i = 0; i < 380; ++i) points.append(QPointF(10.0 + i, 15.0));

        SeriesPainter::paint(painter, points, Qt::blue, style);

        for (const int x: {10, 11, 200, 389}) EXPECT_GT(image.pixelColor(x, 15).alpha(), 100) << "x=" << x;
    }

    TEST(SeriesPainterTest, ALoneObservationIsMarkedWithOrWithoutCarryAndMarkerPreference) {
        auto image = blankImage();
        QPainter painter(&image);
        SeriesPainter::Style style;
        style.markerColor = Qt::white;
        style.showMarkers = false;

        SeriesPainter::paint(painter, {{30.0, 15.0}}, {.leading = QPointF(5.0, 15.0), .trailing = QPointF(55.0, 15.0)},
                             Qt::blue, style);

        EXPECT_EQ(image.pixelColor(30, 15), QColor(Qt::white));
        EXPECT_EQ(image.pixelColor(15, 15), QColor(Qt::blue)) << "the carried line is still drawn";
    }

    TEST(SeriesPainterTest, ALoneObservationIsMarkedInEveryConnectionMode) {
        for (const auto mode: {SeriesPainter::Connection::Smooth, SeriesPainter::Connection::Straight,
                               SeriesPainter::Connection::None}) {
            auto image = blankImage();
            QPainter painter(&image);
            auto style = markedStyle(mode);
            style.showMarkers = false;
            SeriesPainter::paint(painter, {{30.0, 15.0}}, Qt::blue, style);
            EXPECT_EQ(image.pixelColor(30, 15), QColor(Qt::white)) << "mode " << int(mode);
        }
    }

    TEST(SeriesPainterTest, LineWidthComesFromTheStyle) {
        auto thin = blankImage();
        auto thick = blankImage();
        for (auto *image: {&thin, &thick}) {
            QPainter painter(image);
            SeriesPainter::Style style;
            style.lineWidth = image == &thick ? 10.0 : 1.0;
            SeriesPainter::paint(painter, {{5.0, 15.0}, {55.0, 15.0}}, Qt::blue, style);
        }

        EXPECT_EQ(thin.pixelColor(30, 11).alpha(), 0);
        EXPECT_EQ(thick.pixelColor(30, 11), QColor(Qt::blue));
        EXPECT_EQ(thick.pixelColor(30, 19), QColor(Qt::blue));
        EXPECT_EQ(thick.pixelColor(30, 22).alpha(), 0);
    }

    TEST(SeriesPainterTest, ClipRectKeepsLinesAndMarkersInsideIt) {
        auto image = blankImage(60, 40);
        QPainter painter(&image);
        auto style = markedStyle(SeriesPainter::Connection::Straight);
        style.clip = QRectF(0, 0, 60, 20);

        // A descent that leaves the clip at y=20, with a marker beyond it.
        SeriesPainter::paint(painter, {{5.0, 5.0}, {30.0, 35.0}, {55.0, 5.0}}, Qt::blue, style);

        EXPECT_GT(image.pixelColor(10, 11).alpha(), 128) << "inside the clip";
        EXPECT_EQ(image.pixelColor(30, 30).alpha(), 0) << "the line below the clip is cut";
        EXPECT_EQ(image.pixelColor(30, 35).alpha(), 0) << "a marker outside the clip is not drawn";
        EXPECT_EQ(image.pixelColor(5, 5), QColor(Qt::white)) << "a marker inside the clip is whole";
    }
}
