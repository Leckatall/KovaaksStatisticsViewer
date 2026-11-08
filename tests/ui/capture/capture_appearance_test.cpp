#include <gtest/gtest.h>

#include <QColor>
#include <QGuiApplication>
#include <QMetaObject>
#include <QQmlEngine>
#include <QUrl>
#include <QVariant>
#include <QtQml>

#include <cstdlib>
#include <set>
#include <type_traits>
#include <vector>

#include "capture/capture_appearance.h"
#include "capture/capture_snapshot.h"
#include "presentation/benchmarks/benchmark_format.h"

namespace ksv::ui {
    static_assert(std::is_copy_constructible_v<GraphCaptureSnapshot>);
    static_assert(std::is_copy_constructible_v<TableCaptureSnapshot>);

    namespace {
        constexpr auto kThemeUri = "KsvCaptureTest";

        // Theme.qml is a `pragma Singleton` file, so it is loaded by path rather than through the ksv_ui module.
        QObject *loadTheme(QQmlEngine &engine) {
            const QString path = QStringLiteral(KSV_SOURCE_DIR "/src/ui/qml/common/Theme.qml");
            qmlRegisterSingletonType(QUrl::fromLocalFile(path), kThemeUri, 1, 0, "Theme");
            return engine.singletonInstance<QObject *>(kThemeUri, "Theme");
        }

        QColor paletteColor(QObject &theme, const char *name) {
            auto *palette = theme.property("palette").value<QObject *>();
            return palette ? palette->property(name).value<QColor>() : QColor();
        }

        int channelDistance(const QColor &a, const QColor &b) {
            return std::max({std::abs(a.red() - b.red()), std::abs(a.green() - b.green()),
                             std::abs(a.blue() - b.blue())});
        }

        std::vector<QColor> tierAndRampColors() {
            std::vector<QColor> colors{QColor("#cd7f32"), QColor("#c0c0c8"), QColor("#ffc828"),
                                       QColor("#50dcdc"), QColor("#b9f2ff"), QColor("#a020f0")};
            for (int level = 0; level <= 255; level += 15) colors.emplace_back(level, level, level);
            return colors;
        }
    }

    TEST(CaptureAppearance, ReadsEveryTokenFromTheRealTheme) {
        QQmlEngine engine;
        QObject *theme = loadTheme(engine);
        ASSERT_NE(theme, nullptr);

        const CaptureAppearance appearance = captureAppearanceFromTheme(*theme, QGuiApplication::font());

        for (const QColor &colour: {appearance.window, appearance.base, appearance.text, appearance.button,
                                    appearance.buttonText, appearance.mid, appearance.placeholderText,
                                    appearance.highlight, appearance.attention})
            EXPECT_TRUE(colour.isValid());
        EXPECT_EQ(appearance.base, paletteColor(*theme, "base"));
        EXPECT_EQ(appearance.text, paletteColor(*theme, "text"));
        EXPECT_EQ(appearance.mid, paletteColor(*theme, "mid"));
        EXPECT_EQ(appearance.groupTintAlpha, theme->property("groupTintAlpha").toReal());
        EXPECT_FALSE(appearance.font.families.isEmpty());
        EXPECT_GT(appearance.font.pixelSize, 0);
    }

    TEST(CaptureAppearance, ColourPortsAgreeWithTheTheme) {
        QQmlEngine engine;
        QObject *theme = loadTheme(engine);
        ASSERT_NE(theme, nullptr);
        const QColor background = paletteColor(*theme, "button");
        const QColor fallback = paletteColor(*theme, "text");
        ASSERT_TRUE(background.isValid());

        for (const QColor &colour: tierAndRampColors()) {
            QVariant themeReadable;
            ASSERT_TRUE(QMetaObject::invokeMethod(theme, "readableOn", Q_RETURN_ARG(QVariant, themeReadable),
                                                  Q_ARG(QVariant, colour), Q_ARG(QVariant, background),
                                                  Q_ARG(QVariant, fallback)));
            EXPECT_LE(channelDistance(presentation::readableColorOn(colour, background, fallback),
                                      themeReadable.value<QColor>()), 1)
                << colour.name().toStdString();

            QVariant themeLight;
            ASSERT_TRUE(QMetaObject::invokeMethod(theme, "isLightFill", Q_RETURN_ARG(QVariant, themeLight),
                                                  Q_ARG(QVariant, colour)));
            EXPECT_EQ(presentation::isLightFill(colour), themeLight.toBool()) << colour.name().toStdString();
        }
    }

    TEST(CaptureSnapshot, EveryTargetHasADistinctName) {
        std::set<QString> names;
        for (const auto target: {CaptureTarget::Run, CaptureTarget::DailyPlaytime, CaptureTarget::ScenarioHistory,
                                 CaptureTarget::BenchmarkRank, CaptureTarget::BenchmarkRecentRank,
                                 CaptureTarget::BenchmarkPlaytime, CaptureTarget::BenchmarkTable}) {
            const QString name = captureTargetName(target);
            EXPECT_FALSE(name.isEmpty());
            names.insert(name);
        }
        EXPECT_EQ(names.size(), 7u);
    }
}
