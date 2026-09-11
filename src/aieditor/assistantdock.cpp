/*
    SPDX-FileCopyrightText: 2026 Kdenlive AI Editor contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "assistantdock.hpp"

#include "core.h"
#include "editplanexecutor.hpp"
#include "kdenlivesettings.h"
#include "localaimanager.hpp"
#include "localtimelinetranscriber.hpp"
#include "mainwindow.h"
#include "resourcebudget.hpp"
#include "securecredentialstore.hpp"
#include "timeline2/view/timelinewidget.h"

#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QEventLoop>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSet>
#include <QSlider>
#include <QSpinBox>
#include <QTime>
#include <QVBoxLayout>
#include <utility>

namespace Kdenlive {
namespace AiEditor {

namespace {
constexpr int PromptKindRole = Qt::UserRole + 1;
constexpr int PromptNeedsTranscriptRole = Qt::UserRole + 2;
enum PromptKind { BuiltInPrompt = 0, SavedPrompt = 1, SuggestedPrompt = 2 };
} // namespace

AssistantDock::AssistantDock(MainWindow *mainWindow)
    : QWidget(mainWindow)
    , m_mainWindow(mainWindow)
    , m_client(new AiProviderClient(this))
    , m_localAi(new LocalAiManager(this))
{
    auto *layout = new QVBoxLayout(this);
    auto *providerForm = new QFormLayout;

    m_provider = new QComboBox(this);
    for (AiProvider provider : {AiProvider::OpenRouter, AiProvider::OpenAI, AiProvider::Anthropic, AiProvider::Ollama}) {
        m_provider->addItem(AiProviderClient::displayName(provider), int(provider));
    }
    providerForm->addRow(i18n("Provider:"), m_provider);

    m_model = new QLineEdit(this);
    m_model->setClearButtonEnabled(true);
    providerForm->addRow(i18n("Model:"), m_model);

    m_keyInput = new QLineEdit(this);
    m_keyInput->setEchoMode(QLineEdit::Password);
    m_keyInput->setClearButtonEnabled(true);
    m_keyInput->setPlaceholderText(i18n("Paste a provider API key"));
    m_keyLabel = new QLabel(i18n("API key:"), this);
    providerForm->addRow(m_keyLabel, m_keyInput);
    layout->addLayout(providerForm);

    m_keyStatus = new QLabel(this);
    m_keyStatus->setWordWrap(true);
    layout->addWidget(m_keyStatus);

    m_credentialButtons = new QWidget(this);
    auto *credentialButtons = new QHBoxLayout(m_credentialButtons);
    credentialButtons->setContentsMargins(0, 0, 0, 0);
    m_saveKey = new QPushButton(i18n("Save securely"), this);
    m_testKey = new QPushButton(i18n("Test connection"), this);
    m_removeKey = new QPushButton(i18n("Remove saved key"), this);
    credentialButtons->addWidget(m_saveKey);
    credentialButtons->addWidget(m_testKey);
    credentialButtons->addWidget(m_removeKey);
    layout->addWidget(m_credentialButtons);

    m_privacy = new QLabel(
        i18n("Privacy: media files are never uploaded. Whisper runs locally; only the timestamped transcript is sent. Local resume checkpoints expire after "
             "seven days and never contain API keys."),
        this);
    m_privacy->setWordWrap(true);
    layout->addWidget(m_privacy);

    m_localAiGroup = new QGroupBox(i18n("Local AI setup"), this);
    auto *localAiLayout = new QVBoxLayout(m_localAiGroup);
    m_localHardware = new QLabel(m_localAiGroup);
    m_localHardware->setWordWrap(true);
    m_localStatus = new QLabel(m_localAiGroup);
    m_localStatus->setWordWrap(true);
    m_localSetup = new QPushButton(i18n("Prepare / download local model"), m_localAiGroup);
    localAiLayout->addWidget(m_localHardware);
    localAiLayout->addWidget(m_localStatus);
    localAiLayout->addWidget(m_localSetup);
    layout->addWidget(m_localAiGroup);

    auto *performanceGroup = new QGroupBox(i18n("Performance"), this);
    auto *performanceLayout = new QFormLayout(performanceGroup);
    m_performance = new QSlider(Qt::Horizontal, performanceGroup);
    m_performance->setRange(10, 100);
    m_performance->setSingleStep(5);
    m_performance->setPageStep(10);
    m_performance->setValue(qBound(10, KdenliveSettings::aiPerformancePercent(), 100));
    performanceLayout->addRow(i18n("Resource budget:"), m_performance);
    m_performanceSummary = new QLabel(performanceGroup);
    m_performanceSummary->setWordWrap(true);
    performanceLayout->addRow(QString(), m_performanceSummary);

    m_cpuThreads = new QSpinBox(performanceGroup);
    m_cpuThreads->setRange(0, ResourceBudget::logicalCpuCount());
    m_cpuThreads->setSpecialValueText(i18n("Automatic"));
    m_cpuThreads->setValue(qBound(0, KdenliveSettings::aiCpuThreads(), ResourceBudget::logicalCpuCount()));
    performanceLayout->addRow(i18n("CPU threads:"), m_cpuThreads);

    m_processingDevice = new QComboBox(performanceGroup);
    m_processingDevice->addItem(i18n("Automatic (CPU on this system)"), QStringLiteral("auto"));
    m_processingDevice->addItem(i18n("CPU"), QStringLiteral("cpu"));
    if (ResourceBudget::cudaHardwareLikelyAvailable()) {
        m_processingDevice->addItem(i18n("NVIDIA CUDA GPU"), QStringLiteral("cuda"));
    } else {
        m_processingDevice->setToolTip(i18n("The detected GPU is not compatible with this Whisper CUDA runtime. CPU processing remains available."));
    }
    const int deviceIndex = m_processingDevice->findData(KdenliveSettings::aiProcessingDevice());
    m_processingDevice->setCurrentIndex(deviceIndex < 0 ? 0 : deviceIndex);
    performanceLayout->addRow(i18n("Processing device:"), m_processingDevice);
    layout->addWidget(performanceGroup);

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
    for (int index = 0; index < m_preset->count(); ++index) {
        m_preset->setItemData(index, BuiltInPrompt, PromptKindRole);
        m_preset->setItemData(index, index >= 1 && index <= 3, PromptNeedsTranscriptRole);
    }
    presetForm->addRow(i18n("Ready prompt:"), m_preset);
    layout->addLayout(presetForm);

    auto *promptButtons = new QGridLayout;
    m_savePrompt = new QPushButton(i18n("Save prompt"), this);
    m_deletePrompt = new QPushButton(i18n("Delete prompt"), this);
    m_suggestPrompts = new QPushButton(i18n("Suggest prompts from transcript"), this);
    promptButtons->addWidget(m_savePrompt, 0, 0);
    promptButtons->addWidget(m_deletePrompt, 0, 1);
    promptButtons->addWidget(m_suggestPrompts, 1, 0, 1, 2);
    layout->addLayout(promptButtons);

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

    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 100);
    m_progress->setTextVisible(true);
    m_progress->hide();
    layout->addWidget(m_progress);
    m_progressDetails = new QLabel(this);
    m_progressDetails->setWordWrap(true);
    m_progressDetails->hide();
    layout->addWidget(m_progressDetails);

    auto *applyButtons = new QHBoxLayout;
    m_apply = new QPushButton(i18n("Apply"), this);
    m_discard = new QPushButton(i18n("Discard"), this);
    m_apply->setEnabled(false);
    m_discard->setEnabled(false);
    applyButtons->addWidget(m_apply);
    applyButtons->addWidget(m_discard);
    layout->addLayout(applyButtons);

    connect(m_provider, &QComboBox::currentIndexChanged, this, &AssistantDock::updateProvider);
    connect(m_saveKey, &QPushButton::clicked, this, &AssistantDock::saveCredential);
    connect(m_removeKey, &QPushButton::clicked, this, &AssistantDock::removeCredential);
    connect(m_testKey, &QPushButton::clicked, this, &AssistantDock::testCredential);
    connect(m_client, &AiProviderClient::connectionTested, this, [this](bool success, const QString &message) { setStatus(message, !success); });
    connect(m_performance, &QSlider::valueChanged, this, [this](int value) {
        KdenliveSettings::setAiPerformancePercent(value);
        KdenliveSettings::self()->save();
        updatePerformanceSummary();
    });
    connect(m_cpuThreads, qOverload<int>(&QSpinBox::valueChanged), this, [this](int value) {
        KdenliveSettings::setAiCpuThreads(value);
        KdenliveSettings::self()->save();
        updatePerformanceSummary();
    });
    connect(m_processingDevice, &QComboBox::currentIndexChanged, this, [this]() {
        KdenliveSettings::setAiProcessingDevice(m_processingDevice->currentData().toString());
        KdenliveSettings::self()->save();
        updatePerformanceSummary();
    });
    connect(m_preset, &QComboBox::currentIndexChanged, this, [this](int index) {
        const QString prompt = m_preset->itemData(index).toString();
        if (!prompt.isEmpty()) {
            m_prompt->setPlainText(prompt);
        }
        m_analyzeAudio->setChecked(m_preset->itemData(index, PromptNeedsTranscriptRole).toBool());
        updatePromptButtons();
    });
    connect(m_savePrompt, &QPushButton::clicked, this, &AssistantDock::savePrompt);
    connect(m_deletePrompt, &QPushButton::clicked, this, &AssistantDock::deletePrompt);
    connect(m_suggestPrompts, &QPushButton::clicked, this, &AssistantDock::suggestPrompts);
    connect(m_generate, &QPushButton::clicked, this, &AssistantDock::generatePlan);
    connect(m_model, &QLineEdit::editingFinished, this, [this]() {
        if (selectedProvider() == AiProvider::Ollama && !m_localAi->isBusy()) {
            m_localAi->refresh(m_model->text());
        }
    });
    connect(m_localSetup, &QPushButton::clicked, this, [this]() { m_localAi->installAndPrepare(m_model->text()); });
    connect(m_cancel, &QPushButton::clicked, this, [this]() {
        m_client->cancel();
        m_transcriber->cancel();
        m_localAi->cancel();
    });
    connect(m_apply, &QPushButton::clicked, this, &AssistantDock::applyPlan);
    connect(m_discard, &QPushButton::clicked, this, &AssistantDock::discardPlan);
    connect(m_client, &AiProviderClient::busyChanged, this, &AssistantDock::setBusy);
    connect(m_client, &AiProviderClient::planReady, this, &AssistantDock::handleProviderPlan);
    connect(m_client, &AiProviderClient::promptSuggestionsReady, this, &AssistantDock::handlePromptSuggestions);
    connect(m_client, &AiProviderClient::outputLimitReached, this, [this](const QString &message) {
        if (m_chunkedRequest) {
            retryWithSmallerTranscriptChunks(message);
            return;
        }
        hideProgress();
        setStatus(message, true);
    });
    connect(m_client, &AiProviderClient::errorOccurred, this, [this](const QString &message) {
        hideProgress();
        m_suggestionChunks.clear();
        m_suggestionCandidates.clear();
        m_suggestionConsolidating = false;
        setStatus(m_chunkedRequest ? i18n("%1 Progress is saved locally. Choose Generate plan to resume.", message) : message, true);
    });
    connect(m_client, &AiProviderClient::requestCancelled, this, [this]() {
        hideProgress();
        setStatus(m_chunkedRequest ? i18n("Request cancelled. Completed segments are saved locally; choose Generate plan to resume.")
                                   : i18n("Request cancelled."));
    });
    connect(m_localAi, &LocalAiManager::busyChanged, this, &AssistantDock::setBusy);
    connect(m_localAi, &LocalAiManager::progressChanged, this, &AssistantDock::showProgress);
    connect(m_localAi, &LocalAiManager::readyChanged, this, [this](bool ready) {
        Q_UNUSED(ready)
        m_localSetup->setText(i18n("Prepare / download local model"));
    });
    connect(m_localAi, &LocalAiManager::statusChanged, this, [this](const QString &message, bool error) {
        m_localStatus->setText(error ? i18n("Error: %1", message) : message);
        setStatus(message, error);
        if (!m_localAi->isBusy()) {
            hideProgress();
        }
    });

    m_transcriber = new LocalTimelineTranscriber(this);
    connect(m_transcriber, &LocalTimelineTranscriber::busyChanged, this, &AssistantDock::setBusy);
    connect(m_transcriber, &LocalTimelineTranscriber::statusChanged, this, [this](const QString &message) { setStatus(message); });
    connect(m_transcriber, &LocalTimelineTranscriber::progressChanged, this, &AssistantDock::showProgress);
    connect(m_transcriber, &LocalTimelineTranscriber::errorOccurred, this, [this](const QString &message) {
        hideProgress();
        m_suggestionChunks.clear();
        setStatus(message, true);
    });
    connect(m_transcriber, &LocalTimelineTranscriber::cancelled, this, [this]() {
        hideProgress();
        setStatus(i18n("Local transcription cancelled."));
    });
    connect(m_transcriber, &LocalTimelineTranscriber::transcriptReady, this, [this](const QString &transcript, const QString &timelineFingerprint) {
        if (m_transcriptionPurpose == TranscriptionPurpose::PromptSuggestions) {
            startPromptSuggestions(transcript);
            return;
        }
        setStatus(i18n("Local transcript ready. Requesting the edit plan…"));
        requestProviderPlan(transcript, timelineFingerprint);
    });

    loadSavedPrompts();
    updateProvider();
    updatePerformanceSummary();
}

AiProvider AssistantDock::selectedProvider() const
{
    return AiProvider(m_provider->currentData().toInt());
}

QByteArray AssistantDock::selectedApiKey() const
{
    if (selectedProvider() == AiProvider::Ollama) {
        return {};
    }
    const QByteArray stored = SecureCredentialStore::read(selectedProvider());
    if (!stored.isEmpty()) {
        return stored;
    }
    const QByteArray variable = AiProviderClient::environmentVariable(selectedProvider()).toUtf8();
    return qgetenv(variable.constData());
}

void AssistantDock::updateProvider()
{
    const bool local = selectedProvider() == AiProvider::Ollama;
    m_model->setText(local ? m_localAi->recommendedModel() : AiProviderClient::defaultModel(selectedProvider()));
    m_keyInput->clear();
    m_keyLabel->setVisible(!local);
    m_keyInput->setVisible(!local);
    m_keyStatus->setVisible(!local);
    m_credentialButtons->setVisible(!local);
    m_localAiGroup->setVisible(local);
    m_privacy->setText(
        local ? i18n("Privacy: the transcript and edit request stay on this computer. Ollama listens only on the local loopback address; no API key "
                     "is required.")
              : i18n("Privacy: media files are never uploaded. Whisper runs locally; only the timestamped transcript is sent. Local resume "
                     "checkpoints expire after seven days and never contain API keys."));
    updateCredentialStatus();
    resetPlanPreview();
    m_chunkedRequest = false;
    m_checkpoint = {};
    m_transcriptChunks.clear();
    if (local) {
        const QString recommendation = m_localAi->recommendedModel();
        m_localHardware->setText(i18n("%1\nRecommended model: %2 (approximately %3 download).\n%4", m_localAi->hardwareSummary(), recommendation,
                                      LocalAiManager::approximateDownloadSize(recommendation), m_localAi->recommendationReason()));
        m_localAi->refresh(m_model->text());
    }
}

void AssistantDock::updateCredentialStatus()
{
    if (selectedProvider() == AiProvider::Ollama) {
        return;
    }
    const QString variable = AiProviderClient::environmentVariable(selectedProvider());
    if (!SecureCredentialStore::read(selectedProvider()).isEmpty()) {
        m_keyStatus->setText(i18n("Connected: key saved securely in %1.", SecureCredentialStore::backendName()));
        m_removeKey->setEnabled(true);
    } else if (!qgetenv(variable.toUtf8().constData()).isEmpty()) {
        m_keyStatus->setText(i18n("Connected: key loaded from %1.", variable));
        m_removeKey->setEnabled(false);
    } else {
        m_keyStatus->setText(i18n("Not connected. Paste a key above and choose Save securely, or set %1.", variable));
        m_removeKey->setEnabled(false);
    }
}

void AssistantDock::saveCredential()
{
    if (selectedProvider() == AiProvider::Ollama) {
        return;
    }
    QString error;
    if (!SecureCredentialStore::write(selectedProvider(), m_keyInput->text().toUtf8(), &error)) {
        setStatus(error, true);
        return;
    }
    m_keyInput->clear();
    updateCredentialStatus();
    setStatus(i18n("API key saved securely. It will not be written to the project or logs."));
}

void AssistantDock::removeCredential()
{
    if (selectedProvider() == AiProvider::Ollama) {
        return;
    }
    QString error;
    if (!SecureCredentialStore::remove(selectedProvider(), &error)) {
        setStatus(error, true);
        return;
    }
    m_keyInput->clear();
    updateCredentialStatus();
    setStatus(i18n("The saved key was removed. An environment-variable key may still be available."));
}

void AssistantDock::testCredential()
{
    if (selectedProvider() == AiProvider::Ollama) {
        m_localAi->refresh(m_model->text());
        return;
    }
    const QByteArray candidate = m_keyInput->text().trimmed().isEmpty() ? selectedApiKey() : m_keyInput->text().trimmed().toUtf8();
    if (candidate.isEmpty()) {
        setStatus(i18n("Paste or save an API key before testing the connection."), true);
        return;
    }
    setStatus(i18n("Testing the provider connection…"));
    m_client->testConnection(selectedProvider(), candidate);
}

void AssistantDock::updatePerformanceSummary()
{
    const int percent = m_performance->value();
    const int automaticThreads = qMax(1, qRound(double(ResourceBudget::logicalCpuCount()) * double(percent) / 100.0));
    const int threads = m_cpuThreads->value() == 0 ? automaticThreads : m_cpuThreads->value();
    const double memoryGiB = double(ResourceBudget::totalMemoryBytes()) * double(percent) / 100.0 / double(1024ULL * 1024ULL * 1024ULL);
    m_performanceSummary->setText(i18n("%1% best-effort budget · %2 of %3 CPU threads · up to %4 GiB memory", percent, threads,
                                       ResourceBudget::logicalCpuCount(), QString::number(memoryGiB, 'f', 1)));
}

void AssistantDock::loadSavedPrompts()
{
    for (int index = m_preset->count() - 1; index >= 4; --index) {
        if (m_preset->itemData(index, PromptKindRole).toInt() == SavedPrompt) {
            m_preset->removeItem(index);
        }
    }
    const KConfigGroup group(KSharedConfig::openConfig(), QStringLiteral("AiEditorPrompts"));
    const QStringList names = group.readEntry(QStringLiteral("Names"), QStringList());
    const QStringList prompts = group.readEntry(QStringLiteral("Prompts"), QStringList());
    const QList<int> analyzeAudio = group.readEntry(QStringLiteral("AnalyzeAudio"), QList<int>());
    const int count = qMin(names.size(), prompts.size());
    for (int index = 0; index < count; ++index) {
        if (names.at(index).trimmed().isEmpty() || prompts.at(index).trimmed().isEmpty()) {
            continue;
        }
        m_preset->addItem(names.at(index), prompts.at(index));
        const int item = m_preset->count() - 1;
        m_preset->setItemData(item, SavedPrompt, PromptKindRole);
        m_preset->setItemData(item, index < analyzeAudio.size() ? analyzeAudio.at(index) != 0 : true, PromptNeedsTranscriptRole);
    }
    updatePromptButtons();
}

void AssistantDock::savePrompt()
{
    const QString prompt = m_prompt->toPlainText().trimmed();
    if (prompt.isEmpty()) {
        setStatus(i18n("Write a prompt before saving it."), true);
        return;
    }
    QString suggestedName;
    if (m_preset->currentData(PromptKindRole).toInt() == SavedPrompt) {
        suggestedName = m_preset->currentText();
    } else {
        suggestedName = prompt.section(QLatin1Char('\n'), 0, 0).left(60).trimmed();
    }
    bool accepted = false;
    const QString name = QInputDialog::getText(this, i18n("Save prompt"), i18n("Prompt name:"), QLineEdit::Normal, suggestedName, &accepted).trimmed();
    if (!accepted || name.isEmpty()) {
        return;
    }

    KConfigGroup group(KSharedConfig::openConfig(), QStringLiteral("AiEditorPrompts"));
    QStringList names = group.readEntry(QStringLiteral("Names"), QStringList());
    QStringList prompts = group.readEntry(QStringLiteral("Prompts"), QStringList());
    QList<int> analyzeAudio = group.readEntry(QStringLiteral("AnalyzeAudio"), QList<int>());
    while (prompts.size() < names.size()) {
        prompts << QString();
    }
    while (analyzeAudio.size() < names.size()) {
        analyzeAudio << 1;
    }
    int savedIndex = -1;
    for (int index = 0; index < names.size(); ++index) {
        if (names.at(index).compare(name, Qt::CaseInsensitive) == 0) {
            savedIndex = index;
            break;
        }
    }
    if (savedIndex < 0) {
        names << name;
        prompts << prompt;
        analyzeAudio << (m_analyzeAudio->isChecked() ? 1 : 0);
    } else {
        names[savedIndex] = name;
        prompts[savedIndex] = prompt;
        analyzeAudio[savedIndex] = m_analyzeAudio->isChecked() ? 1 : 0;
    }
    group.writeEntry(QStringLiteral("Names"), names);
    group.writeEntry(QStringLiteral("Prompts"), prompts);
    group.writeEntry(QStringLiteral("AnalyzeAudio"), analyzeAudio);
    group.sync();
    loadSavedPrompts();
    for (int index = 4; index < m_preset->count(); ++index) {
        if (m_preset->itemText(index).compare(name, Qt::CaseInsensitive) == 0) {
            m_preset->setCurrentIndex(index);
            break;
        }
    }
    setStatus(i18n("Prompt “%1” saved.", name));
}

void AssistantDock::deletePrompt()
{
    if (m_preset->currentData(PromptKindRole).toInt() != SavedPrompt) {
        return;
    }
    const QString name = m_preset->currentText();
    if (QMessageBox::question(this, i18n("Delete prompt"), i18n("Delete the saved prompt “%1”?", name)) != QMessageBox::Yes) {
        return;
    }
    KConfigGroup group(KSharedConfig::openConfig(), QStringLiteral("AiEditorPrompts"));
    QStringList names = group.readEntry(QStringLiteral("Names"), QStringList());
    QStringList prompts = group.readEntry(QStringLiteral("Prompts"), QStringList());
    QList<int> analyzeAudio = group.readEntry(QStringLiteral("AnalyzeAudio"), QList<int>());
    for (int index = names.size() - 1; index >= 0; --index) {
        if (names.at(index).compare(name, Qt::CaseInsensitive) == 0) {
            names.removeAt(index);
            if (index < prompts.size()) {
                prompts.removeAt(index);
            }
            if (index < analyzeAudio.size()) {
                analyzeAudio.removeAt(index);
            }
        }
    }
    group.writeEntry(QStringLiteral("Names"), names);
    group.writeEntry(QStringLiteral("Prompts"), prompts);
    group.writeEntry(QStringLiteral("AnalyzeAudio"), analyzeAudio);
    group.sync();
    loadSavedPrompts();
    m_preset->setCurrentIndex(0);
    setStatus(i18n("Prompt “%1” deleted.", name));
}

void AssistantDock::updatePromptButtons()
{
    if (!m_deletePrompt) {
        return;
    }
    m_deletePrompt->setEnabled(m_preset->currentData(PromptKindRole).toInt() == SavedPrompt);
}

void AssistantDock::suggestPrompts()
{
    updateCredentialStatus();
    if (m_model->text().trimmed().isEmpty()) {
        setStatus(i18n("Choose a model before requesting suggestions."), true);
        return;
    }
    if (selectedProvider() != AiProvider::Ollama && selectedApiKey().trimmed().isEmpty()) {
        setStatus(i18n("The API key is missing. Paste and save a key before requesting suggestions."), true);
        return;
    }
    if (selectedProvider() == AiProvider::Ollama && !m_localAi->isModelReady(m_model->text())) {
        setStatus(i18n("Local AI is not ready. Choose Prepare / download local model first."), true);
        return;
    }
    TimelineWidget *timelineWidget = m_mainWindow->getCurrentTimeline();
    if (!timelineWidget || !timelineWidget->model()) {
        setStatus(i18n("Open a project timeline before requesting suggestions."), true);
        return;
    }
    m_transcriptionPurpose = TranscriptionPurpose::PromptSuggestions;
    m_suggestionChunks.clear();
    m_suggestionCandidates.clear();
    m_suggestionChunk = 0;
    m_suggestionConsolidating = false;
    setStatus(i18n("Preparing the transcript to suggest useful prompts…"));
    m_transcriber->start(timelineWidget->model(), pCore->getCurrentFps());
}

void AssistantDock::startPromptSuggestions(const QString &transcript)
{
    TimelineWidget *timelineWidget = m_mainWindow->getCurrentTimeline();
    if (!timelineWidget || !timelineWidget->model()) {
        setStatus(i18n("The active timeline is no longer available."), true);
        return;
    }
    m_suggestionChunks = AiSessionStore::splitTranscript(transcript, timelineWidget->model()->duration(), AiSessionStore::LegacyChunkCharacters);
    if (m_suggestionChunks.isEmpty()) {
        setStatus(i18n("The transcript contains no usable dialogue for suggestions."), true);
        return;
    }
    m_suggestionChunk = 0;
    m_suggestionCandidates.clear();
    requestNextPromptSuggestionChunk();
}

void AssistantDock::requestNextPromptSuggestionChunk()
{
    if (m_suggestionChunk >= m_suggestionChunks.size()) {
        if (m_suggestionChunks.size() == 1) {
            finishPromptSuggestions(m_suggestionCandidates);
            return;
        }
        QStringList candidates;
        for (const PromptSuggestion &suggestion : std::as_const(m_suggestionCandidates)) {
            candidates << QStringLiteral("- %1: %2").arg(suggestion.title, suggestion.prompt);
        }
        m_suggestionConsolidating = true;
        showProgress(95, -1, i18n("Consolidating suggestions from the complete transcript"));
        m_client->requestPromptSuggestions(selectedProvider(), m_model->text(), selectedApiKey(), candidates.join(QLatin1Char('\n')), true);
        return;
    }
    const int current = m_suggestionChunk + 1;
    const int total = m_suggestionChunks.size();
    showProgress(qRound(double(m_suggestionChunk) * 90.0 / double(total)), -1, i18n("Finding useful prompts · transcript segment %1 of %2", current, total));
    m_client->requestPromptSuggestions(selectedProvider(), m_model->text(), selectedApiKey(), m_suggestionChunks.at(m_suggestionChunk).text);
}

void AssistantDock::handlePromptSuggestions(const QVector<PromptSuggestion> &suggestions)
{
    if (m_suggestionConsolidating) {
        finishPromptSuggestions(suggestions);
        return;
    }
    m_suggestionCandidates += suggestions;
    ++m_suggestionChunk;
    requestNextPromptSuggestionChunk();
}

void AssistantDock::finishPromptSuggestions(const QVector<PromptSuggestion> &suggestions)
{
    for (int index = m_preset->count() - 1; index >= 4; --index) {
        if (m_preset->itemData(index, PromptKindRole).toInt() == SuggestedPrompt) {
            m_preset->removeItem(index);
        }
    }
    QSet<QString> seen;
    int firstSuggestion = -1;
    for (const PromptSuggestion &suggestion : suggestions) {
        if (seen.contains(suggestion.prompt.toCaseFolded())) {
            continue;
        }
        seen.insert(suggestion.prompt.toCaseFolded());
        m_preset->addItem(i18n("Suggestion: %1", suggestion.title), suggestion.prompt);
        const int item = m_preset->count() - 1;
        m_preset->setItemData(item, SuggestedPrompt, PromptKindRole);
        m_preset->setItemData(item, true, PromptNeedsTranscriptRole);
        if (firstSuggestion < 0) {
            firstSuggestion = item;
        }
    }
    m_suggestionChunks.clear();
    m_suggestionCandidates.clear();
    m_suggestionConsolidating = false;
    m_transcriptionPurpose = TranscriptionPurpose::EditPlan;
    hideProgress();
    if (firstSuggestion < 0) {
        setStatus(i18n("The AI did not find useful prompt suggestions."), true);
        return;
    }
    m_preset->setCurrentIndex(firstSuggestion);
    setStatus(i18n("Suggestions are ready. Select one above, edit it if needed, then choose Save prompt or Generate plan."));
}

void AssistantDock::generatePlan()
{
    m_transcriptionPurpose = TranscriptionPurpose::EditPlan;
    updateCredentialStatus();
    if (m_model->text().trimmed().isEmpty()) {
        setStatus(i18n("Choose a model before sending the request."), true);
        return;
    }
    if (selectedProvider() != AiProvider::Ollama && selectedApiKey().trimmed().isEmpty()) {
        setStatus(i18n("The API key is missing. Set %1 and restart Kdenlive.", AiProviderClient::environmentVariable(selectedProvider())), true);
        return;
    }
    if (selectedProvider() == AiProvider::Ollama && !m_localAi->isModelReady(m_model->text())) {
        setStatus(i18n("Local AI is not ready. Choose Prepare / download local model first."), true);
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
    resetPlanPreview();
    m_pendingPrompt = m_prompt->toPlainText();
    if (m_analyzeAudio->isChecked()) {
        const int timelineFrames = timelineWidget->model()->duration();
        const double fps = pCore->getCurrentFps();
        const auto saved = AiSessionStore::loadUniqueCompatibleRequest(m_pendingPrompt, timelineFrames, fps);
        if (saved) {
            setStatus(i18n("Saved transcript and analysis found. Skipping local audio preparation and continuing at segment %1.", saved->nextChunk + 1));
            requestProviderPlan(saved->transcript, saved->timelineFingerprint);
            return;
        }
        m_transcriber->start(timelineWidget->model(), pCore->getCurrentFps());
    } else {
        requestProviderPlan();
    }
}

void AssistantDock::requestProviderPlan(const QString &transcript, const QString &timelineFingerprint)
{
    TimelineWidget *timelineWidget = m_mainWindow->getCurrentTimeline();
    if (!timelineWidget || !timelineWidget->model()) {
        setStatus(i18n("The active timeline is no longer available."), true);
        return;
    }
    const int timelineFrames = timelineWidget->model()->duration();
    const double fps = pCore->getCurrentFps();
    if (transcript.isEmpty()) {
        m_chunkedRequest = false;
        showProgress(-1, -1, i18n("Waiting for the AI provider"));
        m_client->requestPlan(selectedProvider(), m_model->text(), selectedApiKey(), m_pendingPrompt, timelineFrames, fps);
        return;
    }

    const QString id = AiSessionStore::sessionId(timelineFingerprint, selectedProvider(), m_model->text(), m_pendingPrompt, timelineFrames, fps);
    const auto saved = AiSessionStore::loadMostAdvancedCompatible(timelineFingerprint, m_pendingPrompt, timelineFrames, fps, transcript);
    const qsizetype chunkCharacters = saved ? saved->chunkCharacters : AiSessionStore::DefaultChunkCharacters;
    m_transcriptChunks = AiSessionStore::splitTranscript(transcript, timelineFrames, chunkCharacters);
    if (m_transcriptChunks.isEmpty()) {
        setStatus(i18n("The saved transcript contains no usable dialogue."), true);
        return;
    }
    if (saved && saved->nextChunk <= m_transcriptChunks.size()) {
        m_checkpoint = *saved;
        const bool transferred = m_checkpoint.id != id || m_checkpoint.provider != selectedProvider() || m_checkpoint.model != m_model->text().trimmed();
        m_checkpoint.id = id;
        m_checkpoint.provider = selectedProvider();
        m_checkpoint.model = m_model->text().trimmed();
        QString error;
        if (!AiSessionStore::save(m_checkpoint, &error)) {
            setStatus(error, true);
            return;
        }
        setStatus(transferred ? i18n("Saved analysis transferred to %1. Continuing at segment %2 of %3.", AiProviderClient::displayName(selectedProvider()),
                                     m_checkpoint.nextChunk + 1, m_transcriptChunks.size())
                              : i18n("Saved analysis restored at segment %1 of %2.", m_checkpoint.nextChunk + 1, m_transcriptChunks.size()));
    } else {
        m_checkpoint = {};
        m_checkpoint.id = id;
        m_checkpoint.timelineFingerprint = timelineFingerprint;
        m_checkpoint.provider = selectedProvider();
        m_checkpoint.model = m_model->text().trimmed();
        m_checkpoint.prompt = m_pendingPrompt.trimmed();
        m_checkpoint.timelineFrames = timelineFrames;
        m_checkpoint.fps = fps;
        m_checkpoint.transcript = transcript;
        m_checkpoint.chunkCharacters = AiSessionStore::DefaultChunkCharacters;
        QString error;
        if (!AiSessionStore::save(m_checkpoint, &error)) {
            setStatus(error, true);
            return;
        }
    }
    m_chunkedRequest = true;
    m_providerStartChunk = m_checkpoint.nextChunk;
    m_providerTimer.restart();
    requestNextTranscriptChunk();
}

void AssistantDock::retryWithSmallerTranscriptChunks(const QString &message)
{
    if (m_checkpoint.chunkReductions >= AiSessionStore::MaximumChunkReductions || m_checkpoint.chunkCharacters <= AiSessionStore::MinimumChunkCharacters) {
        hideProgress();
        setStatus(i18n("%1 The transcript was already divided into the smallest safe segments. Choose a model with a larger output limit.", message), true);
        return;
    }

    const int previousCharacters = m_checkpoint.chunkCharacters;
    m_checkpoint.chunkCharacters = qMax<int>(AiSessionStore::MinimumChunkCharacters, previousCharacters / 2);
    ++m_checkpoint.chunkReductions;
    m_checkpoint.nextChunk = 0;
    m_checkpoint.planFragments.clear();
    m_transcriptChunks = AiSessionStore::splitTranscript(m_checkpoint.transcript, m_checkpoint.timelineFrames, m_checkpoint.chunkCharacters);
    QString error;
    if (m_transcriptChunks.isEmpty() || !AiSessionStore::save(m_checkpoint, &error)) {
        hideProgress();
        setStatus(m_transcriptChunks.isEmpty() ? i18n("The saved transcript contains no usable dialogue.") : error, true);
        return;
    }

    m_providerStartChunk = 0;
    m_providerTimer.restart();
    setStatus(
        i18n("The model reached its limit. Retrying automatically with %1 smaller segments; the local transcript is being reused.", m_transcriptChunks.size()));
    requestNextTranscriptChunk();
}

void AssistantDock::requestNextTranscriptChunk()
{
    if (!m_chunkedRequest || m_checkpoint.nextChunk >= m_transcriptChunks.size()) {
        finishChunkedPlan();
        return;
    }
    const int chunkNumber = m_checkpoint.nextChunk + 1;
    const TranscriptChunk &chunk = m_transcriptChunks.at(m_checkpoint.nextChunk);
    const QString chunkPrompt =
        m_checkpoint.prompt +
        QStringLiteral("\n\nThis is transcript segment %1 of %2. Return only operations whose start_frame is greater than or equal to %3 and less than %4. "
                       "Lines outside that owned frame interval are context only. It is valid to return an empty operations array.")
            .arg(chunkNumber)
            .arg(m_transcriptChunks.size())
            .arg(chunk.ownedStartFrame)
            .arg(chunk.ownedEndFrame);
    setStatus(i18n("Requesting AI segment %1 of %2. Each completed segment is saved locally.", chunkNumber, m_transcriptChunks.size()));
    qint64 remainingSeconds = -1;
    const int completedThisRun = m_checkpoint.nextChunk - m_providerStartChunk;
    if (completedThisRun > 0) {
        const double secondsPerChunk = double(m_providerTimer.elapsed()) / 1000.0 / double(completedThisRun);
        remainingSeconds = qRound64(secondsPerChunk * double(m_transcriptChunks.size() - m_checkpoint.nextChunk));
    }
    showProgress(qRound(double(m_checkpoint.nextChunk) * 100.0 / double(m_transcriptChunks.size())), remainingSeconds,
                 i18n("Analyzing transcript with AI · segment %1 of %2", chunkNumber, m_transcriptChunks.size()));
    m_client->requestPlan(m_checkpoint.provider, m_checkpoint.model, selectedApiKey(), chunkPrompt, m_checkpoint.timelineFrames, m_checkpoint.fps, chunk.text,
                          true);
}

void AssistantDock::handleProviderPlan(const QByteArray &planJson)
{
    if (!m_chunkedRequest) {
        showPlan(planJson);
        return;
    }
    const auto parsed = parseEditPlan(planJson, true);
    if (!parsed.isValid()) {
        setStatus(i18n("The AI segment was invalid: %1 Progress remains saved.", parsed.error), true);
        return;
    }
    m_checkpoint.planFragments << QString::fromUtf8(planJson);
    ++m_checkpoint.nextChunk;
    QString error;
    if (!AiSessionStore::save(m_checkpoint, &error)) {
        setStatus(error, true);
        return;
    }
    requestNextTranscriptChunk();
}

void AssistantDock::finishChunkedPlan()
{
    if (!m_chunkedRequest) {
        return;
    }
    QJsonArray operations;
    QSet<QByteArray> seen;
    for (const QString &fragment : std::as_const(m_checkpoint.planFragments)) {
        const QJsonArray fragmentOperations = QJsonDocument::fromJson(fragment.toUtf8()).object().value(QStringLiteral("operations")).toArray();
        for (const QJsonValue &operation : fragmentOperations) {
            const QByteArray canonical = QJsonDocument(operation.toObject()).toJson(QJsonDocument::Compact);
            if (!seen.contains(canonical)) {
                seen.insert(canonical);
                operations.append(operation);
            }
        }
    }
    QByteArray completePlan =
        QJsonDocument(QJsonObject{{QStringLiteral("version"), 1}, {QStringLiteral("operations"), operations}}).toJson(QJsonDocument::Compact);
    if (m_checkpoint.provider == AiProvider::Ollama) {
        completePlan = AiProviderClient::normalizeLocalPlan(completePlan);
    }
    const auto parsed = parseEditPlan(completePlan, true);
    if (!parsed.isValid()) {
        setStatus(i18n("The combined AI plan is unsafe: %1 Progress remains saved.", parsed.error), true);
        return;
    }
    m_chunkedRequest = false;
    hideProgress();
    if (parsed.plan.operations.isEmpty()) {
        AiSessionStore::remove(m_checkpoint.id);
        m_checkpoint = {};
        m_transcriptChunks.clear();
        resetPlanPreview();
        setStatus(i18n("Analysis completed. No matching edits were found in the transcript."));
        return;
    }
    showPlan(completePlan);
}

void AssistantDock::resetPlanPreview()
{
    m_hasPlan = false;
    m_plan = {};
    m_preview->clear();
    m_apply->setEnabled(false);
    m_discard->setEnabled(false);
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

    const auto compatible = EditPlanExecutor::compatiblePlan(timelineWidget->model(), parsed.plan);
    if (compatible.plan.operations.isEmpty()) {
        setStatus(i18n("No proposed operation can be applied safely to this timeline. First reason: %1", compatible.firstSkippedReason), true);
        return;
    }
    m_plan = compatible.plan;
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
    hideProgress();
    setStatus(compatible.skippedOperations > 0 ? i18n("Plan validated with %1 safe operations. %2 incompatible operations were skipped. First reason: %3",
                                                      m_plan.operations.size(), compatible.skippedOperations, compatible.firstSkippedReason)
                                               : i18n("Plan validated. Review it, then choose Apply."));
}

void AssistantDock::applyPlan()
{
    TimelineWidget *timelineWidget = m_mainWindow->getCurrentTimeline();
    if (!m_hasPlan || !timelineWidget || !timelineWidget->model()) {
        setStatus(i18n("There is no valid plan to apply to the active timeline."), true);
        return;
    }
    m_applyInProgress = true;
    m_apply->setEnabled(false);
    m_discard->setEnabled(false);
    setBusy(true);
    QElapsedTimer applyTimer;
    QElapsedTimer paintTimer;
    applyTimer.start();
    paintTimer.start();
    showProgress(0, -1, i18n("Applying AI edit plan"));
    const auto result = EditPlanExecutor::apply(timelineWidget->model(), m_plan, [this, &applyTimer, &paintTimer](int completed, int total) {
        if (paintTimer.elapsed() < 100 && completed < total) {
            return;
        }
        const double secondsPerOperation = double(applyTimer.elapsed()) / 1000.0 / double(completed);
        const qint64 remainingSeconds = qRound64(secondsPerOperation * double(total - completed));
        showProgress(qRound(double(completed) * 100.0 / double(total)), remainingSeconds, i18n("Applying timeline edits · %1 of %2", completed, total));
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        paintTimer.restart();
    });
    m_applyInProgress = false;
    setBusy(false);
    if (!result.isValid()) {
        hideProgress();
        m_apply->setEnabled(true);
        m_discard->setEnabled(true);
        setStatus(result.error, true);
        return;
    }
    m_apply->setEnabled(false);
    m_discard->setEnabled(false);
    m_hasPlan = false;
    AiSessionStore::remove(m_checkpoint.id);
    m_checkpoint = {};
    m_transcriptChunks.clear();
    hideProgress();
    setStatus(i18n("Edit applied. Use Undo once to restore the previous timeline."));
}

void AssistantDock::discardPlan()
{
    AiSessionStore::remove(m_checkpoint.id);
    m_checkpoint = {};
    m_transcriptChunks.clear();
    m_chunkedRequest = false;
    m_pendingPrompt.clear();
    resetPlanPreview();
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
    const bool anyBusy = m_applyInProgress || m_client->isBusy() || (m_transcriber && m_transcriber->isBusy()) || (m_localAi && m_localAi->isBusy());
    m_generate->setEnabled(!anyBusy);
    m_provider->setEnabled(!anyBusy);
    m_model->setEnabled(!anyBusy);
    const bool local = selectedProvider() == AiProvider::Ollama;
    m_keyInput->setEnabled(!anyBusy && !local);
    m_saveKey->setEnabled(!anyBusy && !local);
    m_testKey->setEnabled(!anyBusy && !local);
    m_removeKey->setEnabled(!anyBusy && !local && !SecureCredentialStore::read(selectedProvider()).isEmpty());
    m_localSetup->setEnabled(!anyBusy && local);
    m_performance->setEnabled(!anyBusy);
    m_cpuThreads->setEnabled(!anyBusy);
    m_processingDevice->setEnabled(!anyBusy);
    m_preset->setEnabled(!anyBusy);
    m_prompt->setEnabled(!anyBusy);
    m_savePrompt->setEnabled(!anyBusy);
    m_deletePrompt->setEnabled(!anyBusy && m_preset->currentData(PromptKindRole).toInt() == SavedPrompt);
    m_suggestPrompts->setEnabled(!anyBusy);
    m_analyzeAudio->setEnabled(!anyBusy);
    m_cancel->setEnabled(anyBusy && !m_applyInProgress);
    if (anyBusy && m_client->isBusy() && !m_chunkedRequest) {
        setStatus(i18n("Waiting for the AI provider…"));
    } else if (!anyBusy && !m_hasPlan) {
        hideProgress();
    }
}

void AssistantDock::showProgress(int percent, qint64 remainingSeconds, const QString &phase)
{
    m_progress->show();
    m_progressDetails->show();
    if (percent < 0) {
        m_progress->setRange(0, 0);
        m_progressDetails->setText(phase);
        return;
    }
    m_progress->setRange(0, 100);
    m_progress->setValue(qBound(0, percent, 100));
    QString details = phase;
    if (remainingSeconds >= 0 && percent < 100) {
        const qint64 hours = remainingSeconds / 3600;
        const QTime remaining = QTime::fromMSecsSinceStartOfDay(int((remainingSeconds % 86400) * 1000));
        const QString duration =
            hours > 0 ? QStringLiteral("%1:%2").arg(hours).arg(remaining.toString(QStringLiteral("mm:ss"))) : remaining.toString(QStringLiteral("mm:ss"));
        details += i18n(" · approximately %1 remaining", duration);
    }
    m_progressDetails->setText(details);
}

void AssistantDock::hideProgress()
{
    m_progress->hide();
    m_progressDetails->hide();
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
