/*
    SPDX-FileCopyrightText: 2026 Kdenlive AI Editor contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "localaimanager.hpp"

#include "resourcebudget.hpp"

#include <KLocalizedString>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QTimer>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

namespace Kdenlive {
namespace AiEditor {

namespace {
constexpr quint64 GiB = 1024ULL * 1024ULL * 1024ULL;

QString displayAdapterName()
{
#ifdef Q_OS_WIN
    DISPLAY_DEVICEW device{};
    device.cb = sizeof(device);
    for (DWORD index = 0; EnumDisplayDevicesW(nullptr, index, &device, 0); ++index) {
        if ((device.StateFlags & DISPLAY_DEVICE_MIRRORING_DRIVER) == 0 && (device.StateFlags & DISPLAY_DEVICE_ACTIVE) != 0) {
            return QString::fromWCharArray(device.DeviceString);
        }
        device = {};
        device.cb = sizeof(device);
    }
#endif
    return i18n("Display adapter not reported");
}
} // namespace

LocalAiManager::LocalAiManager(QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
    , m_installer(new QProcess(this))
    , m_server(new QProcess(this))
    , m_hardware(detectHardware())
{
#ifdef Q_OS_WIN
    m_installer->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *arguments) { arguments->flags |= CREATE_NO_WINDOW; });
    m_server->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *arguments) { arguments->flags |= CREATE_NO_WINDOW; });
#endif
    m_server->setStandardOutputFile(QProcess::nullDevice());
    m_server->setStandardErrorFile(QProcess::nullDevice());
    connect(m_installer, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, [this](int exitCode, QProcess::ExitStatus status) {
        if (m_phase != Phase::InstallingRuntime) {
            return;
        }
        if (status != QProcess::NormalExit || exitCode != 0 || ollamaExecutable().isEmpty()) {
            setPhase(Phase::Idle);
            Q_EMIT progressChanged(-1, -1, QString());
            Q_EMIT statusChanged(i18n("Ollama installation did not complete. Install it from ollama.com/download/windows, then choose Check again."), true);
            return;
        }
        Q_EMIT statusChanged(i18n("Ollama installed. Starting the local service…"), false);
        probeServer(true);
    });
}

LocalAiHardware LocalAiManager::detectHardware()
{
    return {ResourceBudget::logicalCpuCount(), ResourceBudget::totalMemoryBytes(), displayAdapterName()};
}

QString LocalAiManager::recommendedModelForMemory(quint64 memoryBytes)
{
    if (memoryBytes >= 48 * GiB) {
        return QStringLiteral("qwen3:30b");
    }
    if (memoryBytes >= 24 * GiB) {
        return QStringLiteral("qwen3:14b");
    }
    if (memoryBytes >= 12 * GiB) {
        return QStringLiteral("qwen3:8b");
    }
    return QStringLiteral("qwen3:4b");
}

QString LocalAiManager::recommendedModelForHardware(const LocalAiHardware &hardware)
{
    const bool dedicatedDesktopGpu = hardware.displayAdapter.contains(QLatin1String("Radeon RX"), Qt::CaseInsensitive) ||
                                     hardware.displayAdapter.contains(QLatin1String("GeForce"), Qt::CaseInsensitive) ||
                                     hardware.displayAdapter.contains(QLatin1String("NVIDIA RTX"), Qt::CaseInsensitive) ||
                                     hardware.displayAdapter.contains(QLatin1String("Intel Arc"), Qt::CaseInsensitive);
    if (dedicatedDesktopGpu) {
        return hardware.memoryBytes >= 12 * GiB ? QStringLiteral("qwen3:8b") : QStringLiteral("qwen3:4b");
    }
    return recommendedModelForMemory(hardware.memoryBytes);
}

QString LocalAiManager::approximateDownloadSize(const QString &model)
{
    if (model.contains(QLatin1String("30b"), Qt::CaseInsensitive)) {
        return QStringLiteral("19 GB");
    }
    if (model.contains(QLatin1String("14b"), Qt::CaseInsensitive)) {
        return QStringLiteral("9.3 GB");
    }
    if (model.contains(QLatin1String("8b"), Qt::CaseInsensitive)) {
        return QStringLiteral("5.2 GB");
    }
    return QStringLiteral("2.5 GB");
}

QString LocalAiManager::ollamaExecutable()
{
    const QString fromPath = QStandardPaths::findExecutable(QStringLiteral("ollama"));
    if (!fromPath.isEmpty()) {
        return fromPath;
    }
#ifdef Q_OS_WIN
    const QString localAppData = QProcessEnvironment::systemEnvironment().value(QStringLiteral("LOCALAPPDATA"));
    const QString candidate = QDir(localAppData).filePath(QStringLiteral("Programs/Ollama/ollama.exe"));
    if (QFileInfo::exists(candidate)) {
        return QDir::toNativeSeparators(candidate);
    }
#endif
    return {};
}

bool LocalAiManager::isBusy() const
{
    return m_phase != Phase::Idle;
}

bool LocalAiManager::isReady() const
{
    return m_ready;
}

bool LocalAiManager::isModelReady(const QString &model) const
{
    return m_ready && !model.trimmed().isEmpty() && model.trimmed() == m_readyModel;
}

QString LocalAiManager::recommendedModel() const
{
    return recommendedModelForHardware(m_hardware);
}

QString LocalAiManager::hardwareSummary() const
{
    const double memoryGiB = double(m_hardware.memoryBytes) / double(GiB);
    return i18n("Detected: %1 CPU threads · %2 GiB memory · %3", m_hardware.cpuThreads, QString::number(memoryGiB, 'f', 1), m_hardware.displayAdapter);
}

void LocalAiManager::refresh(const QString &model)
{
    if (isBusy()) {
        return;
    }
    m_setupRequested = false;
    m_targetModel = model.trimmed();
    if (ollamaExecutable().isEmpty()) {
        m_ready = false;
        m_readyModel.clear();
        Q_EMIT readyChanged(false);
        Q_EMIT statusChanged(i18n("Ollama is not installed. The recommended local model is %1.", recommendedModel()), false);
        return;
    }
    probeServer(false);
}

void LocalAiManager::installAndPrepare(const QString &model)
{
    if (isBusy()) {
        return;
    }
    m_setupRequested = true;
    m_targetModel = model.trimmed().isEmpty() ? recommendedModel() : model.trimmed();
    if (!ollamaExecutable().isEmpty()) {
        probeServer(true);
        return;
    }
    const QString winget = QStandardPaths::findExecutable(QStringLiteral("winget"));
    if (winget.isEmpty()) {
        Q_EMIT statusChanged(i18n("Windows Package Manager was not found. Install Ollama from ollama.com/download/windows, then choose Check again."), true);
        return;
    }
    setPhase(Phase::InstallingRuntime);
    Q_EMIT progressChanged(-1, -1, i18n("Installing Ollama locally"));
    Q_EMIT statusChanged(i18n("Installing Ollama. No project media is involved…"), false);
    m_installer->start(winget, {QStringLiteral("install"), QStringLiteral("--id"), QStringLiteral("Ollama.Ollama"), QStringLiteral("-e"),
                                QStringLiteral("--accept-package-agreements"), QStringLiteral("--accept-source-agreements"), QStringLiteral("--silent")});
    if (!m_installer->waitForStarted(5000)) {
        setPhase(Phase::Idle);
        Q_EMIT statusChanged(i18n("Ollama installation could not be started: %1", m_installer->errorString()), true);
    }
}

void LocalAiManager::cancel()
{
    if (m_reply) {
        disconnect(m_reply, nullptr, this, nullptr);
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    if (m_installer->state() != QProcess::NotRunning) {
        m_installer->terminate();
    }
    setPhase(Phase::Idle);
    Q_EMIT progressChanged(-1, -1, QString());
    Q_EMIT statusChanged(i18n("Local AI setup cancelled."), false);
}

void LocalAiManager::setPhase(Phase phase)
{
    const bool wasBusy = isBusy();
    m_phase = phase;
    if (wasBusy != isBusy()) {
        Q_EMIT busyChanged(isBusy());
    }
}

void LocalAiManager::probeServer(bool mayStartServer)
{
    setPhase(Phase::ProbingServer);
    QNetworkRequest request(QUrl(QStringLiteral("http://127.0.0.1:11434/api/tags")));
    request.setTransferTimeout(5000);
    m_reply = m_network->get(request);
    connect(m_reply, &QNetworkReply::finished, this, [this, mayStartServer]() { handleProbeFinished(mayStartServer); });
}

void LocalAiManager::handleProbeFinished(bool mayStartServer)
{
    if (m_phase != Phase::ProbingServer) {
        if (m_reply) {
            m_reply->deleteLater();
            m_reply = nullptr;
        }
        return;
    }
    QNetworkReply *reply = m_reply;
    const bool success = reply && reply->error() == QNetworkReply::NoError && reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200;
    const QByteArray payload = reply ? reply->readAll() : QByteArray();
    if (reply) {
        reply->deleteLater();
    }
    m_reply = nullptr;
    if (success) {
        if (!m_targetModel.isEmpty() && !responseContainsModel(payload)) {
            if (m_setupRequested) {
                beginModelPull();
                return;
            }
            m_ready = false;
            m_readyModel.clear();
            setPhase(Phase::Idle);
            Q_EMIT readyChanged(false);
            Q_EMIT statusChanged(i18n("Ollama is running, but model %1 is not downloaded. Choose Prepare / download local model.", m_targetModel), false);
            return;
        }
        m_ready = true;
        m_readyModel = m_targetModel;
        setPhase(Phase::Idle);
        Q_EMIT readyChanged(true);
        Q_EMIT statusChanged(m_targetModel.isEmpty() ? i18n("Local Ollama service is ready.") : i18n("Local model %1 is ready.", m_targetModel), false);
        return;
    }
    if (mayStartServer && !ollamaExecutable().isEmpty()) {
        if (m_server->state() == QProcess::NotRunning) {
            m_server->start(ollamaExecutable(), {QStringLiteral("serve")});
        }
        QTimer::singleShot(2500, this, [this]() {
            if (m_phase == Phase::ProbingServer) {
                probeServer(false);
            }
        });
        return;
    }
    m_ready = false;
    m_readyModel.clear();
    setPhase(Phase::Idle);
    Q_EMIT readyChanged(false);
    Q_EMIT statusChanged(ollamaExecutable().isEmpty() ? i18n("Ollama is not installed.")
                                                      : i18n("Ollama is installed, but its local service is not responding. Choose Prepare local AI."),
                         false);
}

void LocalAiManager::beginModelPull()
{
    setPhase(Phase::PullingModel);
    m_pullBuffer.clear();
    m_pullTimer.restart();
    Q_EMIT progressChanged(0, -1, i18n("Downloading local model %1", m_targetModel));
    Q_EMIT statusChanged(i18n("Downloading %1 (approximately %2). You can cancel and resume later.", m_targetModel, approximateDownloadSize(m_targetModel)),
                         false);
    QNetworkRequest request(QUrl(QStringLiteral("http://127.0.0.1:11434/api/pull")));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setTransferTimeout(60 * 60 * 1000);
    m_reply = m_network->post(
        request, QJsonDocument(QJsonObject{{QStringLiteral("model"), m_targetModel}, {QStringLiteral("stream"), true}}).toJson(QJsonDocument::Compact));
    connect(m_reply, &QNetworkReply::readyRead, this, &LocalAiManager::consumePullOutput);
    connect(m_reply, &QNetworkReply::finished, this, &LocalAiManager::finishModelPull);
}

void LocalAiManager::consumePullOutput()
{
    if (!m_reply) {
        return;
    }
    m_pullBuffer.append(m_reply->readAll());
    int newline = -1;
    while ((newline = m_pullBuffer.indexOf('\n')) >= 0) {
        const QByteArray line = m_pullBuffer.left(newline).trimmed();
        m_pullBuffer.remove(0, newline + 1);
        const QJsonObject item = QJsonDocument::fromJson(line).object();
        const qint64 total = qint64(item.value(QStringLiteral("total")).toDouble());
        const qint64 completed = qint64(item.value(QStringLiteral("completed")).toDouble());
        if (total <= 0 || completed < 0) {
            continue;
        }
        const int percent = qBound(0, int(qRound(double(completed) * 100.0 / double(total))), 100);
        const qint64 remaining = percent > 0 && percent < 100 ? qRound64(double(m_pullTimer.elapsed()) * double(100 - percent) / double(percent) / 1000.0) : -1;
        Q_EMIT progressChanged(percent, remaining, i18n("Downloading local model %1", m_targetModel));
    }
}

void LocalAiManager::finishModelPull()
{
    if (m_phase != Phase::PullingModel) {
        if (m_reply) {
            m_reply->deleteLater();
            m_reply = nullptr;
        }
        return;
    }
    consumePullOutput();
    QNetworkReply *reply = m_reply;
    const int status = reply ? reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() : 0;
    const bool success = reply && reply->error() == QNetworkReply::NoError && status >= 200 && status < 300;
    const QString error = reply ? reply->errorString() : i18n("Unknown network error");
    if (reply) {
        reply->deleteLater();
    }
    m_reply = nullptr;
    setPhase(Phase::Idle);
    if (!success) {
        m_ready = false;
        m_readyModel.clear();
        Q_EMIT readyChanged(false);
        Q_EMIT statusChanged(i18n("The local model download stopped: %1. Choose Prepare local AI to resume.", error), true);
        return;
    }
    m_ready = true;
    m_readyModel = m_targetModel;
    Q_EMIT progressChanged(100, 0, i18n("Local model ready"));
    Q_EMIT readyChanged(true);
    Q_EMIT statusChanged(i18n("Local model %1 is ready and will work without an API key.", m_targetModel), false);
}

bool LocalAiManager::responseContainsModel(const QByteArray &payload) const
{
    const QJsonArray models = QJsonDocument::fromJson(payload).object().value(QStringLiteral("models")).toArray();
    for (const QJsonValue &value : models) {
        const QJsonObject model = value.toObject();
        const QString name = model.value(QStringLiteral("name")).toString(model.value(QStringLiteral("model")).toString());
        if (name == m_targetModel || name == m_targetModel + QLatin1String(":latest")) {
            return true;
        }
    }
    return false;
}

} // namespace AiEditor
} // namespace Kdenlive
