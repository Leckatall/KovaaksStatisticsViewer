#include <gtest/gtest.h>

#include "qt_data/case_fold.h"

TEST(CaseFoldTest, FoldsNonAsciiLettersTogether) {
    EXPECT_EQ(ksv::qt_data::foldCase("\xC3\x84"), ksv::qt_data::foldCase("\xC3\xA4"));
}
