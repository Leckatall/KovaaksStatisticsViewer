#pragma once

#include <QList>
#include <QString>
#include <QVariant>
#include <optional>

namespace ksv::qt_data {
    class SettingsService;
}

namespace ksv::driver {
    // Session composition beyond the seeded dataset (see tools/driver/README.md,
    // "Composing a session"). ksv_driver applies these in this order, after
    // seeding and before App is built: overlays, driver defaults, settings.

    // Copies `overlayDir`'s tree onto `dataRoot`, replacing files and creating
    // directories. Returns an empty string on success, otherwise what went wrong.
    [[nodiscard]] QString applyOverlay(const QString &overlayDir, const QString &dataRoot);

    // One `--setting KEY=VALUE`. VALUE is JSON when it parses, otherwise a
    // string; `KEY=` (nothing after '=') removes the key.
    struct SettingAssignment {
        QString key;
        std::optional<QVariant> value;  // nullopt = remove
    };

    // Parses "KEY=VALUE" into `out`. Returns an empty string on success.
    [[nodiscard]] QString parseSettingAssignment(const QString &text, SettingAssignment &out);

    // Writes `assignment` into the session's QSettings. The URL-typed keys
    // SettingsService owns (file/kovaaksDirs, file/kovaaks, file/profilePath)
    // are written in its own encoding, so the app reads them back exactly as if
    // it had stored them. Returns an empty string on success.
    [[nodiscard]] QString applySetting(const SettingAssignment &assignment, qt_data::SettingsService &service);
}
