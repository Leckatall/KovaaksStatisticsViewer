#include "series_config_store.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <cmath>
#include <limits>

namespace ksv::qt_data {
    namespace {
        using namespace application;
        constexpr auto kActiveKey = "graph/seriesConfigV1";

        QString metricTag(const PrimitiveMetric metric) {
            switch (metric) {
                case PrimitiveMetric::Score: return "score";
                case PrimitiveMetric::Shots: return "shots";
                case PrimitiveMetric::Hits: return "hits";
                case PrimitiveMetric::Kills: return "kills";
                case PrimitiveMetric::Dmg: return "dmg";
            }
            return {};
        }

        std::optional<PrimitiveMetric> metricFromTag(const QString &tag) {
            for (const auto metric: kPrimitiveMetrics) if (metricTag(metric) == tag) return metric;
            return std::nullopt;
        }

        bool exactKeys(const QJsonObject &object, const std::initializer_list<const char *> keys) {
            if (object.size() != static_cast<qsizetype>(keys.size())) return false;
            for (const auto key: keys) {
                if (!object.contains(QLatin1String(key))) return false;
            }
            return true;
        }

        std::optional<uint64_t> decimalId(const QJsonValue &value, const bool zeroAllowed) {
            if (!value.isString()) return std::nullopt;
            const auto text = value.toString();
            if (text.isEmpty() || (text.size() > 1 && text.front() == '0')) return std::nullopt;
            for (const auto c: text) if (c < '0' || c > '9') return std::nullopt;
            bool ok = false;
            const auto parsed = text.toULongLong(&ok);
            if (!ok || (!zeroAllowed && parsed == 0)) return std::nullopt;
            return parsed;
        }

        template<typename T>
        std::optional<T> boundedInteger(const QJsonValue &value) {
            if (!value.isDouble()) return std::nullopt;
            const auto number = value.toDouble();
            if (!std::isfinite(number) || std::floor(number) != number || number < 0 ||
                number > static_cast<double>(std::numeric_limits<T>::max()))
                return std::nullopt;
            return static_cast<T>(number);
        }

        std::optional<double> finiteNumber(const QJsonValue &value) {
            if (!value.isDouble() || !std::isfinite(value.toDouble())) return std::nullopt;
            return value.toDouble();
        }

        QJsonObject encodeExpression(const Expression &expression);

        std::optional<Expression> decodeExpression(const QJsonValue &value);

        template<typename Node>
        QJsonObject encodeBinaryExpression(const char *kind, const Node &node) {
            return {{"kind", kind}, {"left", encodeExpression(node.left)}, {"right", encodeExpression(node.right)}};
        }

        template<typename Node>
        QJsonObject encodeUnaryExpression(const char *kind, const Node &node) {
            return {{"kind", kind}, {"input", encodeExpression(node.input)}};
        }

        QJsonObject encodeSelection(const RunSelection &selection) {
            return std::visit([](const auto &item) -> QJsonObject {
                using Selection = std::decay_t<decltype(item)>;
                if constexpr (std::same_as<Selection, RecentRuns>)
                    return {{"kind", "recentRuns"}, {"count", static_cast<double>(item.count)}};
                else return {{"kind", "topPercentile"}, {"percent", item.percent}};
            }, selection);
        }

        QJsonObject encodeStyle(const LineStyle &style) {
            return {
                {
                    "color", QJsonArray{
                        style.color.red,
                        style.color.green,
                        style.color.blue,
                        style.color.alpha
                    }
                },
                {"width", style.width}
            };
        }

        std::optional<LineStyle> decodeStyle(const QJsonValue &value) {
            if (!value.isObject()) return std::nullopt;
            const auto object = value.toObject();
            if (!exactKeys(object, {"color", "width"}) || !object["color"].isArray()) return std::nullopt;
            const auto color = object["color"].toArray();
            if (color.size() != 4) return std::nullopt;
            const auto red = boundedInteger<uint8_t>(color[0]);
            const auto green = boundedInteger<uint8_t>(color[1]);
            const auto blue = boundedInteger<uint8_t>(color[2]);
            const auto alpha = boundedInteger<uint8_t>(color[3]);
            const auto width = finiteNumber(object["width"]);
            if (!red || !green || !blue || !alpha || !width) return std::nullopt;
            return LineStyle{{*red, *green, *blue, *alpha}, *width};
        }

        QJsonObject encodePresentation(const SeriesPresentation &presentation) {
            return {
                {"name", QString::fromStdString(presentation.name)}, {"enabled", presentation.enabled},
                {"displayPosition", static_cast<double>(presentation.displayPosition)},
                {"lineStyle", encodeStyle(presentation.lineStyle)}
            };
        }

        std::optional<SeriesPresentation> decodePresentation(const QJsonValue &value) {
            if (!value.isObject()) return std::nullopt;
            const auto object = value.toObject();
            if (!exactKeys(object, {"name", "enabled", "displayPosition", "lineStyle"}) || !object["name"].isString() ||
                !object["enabled"].isBool())
                return std::nullopt;
            const auto position = boundedInteger<uint32_t>(object["displayPosition"]);
            const auto style = decodeStyle(object["lineStyle"]);
            if (!position || !style) return std::nullopt;
            return SeriesPresentation{
                object["name"].toString().toStdString(), *style, object["enabled"].toBool(), *position
            };
        }

        QJsonObject encodeExpression(const Expression &expression) {
            return std::visit([](const auto &node) -> QJsonObject {
                using Node = std::decay_t<decltype(node)>;
                if constexpr (std::same_as<Node, PrimitiveReference>)
                    return {
                        {"kind", "primitive"}, {"primitiveMetric", metricTag(node.metric)}
                    };
                else if constexpr (std::same_as<Node, NumericConstant>)
                    return {
                        {"kind", "constant"}, {"value", node.value}
                    };
                else if constexpr (std::same_as<Node, Add>)
                    return encodeBinaryExpression("add", node);
                else if constexpr (std::same_as<Node, Subtract>)
                    return encodeBinaryExpression("subtract", node);
                else if constexpr (std::same_as<Node, Multiply>)
                    return encodeBinaryExpression("multiply", node);
                else if constexpr (std::same_as<Node, Divide>)
                    return encodeBinaryExpression("divide", node);
                else if constexpr (std::same_as<Node, RunningSum>)
                    return encodeUnaryExpression("runningSum", node);
                else if constexpr (std::same_as<Node, RollingMean>)
                    return {
                        {"kind", "rollingMean"}, {"input", encodeExpression(node.input)},
                        {"window", static_cast<double>(node.window)}
                    };
                else if constexpr (std::same_as<Node, ProjectedFinalValue>)
                    return encodeUnaryExpression("projectedFinalValue", node);
                else if constexpr (std::same_as<Node, ProjectRateToFinal>)
                    return encodeUnaryExpression("projectRateToFinal", node);
                else
                    return {
                        {"kind", "averageAcrossRuns"}, {"input", encodeExpression(node.input)},
                        {"selection", encodeSelection(node.selection)}
                    };
            }, expression->value());
        }

        using BinaryExpressionFactory = Expression (*)(Expression, Expression);
        using UnaryExpressionFactory = Expression (*)(Expression);

        std::optional<Expression> decodeBinaryExpression(const QJsonObject &object,
                                                         const BinaryExpressionFactory factory) {
            if (!exactKeys(object, {"kind", "left", "right"})) return {};
            const auto left = decodeExpression(object["left"]);
            const auto right = decodeExpression(object["right"]);
            if (!left || !right) return {};
            return factory(*left, *right);
        }

        std::optional<Expression>
        decodeUnaryExpression(const QJsonObject &object, const UnaryExpressionFactory factory) {
            if (!exactKeys(object, {"kind", "input"})) return {};
            const auto input = decodeExpression(object["input"]);
            return input ? std::optional{factory(*input)} : std::nullopt;
        }

        std::optional<RunSelection> decodeSelection(const QJsonValue &value) {
            if (!value.isObject()) return {};
            const auto selection = value.toObject();
            if (!selection["kind"].isString()) return {};
            if (selection["kind"] == "recentRuns") {
                const auto count = boundedInteger<uint32_t>(selection["count"]);
                if (!exactKeys(selection, {"kind", "count"}) || !count) return {};
                return RecentRuns{*count};
            }
            const auto percent = finiteNumber(selection["percent"]);
            if (!exactKeys(selection, {"kind", "percent"}) || selection["kind"] != "topPercentile" || !percent)
                return {};
            return TopPercentile{*percent};
        }

        std::optional<Expression> decodeExpression(const QJsonValue &value) {
            if (!value.isObject()) return std::nullopt;
            const auto object = value.toObject();
            if (!object["kind"].isString()) return std::nullopt;
            const auto kind = object["kind"].toString();
            if (kind == "primitive") {
                const auto metric = metricFromTag(object["primitiveMetric"].toString());
                if (!exactKeys(object, {"kind", "primitiveMetric"}) || !metric) return {};
                return primitive(*metric);
            }
            if (kind == "constant") {
                const auto number = finiteNumber(object["value"]);
                if (!exactKeys(object, {"kind", "value"}) || !number) return {};
                return numericConstant(*number);
            }
            if (kind == "add") return decodeBinaryExpression(object, add);
            if (kind == "subtract") return decodeBinaryExpression(object, subtract);
            if (kind == "multiply") return decodeBinaryExpression(object, multiply);
            if (kind == "divide") return decodeBinaryExpression(object, divide);
            if (kind == "runningSum") return decodeUnaryExpression(object, runningSum);
            if (kind == "projectedFinalValue") return decodeUnaryExpression(object, projectedFinalValue);
            if (kind == "projectRateToFinal") return decodeUnaryExpression(object, projectRateToFinal);
            if (kind == "rollingMean") {
                if (!exactKeys(object, {"kind", "input", "window"})) return {};
                const auto decoded = decodeExpression(object["input"]);
                const auto window = boundedInteger<uint32_t>(object["window"]);
                if (!decoded || !window) return {};
                return rollingMean(*decoded, *window);
            }
            if (kind != "averageAcrossRuns" || !exactKeys(object, {"kind", "input", "selection"})) return {};
            const auto input = decodeExpression(object["input"]);
            const auto selection = decodeSelection(object["selection"]);
            if (!input || !selection) return {};
            return averageAcrossRuns(*input, *selection);
        }

        QString encode(const std::vector<SeriesConfig> &configs, const std::optional<ComputedSeriesId> &next) {
            QJsonArray series;
            for (const auto &config: configs)
                std::visit([&](const auto &record) {
                    using Record = std::decay_t<decltype(record)>;
                    if constexpr (std::same_as<Record, BaseSeriesConfig>)
                        series.append(QJsonObject{
                            {"kind", "base"}, {"primitiveMetric", metricTag(record.metric)},
                            {"presentation", encodePresentation(record.presentation)}
                        });
                    else
                        series.append(QJsonObject{
                            {"kind", "computed"}, {"id", QString::number(record.id.value)},
                            {"presentation", encodePresentation(record.presentation)},
                            {"expression", encodeExpression(record.expression)}
                        });
                }, config);
            return QString::fromUtf8(QJsonDocument(QJsonObject{
                {"schemaVersion", 1},
                {
                    "nextComputedSeriesId",
                    next ? QJsonValue(QString::number(next->value)) : QJsonValue(QJsonValue::Null)
                },
                {"series", series}
            }).toJson(QJsonDocument::Compact));
        }

        struct Document {
            std::vector<SeriesConfig> configs;
            std::optional<ComputedSeriesId> next;
        };

        std::optional<Document> decode(const QVariant &raw) {
            if (raw.metaType().id() != QMetaType::QString) return {};
            QJsonParseError error;
            const auto document = QJsonDocument::fromJson(raw.toString().toUtf8(), &error);
            if (error.error != QJsonParseError::NoError || !document.isObject()) return {};
            const auto root = document.object();
            if (!exactKeys(root, {"schemaVersion", "nextComputedSeriesId", "series"}) || !root["schemaVersion"].
                isDouble() || root["schemaVersion"].toDouble() != 1 || !root["series"].isArray())
                return {};
            std::optional<ComputedSeriesId> next;
            if (!root["nextComputedSeriesId"].isNull()) {
                const auto id = decimalId(root["nextComputedSeriesId"], false);
                if (!id) return {};
                next = ComputedSeriesId{*id};
            }
            std::vector<SeriesConfig> configs;
            for (const auto &item: root["series"].toArray()) {
                if (!item.isObject()) return {};
                const auto object = item.toObject();
                if (!object["kind"].isString()) return {};
                const auto presentation = decodePresentation(object["presentation"]);
                if (!presentation) return {};
                if (object["kind"] == "base") {
                    const auto metric = metricFromTag(object["primitiveMetric"].toString());
                    if (!exactKeys(object, {"kind", "primitiveMetric", "presentation"}) || !metric) return {};
                    configs.emplace_back(BaseSeriesConfig{*metric, *presentation});
                } else if (object["kind"] == "computed") {
                    const auto id = decimalId(object["id"], false);
                    const auto expression = decodeExpression(object["expression"]);
                    if (!exactKeys(object, {"kind", "id", "presentation", "expression"}) || !id || !expression)
                        return
                                {};
                    configs.emplace_back(ComputedSeriesConfig{{*id}, *presentation, *expression});
                } else return {};
            }
            if (!validateSeriesConfigs(configs).empty()) return {};
            uint64_t maximumId = 0;
            for (const auto &config: configs) {
                if (const auto *computed = std::get_if<ComputedSeriesConfig>(&config))
                    maximumId = std::max(maximumId, computed->id.value);
            }
            if ((next && next->value <= maximumId) || (!next && maximumId != UINT64_MAX)) return {};
            return Document{std::move(configs), next};
        }

        void normalizeDisplayPositions(std::vector<SeriesConfig> &configs) {
            for (size_t index = 0; index < configs.size(); ++index)
                std::visit([index](auto &record) {
                    record.presentation.displayPosition = static_cast<uint32_t>(index);
                }, configs[index]);
        }

        StoreMutationFailureCode missingReferenceFailure(const SeriesRecordReference &reference) {
            return std::holds_alternative<PrimitiveMetric>(reference)
                       ? StoreMutationFailureCode::InvalidPrimitiveMetric
                       : StoreMutationFailureCode::UnknownComputedSeriesId;
        }
    }

    SeriesConfigStore::SeriesConfigStore(std::shared_ptr<ISeriesConfigStoreSettingsBackend> backend) : m_backend(
        std::move(backend)) {
        QMutexLocker lock(&m_mutex);
        ensureLoadedLocked();
    }

    SeriesConfigStore::SeriesConfigStore(const QSettings::Format format) : SeriesConfigStore(
        std::make_shared<QSettingsSeriesConfigStoreSettingsBackend>(format)) {
    }

    bool SeriesConfigStore::writeLocked(const std::vector<SeriesConfig> &configs,
                                        const std::optional<ComputedSeriesId> &next) const {
        m_backend->setValue(kActiveKey, encode(configs, next));
        m_backend->sync();
        return m_backend->status() == QSettings::NoError;
    }

    void SeriesConfigStore::seedLocked(const QVariant *invalidRaw) const {
        if (invalidRaw) {
            const auto base = QString("graph/seriesConfigQuarantine/") + QDateTime::currentDateTimeUtc().toString(
                                  "yyyyMMddTHHmmsszzzZ");
            auto key = base;
            for (int suffix = 1; m_backend->contains(key); ++suffix) key = base + "-" + QString::number(suffix);
            m_backend->setValue(key, *invalidRaw);
            m_backend->sync();
            if (m_backend->status() != QSettings::NoError) {
                m_configs = defaultSeriesConfigs();
                m_next = kFirstUserComputedSeriesId;
                m_requiresReload = true;
                return;
            }
        }
        m_configs = defaultSeriesConfigs();
        m_next = kFirstUserComputedSeriesId;
        const auto disabled = m_backend->value("graph/disabledColumns").toStringList();
        constexpr std::array<std::pair<const char *, unsigned>, 8> legacy{
            {
                {"score", 0}, {"accuracy", 1}, {"shots", 2}, {"kills", 4},
                {"dmg", 5}, {"scoreTotal", 6}, {"expectedFinalScore", 7}, {"expectedFinalScoreRecent", 8}
            }
        };
        for (const auto &[legacyKey, recordIndex]: legacy) {
            const auto key = QString::fromLatin1(legacyKey);
            const auto qml_key = QString("graphColumns/") + key;
            const auto is_disabled = disabled.contains(key) || (
                                         m_backend->contains(qml_key) && !m_backend->value(qml_key).toBool());
            if (is_disabled)
                std::visit([](auto &record) { record.presentation.enabled = false; },
                           m_configs[recordIndex]);
        }
        m_requiresReload = !writeLocked(m_configs, m_next);
    }

    void SeriesConfigStore::ensureLoadedLocked() const {
        if (!m_requiresReload) return;
        m_backend->reload();
        if (!m_backend->contains(kActiveKey)) {
            seedLocked();
            return;
        }
        const auto raw = m_backend->value(kActiveKey);
        const auto decoded = decode(raw);
        if (!decoded) {
            seedLocked(&raw);
            return;
        }
        m_configs = decoded->configs;
        m_next = decoded->next;
        m_requiresReload = false;
    }

    std::vector<SeriesConfig> SeriesConfigStore::getAll() const {
        QMutexLocker lock(&m_mutex);
        ensureLoadedLocked();
        return m_configs;
    }

    void SeriesConfigStore::onChanged(std::function<void()> callback) {
        QMutexLocker lock(&m_mutex);
        m_callbacks.push_back(std::move(callback));
    }

    std::optional<size_t> SeriesConfigStore::indexOfLocked(const SeriesRecordReference reference) const {
        for (size_t i = 0; i < m_configs.size(); ++i) {
            if (const auto *metric = std::get_if<PrimitiveMetric>(&reference)) {
                const auto *base = std::get_if<BaseSeriesConfig>(&m_configs[i]);
                if (base && base->metric == *metric) return i;
            } else if (const auto *id = std::get_if<ComputedSeriesId>(&reference)) {
                const auto *computed = std::get_if<ComputedSeriesConfig>(&m_configs[i]);
                if (computed && computed->id == *id) return i;
            }
        }
        return {};
    }

    MutationResult SeriesConfigStore::commitLocked(std::vector<SeriesConfig> configs,
                                                   std::optional<ComputedSeriesId> next,
                                                   std::optional<ComputedSeriesId> created) {
        const auto errors = validateSeriesConfigs(configs);
        if (!errors.empty()) return {errors};
        if (!created && encode(configs, next) == encode(m_configs, m_next)) return {};
        if (!writeLocked(configs, next)) {
            m_configs.clear();
            m_next.reset();
            m_requiresReload = true;
            return {{}, StoreMutationFailureCode::PersistenceWriteFailed, true};
        }
        m_configs = std::move(configs);
        m_next = next;
        m_requiresReload = false;
        return {{}, {}, false, created};
    }

    MutationResult SeriesConfigStore::mutateLocked(
        const std::function<MutationResult(std::vector<SeriesConfig> &)> &mutate) {
        std::vector<std::function<void()> > callbacks;
        MutationResult result; {
            QMutexLocker lock(&m_mutex);
            ensureLoadedLocked();
            auto configs = m_configs;
            result = mutate(configs);
            if (result.succeeded()) callbacks = m_callbacks;
        }
        for (auto &callback: callbacks) callback();
        return result;
    }

    MutationResult SeriesConfigStore::createComputed(const CreateComputedSeriesRequest &request) {
        return mutateLocked([&](std::vector<SeriesConfig> &configs) -> MutationResult {
            if (!m_next) return {{}, StoreMutationFailureCode::ComputedSeriesIdExhausted};
            const auto id = *m_next;
            configs.emplace_back(ComputedSeriesConfig{
                id,
                {
                    request.presentation.name, request.presentation.lineStyle, request.presentation.enabled,
                    static_cast<uint32_t>(configs.size())
                },
                request.expression
            });
            const auto next = id.value == UINT64_MAX ? std::nullopt : std::optional{ComputedSeriesId{id.value + 1}};
            return commitLocked(std::move(configs), next, id);
        });
    }

    MutationResult SeriesConfigStore::updateComputed(
        const UpdateComputedSeriesRequest &request) {
        return mutateLocked([&](std::vector<SeriesConfig> &configs) -> MutationResult {
            const auto index = indexOfLocked(request.id);
            if (!index) return {{}, StoreMutationFailureCode::UnknownComputedSeriesId};
            auto &record = std::get<ComputedSeriesConfig>(configs[*index]);
            record.presentation.name = request.presentation.name;
            record.presentation.lineStyle = request.presentation.lineStyle;
            record.presentation.enabled = request.presentation.enabled;
            record.expression = request.expression;
            return commitLocked(std::move(configs), m_next);
        });
    }

    MutationResult SeriesConfigStore::updateBase(const UpdateBaseSeriesRequest &request) {
        return mutateLocked([&](std::vector<SeriesConfig> &configs) -> MutationResult {
            if (std::ranges::find(kPrimitiveMetrics, request.metric) == kPrimitiveMetrics.end())
                return {
                    {}, StoreMutationFailureCode::InvalidPrimitiveMetric
                };
            const auto index = indexOfLocked(request.metric);
            auto &record = std::get<BaseSeriesConfig>(configs[*index]);
            record.presentation.enabled = request.enabled;
            record.presentation.lineStyle = request.lineStyle;
            return commitLocked(std::move(configs), m_next);
        });
    }

    MutationResult SeriesConfigStore::removeComputed(const ComputedSeriesId id) {
        return mutateLocked([&](std::vector<SeriesConfig> &configs) -> MutationResult {
            const auto index = indexOfLocked(id);
            if (!index) return {{}, StoreMutationFailureCode::UnknownComputedSeriesId};
            configs.erase(configs.begin() + *index);
            normalizeDisplayPositions(configs);
            return commitLocked(std::move(configs), m_next);
        });
    }

    MutationResult SeriesConfigStore::reorder(SeriesRecordReference reference,
                                              const uint32_t position) {
        return mutateLocked([&](std::vector<SeriesConfig> &configs) -> MutationResult {
            const auto index = indexOfLocked(reference);
            if (!index) return {{}, missingReferenceFailure(reference)};
            if (position >= configs.size()) return {{}, StoreMutationFailureCode::DisplayPositionOutOfRange};
            if (*index == position) return {};
            const auto record = std::move(configs[*index]);
            configs.erase(configs.begin() + *index);
            configs.insert(configs.begin() + position, record);
            normalizeDisplayPositions(configs);
            return commitLocked(std::move(configs), m_next);
        });
    }
}
