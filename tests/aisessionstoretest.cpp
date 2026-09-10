/*
    SPDX-FileCopyrightText: 2026 Kdenlive AI Editor contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "test_utils.hpp"

#include "aieditor/aisessionstore.hpp"

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
    checkpoint.nextChunk = 1;
    checkpoint.planFragments = {QStringLiteral(R"({"version":1,"operations":[]})")};

    const QByteArray serialized = AiSessionStore::serialize(checkpoint);
    REQUIRE_FALSE(serialized.contains("api_key"));
    REQUIRE_FALSE(serialized.contains("secret"));
    const auto restored = AiSessionStore::deserialize(serialized);
    REQUIRE(restored.has_value());
    REQUIRE(restored->id == checkpoint.id);
    REQUIRE(restored->nextChunk == 1);
    REQUIRE(restored->planFragments == checkpoint.planFragments);
}

TEST_CASE("AI timeline fingerprint ignores the temporary folder", "[AIEditor][Checkpoint]")
{
    const QByteArray firstScene = QByteArrayLiteral("<producer resource=\"C:/temp/one/timeline.wav\"/>");
    const QByteArray secondScene = QByteArrayLiteral("<producer resource=\"C:/temp/two/timeline.wav\"/>");
    REQUIRE(AiSessionStore::timelineFingerprint(firstScene, 25.0, QStringLiteral("base"), QStringLiteral("Portuguese"), QStringLiteral("C:/temp/one")) ==
            AiSessionStore::timelineFingerprint(secondScene, 25.0, QStringLiteral("base"), QStringLiteral("Portuguese"), QStringLiteral("C:/temp/two")));
}
