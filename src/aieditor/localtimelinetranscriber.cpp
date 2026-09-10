/*
    SPDX-FileCopyrightText: 2026 Kdenlive AI Editor contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "localtimelinetranscriber.hpp"

#include "kdenlivesettings.h"
#include "pythoninterfaces/speechtotextwhisper.h"
#include "timeline2/model/timelineitemmodel.hpp"

#include <KLocalizedString>
#include <QFile>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTextStream>

namespace Kdenlive {
namespace AiEditor {

namespace {
qint64 timestampMilliseconds(const QString &hours, const QString &minutes, const QString &seconds, const QString &milliseconds)
{
    return hours.toLongLong() * 3600000 + minutes.toLongLong() * 60000 + seconds.toLongLong() * 1000 + milliseconds.toLongLong();
}
} // namespace

LocalTimelineTranscriber::LocalTimelineTranscriber(QObject *parent)
    : QObject(parent)
    , m_whisper(new SpeechToTextWhisper(this))
    , m_process(new QProcess(this))
{
    m_process->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, &LocalTimelineTranscriber::finishProcess);
}

LocalTimelineTranscriber::~LocalTimelineTranscriber()
{
    if (m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(3000);
    }
    releaseNativeBudget();
}

bool LocalTimelineTranscriber::isBusy() const
{
    return m_process->state() != QProcess::NotRunning || bool(m_tempDir);
}

void LocalTimelineTranscriber::start(const std::shared_ptr<TimelineItemModel> &timeline, double fps)
{
    if (isBusy()) {
        Q_EMIT errorOccurred(i18n("A local transcription is already running."));
        return;
    }
    if (!timeline || timeline->duration() < 1 || fps <= 0.0) {
        Q_EMIT errorOccurred(i18n("The active project has invalid timeline timing information."));
        return;
    }

    const auto python = m_whisper->venvPythonExecs().python;
    const QString model = KdenliveSettings::whisperModel();
    if (python.isEmpty() || m_whisper->subtitleScript().isEmpty() || !m_whisper->getInstalledModels().contains(model)) {
        Q_EMIT errorOccurred(
            i18n("Local speech recognition is not ready. Open Settings > Configure Kdenlive > Plugins, install Whisper and a model, then try again."));
        return;
    }

    m_tempDir = std::make_unique<QTemporaryDir>();
    if (!m_tempDir->isValid()) {
        reset();
        Q_EMIT errorOccurred(i18n("Could not create temporary files for local transcription."));
        return;
    }
    Q_EMIT busyChanged(true);
    m_cancelRequested = false;
    m_fps = fps;
    m_budget = ResourceBudget::fromSettings();
    const QString scenePath = m_tempDir->filePath(QStringLiteral("timeline.mlt"));
    m_audioPath = m_tempDir->filePath(QStringLiteral("timeline.wav"));
    m_srtPath = m_tempDir->filePath(QStringLiteral("timeline.srt"));

    Q_EMIT statusChanged(i18n("Preparing the current timeline audio locally…"));
    timeline->sceneList(m_tempDir->path(), scenePath);
    if (!QFileInfo::exists(scenePath) || KdenliveSettings::meltpath().isEmpty()) {
        reset();
        Q_EMIT busyChanged(false);
        Q_EMIT errorOccurred(i18n("The current timeline audio could not be prepared for transcription."));
        return;
    }
    m_phase = Phase::ExportAudio;
    configureProcessEnvironment();
    m_process->start(KdenliveSettings::meltpath(),
                     {QStringLiteral("-progress"), scenePath, QStringLiteral("-consumer"), QStringLiteral("avformat:%1").arg(m_audioPath),
                      QStringLiteral("vn=1"), QStringLiteral("ar=16000"), QStringLiteral("ac=1"), QStringLiteral("threads=%1").arg(m_budget.cpuThreads)});
    if (!m_process->waitForStarted(5000)) {
        const QString error = m_process->errorString();
        reset();
        Q_EMIT busyChanged(false);
        Q_EMIT errorOccurred(i18n("Timeline audio export could not be started: %1", error));
    } else {
        applyNativeBudget();
    }
}

void LocalTimelineTranscriber::startWhisper()
{
    Q_EMIT statusChanged(i18n("Transcribing locally with Whisper. This may take several minutes…"));
    QStringList arguments{m_whisper->subtitleScript(), m_audioPath, KdenliveSettings::whisperModel(),
                          QStringLiteral("ffmpeg_path=%1").arg(KdenliveSettings::ffmpegpath())};
    const QString language = KdenliveSettings::whisperLanguage().simplified();
    if (!language.isEmpty()) {
        arguments << QStringLiteral("language=%1").arg(language);
    }
    arguments << QStringLiteral("device=%1").arg(m_budget.device);
    if (KdenliveSettings::whisperDisableFP16() || m_budget.device == QLatin1String("cpu")) {
        arguments << QStringLiteral("fp16=False");
    }
    m_phase = Phase::Transcribe;
    configureProcessEnvironment();
    m_process->start(m_whisper->venvPythonExecs().python, arguments);
    if (!m_process->waitForStarted(5000)) {
        const QString error = m_process->errorString();
        reset();
        Q_EMIT busyChanged(false);
        Q_EMIT errorOccurred(i18n("Whisper could not be started: %1", error));
    } else {
        applyNativeBudget();
    }
}

void LocalTimelineTranscriber::cancel()
{
    if (!isBusy()) {
        return;
    }
    m_cancelRequested = true;
    if (m_process->state() != QProcess::NotRunning) {
        m_process->kill();
    }
}

QString LocalTimelineTranscriber::parseSrt(const QByteArray &srt, double fps)
{
    const QString normalized = QString::fromUtf8(srt).replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    const QRegularExpression cue(
        QStringLiteral(R"((?:^|\n)\d+\s*\n(\d{2}):(\d{2}):(\d{2})[,.](\d{3})\s*-->\s*(\d{2}):(\d{2}):(\d{2})[,.](\d{3})\s*\n([\s\S]*?)(?=\n\s*\n|$))"));
    QStringList lines;
    auto match = cue.globalMatch(normalized);
    while (match.hasNext()) {
        const auto item = match.next();
        const qint64 startMs = timestampMilliseconds(item.captured(1), item.captured(2), item.captured(3), item.captured(4));
        const qint64 endMs = timestampMilliseconds(item.captured(5), item.captured(6), item.captured(7), item.captured(8));
        QString text = item.captured(9).simplified();
        if (text.isEmpty()) {
            continue;
        }
        const int startFrame = qMax(0, int(qRound64(double(startMs) * fps / 1000.0)));
        const int endFrame = qMax(startFrame + 1, int(qRound64(double(endMs) * fps / 1000.0)));
        lines << QStringLiteral("[%1-%2] %3").arg(startFrame).arg(endFrame).arg(text);
    }
    return lines.join(QLatin1Char('\n'));
}

void LocalTimelineTranscriber::finishProcess(int exitCode, QProcess::ExitStatus status)
{
    releaseNativeBudget();
    if (m_cancelRequested) {
        reset();
        Q_EMIT busyChanged(false);
        Q_EMIT cancelled();
        return;
    }
    if (status != QProcess::NormalExit || exitCode != 0) {
        const Phase failedPhase = m_phase;
        QString details = QString::fromUtf8(m_process->readAll()).trimmed();
        if (details.size() > 600) {
            details = details.right(600);
        }
        reset();
        Q_EMIT busyChanged(false);
        Q_EMIT errorOccurred(failedPhase == Phase::ExportAudio ? i18n("Timeline audio export failed: %1", details)
                                                               : i18n("Local Whisper transcription failed: %1", details));
        return;
    }

    if (m_phase == Phase::ExportAudio) {
        m_process->readAll();
        if (!QFileInfo::exists(m_audioPath) || QFileInfo(m_audioPath).size() == 0) {
            reset();
            Q_EMIT busyChanged(false);
            Q_EMIT errorOccurred(i18n("Timeline audio export produced no usable audio."));
            return;
        }
        startWhisper();
        return;
    }

    QFile file(m_srtPath);
    if (!file.open(QIODevice::ReadOnly)) {
        reset();
        Q_EMIT busyChanged(false);
        Q_EMIT errorOccurred(i18n("Whisper finished without producing a readable transcript."));
        return;
    }
    const QString transcript = parseSrt(file.readAll(), m_fps);
    reset();
    Q_EMIT busyChanged(false);
    if (transcript.isEmpty()) {
        Q_EMIT errorOccurred(i18n("No dialogue was detected in the current timeline."));
    } else {
        Q_EMIT transcriptReady(transcript);
    }
}

void LocalTimelineTranscriber::configureProcessEnvironment()
{
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    const QString threads = QString::number(m_budget.cpuThreads);
    environment.insert(QStringLiteral("KDENLIVE_AI_THREADS"), threads);
    environment.insert(QStringLiteral("OMP_NUM_THREADS"), threads);
    environment.insert(QStringLiteral("MKL_NUM_THREADS"), threads);
    environment.insert(QStringLiteral("OPENBLAS_NUM_THREADS"), threads);
    environment.insert(QStringLiteral("KDENLIVE_AI_MEMORY_FRACTION"), QString::number(double(m_budget.percent) / 100.0, 'f', 2));
    m_process->setProcessEnvironment(environment);
}

void LocalTimelineTranscriber::applyNativeBudget()
{
    releaseNativeBudget();
    m_nativeBudgetHandle = ResourceBudget::applyToProcess(m_process->processId(), m_budget);
}

void LocalTimelineTranscriber::releaseNativeBudget()
{
    ResourceBudget::releaseProcessBudget(m_nativeBudgetHandle);
    m_nativeBudgetHandle = 0;
}

void LocalTimelineTranscriber::reset()
{
    releaseNativeBudget();
    m_phase = Phase::Idle;
    m_audioPath.clear();
    m_srtPath.clear();
    m_fps = 0.0;
    m_tempDir.reset();
}

} // namespace AiEditor
} // namespace Kdenlive
