/*
    SPDX-FileCopyrightText: 2026 Kdenlive AI Editor contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "localaimanager.hpp"
#include "resourcebudget.hpp"

#include <QElapsedTimer>
#include <QObject>
#include <QProcess>
#include <QVector>
#include <memory>

class QNetworkAccessManager;
class QNetworkReply;
class QTemporaryDir;
class TimelineItemModel;

namespace Kdenlive {
namespace AiEditor {

struct LocalVisionResult
{
    QVector<int> personFrames;
    int sampleStepFrames{1};
    QString timelineFingerprint;
    QString backend;
    bool restoredFromCache{false};
};

class LocalVisionAnalyzer : public QObject
{
    Q_OBJECT

public:
    explicit LocalVisionAnalyzer(QObject *parent = nullptr);
    ~LocalVisionAnalyzer() override;

    bool isBusy() const;
    bool isReady() const;
    QString hardwareSummary() const;
    QString recommendationSummary() const;
    QString modelPath() const;
    QString helperExecutable() const;

    void refresh();
    void prepare();
    void start(const std::shared_ptr<TimelineItemModel> &timeline, double fps);
    void cancel();

    static QString recommendedModelFile(const LocalAiHardware &hardware);
    static int recommendedSamplesPerMinute(const LocalAiHardware &hardware, const ResourceBudget &budget);
    static qint64 estimateRemainingSeconds(qint64 elapsedMilliseconds, int completed, int total);

Q_SIGNALS:
    void statusChanged(const QString &message, bool error);
    void progressChanged(int percent, qint64 remainingSeconds, const QString &phase);
    void readyChanged(bool ready);
    void busyChanged(bool busy);
    void analysisReady(const Kdenlive::AiEditor::LocalVisionResult &result);
    void cancelled();

private:
    enum class Phase { Idle, DownloadingModel, Analyzing };
    void setPhase(Phase phase);
    void finishDownload();
    void consumeProcessOutput();
    void finishAnalysis(int exitCode, QProcess::ExitStatus status);
    QString cachePath(const QString &fingerprint) const;
    bool loadResultFile(const QString &path, LocalVisionResult &result, QString *error = nullptr) const;
    bool saveResultFile(const QString &path, const LocalVisionResult &result, int timelineFrames, double fps, const QString &fingerprint) const;
    bool restoreLegacyResult(const QString &destination, int timelineFrames, double fps, LocalVisionResult &result) const;
    void resetAnalysis();
    void applyNativeBudget();
    void releaseNativeBudget();

    QNetworkAccessManager *m_network{nullptr};
    QNetworkReply *m_reply{nullptr};
    QProcess *m_process{nullptr};
    std::unique_ptr<QTemporaryDir> m_tempDir;
    LocalAiHardware m_hardware;
    ResourceBudget m_budget;
    Phase m_phase{Phase::Idle};
    QByteArray m_processOutput;
    QString m_outputPath;
    QString m_fingerprint;
    int m_sampleStepFrames{1};
    int m_timelineFrames{0};
    double m_fps{0.0};
    bool m_cancelRequested{false};
    quintptr m_nativeBudgetHandle{0};
    QElapsedTimer m_timer;
};

} // namespace AiEditor
} // namespace Kdenlive

Q_DECLARE_METATYPE(Kdenlive::AiEditor::LocalVisionResult)
