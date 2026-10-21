#include <gtest/gtest.h>

#include <chrono>
#include <vector>

#include <QTest>

#include "app/contracts/range_projection.h"
#include "system_day_source.h"

using namespace ksv::qt_data;
using namespace std::chrono;

namespace {
    constexpr sys_days kStart = sys_days{year{2026} / October / 16};

    struct Harness {
        sys_days now = kStart;
        SystemDaySource source{[this] { return now; }, milliseconds{5}};
        std::vector<sys_days> announced;

        Harness() {
            source.onDayChanged([this](const sys_days day) { announced.push_back(day); });
        }
    };

    TEST(SystemDaySource, RefreshAnnouncesAChangeOnceAndOnlyOnce) {
        Harness harness;
        harness.now = kStart + days{1};
        harness.source.refresh();
        harness.source.refresh();
        EXPECT_EQ(harness.source.today(), kStart + days{1});
        EXPECT_EQ(harness.announced, std::vector{kStart + days{1}});
    }

    TEST(SystemDaySource, UnchangedDayAnnouncesNothing) {
        Harness harness;
        harness.source.refresh();
        EXPECT_TRUE(harness.announced.empty());
    }

    TEST(SystemDaySource, TodayLagsTheClockUntilTheNextCheck) {
        Harness harness;
        harness.now = kStart + days{1};
        EXPECT_EQ(harness.source.today(), kStart);
    }

    TEST(SystemDaySource, TheTimerObservesAChangeWhileTheEventLoopRuns) {
        Harness harness;
        harness.now = kStart + days{3};
        ASSERT_TRUE(QTest::qWaitFor([&] { return !harness.announced.empty(); }, 2000));
        EXPECT_EQ(harness.announced, std::vector{kStart + days{3}});
    }

    TEST(SystemDaySource, BecomingActiveRechecksButOtherStatesDoNot) {
        Harness harness;
        harness.now = kStart + days{2};
        harness.source.onApplicationStateChanged(Qt::ApplicationInactive);
        EXPECT_TRUE(harness.announced.empty());
        harness.source.onApplicationStateChanged(Qt::ApplicationActive);
        EXPECT_EQ(harness.announced, std::vector{kStart + days{2}});
    }

    TEST(SystemDaySource, DefaultClockIsTheUtcDayOfTheSystemTime) {
        const auto before = ksv::application::epochDayOfMillis(
            duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count());
        const auto today = SystemDaySource::utcToday();
        const auto after = ksv::application::epochDayOfMillis(
            duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count());
        EXPECT_GE(today, before);
        EXPECT_LE(today, after);
    }
}
