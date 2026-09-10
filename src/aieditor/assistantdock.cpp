/*
    SPDX-FileCopyrightText: 2026 Kdenlive AI Editor contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "assistantdock.hpp"

#include "core.h"
#include "editplanexecutor.hpp"
#include "localtimelinetranscriber.hpp"
#include "mainwindow.h"
#include "timeline2/view/timelinewidget.h"

#include <KLocalizedString>
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTime>
#include <QVBoxLayout>

namespace Kdenlive {
namespace AiEditor {

AssistantDock::AssistantDock(MainWindow *mainWindow)
    : QWidget(mainWindow)
    , m_mainWindow(mainWindow)
    , m_client(new AiProviderClient(this))
{
    auto *layout = new QVBoxLayout(this);
    auto *providerForm = new QFormLayout;

    m_provider = new QComboBox(this);
    for (AiProvider provider : {AiProvider::OpenRouter, AiProvider::OpenAI, AiProvider::Anthropic}) {
        m_provider->addItem(AiProviderClient::displayName(provider), int(provider));
    }
    providerForm->addRow(i18n("Provider:"), m_provider);

    m_model = new QLineEdit(this);
    m_model->setClearButtonEnabled(true);
    providerForm->addRow(i18n("Model:"), m_model);
    layout->addLayout(providerForm);

    m_keyStatus = new QLabel(this);
    m_keyStatus->setWordWrap(true);
    layout->addWidget(m_keyStatus);

    auto *privacy = new QLabel(
        i18n("Privacy: media files are never uploaded. When audio analysis is enabled, Whisper runs locally and only the timestamped transcript is sent."),
        this);
    privacy->setWordWrap(true);
    layout->addWidget(privacy);

    auto *presetForm = new QFormLayout;
    m_preset = new QComboBox(this);
    m_preset->addItem(i18n("Custom instruction"), QString());
    m_preset->addItem(i18n("Clean work meeting"),
                      QStringLiteral("Silencie todas as conversas que não sejam relacionadas a Salesforce, inteligência artificial ou trabalho. "
                                     "Nos intervalos sem diálogo maiores que 2 segundos, acelere para que durem 0,5 segundo. Preserve o tom do áudio."));
    m_preset->addItem(i18n("Mute off-topic dialogue"),
                      QStringLiteral("Silencie todas as falas que não sejam relacionadas a Salesforce, inteligência artificial ou trabalho. "
                                     "Não remova nem acelere essas partes; apenas deixe o áudio mudo."));
    m_preset->addItem(i18n("Compress silent gaps"),
                      QStringLiteral("Acelere todos os intervalos sem diálogo maiores que 2 segundos para que cada um dure 0,5 segundo. "
                                     "Preserve o tom do áudio e não altere trechos com fala."));
    m_preset->addItem(i18n("Exact-duration speed change"),
                      QStringLiteral("Do tempo 00:00:00 até 00:00:10, acelere para que dure exatamente 2 segundos e preserve o tom do áudio."));
    presetForm->addRow(i18n("Ready prompt:"), m_preset);
    layout->addLayout(presetForm);

    layout->addWidget(new QLabel(i18n("Describe the edit:"), this));
    m_prompt = new QPlainTextEdit(this);
    m_prompt->setPlaceholderText(i18n("Example: From 00:10 to 02:10, speed it up so the result lasts exactly 40 seconds."));
    m_prompt->setMinimumHeight(90);
    layout->addWidget(m_prompt);

    m_analyzeAudio = new QCheckBox(i18n("Analyze timeline audio locally with Whisper"), this);
    m_analyzeAudio->setToolTip(i18n("Needed for requests about dialogue topics or silent gaps. The media stays on this computer."));
    layout->addWidget(m_analyzeAudio);

    auto *requestButtons = new QHBoxLayout;
    m_generate = new QPushButton(i18n("Generate plan"), this);
    m_cancel = new QPushButton(i18n("Cancel"), this);
    m_cancel->setEnabled(false);
    requestButtons->addWidget(m_generate);
    requestButtons->addWidget(m_cancel);
    layout->addLayout(requestButtons);

    layout->addWidget(new QLabel(i18n("Plan preview:"), this));
    m_preview = new QPlainTextEdit(this);
    m_preview->setReadOnly(true);
    m_preview->setPlaceholderText(i18n("The proposed changes will appear here. Nothing changes until you choose Apply."));
    layout->addWidget(m_preview, 1);

    m_status = new QLabel(this);
    m_status->setWordWrap(true);
    layout->addWidget(m_status);

    auto *applyButtons = new QHBoxLayout;
    m_apply = new QPushButton(i18n("Apply"), this);
    m_discard = new QPushButton(i18n("Discard"), this);
    m_apply->setEnabled(false);
    m_discard->setEnabled(false);
    applyButtons->addWidget(m_apply);
    applyButtons->addWidget(m_discard);
    layout->addLayout(applyButtons);

    connect(m_provider, &QComboBox::currentIndexChanged, this, &AssistantDock::updateProvider);
    connect(m_preset, &QComboBox::currentIndexChanged, this, [this](int index) {
        const QString prompt = m_preset->itemData(index).toString();
        if (!prompt.isEmpty()) {
            m_prompt->setPlainText(prompt);
        }
        m_analyzeAudio->setChecked(index >= 1 && index <= 3);
    });
    connect(m_generate, &QPushButton::clicked, this, &AssistantDock::generatePlan);
    connect(m_cancel, &QPushButton::clicked, this, [this]() {
        m_client->cancel();
        m_transcriber->cancel();
    });
    connect(m_apply, &QPushButton::clicked, this, &AssistantDock::applyPlan);
    connect(m_discard, &QPushButton::clicked, this, &AssistantDock::discardPlan);
    connect(m_client, &AiProviderClient::busyChanged, this, &AssistantDock::setBusy);
    connect(m_client, &AiProviderClient::planReady, this, &AssistantDock::showPlan);
    connect(m_client, &AiProviderClient::errorOccurred, this, [this](const QString &message) { setStatus(message, true); });
    connect(m_client, &AiProviderClient::requestCancelled, this, [this]() { setStatus(i18n("Request cancelled.")); });

    m_transcriber = new LocalTimelineTranscriber(this);
    connect(m_transcriber, &LocalTimelineTranscriber::busyChanged, this, &AssistantDock::setBusy);
    connect(m_transcriber, &LocalTimelineTranscriber::statusChanged, this, [this](const QString &message) { setStatus(message); });
    connect(m_transcriber, &LocalTimelineTranscriber::errorOccurred, this, [this](const QString &message) { setStatus(message, true); });
    connect(m_transcriber, &LocalTimelineTranscriber::cancelled, this, [this]() { setStatus(i18n("Local transcription cancelled.")); });
    connect(m_transcriber, &LocalTimelineTranscriber::transcriptReady, this, [this](const QString &transcript) {
        setStatus(i18n("Local transcript ready. Requesting the edit plan…"));
        requestProviderPlan(transcript);
    });

    updateProvider();
}

AiProvider AssistantDock::selectedProvider() const
{
    return AiProvider(m_provider->currentData().toInt());
}

QByteArray AssistantDock::selectedApiKey() const
{
    const QByteArray variable = AiProviderClient::environmentVariable(selectedProvider()).toUtf8();
    return qgetenv(variable.constData());
}

void AssistantDock::updateProvider()
{
    m_model->setText(AiProviderClient::defaultModel(selectedProvider()));
    updateCredentialStatus();
    discardPlan();
}

void AssistantDock::updateCredentialStatus()
{
    const QString variable = AiProviderClient::environmentVariable(selectedProvider());
    if (selectedApiKey().isEmpty()) {
        m_keyStatus->setText(i18n("Not connected. Set the %1 environment variable, then restart Kdenlive.", variable));
    } else {
        m_keyStatus->setText(i18n("Connected: key loaded securely from %1.", variable));
    }
}

void AssistantDock::generatePlan()
{
    updateCredentialStatus();
    if (m_model->text().trimmed().isEmpty()) {
        setStatus(i18n("Choose a model before sending the request."), true);
        return;
    }
    if (selectedApiKey().trimmed().isEmpty()) {
        setStatus(i18n("The API key is missing. Set %1 and restart Kdenlive.", AiProviderClient::environmentVariable(selectedProvider())), true);
        return;
    }
    if (m_prompt->toPlainText().trimmed().isEmpty()) {
        setStatus(i18n("Describe the edit you want before sending the request."), true);
        return;
    }
    TimelineWidget *timelineWidget = m_mainWindow->getCurrentTimeline();
    if (!timelineWidget || !timelineWidget->model()) {
        setStatus(i18n("Open a project timeline before requesting an edit."), true);
        return;
    }
    discardPlan();
    m_pendingPrompt = m_prompt->toPlainText();
    if (m_analyzeAudio->isChecked()) {
        m_transcriber->start(timelineWidget->model(), pCore->getCurrentFps());
    } else {
        requestProviderPlan();
    }
}

void AssistantDock::requestProviderPlan(const QString &transcript)
{
    TimelineWidget *timelineWidget = m_mainWindow->getCurrentTimeline();
    if (!timelineWidget || !timelineWidget->model()) {
        setStatus(i18n("The active timeline is no longer available."), true);
        return;
    }
    m_client->requestPlan(selectedProvider(), m_model->text(), selectedApiKey(), m_pendingPrompt, timelineWidget->model()->duration(), pCore->getCurrentFps(),
                          transcript);
}

void AssistantDock::showPlan(const QByteArray &planJson)
{
    const auto parsed = parseEditPlan(planJson);
    if (!parsed.isValid()) {
        setStatus(parsed.error, true);
        return;
    }

    TimelineWidget *timelineWidget = m_mainWindow->getCurrentTimeline();
    if (!timelineWidget || !timelineWidget->model()) {
        setStatus(i18n("The active timeline is no longer available."), true);
        return;
    }

    m_plan = parsed.plan;
    m_hasPlan = true;
    const double fps = pCore->getCurrentFps();
    QStringList preview;
    for (qsizetype index = 0; index < m_plan.operations.size(); ++index) {
        const EditOperation &edit = m_plan.operations.at(index);
        if (edit.type == EditOperationType::RetimeRange) {
            const auto &operation = edit.retimeRange;
            const int sourceDuration = operation.endFrame - operation.startFrame;
            preview << i18n("%1. Speed up range\n   %2 to %3 · %4 → %5 · %6x · preserve pitch: %7", index + 1, formatFrames(operation.startFrame, fps),
                            formatFrames(operation.endFrame, fps), formatFrames(sourceDuration, fps), formatFrames(operation.targetDurationFrames, fps),
                            QString::number(operation.speedMultiplier(), 'f', 3), operation.preservePitch ? i18n("Yes") : i18n("No"));
        } else {
            const auto &operation = edit.muteRange;
            preview << i18n("%1. Mute audio\n   %2 to %3 · duration %4", index + 1, formatFrames(operation.startFrame, fps),
                            formatFrames(operation.endFrame, fps), formatFrames(operation.endFrame - operation.startFrame, fps));
        }
    }
    m_preview->setPlainText(preview.join(QStringLiteral("\n\n")));
    m_discard->setEnabled(true);

    const auto preflight = EditPlanExecutor::preflight(timelineWidget->model(), m_plan);
    if (!preflight.isValid()) {
        m_apply->setEnabled(false);
        setStatus(preflight.error, true);
        return;
    }
    m_apply->setEnabled(true);
    setStatus(i18n("Plan validated. Review it, then choose Apply."));
}

void AssistantDock::applyPlan()
{
    TimelineWidget *timelineWidget = m_mainWindow->getCurrentTimeline();
    if (!m_hasPlan || !timelineWidget || !timelineWidget->model()) {
        setStatus(i18n("There is no valid plan to apply to the active timeline."), true);
        return;
    }
    const auto result = EditPlanExecutor::apply(timelineWidget->model(), m_plan);
    if (!result.isValid()) {
        setStatus(result.error, true);
        return;
    }
    m_apply->setEnabled(false);
    m_discard->setEnabled(false);
    m_hasPlan = false;
    setStatus(i18n("Edit applied. Use Undo once to restore the previous timeline."));
}

void AssistantDock::discardPlan()
{
    m_hasPlan = false;
    m_plan = {};
    m_pendingPrompt.clear();
    m_preview->clear();
    m_apply->setEnabled(false);
    m_discard->setEnabled(false);
    if (!m_client->isBusy()) {
        setStatus(QString());
    }
}

void AssistantDock::setStatus(const QString &message, bool error)
{
    m_status->setText(error && !message.isEmpty() ? i18n("Error: %1", message) : message);
}

void AssistantDock::setBusy(bool busy)
{
    Q_UNUSED(busy)
    const bool anyBusy = m_client->isBusy() || (m_transcriber && m_transcriber->isBusy());
    m_generate->setEnabled(!anyBusy);
    m_provider->setEnabled(!anyBusy);
    m_model->setEnabled(!anyBusy);
    m_preset->setEnabled(!anyBusy);
    m_analyzeAudio->setEnabled(!anyBusy);
    m_cancel->setEnabled(anyBusy);
    if (anyBusy && m_client->isBusy()) {
        setStatus(i18n("Waiting for the AI provider…"));
    }
}

QString AssistantDock::formatFrames(int frames, double fps) const
{
    const qint64 milliseconds = qRound64(double(frames) * 1000.0 / fps);
    const qint64 hours = milliseconds / 3600000;
    const QTime time = QTime::fromMSecsSinceStartOfDay(int(milliseconds % 86400000));
    return QStringLiteral("%1:%2").arg(hours, 2, 10, QLatin1Char('0')).arg(time.toString(QStringLiteral("mm:ss.zzz")));
}

} // namespace AiEditor
} // namespace Kdenlive
