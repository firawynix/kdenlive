/*
    SPDX-FileCopyrightText: 2026 Kdenlive AI Editor contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "resourcebudget.hpp"

#include <QElapsedTimer>
#include <QObject>
#include <QProcess>
#include <memory>

class QTemporaryDir;
class SpeechToTextWhisper;
class TimelineItemModel;

namespace Kdenlive {
namespace AiEditor {

class LocalTimelineTranscriber : public QObject
{
    Q_OBJECT

public:
    explicit LocalTimelineTranscriber(QObject *parent = nullptr);
    ~LocalTimelineTranscriber() override;

    bool isBusy() const;
    void start(const std::shared_ptr<TimelineItemModel> &timeline, double fps);
    void cancel();

    static QString parseSrt(const QByteArray &srt, double fps);
    static QString selectAvailableModel(const QString &configuredModel, const QStringList &installedModels);
    static int parseProgressPercent(const QByteArray &output, bool whisperPhase);
    static qint64 estimateRemainingSeconds(qint64 elapsedMilliseconds, int percent);

Q_SIGNALS:
    void statusChanged(const QString &message);
    void transcriptReady(const QString &transcript, const QString &timelineFingerprint);
    void errorOccurred(const QString &message);
    void cancelled();
    void busyChanged(bool busy);
    void progressChanged(int percent, qint64 remainingSeconds, const QString &phase);

private:
    enum class Phase { Idle, ExportAudio, Transcribe };
    void startWhisper();
    void finishProcess(int exitCode, QProcess::ExitStatus status);
    void configureProcessEnvironment();
    void applyNativeBudget();
    void releaseNativeBudget();
    void consumeProcessOutput();
    void beginProgressPhase(const QString &phase);
    void emitParsedProgress();
    void reset();

    SpeechToTextWhisper *m_whisper{nullptr};
    QProcess *m_process{nullptr};
    std::unique_ptr<QTemporaryDir> m_tempDir;
    QString m_audioPath;
    QString m_srtPath;
    QString m_timelineFingerprint;
    QString m_model;
    double m_fps{0.0};
    bool m_cancelRequested{false};
    Phase m_phase{Phase::Idle};
    ResourceBudget m_budget;
    quintptr m_nativeBudgetHandle{0};
    QByteArray m_processOutput;
    QElapsedTimer m_phaseTimer;
    QString m_progressPhase;
    int m_lastProgress{-1};
};

} // namespace AiEditor
} // namespace Kdenlive
