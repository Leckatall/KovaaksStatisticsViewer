#include <gtest/gtest.h>

#include <QElapsedTimer>
#include <QString>
#include <QTest>

#include "capture/capture_feedback.h"

namespace ksv::ui {
    namespace {
        constexpr int kExpiryMs = 20;
        constexpr int kDeadlineMs = 5000;

        bool expires(const CaptureFeedback &feedback) {
            return QTest::qWaitFor([&] { return feedback.kind() == CaptureFeedback::Kind::None; }, kDeadlineMs);
        }

        TEST(CaptureFeedback, ASuccessExpires) {
            CaptureFeedback feedback(kExpiryMs);

            feedback.showSuccess(QStringLiteral("x"));

            EXPECT_EQ(feedback.kind(), CaptureFeedback::Kind::Success);
            EXPECT_TRUE(expires(feedback));
        }

        TEST(CaptureFeedback, AFailurePersistsUntilDismissed) {
            CaptureFeedback feedback(kExpiryMs);

            feedback.showFailure(QStringLiteral("x"));
            EXPECT_EQ(feedback.kind(), CaptureFeedback::Kind::Failure);

            QTest::qWait(kExpiryMs * 3);
            EXPECT_EQ(feedback.kind(), CaptureFeedback::Kind::Failure);

            feedback.dismiss();
            EXPECT_EQ(feedback.kind(), CaptureFeedback::Kind::None);
        }

        TEST(CaptureFeedback, ANoticeLeavesAnUnresolvedFailure) {
            CaptureFeedback feedback(kExpiryMs);
            feedback.showFailure(QStringLiteral("x"));

            feedback.showNotice(QStringLiteral("y"));

            EXPECT_EQ(feedback.kind(), CaptureFeedback::Kind::Failure);
            EXPECT_EQ(feedback.text(), QStringLiteral("x"));
        }

        TEST(CaptureFeedback, ANewOutcomeReplacesAFailure) {
            CaptureFeedback feedback(kExpiryMs);
            feedback.showFailure(QStringLiteral("x"));

            feedback.showSuccess(QStringLiteral("y"));

            EXPECT_EQ(feedback.kind(), CaptureFeedback::Kind::Success);
            EXPECT_TRUE(expires(feedback));
        }

        // Measured as a lower bound on the second success's life, so a slow machine can only miss
        // a leaked timer, never fail a correct one.
        TEST(CaptureFeedback, ADismissedSuccessCancelsItsExpiry) {
            constexpr int expiryMs = 200;
            constexpr int beforeDismissMs = 150;
            constexpr int leakedTimerWouldLeaveMs = expiryMs - beforeDismissMs;
            CaptureFeedback feedback(expiryMs);

            feedback.showSuccess(QStringLiteral("x"));
            QTest::qWait(beforeDismissMs);
            feedback.dismiss();
            QElapsedTimer sinceSecond;
            sinceSecond.start();
            feedback.showSuccess(QStringLiteral("y"));

            EXPECT_EQ(feedback.kind(), CaptureFeedback::Kind::Success);
            EXPECT_TRUE(expires(feedback));
            EXPECT_GT(sinceSecond.elapsed(), leakedTimerWouldLeaveMs * 2);
        }
    }
}
