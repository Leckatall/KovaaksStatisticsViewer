#include "presentation/capture/capture_vm.h"

#include <QDebug>
#include <QPointer>

#include <algorithm>
#include <filesystem>
#include <iterator>
#include <utility>

#include "capture/capture_appearance.h"
#include "capture/capture_sources.h"
#include "components/graph_canvas.h"
#include "presentation/benchmarks/tracking/benchmark_tracking_vm.h"
#include "presentation/graphs/completion_history_vm.h"
#include "presentation/graphs/graph_vm.h"
#include "presentation/graphs/playtime_graph_vm.h"

namespace ksv::presentation {
    namespace {
        constexpr int kFeedbackExpiryMs = 4000;

        struct KeyEntry {
            const char *key;
            ui::CaptureTarget target;
        };

        constexpr KeyEntry kKeys[] = {
            {"run", ui::CaptureTarget::Run},
            {"dailyPlaytime", ui::CaptureTarget::DailyPlaytime},
            {"scenarioHistory", ui::CaptureTarget::ScenarioHistory},
            {"benchmarkRank", ui::CaptureTarget::BenchmarkRank},
            {"benchmarkRecentRank", ui::CaptureTarget::BenchmarkRecentRank},
            {"benchmarkPlaytime", ui::CaptureTarget::BenchmarkPlaytime},
            {"benchmarkTable", ui::CaptureTarget::BenchmarkTable},
        };

        BenchmarkHistoryViewModel *historyFor(const CaptureLiveViewModels &live, const ui::CaptureTarget target) {
            if (!live.tracking) return nullptr;
            switch (target) {
                case ui::CaptureTarget::BenchmarkRank: return live.tracking->rankHistory();
                case ui::CaptureTarget::BenchmarkRecentRank: return live.tracking->recentRankHistory();
                case ui::CaptureTarget::BenchmarkPlaytime: return live.tracking->playtimeHistory();
                default: return nullptr;
            }
        }

        std::optional<ui::GraphCaptureContext> contextFor(const CaptureLiveViewModels &live,
                                                          const ui::CaptureTarget target) {
            switch (target) {
                case ui::CaptureTarget::Run:
                    if (live.graph) return ui::runCaptureContext(*live.graph);
                    break;
                case ui::CaptureTarget::DailyPlaytime:
                    if (live.playtime) return ui::dailyPlaytimeCaptureContext(*live.playtime);
                    break;
                case ui::CaptureTarget::ScenarioHistory:
                    if (live.history) return ui::scenarioHistoryCaptureContext(*live.history);
                    break;
                case ui::CaptureTarget::BenchmarkRank:
                case ui::CaptureTarget::BenchmarkRecentRank:
                case ui::CaptureTarget::BenchmarkPlaytime:
                    if (const auto *history = historyFor(live, target))
                        return ui::benchmarkHistoryCaptureContext(live.tracking->selectedName(), *history);
                    break;
                case ui::CaptureTarget::BenchmarkTable:
                    break;
            }
            return std::nullopt;
        }
    }

    CaptureViewModel::CaptureViewModel(std::shared_ptr<application::IImageOutputUseCase> output,
                                       CaptureLiveViewModels live, ui::CaptureRenderFunction render, QObject *parent)
        : QObject(parent),
          m_live(live),
          m_feedback(new ui::CaptureFeedback(kFeedbackExpiryMs, this)),
          m_coordinator(new ui::CaptureCoordinator(std::move(output), std::move(render), m_feedback, this)) {
        connect(m_feedback, &ui::CaptureFeedback::changed, this, [this] {
            emit feedbackKindChanged();
            emit feedbackTextChanged();
        });
        connect(m_coordinator, &ui::CaptureCoordinator::previewClosed, this, &CaptureViewModel::previewClosed);
        connect(m_coordinator, &ui::CaptureCoordinator::previewChanged, this, [this] {
            emit previewOpenChanged();
            emit previewStateChanged();
            emit previewTargetNameChanged();
            emit previewImageChanged();
            emit outputWidthChanged();
            emit outputHeightChanged();
            emit percentChanged();
            emit includeSummaryChanged();
            emit summaryAvailableChanged();
            emit isGraphChanged();
            emit keepAspectChanged();
            emit widthTextChanged();
            emit heightTextChanged();
            emit widthErrorChanged();
            emit heightErrorChanged();
            emit previewErrorChanged();
            emit canOutputChanged();
            emit optionsLockedChanged();
            emit suggestedFileNameChanged();
        });
    }

    std::optional<CaptureViewModel::Resolved> CaptureViewModel::resolve(const QString &key, QObject *canvas) const {
        const auto entry = std::ranges::find_if(kKeys, [&](const KeyEntry &candidate) {
            return key == QLatin1StringView(candidate.key);
        });
        if (entry == std::end(kKeys)) return std::nullopt;

        const ui::CaptureTarget target = entry->target;
        if (target == ui::CaptureTarget::BenchmarkTable) {
            return Resolved{target, [this]() -> std::optional<ui::CaptureContent> {
                if (!m_appearance || !m_live.tracking) return std::nullopt;
                if (auto snapshot = ui::captureTable(*m_live.tracking, *m_appearance)) return std::move(*snapshot);
                return std::nullopt;
            }};
        }
        return Resolved{target, [this, target, canvas = QPointer<QObject>(canvas)]() -> std::optional<ui::CaptureContent> {
            const auto *graphCanvas = qobject_cast<ui::GraphCanvas *>(canvas.data());
            const auto context = contextFor(m_live, target);
            if (!m_appearance || !graphCanvas || !context) return std::nullopt;
            if (auto snapshot = ui::captureGraph(target, *graphCanvas, *context, *m_appearance))
                return std::move(*snapshot);
            return std::nullopt;
        }};
    }

    bool CaptureViewModel::previewOpen() const { return m_coordinator->previewOpen(); }

    QString CaptureViewModel::previewState() const {
        using State = ui::CaptureCoordinator::PreviewState;
        switch (m_coordinator->previewState()) {
            case State::Ready: return QStringLiteral("ready");
            case State::Invalid: return QStringLiteral("invalid");
            case State::Failed: return QStringLiteral("failed");
            case State::Rendering: break;
        }
        return QStringLiteral("rendering");
    }

    QString CaptureViewModel::previewTargetName() const { return m_coordinator->previewTargetName(); }
    QImage CaptureViewModel::previewImage() const { return m_coordinator->previewImage(); }
    int CaptureViewModel::outputWidth() const { return m_coordinator->outputSize().width(); }
    int CaptureViewModel::outputHeight() const { return m_coordinator->outputSize().height(); }
    int CaptureViewModel::percent() const { return m_coordinator->percent(); }
    bool CaptureViewModel::includeSummary() const { return m_coordinator->includeSummary(); }
    bool CaptureViewModel::summaryAvailable() const { return m_coordinator->summaryAvailable(); }
    bool CaptureViewModel::isGraph() const { return m_coordinator->isGraph(); }
    bool CaptureViewModel::keepAspect() const { return m_coordinator->keepAspect(); }
    QString CaptureViewModel::widthText() const { return m_coordinator->widthText(); }
    QString CaptureViewModel::heightText() const { return m_coordinator->heightText(); }
    QString CaptureViewModel::widthError() const { return m_coordinator->widthError(); }
    QString CaptureViewModel::heightError() const { return m_coordinator->heightError(); }
    QString CaptureViewModel::previewError() const { return m_coordinator->previewError(); }
    bool CaptureViewModel::canOutput() const { return m_coordinator->canOutput(); }
    bool CaptureViewModel::optionsLocked() const { return m_coordinator->optionsLocked(); }
    QString CaptureViewModel::suggestedFileName() const { return m_coordinator->suggestedFileName(); }

    QString CaptureViewModel::feedbackKind() const {
        switch (m_feedback->kind()) {
            case ui::CaptureFeedback::Kind::Success: return QStringLiteral("success");
            case ui::CaptureFeedback::Kind::Notice: return QStringLiteral("notice");
            case ui::CaptureFeedback::Kind::Failure: return QStringLiteral("failure");
            case ui::CaptureFeedback::Kind::None: break;
        }
        return QStringLiteral("none");
    }

    QString CaptureViewModel::feedbackText() const { return m_feedback->text(); }

    void CaptureViewModel::useTheme(QObject *theme, const QFont &font) {
        if (theme) m_appearance = ui::captureAppearanceFromTheme(*theme, font);
    }

    void CaptureViewModel::quickCopy(const QString &key, QObject *canvas) {
        const auto resolved = resolve(key, canvas);
        if (!resolved) {
            qWarning() << "Unknown capture key" << key;
            return;
        }
        m_coordinator->quickCopy(resolved->target, resolved->source);
    }

    void CaptureViewModel::openPreview(const QString &key, QObject *canvas) {
        const auto resolved = resolve(key, canvas);
        if (!resolved) {
            qWarning() << "Unknown capture key" << key;
            return;
        }
        m_coordinator->openPreview(resolved->target, resolved->source);
    }

    void CaptureViewModel::closePreview() { m_coordinator->closePreview(); }
    void CaptureViewModel::setPercent(const int percent) { m_coordinator->setPercent(percent); }
    void CaptureViewModel::setIncludeSummary(const bool include) { m_coordinator->setIncludeSummary(include); }
    void CaptureViewModel::setKeepAspect(const bool keep) { m_coordinator->setKeepAspect(keep); }
    void CaptureViewModel::commitWidth(const QString &text) { m_coordinator->commitDimension(Qt::Horizontal, text); }
    void CaptureViewModel::commitHeight(const QString &text) { m_coordinator->commitDimension(Qt::Vertical, text); }
    void CaptureViewModel::retryRender() { m_coordinator->retryRender(); }
    void CaptureViewModel::copyPreview() { m_coordinator->copyPreview(); }
    bool CaptureViewModel::beginExport() { return m_coordinator->beginExport(); }
    void CaptureViewModel::cancelExport() { m_coordinator->cancelExport(); }

    void CaptureViewModel::confirmExport(const QUrl &destination) {
        m_coordinator->confirmExport(std::filesystem::path(destination.toLocalFile().toStdU16String()));
    }

    void CaptureViewModel::dismissFeedback() { m_feedback->dismiss(); }
}
