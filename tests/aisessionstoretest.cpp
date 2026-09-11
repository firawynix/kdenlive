/*
    SPDX-FileCopyrightText: 2026 Kdenlive AI Editor contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "test_utils.hpp"

#include "aieditor/aisessionstore.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

using namespace Kdenlive::AiEditor;

TEST_CASE("AI transcript chunks stay bounded and retain boundary context", "[AIEditor][Checkpoint]")
{
    QStringList lines;
    for (int index = 0; index < 12; ++index) {
        lines << QStringLiteral("[%1-%2] dialogue line %3 with enough text to require chunking").arg(index * 100).arg(index * 100 + 50).arg(index);
    }
    const auto chunks = AiSessionStore::splitTranscript(lines.join(QLatin1Char('\n')), 1200, 256);
    REQUIRE(chunks.size() > 1);
    REQUIRE(chunks.constFirst().ownedStartFrame == 0);
    REQUIRE(chunks.constLast().ownedEndFrame == 1200);
    for (int index = 1; index < chunks.size(); ++index) {
        REQUIRE(chunks.at(index - 1).ownedEndFrame == chunks.at(index).ownedStartFrame);
        REQUIRE(chunks.at(index).text.contains(QStringLiteral("dialogue line")));
    }
}

TEST_CASE("AI checkpoint round trip excludes credentials", "[AIEditor][Checkpoint]")
{
    AiSessionCheckpoint checkpoint;
    checkpoint.id = QString(64, QLatin1Char('a'));
    checkpoint.timelineFingerprint = QString(64, QLatin1Char('b'));
    checkpoint.provider = AiProvider::OpenRouter;
    checkpoint.model = QStringLiteral("anthropic/claude-sonnet-4.6");
    checkpoint.prompt = QStringLiteral("Mute off-topic dialogue");
    checkpoint.timelineFrames = 90000;
    checkpoint.fps = 25.0;
    checkpoint.transcript = QStringLiteral("[0-25] work");
    checkpoint.chunkCharacters = 4000;
    checkpoint.chunkReductions = 1;
    checkpoint.nextChunk = 1;
    checkpoint.planFragments = {QStringLiteral(R"({"version":1,"operations":[]})")};

    const QByteArray serialized = AiSessionStore::serialize(checkpoint);
    REQUIRE_FALSE(serialized.contains("api_key"));
    REQUIRE_FALSE(serialized.contains("secret"));
    const auto restored = AiSessionStore::deserialize(serialized);
    REQUIRE(restored.has_value());
    REQUIRE(restored->id == checkpoint.id);
    REQUIRE(restored->nextChunk == 1);
    REQUIRE(restored->chunkCharacters == 4000);
    REQUIRE(restored->chunkReductions == 1);
    REQUIRE(restored->planFragments == checkpoint.planFragments);
}

TEST_CASE("Legacy AI checkpoints use safe compatible transcript chunk sizes", "[AIEditor][Checkpoint]")
{
    AiSessionCheckpoint checkpoint;
    checkpoint.id = QString(64, QLatin1Char('a'));
    checkpoint.timelineFingerprint = QString(64, QLatin1Char('b'));
    checkpoint.model = QStringLiteral("model");
    checkpoint.prompt = QStringLiteral("prompt");
    checkpoint.timelineFrames = 100;
    checkpoint.fps = 25.0;
    checkpoint.transcript = QStringLiteral("[0-25] work");
    QJsonObject root = QJsonDocument::fromJson(AiSessionStore::serialize(checkpoint)).object();
    root.remove(QStringLiteral("chunk_characters"));
    root.remove(QStringLiteral("chunk_reductions"));

    const auto untouched = AiSessionStore::deserialize(QJsonDocument(root).toJson(QJsonDocument::Compact));
    REQUIRE(untouched.has_value());
    REQUIRE(untouched->chunkCharacters == AiSessionStore::DefaultChunkCharacters);
    REQUIRE(untouched->chunkReductions == 0);

    root.insert(QStringLiteral("next_chunk"), 1);
    root.insert(QStringLiteral("plan_fragments"), QJsonArray{QStringLiteral(R"({"version":1,"operations":[]})")});
    const auto inProgress = AiSessionStore::deserialize(QJsonDocument(root).toJson(QJsonDocument::Compact));
    REQUIRE(inProgress.has_value());
    REQUIRE(inProgress->chunkCharacters == AiSessionStore::LegacyChunkCharacters);
}

TEST_CASE("AI timeline fingerprint ignores the temporary folder", "[AIEditor][Checkpoint]")
{
    const QByteArray firstScene = QByteArrayLiteral("<producer resource=\"C:/temp/one/timeline.wav\"/>");
    const QByteArray secondScene = QByteArrayLiteral("<producer resource=\"C:/temp/two/timeline.wav\"/>");
    REQUIRE(AiSessionStore::timelineFingerprint(firstScene, 25.0, QStringLiteral("base"), QStringLiteral("Portuguese"), QStringLiteral("C:/temp/one")) ==
            AiSessionStore::timelineFingerprint(secondScene, 25.0, QStringLiteral("base"), QStringLiteral("Portuguese"), QStringLiteral("C:/temp/two")));
}

TEST_CASE("AI analysis can continue with another provider without losing completed segments", "[AIEditor][Checkpoint]")
{
    AiSessionCheckpoint source;
    source.id = QString(64, QLatin1Char('c'));
    source.timelineFingerprint = QString(64, QLatin1Char('d'));
    source.provider = AiProvider::OpenRouter;
    source.model = QStringLiteral("remote-model");
    source.prompt = QStringLiteral("Clean this meeting");
    source.timelineFrames = 400;
    source.fps = 25.0;
    source.transcript = QStringLiteral("[0-100] first\n[100-200] second\n[200-300] third\n[300-400] fourth");
    source.chunkCharacters = 1000;
    source.nextChunk = 1;
    source.planFragments = {QStringLiteral(R"({"version":1,"operations":[]})")};
    REQUIRE(AiSessionStore::save(source));

    const auto restored = AiSessionStore::loadMostAdvancedCompatible(source.timelineFingerprint, source.prompt, source.timelineFrames, source.fps,
                                                                      source.transcript);
    REQUIRE(restored.has_value());
    REQUIRE(restored->provider == AiProvider::OpenRouter);
    REQUIRE(restored->nextChunk == 1);
    REQUIRE(restored->planFragments == source.planFragments);
    const auto requestResume = AiSessionStore::loadUniqueCompatibleRequest(source.prompt, source.timelineFrames, source.fps);
    REQUIRE(requestResume.has_value());
    REQUIRE(requestResume->timelineFingerprint == source.timelineFingerprint);
    REQUIRE(requestResume->transcript == source.transcript);
    REQUIRE_FALSE(AiSessionStore::loadMostAdvancedCompatible(source.timelineFingerprint, QStringLiteral("Different request"), source.timelineFrames,
                                                              source.fps, source.transcript)
                      .has_value());
    REQUIRE_FALSE(AiSessionStore::loadUniqueCompatibleRequest(QStringLiteral("Different request"), source.timelineFrames, source.fps).has_value());
    AiSessionStore::remove(source.id);
}
