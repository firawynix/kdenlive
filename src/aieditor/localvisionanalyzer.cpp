/*
    SPDX-FileCopyrightText: 2026 Kdenlive AI Editor contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "localvisionanalyzer.hpp"

#include "aisessionstore.hpp"
#include "timeline2/model/timelineitemmodel.hpp"

#include <KLocalizedString>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <algorithm>
#include <climits>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

namespace Kdenlive {
namespace AiEditor {

namespace {
constexpr quint64 GiB = 1024ULL * 1024ULL * 1024ULL;
constexpr auto FloatModel = "object_detection_nanodet_2022nov.onnx";
constexpr auto Int8Model = "object_detection_nanodet_2022nov_int8.onnx";
constexpr auto FloatHash = "4b82da9944b88577175ee23a459dce2e26e6e4be573def65b1055dc2d9720186";
constexpr auto Int8Hash = "8dd32b85f2d273e9047f1d6b59e0b2fd008b1076338107bb547ac28942cdf90b";
constexpr auto ModelRoot =
    "https://media.githubusercontent.com/media/opencv/opencv_zoo/47534e27c9851bb1128ccc0102f1145e27f23f98/models/object_detection_nanodet/";

QString modelHash(const QString &file)
{
    return file == QLatin1String(Int8Model) ? QString::fromLatin1(Int8Hash) : QString::fromLatin1(FloatHash);
}

QString modelUrl(const QString &file)
{
    return QString::fromLatin1(ModelRoot) + file;
}

bool modelFileIsVerified(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    const QString actual = QString::fromLatin1(QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha256).toHex());
    return actual == modelHash(QFileInfo(path).fileName());
}
} // namespace

LocalVisionAnalyzer::LocalVisionAnalyzer(QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
    , m_process(new QProcess(this))
    , m_hardware(LocalAiManager::detectHardware())
{
    qRegisterMetaType<LocalVisionResult>();
    m_process->setProcessChannelMode(QProcess::MergedChannels);
#ifdef Q_OS_WIN
    m_process->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *arguments) { arguments->flags |= CREATE_NO_WINDOW; });
#endif
    connect(m_process, &QProcess::readyReadStandardOutput, this, &LocalVisionAnalyzer::consumeProcessOutput);
    connect(m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, &LocalVisionAnalyzer::finishAnalysis);
}

LocalVisionAnalyzer::~LocalVisionAnalyzer()
{
    if (m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(3000);
    }
    releaseNativeBudget();
}

bool LocalVisionAnalyzer::isBusy() const
{
    return m_phase != Phase::Idle;
}

bool LocalVisionAnalyzer::isReady() const
{
    const QFileInfo model(modelPath());
    return model.isFile() && model.size() > 0 && !helperExecutable().isEmpty();
}

QString LocalVisionAnalyzer::hardwareSummary() const
{
    QString result = i18n("Detected for visual analysis: %1 CPU threads · %2 GiB memory · %3", m_hardware.cpuThreads,
                          QString::number(double(m_hardware.memoryBytes) / double(GiB), 'f', 1), m_hardware.displayAdapter);
    if (m_hardware.videoMemoryBytes > 0) {
        result += i18n(" · %1 GiB GPU memory", QString::number(double(m_hardware.videoMemoryBytes) / double(GiB), 'f', 1));
    }
    return result;
}

QString LocalVisionAnalyzer::recommendationSummary() const
{
    const QString precision = recommendedModelFile(m_hardware) == QLatin1String(Int8Model) ? i18n("compact INT8") : i18n("high-accuracy FP32");
    const int samples = recommendedSamplesPerMinute(m_hardware, ResourceBudget::fromSettings());
    return i18n("Recommended automatically: NanoDet %1 · %2 video samples per minute · OpenCV acceleration with safe CPU fallback.", precision, samples);
}

QString LocalVisionAnalyzer::modelPath() const
{
    const QString root = QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath(QStringLiteral("ai-editor/models"));
    return QDir(root).filePath(recommendedModelFile(m_hardware));
}

QString LocalVisionAnalyzer::helperExecutable() const
{
#ifdef Q_OS_WIN
    const QString name = QStringLiteral("firawynix-kdenlive-ai-vision.exe");
#else
    const QString name = QStringLiteral("firawynix-kdenlive-ai-vision");
#endif
    const QString besideApplication = QDir(QCoreApplication::applicationDirPath()).filePath(name);
    if (QFileInfo::exists(besideApplication)) {
        return QDir::toNativeSeparators(besideApplication);
    }
    return QStandardPaths::findExecutable(name);
}

void LocalVisionAnalyzer::refresh()
{
    Q_EMIT readyChanged(isReady());
    if (helperExecutable().isEmpty()) {
        Q_EMIT statusChanged(i18n("The local visual analyzer is not included in this installation. Install the newest Firawynix - Kdenlive update."), true);
    } else if (!QFileInfo::exists(modelPath())) {
        Q_EMIT statusChanged(i18n("The person detector is not downloaded yet. Choose Prepare visual analysis."), false);
    } else {
        Q_EMIT statusChanged(i18n("The best compatible local person detector is installed. Choose Check for updates to verify it at any time."), false);
    }
}

void LocalVisionAnalyzer::prepare()
{
    if (isBusy()) {
        return;
    }
    if (helperExecutable().isEmpty()) {
        refresh();
        return;
    }
    if (QFileInfo::exists(modelPath()) && modelFileIsVerified(modelPath())) {
        Q_EMIT progressChanged(100, 0, i18n("Local person detector verified"));
        Q_EMIT readyChanged(true);
        Q_EMIT statusChanged(i18n("The best compatible local person detector is installed, verified, and ready."), false);
        return;
    }
    QDir().mkpath(QFileInfo(modelPath()).absolutePath());
    setPhase(Phase::DownloadingModel);
    m_timer.restart();
    const QString file = QFileInfo(modelPath()).fileName();
    QNetworkRequest request(QUrl(modelUrl(file)));
    request.setTransferTimeout(10 * 60 * 1000);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Firawynix-Kdenlive/26.11"));
    m_reply = m_network->get(request);
    connect(m_reply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) {
        const int percent = total > 0 ? qBound(0, int(received * 100 / total), 100) : -1;
        Q_EMIT progressChanged(percent, estimateRemainingSeconds(m_timer.elapsed(), int(received), int(qMin<qint64>(total, INT_MAX))),
                               i18n("Downloading the local person detector"));
    });
    connect(m_reply, &QNetworkReply::finished, this, &LocalVisionAnalyzer::finishDownload);
    Q_EMIT statusChanged(i18n("Downloading the detector selected for this computer. No project media is uploaded."), false);
}

void LocalVisionAnalyzer::finishDownload()
{
    if (m_phase != Phase::DownloadingModel || !m_reply) {
        return;
    }
    QNetworkReply *reply = m_reply;
    m_reply = nullptr;
    const QByteArray payload = reply->readAll();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const bool downloaded = reply->error() == QNetworkReply::NoError && status >= 200 && status < 300;
    const QString networkError = reply->errorString();
    reply->deleteLater();
    if (!downloaded) {
        setPhase(Phase::Idle);
        Q_EMIT statusChanged(i18n("The visual detector download failed: %1", networkError), true);
        return;
    }
    const QString actualHash = QString::fromLatin1(QCryptographicHash::hash(payload, QCryptographicHash::Sha256).toHex());
    if (actualHash != modelHash(QFileInfo(modelPath()).fileName())) {
        setPhase(Phase::Idle);
        Q_EMIT statusChanged(i18n("The downloaded visual detector failed its security verification and was not installed."), true);
        return;
    }
    QSaveFile file(modelPath());
    if (!file.open(QIODevice::WriteOnly) || file.write(payload) != payload.size() || !file.commit()) {
        setPhase(Phase::Idle);
        Q_EMIT statusChanged(i18n("The verified visual detector could not be saved on this computer."), true);
        return;
    }
    setPhase(Phase::Idle);
    Q_EMIT progressChanged(100, 0, i18n("Local person detector ready"));
    Q_EMIT readyChanged(true);
    Q_EMIT statusChanged(i18n("Local person detection is ready. Video frames remain on this computer."), false);
}

void LocalVisionAnalyzer::start(const std::shared_ptr<TimelineItemModel> &timeline, double fps)
{
    if (isBusy()) {
        Q_EMIT statusChanged(i18n("A local visual analysis is already running."), true);
        return;
    }
    if (!timeline || timeline->duration() < 2 || fps <= 0.0) {
        Q_EMIT statusChanged(i18n("The active project has invalid timeline timing information."), true);
        return;
    }
    if (!isReady()) {
        Q_EMIT statusChanged(i18n("Local visual analysis is not ready. Choose Prepare visual analysis first."), true);
        return;
    }
    m_timelineFrames = timeline->duration();
    m_fps = fps;
    m_tempDir = std::make_unique<QTemporaryDir>();
    if (!m_tempDir->isValid()) {
        resetAnalysis();
        Q_EMIT statusChanged(i18n("Could not create temporary files for local visual analysis."), true);
        return;
    }
    const QString scenePath = m_tempDir->filePath(QStringLiteral("timeline.mlt"));
    m_outputPath = m_tempDir->filePath(QStringLiteral("person-detections.json"));
    timeline->sceneList(m_tempDir->path(), scenePath);
    QFile scene(scenePath);
    if (!scene.open(QIODevice::ReadOnly)) {
        resetAnalysis();
        Q_EMIT statusChanged(i18n("The current timeline could not be prepared for local visual analysis."), true);
        return;
    }
    const QByteArray sceneData = scene.readAll();
    m_budget = ResourceBudget::fromSettings();
    const int samplesPerMinute = recommendedSamplesPerMinute(m_hardware, m_budget);
    m_sampleStepFrames = qMax(1, int(qRound(fps * 60.0 / double(samplesPerMinute))));
    m_fingerprint = AiSessionStore::timelineFingerprint(sceneData, fps, QFileInfo(modelPath()).fileName(), QStringLiteral("person-detection-v2"),
                                                        m_tempDir->path());
    LocalVisionResult cached;
    const QString saved = cachePath(m_fingerprint);
    const QFileInfo savedInfo(saved);
    if (savedInfo.exists() && savedInfo.lastModified().daysTo(QDateTime::currentDateTimeUtc()) <= 7 && loadResultFile(saved, cached)) {
        cached.timelineFingerprint = m_fingerprint;
        cached.restoredFromCache = true;
        resetAnalysis();
        Q_EMIT statusChanged(i18n("Saved local person detections restored. The video does not need to be analyzed again."), false);
        Q_EMIT analysisReady(cached);
        return;
    }
    if (restoreLegacyResult(saved, timeline->duration(), fps, cached)) {
        cached.timelineFingerprint = m_fingerprint;
        cached.restoredFromCache = true;
        resetAnalysis();
        Q_EMIT statusChanged(i18n("Compatible person detections from an earlier version were restored. The video does not need to be analyzed again."), false);
        Q_EMIT analysisReady(cached);
        return;
    }

    m_cancelRequested = false;
    m_processOutput.clear();
    m_timer.restart();
    setPhase(Phase::Analyzing);
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("OMP_NUM_THREADS"), QString::number(m_budget.cpuThreads));
    environment.insert(QStringLiteral("OPENCV_FOR_THREADS_NUM"), QString::number(m_budget.cpuThreads));
    m_process->setProcessEnvironment(environment);
    const QString visionDevice = m_budget.device == QLatin1String("cuda") ? QStringLiteral("cuda")
                                 : m_budget.device == QLatin1String("cpu") ? QStringLiteral("cpu")
                                                                          : QStringLiteral("auto");
    m_process->start(helperExecutable(), {QStringLiteral("--scene"), scenePath, QStringLiteral("--model"), modelPath(), QStringLiteral("--output"),
                                          m_outputPath, QStringLiteral("--sample-step"), QString::number(m_sampleStepFrames),
                                          QStringLiteral("--timeline-frames"), QString::number(timeline->duration()), QStringLiteral("--device"), visionDevice});
    if (!m_process->waitForStarted(5000)) {
        const QString error = m_process->errorString();
        resetAnalysis();
        setPhase(Phase::Idle);
        Q_EMIT statusChanged(i18n("Local visual analysis could not be started: %1", error), true);
        return;
    }
    applyNativeBudget();
    Q_EMIT progressChanged(0, -1, i18n("Analyzing the timeline locally for people"));
    Q_EMIT statusChanged(i18n("Detecting people locally. You can cancel; completed analysis is cached after it finishes."), false);
}

void LocalVisionAnalyzer::cancel()
{
    if (!isBusy()) {
        return;
    }
    m_cancelRequested = true;
    if (m_reply) {
        disconnect(m_reply, nullptr, this, nullptr);
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    if (m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        return;
    }
    setPhase(Phase::Idle);
    Q_EMIT cancelled();
}

QString LocalVisionAnalyzer::recommendedModelFile(const LocalAiHardware &hardware)
{
    return hardware.cpuThreads < 8 || (hardware.memoryBytes > 0 && hardware.memoryBytes < 8 * GiB) ? QString::fromLatin1(Int8Model)
                                                                                                   : QString::fromLatin1(FloatModel);
}

int LocalVisionAnalyzer::recommendedSamplesPerMinute(const LocalAiHardware &hardware, const ResourceBudget &budget)
{
    const int effectiveThreads = qMin(hardware.cpuThreads, budget.cpuThreads);
    if (effectiveThreads >= 16 && hardware.memoryBytes >= 16 * GiB) {
        return 120;
    }
    if (effectiveThreads >= 8 && hardware.memoryBytes >= 8 * GiB) {
        return 60;
    }
    return 30;
}

qint64 LocalVisionAnalyzer::estimateRemainingSeconds(qint64 elapsedMilliseconds, int completed, int total)
{
    if (elapsedMilliseconds < 1000 || completed <= 0 || total <= completed) {
        return -1;
    }
    return qMax<qint64>(0, qRound64(double(elapsedMilliseconds) * double(total - completed) / double(completed) / 1000.0));
}

void LocalVisionAnalyzer::setPhase(Phase phase)
{
    const bool wasBusy = isBusy();
    m_phase = phase;
    if (wasBusy != isBusy()) {
        Q_EMIT busyChanged(isBusy());
    }
}

void LocalVisionAnalyzer::consumeProcessOutput()
{
    m_processOutput.append(m_process->readAllStandardOutput());
    constexpr qsizetype MaxDiagnostics = 64 * 1024;
    if (m_processOutput.size() > MaxDiagnostics) {
        m_processOutput = m_processOutput.right(MaxDiagnostics);
    }
    const QRegularExpression progress(QStringLiteral(R"(PROGRESS\s+(\d+)\s+(\d+))"));
    auto matches = progress.globalMatch(QString::fromUtf8(m_processOutput));
    int completed = -1;
    int total = -1;
    while (matches.hasNext()) {
        const auto match = matches.next();
        completed = match.captured(1).toInt();
        total = match.captured(2).toInt();
    }
    if (completed >= 0 && total > 0) {
        const int percent = qBound(0, int(qRound(double(completed) * 100.0 / double(total))), 100);
        Q_EMIT progressChanged(percent, estimateRemainingSeconds(m_timer.elapsed(), completed, total), i18n("Analyzing the timeline locally for people"));
    }
}

void LocalVisionAnalyzer::finishAnalysis(int exitCode, QProcess::ExitStatus status)
{
    consumeProcessOutput();
    releaseNativeBudget();
    if (m_cancelRequested) {
        resetAnalysis();
        setPhase(Phase::Idle);
        Q_EMIT cancelled();
        return;
    }
    if (status != QProcess::NormalExit || exitCode != 0) {
        QString details = QString::fromUtf8(m_processOutput).trimmed();
        if (details.size() > 700) {
            details = details.right(700);
        }
        resetAnalysis();
        setPhase(Phase::Idle);
        Q_EMIT statusChanged(i18n("Local visual analysis failed: %1", details), true);
        return;
    }
    LocalVisionResult result;
    QString error;
    if (!loadResultFile(m_outputPath, result, &error)) {
        resetAnalysis();
        setPhase(Phase::Idle);
        Q_EMIT statusChanged(error, true);
        return;
    }
    result.timelineFingerprint = m_fingerprint;
    result.sampleStepFrames = m_sampleStepFrames;
    saveResultFile(cachePath(m_fingerprint), result, m_timelineFrames, m_fps, m_fingerprint);
    resetAnalysis();
    setPhase(Phase::Idle);
    Q_EMIT progressChanged(100, 0, i18n("Local person analysis complete"));
    Q_EMIT analysisReady(result);
}

QString LocalVisionAnalyzer::cachePath(const QString &fingerprint) const
{
    const QString root = QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath(QStringLiteral("ai-editor/vision-cache"));
    return QDir(root).filePath(fingerprint + QStringLiteral(".json"));
}

bool LocalVisionAnalyzer::loadResultFile(const QString &path, LocalVisionResult &result, QString *error) const
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = i18n("The local visual analysis result could not be read.");
        }
        return false;
    }
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("version")).toInt() != 1 || !root.value(QStringLiteral("person_frames")).isArray()) {
        if (error) {
            *error = i18n("The local visual analysis result is invalid.");
        }
        return false;
    }
    result = {};
    result.sampleStepFrames = qMax(1, root.value(QStringLiteral("sample_step_frames")).toInt(1));
    result.backend = root.value(QStringLiteral("backend")).toString(i18n("CPU"));
    const QJsonArray frames = root.value(QStringLiteral("person_frames")).toArray();
    result.personFrames.reserve(frames.size());
    for (const QJsonValue &value : frames) {
        if (value.isDouble() && value.toInteger(-1) >= 0 && value.toInteger(-1) <= INT_MAX) {
            result.personFrames.push_back(int(value.toInteger()));
        }
    }
    return true;
}

bool LocalVisionAnalyzer::saveResultFile(const QString &path, const LocalVisionResult &result, int timelineFrames, double fps,
                                         const QString &fingerprint) const
{
    QJsonArray frames;
    for (int frame : result.personFrames) {
        frames.append(frame);
    }
    const QJsonObject root{{QStringLiteral("version"), 1},
                           {QStringLiteral("sample_step_frames"), result.sampleStepFrames},
                           {QStringLiteral("backend"), result.backend},
                           {QStringLiteral("person_frames"), frames},
                           {QStringLiteral("timeline_fingerprint"), fingerprint},
                           {QStringLiteral("timeline_frames"), timelineFrames},
                           {QStringLiteral("fps"), fps}};
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    const QByteArray payload = QJsonDocument(root).toJson(QJsonDocument::Compact);
    return file.open(QIODevice::WriteOnly) && file.write(payload) == payload.size() && file.commit();
}

bool LocalVisionAnalyzer::restoreLegacyResult(const QString &destination, int timelineFrames, double fps, LocalVisionResult &result) const
{
    // Firawynix 26.11.70-firaw.5 included the random temporary directory in the
    // cache key. Only recover that legacy cache when repeated runs produced the
    // exact same detections and their coverage matches both ends of this timeline.
    const QDir cacheDir(QFileInfo(destination).absolutePath());
    const QFileInfoList files = cacheDir.entryInfoList({QStringLiteral("*.json")}, QDir::Files, QDir::Time);
    QByteArray matchingDigest;
    LocalVisionResult matchingResult;
    int matchingCopies = 0;
    const int edgeTolerance = qMax(1, int(qRound(fps * 5.0 * 60.0)));
    const QDateTime now = QDateTime::currentDateTimeUtc();
    for (const QFileInfo &info : files) {
        if (info.absoluteFilePath() == destination || info.lastModified().toUTC().secsTo(now) > 7 * 24 * 60 * 60) {
            continue;
        }
        QFile file(info.absoluteFilePath());
        if (!file.open(QIODevice::ReadOnly)) {
            continue;
        }
        const QByteArray payload = file.readAll();
        const QJsonObject root = QJsonDocument::fromJson(payload).object();
        if (root.contains(QStringLiteral("timeline_fingerprint"))) {
            continue;
        }
        LocalVisionResult candidate;
        if (!loadResultFile(info.absoluteFilePath(), candidate) || candidate.sampleStepFrames != m_sampleStepFrames || candidate.personFrames.isEmpty()) {
            continue;
        }
        const auto [minimum, maximum] = std::minmax_element(candidate.personFrames.cbegin(), candidate.personFrames.cend());
        if (*minimum > edgeTolerance || *maximum >= timelineFrames || timelineFrames - *maximum > edgeTolerance) {
            continue;
        }
        const QByteArray digest = QCryptographicHash::hash(payload, QCryptographicHash::Sha256);
        if (!matchingDigest.isEmpty() && digest != matchingDigest) {
            return false;
        }
        matchingDigest = digest;
        matchingResult = candidate;
        ++matchingCopies;
    }
    if (matchingCopies < 2) {
        return false;
    }
    if (!saveResultFile(destination, matchingResult, timelineFrames, fps, QFileInfo(destination).completeBaseName())) {
        return false;
    }
    result = matchingResult;
    return true;
}

void LocalVisionAnalyzer::resetAnalysis()
{
    releaseNativeBudget();
    m_tempDir.reset();
    m_outputPath.clear();
    m_processOutput.clear();
    m_fingerprint.clear();
    m_sampleStepFrames = 1;
    m_timelineFrames = 0;
    m_fps = 0.0;
    m_cancelRequested = false;
}

void LocalVisionAnalyzer::applyNativeBudget()
{
    releaseNativeBudget();
    m_nativeBudgetHandle = ResourceBudget::applyToProcess(m_process->processId(), m_budget);
}

void LocalVisionAnalyzer::releaseNativeBudget()
{
    ResourceBudget::releaseProcessBudget(m_nativeBudgetHandle);
    m_nativeBudgetHandle = 0;
}

} // namespace AiEditor
} // namespace Kdenlive
