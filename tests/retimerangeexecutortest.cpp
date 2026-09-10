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
