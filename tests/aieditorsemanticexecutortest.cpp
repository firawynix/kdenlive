/*
    SPDX-FileCopyrightText: 2026 Kdenlive AI Editor contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "test_utils.hpp"

#include "aieditor/editplanexecutor.hpp"
#include "aieditor/localtimelinetranscriber.hpp"
#include "doc/kdenlivedoc.h"

using namespace Kdenlive::AiEditor;

TEST_CASE("AI local SRT transcript uses timeline frames", "[AIEditor][Transcript]")
{
    const QByteArray srt = "1\r\n00:00:01,000 --> 00:00:02,500\r\nOlá mundo\r\n\r\n"
                           "2\r\n00:00:04,000 --> 00:00:05,000\r\nSalesforce e IA\r\n";
    REQUIRE(LocalTimelineTranscriber::parseSrt(srt, 25.0) == QStringLiteral("[25-63] Olá mundo\n[100-125] Salesforce e IA"));
}

TEST_CASE("AI local transcription selects an installed Whisper model", "[AIEditor][Transcript]")
{
    REQUIRE(LocalTimelineTranscriber::selectAvailableModel(QStringLiteral("turbo"), {QStringLiteral("base")}) == QStringLiteral("base"));
    REQUIRE(LocalTimelineTranscriber::selectAvailableModel(QStringLiteral("small"), {QStringLiteral("base"), QStringLiteral("small")}) ==
            QStringLiteral("small"));
    REQUIRE(LocalTimelineTranscriber::selectAvailableModel(QStringLiteral("turbo"), {QStringLiteral("tiny")}) == QStringLiteral("tiny"));
    REQUIRE(LocalTimelineTranscriber::selectAvailableModel(QStringLiteral("turbo"), {}).isEmpty());
}

TEST_CASE("AI local transcription parses tool progress and estimates remaining time", "[AIEditor][Transcript][Progress]")
{
    REQUIRE(LocalTimelineTranscriber::parseProgressPercent("frame=10 percentage: 7\npercentage: 42", false) == 42);
    REQUIRE(LocalTimelineTranscriber::parseProgressPercent(" 18%|##       |\r 63%|######   |", true) == 63);
    REQUIRE(LocalTimelineTranscriber::parseProgressPercent("no progress here", true) == -1);
    REQUIRE(LocalTimelineTranscriber::estimateRemainingSeconds(10000, 25) == 30);
    REQUIRE(LocalTimelineTranscriber::estimateRemainingSeconds(100, 25) == -1);
    REQUIRE(LocalTimelineTranscriber::estimateRemainingSeconds(10000, 0) == -1);
}

TEST_CASE("AI mixed semantic plan applies as one undo action", "[AIEditor][Semantic]")
{
    auto binModel = pCore->projectItemModel();
    binModel->clean();
    auto undoStack = std::make_shared<DocUndoStack>(nullptr);
    KdenliveDoc document(undoStack, {1, 1});
    pCore->projectManager()->testSetDocument(&document);
    KdenliveTests::updateTimeline(false, QString(), QString(), QDateTime::currentDateTime(), false);
    auto timeline = document.getTimeline(document.uuid());
    pCore->projectManager()->testSetActiveTimeline(timeline);
    KdenliveTests::resetNextId();

    QMap<int, QString> audioInfo;
    audioInfo.insert(1, QStringLiteral("stream1"));
    KdenliveTests::setAudioTargets(timeline, audioInfo);
    const int videoTrack = timeline->getTrackIndexFromPosition(1);
    const QString producer = KdenliveTests::createProducerWithSound(pCore->getProjectProfile(), binModel, 500);
    int videoClip = -1;
    REQUIRE(timeline->requestClipInsertion(producer, videoTrack, 0, videoClip, true, true, false));
    const int audioClip = KdenliveTests::groupsModel(timeline)->getSplitPartner(videoClip);
    REQUIRE(audioClip >= 0);
    const int audioTrack = timeline->getClipTrackId(audioClip);

    EditPlan plan;
    plan.version = 1;
    EditOperation mute;
    mute.type = EditOperationType::MuteRange;
    mute.muteRange = {50, 100};
    plan.operations << mute;
    EditOperation retime;
    retime.type = EditOperationType::RetimeRange;
    retime.retimeRange = {200, 300, 25, true};
    plan.operations << retime;

    const int undoIndex = undoStack->index();
    const auto result = EditPlanExecutor::apply(timeline, plan);
    INFO(result.error.toStdString());
    REQUIRE(result.isValid());
    REQUIRE(undoStack->index() == undoIndex + 1);
    const int mutedAudio = timeline->getClipByPosition(audioTrack, 50);
    REQUIRE(mutedAudio >= 0);
    REQUIRE(timeline->getItemPlaytime(mutedAudio) == 50);
    REQUIRE(timeline->getClipState(mutedAudio).first == PlaylistState::Disabled);
    REQUIRE(timeline->getClipState(timeline->getClipByPosition(videoTrack, 50)).first == PlaylistState::VideoOnly);
    REQUIRE(timeline->getItemPlaytime(timeline->getClipByPosition(videoTrack, 200)) == 25);
    REQUIRE(timeline->checkConsistency());

    undoStack->undo();
    REQUIRE(timeline->getClipByPosition(videoTrack, 50) == videoClip);
    REQUIRE(timeline->getClipByPosition(audioTrack, 50) == audioClip);
    REQUIRE(timeline->getItemPlaytime(videoClip) == 500);
    REQUIRE(timeline->getItemPlaytime(audioClip) == 500);
    REQUIRE(timeline->getClipState(audioClip).first == PlaylistState::AudioOnly);
    REQUIRE(timeline->checkConsistency());

    pCore->projectManager()->closeCurrentDocument(false, false);
}
