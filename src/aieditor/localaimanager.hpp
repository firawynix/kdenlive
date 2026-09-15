/*
    SPDX-FileCopyrightText: 2026 Kdenlive AI Editor contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QStringList>

class QNetworkAccessManager;
class QNetworkReply;
class QProcess;

namespace Kdenlive {
namespace AiEditor {

struct LocalAiHardware
{
    int cpuThreads{1};
    quint64 memoryBytes{0};
    QString displayAdapter;
    quint64 videoMemoryBytes{0};
};

class LocalAiManager : public QObject
{
    Q_OBJECT

public:
    explicit LocalAiManager(QObject *parent = nullptr);

    static LocalAiHardware detectHardware();
    static QString recommendedModelForMemory(quint64 memoryBytes);
    static QString recommendedModelForHardware(const LocalAiHardware &hardware);
    static QStringList modelCatalog();
    static QStringList modelsFromTagsResponse(const QByteArray &payload);
    static QString approximateDownloadSize(const QString &model);
    static QString recommendationReason(const LocalAiHardware &hardware, const QString &model);
    static QString ollamaExecutable();

    bool isBusy() const;
    bool isReady() const;
    bool isModelReady(const QString &model) const;
    QString recommendedModel() const;
    QString recommendationReason() const;
    QString hardwareSummary() const;
    QStringList installedModels() const;

    void refresh(const QString &model = QString());
    void installAndPrepare(const QString &model);
    void cancel();

Q_SIGNALS:
    void statusChanged(const QString &message, bool error);
    void progressChanged(int percent, qint64 remainingSeconds, const QString &phase);
    void readyChanged(bool ready);
    void installedModelsChanged(const QStringList &models);
    void busyChanged(bool busy);

private:
    enum class Phase { Idle, InstallingRuntime, ProbingServer, PullingModel };
    void setPhase(Phase phase);
    void probeServer(bool mayStartServer);
    void handleProbeFinished(bool mayStartServer);
    void beginModelPull();
    void consumePullOutput();
    void finishModelPull();
    bool responseContainsModel(const QByteArray &payload) const;

    QNetworkAccessManager *m_network{nullptr};
    QNetworkReply *m_reply{nullptr};
    QProcess *m_installer{nullptr};
    QProcess *m_server{nullptr};
    LocalAiHardware m_hardware;
    QString m_targetModel;
    QString m_readyModel;
    QStringList m_installedModels;
    QByteArray m_pullBuffer;
    QElapsedTimer m_pullTimer;
    Phase m_phase{Phase::Idle};
    bool m_ready{false};
    bool m_setupRequested{false};
    bool m_forcePull{false};
};

} // namespace AiEditor
} // namespace Kdenlive
