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

TEST_CASE("Visual setup recommendation adapts to the host", "[AIEditor][Vision][Configuration]")
{
    constexpr quint64 GiB = 1024ULL * 1024ULL * 1024ULL;
    const LocalAiHardware small{4, 6 * GiB, QStringLiteral("Integrated graphics"), 0};
    const LocalAiHardware workstation{32, 128 * GiB, QStringLiteral("AMD Radeon RX 9060 XT"), 16 * GiB};
    REQUIRE(LocalVisionAnalyzer::recommendedModelFile(small).contains(QStringLiteral("int8")));
    REQUIRE_FALSE(LocalVisionAnalyzer::recommendedModelFile(workstation).contains(QStringLiteral("int8")));
    REQUIRE(LocalVisionAnalyzer::recommendedSamplesPerMinute(small, {80, 3, 4 * GiB, QStringLiteral("cpu")}) == 30);
    REQUIRE(LocalVisionAnalyzer::recommendedSamplesPerMinute(workstation, {80, 26, 100 * GiB, QStringLiteral("cpu")}) == 120);
}
