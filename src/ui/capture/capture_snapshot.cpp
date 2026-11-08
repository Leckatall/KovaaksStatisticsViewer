#include "capture/capture_snapshot.h"

#include <QObject>

namespace ksv::ui {
    QString captureTargetName(const CaptureTarget target) {
        switch (target) {
            case CaptureTarget::Run: return QObject::tr("Run graph");
            case CaptureTarget::DailyPlaytime: return QObject::tr("Daily playtime");
            case CaptureTarget::ScenarioHistory: return QObject::tr("Scenario history");
            case CaptureTarget::BenchmarkRank: return QObject::tr("Benchmark rank history");
            case CaptureTarget::BenchmarkRecentRank: return QObject::tr("Benchmark recent rank history");
            case CaptureTarget::BenchmarkPlaytime: return QObject::tr("Benchmark playtime");
            case CaptureTarget::BenchmarkTable: return QObject::tr("Benchmark table");
        }
        return {};
    }
}
