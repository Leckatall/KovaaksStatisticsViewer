#include "selector.h"

#include <QRegularExpressionMatch>

namespace ksv::driver {
    namespace {
        SelectorTerm parseTerm(const QString &raw, const QString &source) {
            static const QList<std::pair<QString, SelectorTerm::Kind>> prefixes{
                {QStringLiteral("name:"), SelectorTerm::Kind::Name},
                {QStringLiteral("id:"), SelectorTerm::Kind::Id},
                {QStringLiteral("text:"), SelectorTerm::Kind::Text},
                {QStringLiteral("type:"), SelectorTerm::Kind::Type},
                {QStringLiteral("acc:"), SelectorTerm::Kind::Accessible},
            };
            SelectorTerm term;
            term.value = raw;
            for (const auto &[prefix, kind]: prefixes) {
                if (raw.startsWith(prefix)) {
                    term.kind = kind;
                    term.value = raw.mid(prefix.size());
                    break;
                }
            }
            if (term.value.isEmpty()) {
                throw DriverError(QStringLiteral("selector '%1' has an empty term").arg(source));
            }
            // Visible strings are matched case-insensitively; identifiers exactly.
            const bool caseInsensitive = term.kind == SelectorTerm::Kind::Text ||
                                         term.kind == SelectorTerm::Kind::Accessible;
            term.pattern = QRegularExpression::fromWildcard(
                term.value, caseInsensitive ? Qt::CaseInsensitive : Qt::CaseSensitive,
                QRegularExpression::NonPathWildcardConversion);
            return term;
        }

        SelectorSegment parseSegment(QString raw, const QString &source) {
            SelectorSegment segment;
            static const QRegularExpression indexSuffix(QStringLiteral("@(-?\\d+)$"));
            if (const auto match = indexSuffix.match(raw); match.hasMatch()) {
                segment.index = match.captured(1).toInt();
                raw.chop(match.capturedLength(0));
            }
            for (const auto &part: raw.split(u'&')) {
                const QString trimmed = part.trimmed();
                if (trimmed.isEmpty()) {
                    throw DriverError(QStringLiteral("selector '%1' has an empty segment").arg(source));
                }
                segment.terms.append(parseTerm(trimmed, source));
            }
            return segment;
        }
    }

    Selector parseSelector(const QString &text) {
        Selector selector;
        selector.source = text;
        QString rest = text.trimmed();
        if (rest.startsWith(QStringLiteral("win:"))) {
            const auto slash = rest.indexOf(u'/');
            selector.window = (slash < 0 ? rest.mid(4) : rest.mid(4, slash - 4)).trimmed();
            rest = slash < 0 ? QString() : rest.mid(slash + 1).trimmed();
            if (selector.window.isEmpty()) {
                throw DriverError(QStringLiteral("selector '%1' names an empty window").arg(text));
            }
        }
        if (rest.isEmpty()) return selector;
        for (const auto &part: rest.split(QStringLiteral(" > "))) {
            selector.segments.append(parseSegment(part.trimmed(), text));
        }
        return selector;
    }
}
