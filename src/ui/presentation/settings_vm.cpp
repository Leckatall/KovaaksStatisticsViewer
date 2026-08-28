//
// Created by Lecka on 03/08/2026.
//

#include "settings_vm.h"
#include "series_expression_editor_model.h"

#include "app/contracts/expression_dsl.h"

namespace ksv::presentation {
    namespace {
        std::optional<application::Expression> decodeTransportExpression(const QString &text) {
            if (text.isEmpty()) return application::Expression{};
            const auto decoded = application::decodeExpressionDsl(text.toStdString());
            if (!decoded || !*decoded) return std::nullopt;
            return *decoded;
        }

        QVariantMap mutationMap(const application::MutationResult &result) {
            QVariantMap map;
            map["succeeded"] = result.succeeded();
            map["requiresReload"] = result.requiresReload;
            map["failure"] = result.failure ? QVariant(static_cast<int>(*result.failure)) : QVariant();
            map["createdId"] = result.createdId ? QVariant(QString::number(result.createdId->value)) : QVariant();
            map["createdAxisId"] = result.createdAxisId
                                     ? QVariant(QString::number(result.createdAxisId->value))
                                     : QVariant();
            QVariantList errors;
            for (const auto &error: result.errors)
                errors.append(QVariantMap{{"code", static_cast<int>(error.code)}, {"path", QString::fromStdString(error.path)}});
            map["validationErrors"] = errors;
            return map;
        }

        QVariantMap invalidMutationMap() {
            return {{"succeeded", false}, {"failure", "invalidRequest"}, {"requiresReload", false},
                    {"createdId", QVariant()}, {"createdAxisId", QVariant()}, {"validationErrors", QVariantList{}}};
        }
    }

    SettingsViewModel::SettingsViewModel(
        std::shared_ptr<application::ISettingsService> settings_service,
        std::shared_ptr<application::IProfileService> profile_service,
        std::shared_ptr<application::ISeriesManagementUseCase> series_management,
        QObject *parent) : QObject(parent),
                           m_settings_service(std::move(settings_service)),
                           m_profile_service(std::move(profile_service)),
                           m_series_management(std::move(series_management)),
                           m_kovaaks_dir([&] {
                               const auto dirs = m_settings_service->getKovaaksDirs();
                               return dirs.empty() ? QUrl{} : QUrl::fromLocalFile(QString::fromStdString(dirs.front()));
                           }()) {
        const QPointer<SettingsViewModel> self(this);
        m_profile_service->onProfileChanged([self] { if (self) emit self->profileLoadedChanged(); });
        m_series_management->onChanged([self] { if (self) { emit self->seriesConfigurationChanged(); emit self->pendingChangesChanged(); } });
    }

    void SettingsViewModel::setKovaaksDir(const QUrl &dir) {
        if (dir == m_kovaaks_dir) return;
        auto dirs = m_settings_service->getKovaaksDirs();
        const auto path = dir.toLocalFile().toStdString();
        if (dirs.empty()) dirs.push_back(path);
        else dirs.front() = path;
        m_settings_service->setKovaaksDirs(dirs);
        m_kovaaks_dir = dir;
        emit kovaaksDirChanged();
    }

    void SettingsViewModel::setProfilePath(const QUrl &path) {
        if (path == getProfilePath()) return;
        // Setting path triggers ProfileService to repoint the store and reload.
        m_settings_service->setProfilePath(path.toLocalFile().toStdString());
        emit profilePathChanged();
    }

    QVariantList SettingsViewModel::getAllSeriesConfigs() const {
        QVariantList result;
        for (const auto &config: m_series_management->getAll()) {
            result.append(QVariantMap{
                {"id", QString::number(config.id.value)},
                {"name", QString::fromStdString(config.presentation.name)},
                {"color", QColor(config.presentation.lineStyle.color.red, config.presentation.lineStyle.color.green,
                                  config.presentation.lineStyle.color.blue, config.presentation.lineStyle.color.alpha)},
                {"width", config.presentation.lineStyle.width},
                {"enabled", config.presentation.enabled},
                {"displayPosition", config.presentation.displayPosition},
                {"isPrimitive", config.isPrimitive()},
                {"expression", QString::fromStdString(application::encodeExpressionDsl(config.expression))},
                {"yAxisId", config.yAxisId ? QString::number(config.yAxisId->value) : QString()},
            });
        }
        return result;
    }

    QVariantMap SettingsViewModel::setSeriesEnabled(const QString &id, const bool enabled) {
        bool ok = false;
        const auto value = id.toULongLong(&ok);
        return ok ? mutationMap(m_series_management->setSeriesEnabled({value}, enabled)) : invalidMutationMap();
    }

    QVariantMap SettingsViewModel::createComputedSeries(const QString &name, const QColor &color, const double width,
                                                         const bool enabled, const QString &expression) {
        const auto parsed = decodeTransportExpression(expression);
        if (!parsed) return invalidMutationMap();
        return mutationMap(m_series_management->createComputed({
            {name.toStdString(),
             {{static_cast<uint8_t>(color.red()), static_cast<uint8_t>(color.green()),
               static_cast<uint8_t>(color.blue()), static_cast<uint8_t>(color.alpha())}, width},
             enabled},
            *parsed
        }));
    }

    QVariantMap SettingsViewModel::updateComputedSeries(const QString &id, const QString &name, const QColor &color,
                                                         const double width, const bool enabled,
                                                         const QString &expression) {
        bool ok = false;
        const auto value = id.toULongLong(&ok);
        if (!ok) return invalidMutationMap();
        const auto parsed = decodeTransportExpression(expression);
        if (!parsed) return invalidMutationMap();
        application::UpdateSeriesRequest request;
        request.id = {value};
        request.presentation = application::UpdatedSeriesPresentation{
            name.toStdString(),
            application::LineStyle{{static_cast<uint8_t>(color.red()), static_cast<uint8_t>(color.green()),
                                     static_cast<uint8_t>(color.blue()), static_cast<uint8_t>(color.alpha())}, width},
            enabled
        };
        request.expression = *parsed;
        return mutationMap(m_series_management->updateSeries(request));
    }

    QVariantMap SettingsViewModel::removeComputedSeries(const QString &id) {
        bool ok = false;
        const auto value = id.toULongLong(&ok);
        return ok ? mutationMap(m_series_management->removeComputed({value})) : invalidMutationMap();
    }

    QVariantMap SettingsViewModel::reorderSeries(const QString &id, const int displayPosition) {
        bool ok = false;
        const auto value = id.toULongLong(&ok);
        if (!ok || displayPosition < 0) return invalidMutationMap();
        return mutationMap(m_series_management->reorder({value}, static_cast<uint32_t>(displayPosition)));
    }

    SeriesExpressionEditorModel *SettingsViewModel::beginExpressionEdit(const QString &id) {
        auto *editor = new SeriesExpressionEditorModel();
        bool ok = false;
        const auto value = id.toULongLong(&ok);
        if (!ok) return editor;
        for (const auto &config: m_series_management->getAll()) {
            if (config.id.value == value) {
                editor->loadFrom(config.expression);
                break;
            }
        }
        return editor;
    }

    QVariantList SettingsViewModel::getAllAxes() const {
        QVariantList result;
        for (const auto &axis: m_series_management->getAllAxes()) {
            result.append(QVariantMap{
                {"id", QString::number(axis.id.value)},
                {"name", QString::fromStdString(axis.name)}
            });
        }
        return result;
    }

    QVariantMap SettingsViewModel::createAxis(const QString &name) {
        return mutationMap(m_series_management->createAxis({name.toStdString()}));
    }

    QVariantMap SettingsViewModel::updateSeriesAxis(const QString &id, const QString &axisId) {
        bool ok = false;
        const auto value = id.toULongLong(&ok);
        if (!ok) return invalidMutationMap();
        application::UpdateSeriesRequest request;
        request.id = {value};
        if (axisId.isEmpty()) {
            request.yAxisId = std::optional<application::AxisId>{std::nullopt};
        } else {
            bool axisOk = false;
            const auto axisValue = axisId.toULongLong(&axisOk);
            if (!axisOk) return invalidMutationMap();
            request.yAxisId = std::optional<application::AxisId>{application::AxisId{axisValue}};
        }
        return mutationMap(m_series_management->updateSeries(request));
    }

    QVariantMap SettingsViewModel::commitSeriesDraft() {
        const auto result = mutationMap(m_series_management->commitDraft());
        emit pendingChangesChanged();
        return result;
    }

    void SettingsViewModel::discardSeriesDraft() {
        m_series_management->discardDraft();
    }

}
