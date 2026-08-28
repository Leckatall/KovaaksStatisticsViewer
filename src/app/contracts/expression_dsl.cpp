#include "expression_dsl.h"

#include <array>
#include <cctype>
#include <charconv>
#include <cmath>
#include <string>
#include <type_traits>

namespace ksv::application {
    namespace {
        std::string formatNumber(const double value) {
            std::array<char, 32> buffer{};
            const auto [ptr, ec] = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
            return std::string(buffer.data(), ptr);
        }

        std::string_view metricToken(const PrimitiveMetric metric) {
            switch (metric) {
                case PrimitiveMetric::Score: return "SCORE";
                case PrimitiveMetric::Shots: return "SHOTS";
                case PrimitiveMetric::Hits: return "HITS";
                case PrimitiveMetric::Kills: return "KILLS";
                case PrimitiveMetric::Dmg: return "DMG";
            }
            return {};
        }

        std::string encodeSelection(const RunSelection &selection) {
            return std::visit([](const auto &value) -> std::string {
                using Selection = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<Selection, RecentRuns>) {
                    return "recent " + std::to_string(value.count);
                } else {
                    return "top " + formatNumber(value.percent) + "%";
                }
            }, selection);
        }
    }

    std::string encodeExpressionDsl(const Expression &expression) {
        if (!expression) return {};
        return std::visit([](const auto &node) -> std::string {
            using Node = std::decay_t<decltype(node)>;
            if constexpr (std::is_same_v<Node, PrimitiveReference>) {
                return std::string(metricToken(node.metric));
            } else if constexpr (std::is_same_v<Node, NumericConstant>) {
                return formatNumber(node.value);
            } else if constexpr (std::is_same_v<Node, Add>) {
                return "Add(" + encodeExpressionDsl(node.left) + ", " + encodeExpressionDsl(node.right) + ")";
            } else if constexpr (std::is_same_v<Node, Subtract>) {
                return "Subtract(" + encodeExpressionDsl(node.left) + ", " + encodeExpressionDsl(node.right) + ")";
            } else if constexpr (std::is_same_v<Node, Multiply>) {
                return "Multiply(" + encodeExpressionDsl(node.left) + ", " + encodeExpressionDsl(node.right) + ")";
            } else if constexpr (std::is_same_v<Node, Divide>) {
                return "Divide(" + encodeExpressionDsl(node.left) + ", " + encodeExpressionDsl(node.right) + ")";
            } else if constexpr (std::is_same_v<Node, RunningSum>) {
                return "RunningSum(" + encodeExpressionDsl(node.input) + ")";
            } else if constexpr (std::is_same_v<Node, ProjectedFinalValue>) {
                return "ProjectedFinalValue(" + encodeExpressionDsl(node.input) + ")";
            } else if constexpr (std::is_same_v<Node, ProjectRateToFinal>) {
                return "ProjectRateToFinal(" + encodeExpressionDsl(node.input) + ")";
            } else if constexpr (std::is_same_v<Node, RollingMean>) {
                return "RollingMean(window: " + std::to_string(node.window) + ", "
                       + encodeExpressionDsl(node.input) + ")";
            } else if constexpr (std::is_same_v<Node, AverageAcrossRuns>) {
                return "AverageAcrossRuns(over: " + encodeSelection(node.selection) + ", "
                       + encodeExpressionDsl(node.input) + ")";
            } else {
                return {};
            }
        }, expression->value());
    }

    namespace {
        // Recursive-descent parser for the grammar in expression_dsl.h. Every production returns nullopt
        // on any mismatch, which unwinds the whole parse; the public entry point additionally requires the
        // parse to consume all input.
        class Parser {
        public:
            explicit Parser(const std::string_view text) : m_text(text) {}

            std::optional<Expression> parseComplete() {
                auto expr = parseExpr();
                skipWs();
                if (!expr || m_pos != m_text.size()) return std::nullopt;
                return expr;
            }

        private:
            void skipWs() {
                while (m_pos < m_text.size() && std::isspace(static_cast<unsigned char>(m_text[m_pos]))) ++m_pos;
            }

            char peek() {
                skipWs();
                return m_pos < m_text.size() ? m_text[m_pos] : '\0';
            }

            bool consume(const char expected) {
                if (peek() == expected) {
                    ++m_pos;
                    return true;
                }
                return false;
            }

            // Lowercased [A-Za-z]+ run, or nullopt if none is present.
            std::optional<std::string> parseIdentifier() {
                skipWs();
                const size_t start = m_pos;
                while (m_pos < m_text.size() && std::isalpha(static_cast<unsigned char>(m_text[m_pos]))) ++m_pos;
                if (m_pos == start) return std::nullopt;
                std::string word(m_text.substr(start, m_pos - start));
                for (char &character: word) character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
                return word;
            }

            std::optional<double> parseDouble() {
                skipWs();
                double value = 0.0;
                const auto [ptr, ec] = std::from_chars(m_text.data() + m_pos, m_text.data() + m_text.size(), value);
                if (ec != std::errc{}) return std::nullopt;
                m_pos = static_cast<size_t>(ptr - m_text.data());
                if (!std::isfinite(value)) return std::nullopt;
                return value;
            }

            std::optional<uint32_t> parseUint() {
                skipWs();
                uint32_t value = 0;
                const auto [ptr, ec] = std::from_chars(m_text.data() + m_pos, m_text.data() + m_text.size(), value);
                if (ec != std::errc{}) return std::nullopt;
                m_pos = static_cast<size_t>(ptr - m_text.data());
                return value;
            }

            std::optional<Expression> parseExpr() {
                const char c = peek();
                if (c == '\0') return std::nullopt;
                if (std::isdigit(static_cast<unsigned char>(c)) || c == '-' || c == '+' || c == '.') {
                    const auto value = parseDouble();
                    if (!value) return std::nullopt;
                    return numericConstant(*value);
                }
                const auto word = parseIdentifier();
                if (!word) return std::nullopt;
                if (peek() == '(') return parseCall(*word);
                return metricFor(*word);
            }

            static std::optional<Expression> metricFor(const std::string &word) {
                if (word == "score") return primitive(PrimitiveMetric::Score);
                if (word == "shots") return primitive(PrimitiveMetric::Shots);
                if (word == "hits") return primitive(PrimitiveMetric::Hits);
                if (word == "kills") return primitive(PrimitiveMetric::Kills);
                if (word == "dmg") return primitive(PrimitiveMetric::Dmg);
                return std::nullopt;
            }

            std::optional<Expression> parseCall(const std::string &name) {
                if (!consume('(')) return std::nullopt;
                std::optional<Expression> result;
                if (name == "add" || name == "subtract" || name == "multiply" || name == "divide") {
                    result = parseBinary(name);
                } else if (name == "runningsum" || name == "projectedfinalvalue" || name == "projectratetofinal") {
                    result = parseUnary(name);
                } else if (name == "rollingmean") {
                    result = parseRollingMean();
                } else if (name == "averageacrossruns") {
                    result = parseAverageAcrossRuns();
                }
                if (!result || !consume(')')) return std::nullopt;
                return result;
            }

            std::optional<Expression> parseBinary(const std::string &name) {
                auto left = parseExpr();
                if (!left || !consume(',')) return std::nullopt;
                auto right = parseExpr();
                if (!right) return std::nullopt;
                if (name == "add") return add(*left, *right);
                if (name == "subtract") return subtract(*left, *right);
                if (name == "multiply") return multiply(*left, *right);
                return divide(*left, *right);
            }

            std::optional<Expression> parseUnary(const std::string &name) {
                auto input = parseExpr();
                if (!input) return std::nullopt;
                if (name == "runningsum") return runningSum(*input);
                if (name == "projectedfinalvalue") return projectedFinalValue(*input);
                return projectRateToFinal(*input);
            }

            std::optional<Expression> parseRollingMean() {
                if (parseIdentifier() != "window" || !consume(':')) return std::nullopt;
                const auto window = parseUint();
                if (!window || *window == 0 || !consume(',')) return std::nullopt;
                auto input = parseExpr();
                if (!input) return std::nullopt;
                return rollingMean(*input, *window);
            }

            std::optional<Expression> parseAverageAcrossRuns() {
                if (parseIdentifier() != "over" || !consume(':')) return std::nullopt;
                const auto selection = parseSelection();
                if (!selection || !consume(',')) return std::nullopt;
                auto input = parseExpr();
                if (!input) return std::nullopt;
                return averageAcrossRuns(*input, *selection);
            }

            std::optional<RunSelection> parseSelection() {
                const auto kind = parseIdentifier();
                if (kind == "recent") {
                    const auto count = parseUint();
                    if (!count || *count == 0) return std::nullopt;
                    return RunSelection{RecentRuns{*count}};
                }
                if (kind == "top") {
                    const auto percent = parseDouble();
                    if (!percent || !consume('%')) return std::nullopt;
                    return RunSelection{TopPercentile{*percent}};
                }
                return std::nullopt;
            }

            std::string_view m_text;
            size_t m_pos = 0;
        };
    }

    std::optional<Expression> decodeExpressionDsl(const std::string_view text) {
        return Parser(text).parseComplete();
    }
}
