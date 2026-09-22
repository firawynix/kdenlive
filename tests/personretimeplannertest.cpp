/*
    SPDX-FileCopyrightText: 2026 Kdenlive AI Editor contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "catch.hpp"
#include "src/aieditor/localvisionanalyzer.hpp"
#include "src/aieditor/personretimeplanner.hpp"

#include <numeric>

using namespace Kdenlive::AiEditor;

TEST_CASE("Person-aware retime reads requested final durations", "[AIEditor][Vision]")
{
    REQUIRE(PersonRetimePlanner::targetDurationFrames(
                QStringLiteral("Preciso que crie um fast motion e reduza o vídeo para 5 minutos, mantendo pessoas no tempo normal."), 60.0) == 18000);
    REQUIRE(PersonRetimePlanner::targetDurationFrames(QStringLiteral("Reduza para 00:05:00 e preserve as pessoas."), 25.0) == 7500);
    REQUIRE(PersonRetimePlanner::targetDurationFrames(QStringLiteral("Make it last 90 seconds."), 30.0) == 2700);
    REQUIRE(PersonRetimePlanner::targetDurationFrames(QStringLiteral("Detecte pessoas sem informar duração."), 30.0) == -1);
    REQUIRE(PersonRetimePlanner::isPersonAwareFastMotionInstruction(
        QStringLiteral("Reduza o vídeo para 5 minutos usando fast motion, mas mantenha a velocidade normal quando detectar pessoas.")));
    REQUIRE_FALSE(PersonRetimePlanner::isPersonAwareFastMotionInstruction(
        QStringLiteral("Do tempo 00:46:10 até 00:51:30, acelere para que dure 10 segundos.")));
    REQUIRE_FALSE(PersonRetimePlanner::isPersonAwareFastMotionInstruction(QStringLiteral("Silencie conversas fora do tema e detecte pessoas.")));
}

TEST_CASE("Sparse person detections become padded stable intervals", "[AIEditor][Vision]")
{
    const QVector<VisualFrameRange> ranges = PersonRetimePlanner::normalizeDetections({100, 125, 400}, 25, 25, 50, 500);
    REQUIRE(ranges.size() == 2);
    REQUIRE(ranges.at(0).startFrame == 63);
    REQUIRE(ranges.at(0).endFrame == 163);
    REQUIRE(ranges.at(1).startFrame == 363);
    REQUIRE(ranges.at(1).endFrame == 438);
}

TEST_CASE("Exact reduction is distributed across clip-safe segments", "[AIEditor][Vision]")
{
    QString error;
    const QVector<int> source{100, 200, 300};
    const QVector<int> targets = PersonRetimePlanner::allocateTargetDurations(source, 450, &error);
    INFO(error.toStdString());
    REQUIRE(error.isEmpty());
    REQUIRE(targets.size() == source.size());
    REQUIRE(std::accumulate(source.begin(), source.end(), 0) - std::accumulate(targets.begin(), targets.end(), 0) == 450);
    for (qsizetype index = 0; index < targets.size(); ++index) {
        REQUIRE(targets.at(index) >= 1);
        REQUIRE(targets.at(index) < source.at(index));
    }

    REQUIRE(PersonRetimePlanner::allocateTargetDurations({2, 2}, 3, &error).isEmpty());
    REQUIRE_FALSE(error.isEmpty());
}

TEST_CASE("Person-aware retime reports its shortest safe result", "[AIEditor][Vision]")
{
    REQUIRE(PersonRetimePlanner::minimumTargetDurationFrames(1000, {100, 200}) == 702);
    REQUIRE(PersonRetimePlanner::minimumTargetDurationFrames(1000, {}) == 1000);
}

TEST_CASE("Combined local analysis protects semantic edits from visual retiming", "[AIEditor][Vision][Combined]")
{
    EditPlan semantic;
    semantic.version = 1;
    EditOperation mute;
    mute.type = EditOperationType::MuteRange;
    mute.muteRange = {120, 180};
    semantic.operations.push_back(mute);
    EditOperation retime;
    retime.type = EditOperationType::RetimeRange;
    retime.retimeRange = {300, 500, 50, true};
    semantic.operations.push_back(retime);

    const QVector<VisualFrameRange> protectedRanges = PersonRetimePlanner::combinedProtectedRanges({{100, 140}, {700, 750}}, semantic, 1000);
    REQUIRE(protectedRanges.size() == 3);
    REQUIRE(protectedRanges.at(0).startFrame == 100);
    REQUIRE(protectedRanges.at(0).endFrame == 180);
    REQUIRE(protectedRanges.at(1).startFrame == 300);
    REQUIRE(protectedRanges.at(1).endFrame == 500);
    REQUIRE(PersonRetimePlanner::retimeReductionFrames(semantic) == 150);

    EditPlan conflicting = semantic;
    EditOperation overlapsPerson;
    overlapsPerson.type = EditOperationType::RetimeRange;
    overlapsPerson.retimeRange = {130, 220, 20, true};
    conflicting.operations.push_back(overlapsPerson);
    int skipped = 0;
    const EditPlan filtered = PersonRetimePlanner::withoutPersonRetimeConflicts(conflicting, {{100, 140}}, &skipped);
    REQUIRE(skipped == 1);
    REQUIRE(filtered.operations.size() == 2);
    REQUIRE(PersonRetimePlanner::retimeReductionFrames(filtered) == 150);
}

TEST_CASE("Visual setup recommendation adapts to the host", "[AIEditor][Vision][Configuration]")
{
    constexpr quint64 GiB = 1024ULL * 1024ULL * 1024ULL;
    const LocalAiHardware small{4, 6 * GiB, QStringLiteral("Integrated graphics"), 0};
    const LocalAiHardware workstation{32, 128 * GiB, QStringLiteral("AMD Radeon RX 9060 XT"), 16 * GiB};
    REQUIRE(LocalVisionAnalyzer::recommendedModelFile(small).contains(QStringLiteral("int8")));
    REQUIRE_FALSE(LocalVisionAnalyzer::recommendedModelFile(workstation).contains(QStringLiteral("int8")));
    REQUIRE(LocalVisionAnalyzer::recommendedSamplesPerMinute(small, {80, 0, 80, 3, 4 * GiB, QStringLiteral("cpu")}) == 30);
    REQUIRE(LocalVisionAnalyzer::recommendedSamplesPerMinute(workstation, {80, 80, 80, 26, 100 * GiB, QStringLiteral("gpu")}) == 120);
}
