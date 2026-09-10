/*
    SPDX-FileCopyrightText: 2026 Kdenlive AI Editor contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

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

Q_SIGNALS:
    void statusChanged(const QString &message);
    void transcriptReady(const QString &transcript);
    void errorOccurred(const QString &message);
    void cancelled();
    void busyChanged(bool busy);

private:
    enum class Phase { Idle, ExportAudio, Transcribe };
    void startWhisper();
    void finishProcess(int exitCode, QProcess::ExitStatus status);
    void reset();

    SpeechToTextWhisper *m_whisper{nullptr};
    QProcess *m_process{nullptr};
    std::unique_ptr<QTemporaryDir> m_tempDir;
    QString m_audioPath;
    QString m_srtPath;
    double m_fps{0.0};
    bool m_cancelRequested{false};
    Phase m_phase{Phase::Idle};
};

} // namespace AiEditor
} // namespace Kdenlive
