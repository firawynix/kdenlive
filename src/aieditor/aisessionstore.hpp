/*
    SPDX-FileCopyrightText: 2026 Kdenlive AI Editor contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "aiproviderclient.hpp"

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QVector>
#include <optional>

namespace Kdenlive {
namespace AiEditor {

struct TranscriptChunk
{
    QString text;
    int ownedStartFrame{0};
    int ownedEndFrame{0};
};

struct AiSessionCheckpoint
{
    int version{1};
    QString id;
    QString timelineFingerprint;
    AiProvider provider{AiProvider::OpenRouter};
    QString model;
    QString prompt;
    int timelineFrames{0};
    double fps{0.0};
    QString transcript;
    int chunkCharacters{4000};
    int chunkReductions{0};
    int nextChunk{0};
    QStringList planFragments;
};

class AiSessionStore
{
public:
    static constexpr qsizetype DefaultChunkCharacters = 4000;
    static constexpr qsizetype LegacyChunkCharacters = 16000;
    static constexpr qsizetype MinimumChunkCharacters = 1000;
    static constexpr int MaximumChunkReductions = 4;

    static QString timelineFingerprint(const QByteArray &sceneData, double fps, const QString &whisperModel, const QString &language,
                                       const QString &temporaryPath = QString());
    static QString sessionId(const QString &timelineFingerprint, AiProvider provider, const QString &model, const QString &prompt, int timelineFrames,
                             double fps);
    static QVector<TranscriptChunk> splitTranscript(const QString &transcript, int timelineFrames, qsizetype maximumCharacters = DefaultChunkCharacters);

    static QByteArray serialize(const AiSessionCheckpoint &checkpoint);
    static std::optional<AiSessionCheckpoint> deserialize(const QByteArray &data);
    static bool save(const AiSessionCheckpoint &checkpoint, QString *error = nullptr);
    static std::optional<AiSessionCheckpoint> load(const QString &id);
    static std::optional<AiSessionCheckpoint> loadMostAdvancedCompatible(const QString &timelineFingerprint, const QString &prompt, int timelineFrames,
                                                                         double fps, const QString &transcript);
    static std::optional<AiSessionCheckpoint> loadUniqueCompatibleRequest(const QString &prompt, int timelineFrames, double fps);
    static void remove(const QString &id);

    static bool saveTranscript(const QString &timelineFingerprint, const QString &transcript, QString *error = nullptr);
    static QString loadTranscript(const QString &timelineFingerprint);

private:
    static QString checkpointPath(const QString &id);
    static QString transcriptPath(const QString &timelineFingerprint);
};

} // namespace AiEditor
} // namespace Kdenlive
