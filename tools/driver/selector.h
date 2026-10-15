#pragma once

#include <QJsonObject>
#include <QList>
#include <QRegularExpression>
#include <QString>
#include <optional>
#include <stdexcept>

namespace ksv::driver {
    // A command failure reported back to the client: a message plus optional
    // structured detail (e.g. the candidates of an ambiguous selector).
    class DriverError : public std::runtime_error {
    public:
        explicit DriverError(const QString &message, QJsonObject details = {})
            : std::runtime_error(message.toStdString()), m_message(message), m_details(std::move(details)) {}

        [[nodiscard]] const QString &message() const { return m_message; }
        [[nodiscard]] const QJsonObject &details() const { return m_details; }

    private:
        QString m_message;
        QJsonObject m_details;
    };

    // Selector grammar (see tools/driver/README.md):
    //   [win:<window>/] segment ( " > " segment )*
    //   segment := term ( "&" term )* [ "@" N ]
    //   term    := name-glob | name:<glob> | id:<glob> | text:<glob> | rawtext:<glob>
    //            | type:<glob> | acc:<glob> | obj:<glob>
    // Later segments match descendants of the previous segment's matches; @N picks
    // the Nth (0-based) match of that segment. Inside a term, `\` escapes & > @ : " \
    // and "..." quotes a literal run (text:"Save & close"). text: and acc: ignore
    // mnemonic markers (&File matches File; && is a literal &); rawtext: does not.
    // obj: matches non-visual QML objects (Action, Timer, dialogs) by objectName or id.
    struct SelectorTerm {
        enum class Kind { Name, Id, Text, RawText, Type, Accessible, Object };
        Kind kind = Kind::Name;
        QString value;
        QRegularExpression pattern;
    };

    struct SelectorSegment {
        QList<SelectorTerm> terms;
        std::optional<int> index;
    };

    struct Selector {
        QString source;
        QString window;  // empty = every visible window
        QList<SelectorSegment> segments;  // empty = the window's root

        // Whether any term is obj: (the scene walk then includes non-visual objects).
        [[nodiscard]] bool targetsObjects() const;
    };

    [[nodiscard]] Selector parseSelector(const QString &text);

    // Drops mnemonic markers the way a label displays them: a single & before a
    // character is removed and && becomes &. A trailing & or one before a space stays.
    [[nodiscard]] QString stripMnemonics(const QString &text);
}
