#include "selector.h"

#include <QRegularExpressionMatch>

namespace ksv::driver {
    namespace {
        // A selector character after escape/quote processing. `literal` characters
        // (escaped or quoted) never act as operators: & > @ : and the spaces of " > ".
        struct Unit {
            QChar ch;
            bool literal = false;
        };
        using Units = QList<Unit>;

        Units lex(const QString &text, const QString &source) {
            static const QString kEscapable = QStringLiteral("&>@:\\\"");
            Units units;
            bool quoted = false;
            for (qsizetype i = 0; i < text.size(); ++i) {
                const QChar ch = text[i];
                if (ch == u'\\' && i + 1 < text.size() && kEscapable.contains(text[i + 1])) {
                    units.append({text[++i], true});
                } else if (ch == u'"') {
                    quoted = !quoted;
                } else {
                    // An unknown escape keeps its backslash, so `\d` or a path stays as typed.
                    units.append({ch, quoted});
                }
            }
            if (quoted) throw DriverError(QStringLiteral("selector '%1' has an unterminated quote").arg(source));
            return units;
        }

        QString textOf(const Units &units) {
            QString text;
            text.reserve(units.size());
            for (const auto &unit: units) text.append(unit.ch);
            return text;
        }

        bool isOperator(const Units &units, const qsizetype at, const QChar ch) {
            return at >= 0 && at < units.size() && !units[at].literal && units[at].ch == ch;
        }

        Units trimmed(Units units) {
            while (!units.isEmpty() && !units.first().literal && units.first().ch.isSpace()) units.removeFirst();
            while (!units.isEmpty() && !units.last().literal && units.last().ch.isSpace()) units.removeLast();
            return units;
        }

        QList<Units> splitOn(const Units &units, const QString &separator) {
            QList<Units> parts;
            Units current;
            for (qsizetype i = 0; i < units.size(); ++i) {
                bool matches = i + separator.size() <= units.size();
                for (qsizetype k = 0; matches && k < separator.size(); ++k) {
                    matches = isOperator(units, i + k, separator[k]);
                }
                if (matches) {
                    parts.append(current);
                    current.clear();
                    i += separator.size() - 1;
                } else {
                    current.append(units[i]);
                }
            }
            parts.append(current);
            return parts;
        }

        SelectorTerm parseTerm(const Units &units, const QString &source) {
            static const QList<std::pair<QString, SelectorTerm::Kind>> prefixes{
                {QStringLiteral("name"), SelectorTerm::Kind::Name},
                {QStringLiteral("id"), SelectorTerm::Kind::Id},
                {QStringLiteral("text"), SelectorTerm::Kind::Text},
                {QStringLiteral("rawtext"), SelectorTerm::Kind::RawText},
                {QStringLiteral("type"), SelectorTerm::Kind::Type},
                {QStringLiteral("acc"), SelectorTerm::Kind::Accessible},
                {QStringLiteral("obj"), SelectorTerm::Kind::Object},
            };
            SelectorTerm term;
            Units value = units;
            for (const auto &[prefix, kind]: prefixes) {
                if (!isOperator(units, prefix.size(), u':')) continue;
                bool plain = true;
                for (qsizetype k = 0; plain && k < prefix.size(); ++k) plain = isOperator(units, k, prefix[k]);
                if (!plain) continue;
                term.kind = kind;
                value = units.mid(prefix.size() + 1);
                break;
            }
            term.value = textOf(value);
            if (term.value.isEmpty()) {
                throw DriverError(QStringLiteral("selector '%1' has an empty term").arg(source));
            }
            // Visible strings are matched case-insensitively (text: and acc: without
            // their mnemonic markers); identifiers exactly.
            const bool visibleText = term.kind == SelectorTerm::Kind::Text || term.kind == SelectorTerm::Kind::RawText ||
                                     term.kind == SelectorTerm::Kind::Accessible;
            const bool mnemonicFree = term.kind == SelectorTerm::Kind::Text ||
                                      term.kind == SelectorTerm::Kind::Accessible;
            term.pattern = QRegularExpression::fromWildcard(
                mnemonicFree ? stripMnemonics(term.value) : term.value,
                visibleText ? Qt::CaseInsensitive : Qt::CaseSensitive,
                QRegularExpression::NonPathWildcardConversion);
            return term;
        }

        SelectorSegment parseSegment(Units units, const QString &source) {
            SelectorSegment segment;
            units = trimmed(units);
            // A trailing unescaped @N (or @-N) picks one match of the segment.
            static const QRegularExpression indexSuffix(QStringLiteral("^-?\\d+$"));
            for (qsizetype at = units.size() - 1; at >= 0; --at) {
                if (!isOperator(units, at, u'@')) continue;
                const QString suffix = textOf(units.mid(at + 1));
                if (indexSuffix.match(suffix).hasMatch()) {
                    segment.index = suffix.toInt();
                    units = units.mid(0, at);
                }
                break;
            }
            for (const auto &part: splitOn(units, QStringLiteral("&"))) {
                const Units term = trimmed(part);
                if (term.isEmpty()) {
                    throw DriverError(QStringLiteral("selector '%1' has an empty segment").arg(source));
                }
                segment.terms.append(parseTerm(term, source));
            }
            return segment;
        }
    }

    bool Selector::targetsObjects() const {
        for (const auto &segment: segments) {
            for (const auto &term: segment.terms) {
                if (term.kind == SelectorTerm::Kind::Object) return true;
            }
        }
        return false;
    }

    QString stripMnemonics(const QString &text) {
        QString result;
        result.reserve(text.size());
        for (qsizetype i = 0; i < text.size(); ++i) {
            if (text[i] == u'&' && i + 1 < text.size()) {
                if (text[i + 1] == u'&') {
                    result.append(u'&');
                    ++i;
                    continue;
                }
                if (!text[i + 1].isSpace()) continue;
            }
            result.append(text[i]);
        }
        return result;
    }

    Selector parseSelector(const QString &text) {
        Selector selector;
        selector.source = text;
        Units rest = trimmed(lex(text, text));
        const QString head = textOf(rest.mid(0, 4));
        if (head == u"win:" && isOperator(rest, 3, u':')) {
            qsizetype slash = 4;
            while (slash < rest.size() && !isOperator(rest, slash, u'/')) ++slash;
            selector.window = textOf(trimmed(rest.mid(4, slash - 4)));
            rest = slash < rest.size() ? trimmed(rest.mid(slash + 1)) : Units();
            if (selector.window.isEmpty()) {
                throw DriverError(QStringLiteral("selector '%1' names an empty window").arg(text));
            }
        }
        if (rest.isEmpty()) return selector;
        for (const auto &part: splitOn(rest, QStringLiteral(" > "))) {
            selector.segments.append(parseSegment(part, text));
        }
        return selector;
    }
}
