/*
    SPDX-FileCopyrightText: 2026 Kdenlive AI Editor contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "assistantdock.hpp"

#include "core.h"
#include "mainwindow.h"
#include "retimerangeexecutor.hpp"
#include "timeline2/view/timelinewidget.h"

#include <KLocalizedString>
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

    auto *privacy = new QLabel(i18n("Privacy: only your instruction, project FPS, and timeline duration are sent. Media files are not uploaded."), this);
    privacy->setWordWrap(true);
    layout->addWidget(privacy);

    layout->addWidget(new QLabel(i18n("Describe the edit:"), this));
    m_prompt = new QPlainTextEdit(this);
    m_prompt->setPlaceholderText(i18n("Example: From 00:10 to 02:10, speed it up so the result lasts exactly 40 seconds."));
    m_prompt->setMinimumHeight(90);
    layout->addWidget(m_prompt);

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
    connect(m_generate, &QPushButton::clicked, this, &AssistantDock::generatePlan);
    connect(m_cancel, &QPushButton::clicked, m_client, &AiProviderClient::cancel);
    connect(m_apply, &QPushButton::clicked, this, &AssistantDock::applyPlan);
    connect(m_discard, &QPushButton::clicked, this, &AssistantDock::discardPlan);
    connect(m_client, &AiProviderClient::busyChanged, this, &AssistantDock::setBusy);
    connect(m_client, &AiProviderClient::planReady, this, &AssistantDock::showPlan);
    connect(m_client, &AiProviderClient::errorOccurred, this, [this](const QString &message) { setStatus(message, true); });
    connect(m_client, &AiProviderClient::requestCancelled, this, [this]() { setStatus(i18n("Request cancelled.")); });

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
    TimelineWidget *timelineWidget = m_mainWindow->getCurrentTimeline();
    if (!timelineWidget || !timelineWidget->model()) {
        setStatus(i18n("Open a project timeline before requesting an edit."), true);
        return;
    }
    discardPlan();
    m_client->requestPlan(selectedProvider(), m_model->text(), selectedApiKey(), m_prompt->toPlainText(), timelineWidget->model()->duration(),
                          pCore->getCurrentFps());
}

void AssistantDock::showPlan(const QByteArray &planJson)
{
    const auto parsed = parseEditPlan(planJson);
    if (!parsed.isValid() || parsed.plan.operations.size() != 1) {
        setStatus(parsed.isValid() ? i18n("This version can apply exactly one operation at a time.") : parsed.error, true);
        return;
    }

    TimelineWidget *timelineWidget = m_mainWindow->getCurrentTimeline();
    if (!timelineWidget || !timelineWidget->model()) {
        setStatus(i18n("The active timeline is no longer available."), true);
        return;
    }

    m_plan = parsed.plan;
    m_hasPlan = true;
    const RetimeRangeOperation &operation = m_plan.operations.constFirst().retimeRange;
    const double fps = pCore->getCurrentFps();
    const int sourceDuration = operation.endFrame - operation.startFrame;
    m_preview->setPlainText(
        i18n("Operation: speed up a range\nRange: %1 to %2 (frames %3–%4)\nOriginal duration: %5\nNew duration: %6\nSpeed: %7x\nPreserve audio pitch: %8",
             formatFrames(operation.startFrame, fps), formatFrames(operation.endFrame, fps), operation.startFrame, operation.endFrame,
             formatFrames(sourceDuration, fps), formatFrames(operation.targetDurationFrames, fps), QString::number(operation.speedMultiplier(), 'f', 3),
             operation.preservePitch ? i18n("Yes") : i18n("No")));
    m_discard->setEnabled(true);

    if (operation.targetDurationFrames > sourceDuration) {
        m_apply->setEnabled(false);
        setStatus(i18n("This first version can shorten/speed up a range, but cannot make it longer yet."), true);
        return;
    }
    const auto preflight = RetimeRangeExecutor::preflight(timelineWidget->model(), operation);
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
    if (!m_hasPlan || m_plan.operations.size() != 1 || !timelineWidget || !timelineWidget->model()) {
        setStatus(i18n("There is no valid plan to apply to the active timeline."), true);
        return;
    }
    const auto result = RetimeRangeExecutor::apply(timelineWidget->model(), m_plan.operations.constFirst().retimeRange);
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
    m_preview->clear();
    m_apply->setEnabled(false);
    m_discard->setEnabled(false);
    if (!m_client->isBusy()) {
        setStatus(QString());
    }
}

void AssistantDock::setStatus(const QString &message, bool error)
{
    m_status->setText(message);
    QPalette palette = m_status->palette();
    palette.setColor(QPalette::WindowText, error ? palette.color(QPalette::Active, QPalette::BrightText) : palette.color(QPalette::Active, QPalette::Text));
    m_status->setPalette(palette);
}

void AssistantDock::setBusy(bool busy)
{
    m_generate->setEnabled(!busy);
    m_provider->setEnabled(!busy);
    m_model->setEnabled(!busy);
    m_cancel->setEnabled(busy);
    if (busy) {
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

