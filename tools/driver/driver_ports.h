#pragma once

#include <QHash>
#include <QList>
#include <QMutex>
#include <QString>
#include <QStringList>
#include <memory>

#include "data/interfaces/i_benchmark_store.h"
#include "data/interfaces/i_series_config_store.h"
#include "data/interfaces/i_stats_csv_parser.h"
#include "interfaces/i_proto_decoder.h"

namespace ksv::driver {
    // Fault and latency injection on the services ksv_driver hands to App's
    // injecting constructor (`--inject PORT[.OP]=EFFECT[,EFFECT...]`, see
    // tools/driver/README.md). Each decorator delegates to the real service and
    // asks the shared FaultPlan, per call, whether to sleep first and whether to
    // return the port's natural failure instead.
    struct Effect {
        enum class Kind {
            Delay,      // delay:MS -- sleep before delegating (on the calling thread)
            Fail,       // fail -- every call fails
            FailNth,    // fail:N -- only the Nth call (1-based) fails
            FailAfter,  // fail:after:N -- every call after the Nth fails
        };
        Kind kind = Kind::Fail;
        int value = 0;
    };

    class FaultPlan {
    public:
        // The ports and their ops, in the order the README lists them.
        [[nodiscard]] static const QList<std::pair<QString, QStringList>> &ports();

        // Parses and adds one `--inject` value. Returns an empty string on success,
        // otherwise a message naming the offending part and the valid choices.
        [[nodiscard]] QString add(const QString &spec);

        [[nodiscard]] bool targets(const QString &port) const;
        // The specs as given, normalized (`benchmarkStore.write=fail`), for logging.
        [[nodiscard]] QStringList describe() const { return m_specs; }

        // Called by a decorator before delegating `port.op`: applies any delay and
        // returns whether this call should fail. Thread-safe.
        bool beforeCall(const QString &port, const QString &op);

    private:
        QHash<QString, QList<Effect>> m_effects;  // "port.op" -> effects
        QHash<QString, int> m_calls;
        QStringList m_specs;
        QMutex m_mutex;
    };

    // Wraps each service only when the plan targets its port.
    [[nodiscard]] std::shared_ptr<application::IProtoDecoder> injectDecoder(
        std::shared_ptr<application::IProtoDecoder> inner, std::shared_ptr<FaultPlan> plan);
    [[nodiscard]] std::shared_ptr<data::IStatsCsvParser> injectCsvParser(
        std::shared_ptr<data::IStatsCsvParser> inner, std::shared_ptr<FaultPlan> plan);
    [[nodiscard]] std::shared_ptr<data::IBenchmarkStore> injectBenchmarkStore(
        std::shared_ptr<data::IBenchmarkStore> inner, std::shared_ptr<FaultPlan> plan);
    [[nodiscard]] std::shared_ptr<application::ISeriesConfigStore> injectSeriesStore(
        std::shared_ptr<application::ISeriesConfigStore> inner, std::shared_ptr<FaultPlan> plan);
}
