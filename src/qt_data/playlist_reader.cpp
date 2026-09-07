#include "playlist_reader.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include <unordered_set>
#include <utility>

namespace ksv::qt_data {
    application::PlaylistReadResult PlaylistReader::read(const std::string &path) const {
        QFile file(QString::fromStdString(path));
        if (!file.open(QIODevice::ReadOnly))
            return {std::nullopt, application::PlaylistImportFailure::FileUnreadable};

        QJsonParseError parseError;
        const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject())
            return {std::nullopt, application::PlaylistImportFailure::MalformedJson};

        const auto root = document.object();
        std::optional<std::string> name;
        if (const auto playlistName = root["playlistName"]; playlistName.isString()) {
            const auto text = playlistName.toString();
            if (!text.isEmpty()) name = text.toStdString();
        }

        std::vector<std::string> scenarioNames;
        std::vector<int> skippedDuplicateIndices;
        std::unordered_set<std::string> seen;
        const auto scenarioList = root["scenarioList"].toArray();
        for (qsizetype i = 0; i < scenarioList.size(); ++i) {
            if (!scenarioList[i].isObject()) continue;
            const auto scenarioName = scenarioList[i].toObject()["scenario_name"];
            if (!scenarioName.isString()) continue;
            const auto text = scenarioName.toString();
            if (text.trimmed().isEmpty()) continue;
            const auto nameUtf8 = text.toStdString();
            if (!seen.insert(nameUtf8).second) {
                skippedDuplicateIndices.push_back(static_cast<int>(i));
                continue;
            }
            scenarioNames.push_back(nameUtf8);
        }

        if (scenarioNames.empty())
            return {std::nullopt, application::PlaylistImportFailure::NoUsableScenarioList};
        return {application::PlaylistSeed{name, std::move(scenarioNames), std::move(skippedDuplicateIndices)},
                std::nullopt};
    }
}
