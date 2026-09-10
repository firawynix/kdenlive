/*
    SPDX-FileCopyrightText: 2026 Kdenlive AI Editor contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "test_utils.hpp"

#include "aieditor/retimerangeexecutor.hpp"
#include "doc/kdenlivedoc.h"

using Kdenlive::AiEditor::RetimeRangeExecutor;
using Kdenlive::AiEditor::RetimeRangeOperation;

TEST_CASE("AI retime range preflight", "[AIEditor][Retime]")
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

    const int videoTrack = timeline->getTrackIndexFromPosition(1);
    const QString red = KdenliveTests::createProducer(pCore->getProjectProfile(), "red", binModel, 200);
    int clipId = -1;
    REQUIRE(timeline->requestClipInsertion(red, videoTrack, 0, clipId));

    RetimeRangeOperation operation{20, 140, 40, true};

    SECTION("accepts one continuous clip")
    {
        const auto result = RetimeRangeExecutor::preflight(timeline, operation);
        INFO(result.error.toStdString());
        REQUIRE(result.isValid());
        REQUIRE(result.clipIds == QVector<int>{clipId});
        REQUIRE(result.trackIds == QVector<int>{videoTrack});
    }

    SECTION("rejects an empty range")
    {
        operation.startFrame = 220;
        operation.endFrame = 240;
        const auto result = RetimeRangeExecutor::preflight(timeline, operation);
        REQUIRE_FALSE(result.isValid());
        REQUIRE(result.error.contains(QStringLiteral("No clip")));
    }

    SECTION("rejects a clip boundary inside the range")
    {
        int secondClip = -1;
        const QString blue = KdenliveTests::createProducer(pCore->getProjectProfile(), "blue", binModel, 200);
        REQUIRE(timeline->requestItemResize(clipId, 80, true) == 80);
        REQUIRE(timeline->requestClipInsertion(blue, videoTrack, 80, secondClip));
        const auto result = RetimeRangeExecutor::preflight(timeline, operation);
        REQUIRE_FALSE(result.isValid());
        REQUIRE(result.error.contains(QStringLiteral("continuous clip")));
    }

    SECTION("rejects locked content that ripple would move")
    {
        timeline->setTrackLockedState(videoTrack, true);
        const auto result = RetimeRangeExecutor::preflight(timeline, operation);
        REQUIRE_FALSE(result.isValid());
        REQUIRE(result.error.contains(QStringLiteral("locked track")));
        timeline->setTrackLockedState(videoTrack, false);
    }

    REQUIRE(timeline->checkConsistency());
    pCore->projectManager()->closeCurrentDocument(false, false);
}

TEST_CASE("AI retime range execution", "[AIEditor][Retime]")
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
    const QString avProducer = KdenliveTests::createProducerWithSound(pCore->getProjectProfile(), binModel, 4000);
    int videoClip = -1;
    REQUIRE(timeline->requestClipInsertion(avProducer, videoTrack, 0, videoClip, true, true, false));
    const int audioClip = KdenliveTests::groupsModel(timeline)->getSplitPartner(videoClip);
    REQUIRE(audioClip >= 0);

    // At 25 fps this is 00:10 to 02:10 (120 seconds), targeting 40 seconds.
    const RetimeRangeOperation operation{250, 3250, 1000, true};
    const int undoIndex = undoStack->index();
    const auto result = RetimeRangeExecutor::apply(timeline, operation);
    INFO(result.error.toStdString());
    REQUIRE(result.isValid());
    REQUIRE(result.retimedClipIds.size() == 2);

    const int retimedVideo = timeline->getClipByPosition(videoTrack, operation.startFrame);
    const int audioTrack = timeline->getClipTrackId(audioClip);
    const int retimedAudio = timeline->getClipByPosition(audioTrack, operation.startFrame);
    REQUIRE(retimedVideo >= 0);
    REQUIRE(retimedAudio >= 0);
    REQUIRE(timeline->getItemPlaytime(retimedVideo) == 1000);
    REQUIRE(timeline->getItemPlaytime(retimedAudio) == 1000);
    REQUIRE(timeline->getClipSpeed(retimedVideo) == Approx(3.0));
    REQUIRE(timeline->getClipSpeed(retimedAudio) == Approx(3.0));
    REQUIRE(timeline->getGroupElements(retimedVideo) == std::unordered_set<int>{retimedVideo, retimedAudio});
    const int rightVideo = timeline->getClipByPosition(videoTrack, 1250);
    const int rightAudio = timeline->getClipByPosition(audioTrack, 1250);
    REQUIRE(rightVideo >= 0);
    REQUIRE(rightAudio >= 0);
    REQUIRE(timeline->getItemPosition(rightVideo) == 1250);
    REQUIRE(timeline->getItemPosition(rightAudio) == 1250);
    REQUIRE(undoStack->index() == undoIndex + 1);
    REQUIRE(timeline->checkConsistency());

    undoStack->undo();
    REQUIRE(timeline->getClipByPosition(videoTrack, operation.startFrame) == videoClip);
    REQUIRE(timeline->getItemPlaytime(videoClip) == 4000);
    REQUIRE(timeline->getItemPlaytime(audioClip) == 4000);
    REQUIRE(timeline->getClipSpeed(videoClip) == Approx(1.0));
    REQUIRE(timeline->getClipSpeed(audioClip) == Approx(1.0));
    REQUIRE(timeline->getGroupElements(videoClip) == std::unordered_set<int>{videoClip, audioClip});
    REQUIRE(timeline->checkConsistency());

    undoStack->redo();
    REQUIRE(timeline->getItemPlaytime(timeline->getClipByPosition(videoTrack, operation.startFrame)) == 1000);
    REQUIRE(timeline->getItemPlaytime(timeline->getClipByPosition(audioTrack, operation.startFrame)) == 1000);
    REQUIRE(timeline->checkConsistency());

    pCore->projectManager()->closeCurrentDocument(false, false);
}
