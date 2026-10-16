#include "driver_ports.h"

#include <QMutexLocker>
#include <QRegularExpression>
#include <QThread>
#include <algorithm>
#include <stdexcept>
#include <utility>

namespace ksv::driver {
    namespace {
        constexpr auto kDecoder = "decoder";
        constexpr auto kCsv = "csv";
        constexpr auto kBenchmarkStore = "benchmarkStore";
        constexpr auto kSeriesStore = "seriesStore";

        QString portsHelp() {
            QStringList lines;
            for (const auto &[port, ops]: FaultPlan::ports()) {
                lines.append(QStringLiteral("%1 (%2)").arg(port, ops.join(QStringLiteral(", "))));
            }
            return lines.join(QStringLiteral("; "));
        }

        QString parseEffect(const QString &text, Effect &effect) {
            static const QRegularExpression delay(QStringLiteral("^delay:(\\d+)$"));
            static const QRegularExpression failNth(QStringLiteral("^fail:(\\d+)$"));
            static const QRegularExpression failAfter(QStringLiteral("^fail:after:(\\d+)$"));
            if (text == u"fail") {
                effect = {Effect::Kind::Fail, 0};
            } else if (const auto match = delay.match(text); match.hasMatch()) {
                effect = {Effect::Kind::Delay, match.captured(1).toInt()};
            } else if (const auto match = failNth.match(text); match.hasMatch() && match.captured(1).toInt() > 0) {
                effect = {Effect::Kind::FailNth, match.captured(1).toInt()};
            } else if (const auto match = failAfter.match(text); match.hasMatch()) {
                effect = {Effect::Kind::FailAfter, match.captured(1).toInt()};
            } else {
                return QStringLiteral("unknown effect '%1' (expected delay:MS, fail, fail:N with N >= 1, or fail:after:N)")
                    .arg(text);
            }
            return {};
        }

        // ---- decorators ------------------------------------------------------------

        class InjectedDecoder final : public application::IProtoDecoder {
        public:
            InjectedDecoder(std::shared_ptr<application::IProtoDecoder> inner, std::shared_ptr<FaultPlan> plan)
                : m_inner(std::move(inner)), m_plan(std::move(plan)) {}

            // A decoder signals a bad file by throwing (ProtoDecoder::decode_file).
            domain::Run decode(const perf::PerfLog &perfLog) override {
                if (m_plan->beforeCall(kDecoder, "decode")) throw std::runtime_error("injected decoder.decode failure");
                return m_inner->decode(perfLog);
            }
            domain::Run decode_file(const std::string_view filename) override {
                if (m_plan->beforeCall(kDecoder, "decode_file")) {
                    throw std::runtime_error("injected decoder.decode_file failure");
                }
                return m_inner->decode_file(filename);
            }

        private:
            std::shared_ptr<application::IProtoDecoder> m_inner;
            std::shared_ptr<FaultPlan> m_plan;
        };

        class InjectedCsvParser final : public data::IStatsCsvParser {
        public:
            InjectedCsvParser(std::shared_ptr<data::IStatsCsvParser> inner, std::shared_ptr<FaultPlan> plan)
                : m_inner(std::move(inner)), m_plan(std::move(plan)) {}

            [[nodiscard]] std::optional<data::ParsedStatsCsv> parseFile(const std::filesystem::path &path) const override {
                if (m_plan->beforeCall(kCsv, "parseFile")) return std::nullopt;
                return m_inner->parseFile(path);
            }

        private:
            std::shared_ptr<data::IStatsCsvParser> m_inner;
            std::shared_ptr<FaultPlan> m_plan;
        };

        class InjectedBenchmarkStore final : public data::IBenchmarkStore {
        public:
            InjectedBenchmarkStore(std::shared_ptr<data::IBenchmarkStore> inner, std::shared_ptr<FaultPlan> plan)
                : m_inner(std::move(inner)), m_plan(std::move(plan)) {}

            [[nodiscard]] data::BenchmarkScanResult scan() const override {
                if (m_plan->beforeCall(kBenchmarkStore, "scan")) {
                    return {std::nullopt, data::BenchmarkScanFailure::DirectoryUnavailable};
                }
                return m_inner->scan();
            }
            data::BenchmarkWriteResult write(const domain::Benchmark &definition, const std::string &filename,
                                             const std::optional<std::string> &expectedDigest) override {
                if (m_plan->beforeCall(kBenchmarkStore, "write")) {
                    return {std::nullopt, data::BenchmarkWriteFailure::WriteFailed};
                }
                return m_inner->write(definition, filename, expectedDigest);
            }
            data::BenchmarkDeleteResult remove(const std::string &filename, const std::string &expectedDigest) override {
                if (m_plan->beforeCall(kBenchmarkStore, "remove")) {
                    return {false, data::BenchmarkWriteFailure::WriteFailed};
                }
                return m_inner->remove(filename, expectedDigest);
            }
            [[nodiscard]] std::string managedDirectoryPath() const override { return m_inner->managedDirectoryPath(); }

        private:
            std::shared_ptr<data::IBenchmarkStore> m_inner;
            std::shared_ptr<FaultPlan> m_plan;
        };

        class InjectedGraphConfigurationPersistence final : public data::IGraphConfigurationPersistence {
        public:
            InjectedGraphConfigurationPersistence(std::shared_ptr<data::IGraphConfigurationPersistence> inner,
                                                  std::shared_ptr<FaultPlan> plan)
                : m_inner(std::move(inner)), m_plan(std::move(plan)) {}

            // As if the stored document could not be read, but without quarantining or rewriting it.
            [[nodiscard]] application::GraphConfiguration load() override {
                if (m_plan->beforeCall(kSeriesStore, "load")) return application::defaultGraphConfiguration();
                return m_inner->load();
            }
            // Writes are never verified (EDGE-05), so a lost one goes unreported, as a failed QSettings write would.
            void save(const application::GraphConfiguration &configuration) override {
                if (m_plan->beforeCall(kSeriesStore, "save")) return;
                m_inner->save(configuration);
            }

        private:
            std::shared_ptr<data::IGraphConfigurationPersistence> m_inner;
            std::shared_ptr<FaultPlan> m_plan;
        };
    }

    const QList<std::pair<QString, QStringList>> &FaultPlan::ports() {
        static const QList<std::pair<QString, QStringList>> kPorts{
            {kDecoder, {"decode", "decode_file"}},
            {kCsv, {"parseFile"}},
            {kBenchmarkStore, {"scan", "write", "remove"}},
            {kSeriesStore, {"load", "save"}},
        };
        return kPorts;
    }

    QString FaultPlan::add(const QString &spec) {
        const auto equals = spec.indexOf(u'=');
        if (equals <= 0) return QStringLiteral("--inject '%1' must look like PORT[.OP]=EFFECT[,EFFECT...]").arg(spec);
        const QString target = spec.left(equals).trimmed();
        const QString port = target.section(u'.', 0, 0);
        const QString op = target.section(u'.', 1);
        const auto found = std::find_if(ports().cbegin(), ports().cend(), [&](const auto &entry) { return entry.first == port; });
        if (found == ports().cend()) {
            return QStringLiteral("--inject '%1': unknown port '%2'. Ports and ops: %3").arg(spec, port, portsHelp());
        }
        if (!op.isEmpty() && !found->second.contains(op)) {
            return QStringLiteral("--inject '%1': %2 has no op '%3' (ops: %4)")
                .arg(spec, port, op, found->second.join(QStringLiteral(", ")));
        }
        QList<Effect> effects;
        const QStringList parts = spec.mid(equals + 1).split(u',');
        for (const QString &part: parts) {
            Effect effect;
            if (const QString error = parseEffect(part.trimmed(), effect); !error.isEmpty()) {
                return QStringLiteral("--inject '%1': %2").arg(spec, error);
            }
            effects.append(effect);
        }
        const QMutexLocker locker(&m_mutex);
        for (const QString &each: op.isEmpty() ? found->second : QStringList{op}) {
            m_effects[port + u'.' + each].append(effects);
        }
        m_specs.append(QStringLiteral("%1=%2").arg(target, parts.join(u',')));
        return {};
    }

    bool FaultPlan::targets(const QString &port) const {
        for (auto it = m_effects.cbegin(); it != m_effects.cend(); ++it) {
            if (it.key().startsWith(port + u'.')) return true;
        }
        return false;
    }

    bool FaultPlan::beforeCall(const QString &port, const QString &op) {
        int delayMs = 0;
        bool fail = false;
        {
            const QMutexLocker locker(&m_mutex);
            const QString key = port + u'.' + op;
            const auto effects = m_effects.constFind(key);
            if (effects == m_effects.cend()) return false;
            const int call = ++m_calls[key];
            for (const Effect &effect: *effects) {
                switch (effect.kind) {
                    case Effect::Kind::Delay: delayMs += effect.value; break;
                    case Effect::Kind::Fail: fail = true; break;
                    case Effect::Kind::FailNth: fail = fail || call == effect.value; break;
                    case Effect::Kind::FailAfter: fail = fail || call > effect.value; break;
                }
            }
        }
        if (delayMs > 0) QThread::msleep(static_cast<unsigned long>(delayMs));
        return fail;
    }

    std::shared_ptr<application::IProtoDecoder> injectDecoder(std::shared_ptr<application::IProtoDecoder> inner,
                                                              std::shared_ptr<FaultPlan> plan) {
        if (!plan || !plan->targets(kDecoder)) return inner;
        return std::make_shared<InjectedDecoder>(std::move(inner), std::move(plan));
    }

    std::shared_ptr<data::IStatsCsvParser> injectCsvParser(std::shared_ptr<data::IStatsCsvParser> inner,
                                                           std::shared_ptr<FaultPlan> plan) {
        if (!plan || !plan->targets(kCsv)) return inner;
        return std::make_shared<InjectedCsvParser>(std::move(inner), std::move(plan));
    }

    std::shared_ptr<data::IBenchmarkStore> injectBenchmarkStore(std::shared_ptr<data::IBenchmarkStore> inner,
                                                                std::shared_ptr<FaultPlan> plan) {
        if (!plan || !plan->targets(kBenchmarkStore)) return inner;
        return std::make_shared<InjectedBenchmarkStore>(std::move(inner), std::move(plan));
    }

    std::shared_ptr<data::IGraphConfigurationPersistence> injectGraphConfigurationPersistence(
        std::shared_ptr<data::IGraphConfigurationPersistence> inner, std::shared_ptr<FaultPlan> plan) {
        if (!plan || !plan->targets(kSeriesStore)) return inner;
        return std::make_shared<InjectedGraphConfigurationPersistence>(std::move(inner), std::move(plan));
    }
}
