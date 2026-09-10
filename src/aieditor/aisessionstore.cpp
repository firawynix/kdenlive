/*
    SPDX-FileCopyrightText: 2026 Kdenlive AI Editor contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "aisessionstore.hpp"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>

namespace Kdenlive {
namespace AiEditor {

namespace {
constexpr qint64 TranscriptLifetimeSeconds = 7 * 24 * 60 * 60;

QString storageRoot()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/ai-editor/checkpoints");
}

QString safeFileId(const QString &id)
{
    static const QRegularExpression safe(QStringLiteral("^[a-f0-9]{64}$"));
    return safe.match(id).hasMatch() ? id : QString();
}

void pruneExpiredFiles()
{
    const QDateTime cutoff = QDateTime::currentDateTimeUtc().addSecs(-TranscriptLifetimeSeconds);
    for (const QString &folderName : {QStringLiteral("sessions"), QStringLiteral("transcripts")}) {
        QDir folder(storageRoot() + QLatin1Char('/') + folderName);
        const QFileInfoList files = folder.entryInfoList({QStringLiteral("*.json")}, QDir::Files | QDir::NoSymLinks);
        for (const QFileInfo &file : files) {
            if (file.lastModified().toUTC() < cutoff) {
                QFile::remove(file.absoluteFilePath());
            }
        }
    }
}

bool writeAtomically(const QString &path, const QByteArray &data, QString *error)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        if (error) {
            *error = QStringLiteral("Could not create the local AI checkpoint folder.");
        }
        return false;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
        if (error) {
            *error = QStringLiteral("Could not save the local AI checkpoint.");
        }
        return false;
    }
    return true;
}

struct TranscriptLine
{
    QString text;
    int startFrame{0};
    int endFrame{0};
};
} // namespace

QString AiSessionStore::timelineFingerprint(const QByteArray &sceneData, double fps, const QString &whisperModel, const QString &language,
                                            const QString &temporaryPath)
{
    QByteArray stableScene = sceneData;
    if (!temporaryPath.isEmpty()) {
        const QByteArray nativePath = QDir::toNativeSeparators(temporaryPath).toUtf8();
        const QByteArray portablePath = QDir::fromNativeSeparators(temporaryPath).toUtf8();
        stableScene.replace(nativePath, QByteArrayLiteral("<AI_TEMP>"));
        stableScene.replace(portablePath, QByteArrayLiteral("<AI_TEMP>"));
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(stableScene);
    hash.addData(QByteArray::number(fps, 'g', 12));
    hash.addData(whisperModel.toUtf8());
    hash.addData(language.toUtf8());
    return QString::fromLatin1(hash.result().toHex());
}

QString AiSessionStore::sessionId(const QString &timelineFingerprint, AiProvider provider, const QString &model, const QString &prompt, int timelineFrames,
                                  double fps)
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(timelineFingerprint.toUtf8());
    hash.addData(QByteArray::number(int(provider)));
    hash.addData(model.trimmed().toUtf8());
    hash.addData(prompt.trimmed().toUtf8());
    hash.addData(QByteArray::number(timelineFrames));
    hash.addData(QByteArray::number(fps, 'g', 12));
    return QString::fromLatin1(hash.result().toHex());
}

QVector<TranscriptChunk> AiSessionStore::splitTranscript(const QString &transcript, int timelineFrames, qsizetype maximumCharacters)
{
    const QStringList sourceLines = transcript.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    if (sourceLines.isEmpty()) {
        return {};
    }
    maximumCharacters = qMax<qsizetype>(256, maximumCharacters);
    const QRegularExpression range(QStringLiteral("^\\[(\\d+)-(\\d+)\\]"));
    QVector<TranscriptLine> lines;
    lines.reserve(sourceLines.size());
    int previousEnd = 0;
    for (const QString &sourceLine : sourceLines) {
        const auto match = range.match(sourceLine);
        TranscriptLine line{sourceLine, previousEnd, previousEnd + 1};
        if (match.hasMatch()) {
            line.startFrame = match.captured(1).toInt();
            line.endFrame = qMax(line.startFrame + 1, match.captured(2).toInt());
        }
        previousEnd = line.endFrame;
        lines.push_back(line);
    }

    QVector<QPair<int, int>> groups;
    int groupStart = 0;
    qsizetype groupCharacters = 0;
    for (int index = 0; index < lines.size(); ++index) {
        const qsizetype addition = lines.at(index).text.size() + 1;
        if (index > groupStart && groupCharacters + addition > maximumCharacters) {
            groups.push_back({groupStart, index});
            groupStart = index;
            groupCharacters = 0;
        }
        groupCharacters += addition;
    }
    groups.push_back({groupStart, lines.size()});

    QVector<TranscriptChunk> chunks;
    chunks.reserve(groups.size());
    for (int groupIndex = 0; groupIndex < groups.size(); ++groupIndex) {
        const int coreStart = groups.at(groupIndex).first;
        const int coreEnd = groups.at(groupIndex).second;
        const int contextStart = qMax(0, coreStart - 1);
        const int contextEnd = qMin(lines.size(), coreEnd + 1);
        QStringList chunkLines;
        for (int lineIndex = contextStart; lineIndex < contextEnd; ++lineIndex) {
            chunkLines << lines.at(lineIndex).text;
        }
        TranscriptChunk chunk;
        chunk.text = chunkLines.join(QLatin1Char('\n'));
        chunk.ownedStartFrame = groupIndex == 0 ? 0 : lines.at(coreStart).startFrame;
        chunk.ownedEndFrame = groupIndex + 1 < groups.size() ? lines.at(groups.at(groupIndex + 1).first).startFrame : timelineFrames;
        chunks.push_back(chunk);
    }
    return chunks;
}

QByteArray AiSessionStore::serialize(const AiSessionCheckpoint &checkpoint)
{
    QJsonArray fragments;
    for (const QString &fragment : checkpoint.planFragments) {
        fragments.append(fragment);
    }
    const QJsonObject root{{QStringLiteral("version"), checkpoint.version},
                           {QStringLiteral("id"), checkpoint.id},
                           {QStringLiteral("timeline_fingerprint"), checkpoint.timelineFingerprint},
                           {QStringLiteral("provider"), int(checkpoint.provider)},
                           {QStringLiteral("model"), checkpoint.model},
                           {QStringLiteral("prompt"), checkpoint.prompt},
                           {QStringLiteral("timeline_frames"), checkpoint.timelineFrames},
                           {QStringLiteral("fps"), checkpoint.fps},
                           {QStringLiteral("transcript"), checkpoint.transcript},
                           {QStringLiteral("next_chunk"), checkpoint.nextChunk},
                           {QStringLiteral("plan_fragments"), fragments},
                           {QStringLiteral("updated_at_utc"), QDateTime::currentDateTimeUtc().toSecsSinceEpoch()}};
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

std::optional<AiSessionCheckpoint> AiSessionStore::deserialize(const QByteArray &data)
{
    const QJsonDocument document = QJsonDocument::fromJson(data);
    if (!document.isObject()) {
        return std::nullopt;
    }
    const QJsonObject root = document.object();
    AiSessionCheckpoint checkpoint;
    checkpoint.version = root.value(QStringLiteral("version")).toInt();
    checkpoint.id = root.value(QStringLiteral("id")).toString();
    checkpoint.timelineFingerprint = root.value(QStringLiteral("timeline_fingerprint")).toString();
    const int provider = root.value(QStringLiteral("provider")).toInt(-1);
    checkpoint.model = root.value(QStringLiteral("model")).toString();
    checkpoint.prompt = root.value(QStringLiteral("prompt")).toString();
    checkpoint.timelineFrames = root.value(QStringLiteral("timeline_frames")).toInt();
    checkpoint.fps = root.value(QStringLiteral("fps")).toDouble();
    checkpoint.transcript = root.value(QStringLiteral("transcript")).toString();
    checkpoint.nextChunk = root.value(QStringLiteral("next_chunk")).toInt(-1);
    const QJsonArray fragments = root.value(QStringLiteral("plan_fragments")).toArray();
    for (const QJsonValue &fragment : fragments) {
        if (!fragment.isString()) {
            return std::nullopt;
        }
        checkpoint.planFragments << fragment.toString();
    }
    if (checkpoint.version != 1 || safeFileId(checkpoint.id).isEmpty() || safeFileId(checkpoint.timelineFingerprint).isEmpty() || provider < 0 ||
        provider > 2 || checkpoint.model.isEmpty() || checkpoint.prompt.isEmpty() || checkpoint.timelineFrames < 1 || checkpoint.fps <= 0.0 ||
        checkpoint.transcript.isEmpty() || checkpoint.nextChunk < 0 || checkpoint.nextChunk != checkpoint.planFragments.size()) {
        return std::nullopt;
    }
    checkpoint.provider = AiProvider(provider);
    return checkpoint;
}

bool AiSessionStore::save(const AiSessionCheckpoint &checkpoint, QString *error)
{
    pruneExpiredFiles();
    if (safeFileId(checkpoint.id).isEmpty()) {
        if (error) {
            *error = QStringLiteral("The AI checkpoint identifier is invalid.");
        }
        return false;
    }
    return writeAtomically(checkpointPath(checkpoint.id), serialize(checkpoint), error);
}

std::optional<AiSessionCheckpoint> AiSessionStore::load(const QString &id)
{
    pruneExpiredFiles();
    QFile file(checkpointPath(id));
    if (!file.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    const QByteArray data = file.readAll();
    const qint64 updatedAt = QJsonDocument::fromJson(data).object().value(QStringLiteral("updated_at_utc")).toInteger();
    if (updatedAt <= 0 || QDateTime::currentDateTimeUtc().toSecsSinceEpoch() - updatedAt > TranscriptLifetimeSeconds) {
        file.close();
        QFile::remove(file.fileName());
        return std::nullopt;
    }
    const auto checkpoint = deserialize(data);
    if (!checkpoint || checkpoint->id != id) {
        return std::nullopt;
    }
    return checkpoint;
}

void AiSessionStore::remove(const QString &id)
{
    const QString path = checkpointPath(id);
    if (!path.isEmpty()) {
        QFile::remove(path);
    }
}

bool AiSessionStore::saveTranscript(const QString &timelineFingerprint, const QString &transcript, QString *error)
{
    pruneExpiredFiles();
    if (safeFileId(timelineFingerprint).isEmpty() || transcript.isEmpty()) {
        return false;
    }
    const QJsonObject root{{QStringLiteral("version"), 1},
                           {QStringLiteral("timeline_fingerprint"), timelineFingerprint},
                           {QStringLiteral("transcript"), transcript},
                           {QStringLiteral("updated_at_utc"), QDateTime::currentDateTimeUtc().toSecsSinceEpoch()}};
    return writeAtomically(transcriptPath(timelineFingerprint), QJsonDocument(root).toJson(QJsonDocument::Compact), error);
}

QString AiSessionStore::loadTranscript(const QString &timelineFingerprint)
{
    pruneExpiredFiles();
    QFile file(transcriptPath(timelineFingerprint));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    const qint64 updatedAt = root.value(QStringLiteral("updated_at_utc")).toInteger();
    if (root.value(QStringLiteral("version")).toInt() != 1 || root.value(QStringLiteral("timeline_fingerprint")).toString() != timelineFingerprint ||
        updatedAt <= 0 || QDateTime::currentDateTimeUtc().toSecsSinceEpoch() - updatedAt > TranscriptLifetimeSeconds) {
        file.close();
        QFile::remove(file.fileName());
        return {};
    }
    return root.value(QStringLiteral("transcript")).toString();
}

QString AiSessionStore::checkpointPath(const QString &id)
{
    const QString safeId = safeFileId(id);
    return safeId.isEmpty() ? QString() : storageRoot() + QStringLiteral("/sessions/") + safeId + QStringLiteral(".json");
}

QString AiSessionStore::transcriptPath(const QString &timelineFingerprint)
{
    const QString safeId = safeFileId(timelineFingerprint);
    return safeId.isEmpty() ? QString() : storageRoot() + QStringLiteral("/transcripts/") + safeId + QStringLiteral(".json");
}

} // namespace AiEditor
} // namespace Kdenlive
