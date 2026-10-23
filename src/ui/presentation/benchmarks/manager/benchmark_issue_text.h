#ifndef KOVAAKSSTATSVIEWER_BENCHMARK_ISSUE_TEXT_H
#define KOVAAKSSTATSVIEWER_BENCHMARK_ISSUE_TEXT_H

#include <QString>

#include "benchmarks/benchmark_validation.h"

namespace ksv::presentation {
    [[nodiscard]] QString benchmarkIssueText(domain::BenchmarkIssueCode code);
}

#endif
