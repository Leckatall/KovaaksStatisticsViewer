#include <gtest/gtest.h>

#include "app/contracts/series_config.h"

// Every test previously in this file referenced BaseSeriesConfig/ComputedSeriesConfig/
// seriesPresentation(), the pre-unification variant type that series_config.h no longer defines
// (retired, commented out). They were removed because they no longer compiled, not because the
// coverage they gave was decided to be unnecessary — see .plans/series-config-migration-completion/
// plans/05-store-api-collapse.md, which rewrites this file's coverage against the unified
// SeriesConfig type.
