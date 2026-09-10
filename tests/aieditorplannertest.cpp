/*
    SPDX-FileCopyrightText: 2026 Kdenlive AI Editor contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "catch.hpp"
#include "src/aieditor/editplan.hpp"

#include <QByteArray>

using Kdenlive::AiEditor::EditOperationType;
using Kdenlive::AiEditor::parseEditPlan;

TEST_CASE("AI edit plans are parsed into safe typed operations", "[AIEditor][EditPlan]")
{
    SECTION("A 120 second range becomes 40 seconds at 25 fps")
    {
        const QByteArray json = R"({
            "version": 1,
            "operations": [{
                "type": "retime_range",
                "start_frame": 0,
                "end_frame": 3000,
                "target_duration_frames": 1000
            }]
        })";

        const auto result = parseEditPlan(json);
        REQUIRE(result.isValid());
        REQUIRE(result.plan.version == 1);
        REQUIRE(result.plan.operations.size() == 1);
        const auto &operation = result.plan.operations.constFirst().retimeRange;
        REQUIRE(operation.startFrame == 0);
        REQUIRE(operation.endFrame == 3000);
        REQUIRE(operation.targetDurationFrames == 1000);
        REQUIRE(operation.preservePitch);
        REQUIRE(operation.speedMultiplier() == Approx(3.0));
    }

    SECTION("Pitch preservation can be disabled explicitly")
    {
        const auto result = parseEditPlan(
            R"({"version":1,"operations":[{"type":"retime_range","start_frame":10,"end_frame":30,"target_duration_frames":10,"preserve_pitch":false}]})");

        REQUIRE(result.isValid());
        REQUIRE_FALSE(result.plan.operations.constFirst().retimeRange.preservePitch);
    }

    SECTION("Mute and retime operations can be combined")
    {
        const auto result = parseEditPlan(
            R"({"version":1,"operations":[{"type":"mute_range","start_frame":10,"end_frame":20},{"type":"retime_range","start_frame":30,"end_frame":80,"target_duration_frames":10,"preserve_pitch":true}]})");

        INFO(result.error.toStdString());
        REQUIRE(result.isValid());
        REQUIRE(result.plan.operations.size() == 2);
        REQUIRE(result.plan.operations.at(0).type == EditOperationType::MuteRange);
        REQUIRE(result.plan.operations.at(0).muteRange.startFrame == 10);
        REQUIRE(result.plan.operations.at(0).muteRange.endFrame == 20);
        REQUIRE(result.plan.operations.at(1).type == EditOperationType::RetimeRange);
    }
}

TEST_CASE("Unsafe AI edit plans are rejected before execution", "[AIEditor][EditPlan]")
{
    SECTION("Empty and oversized payloads")
    {
        REQUIRE_FALSE(parseEditPlan(QByteArray()).isValid());
        REQUIRE_FALSE(parseEditPlan(QByteArray(256 * 1024 + 1, ' ')).isValid());
    }

    SECTION("Malformed JSON and non-object roots")
    {
        REQUIRE_FALSE(parseEditPlan("{").isValid());
        REQUIRE_FALSE(parseEditPlan("[]").isValid());
    }

    SECTION("Unsupported versions and operation types")
    {
        REQUIRE_FALSE(parseEditPlan(R"({"version":2,"operations":[{}]})").isValid());
        REQUIRE_FALSE(parseEditPlan(R"({"version":1,"operations":[{"type":"delete_everything"}]})").isValid());
    }

    SECTION("Operation count is bounded")
    {
        REQUIRE_FALSE(parseEditPlan(R"({"version":1,"operations":[]})").isValid());

        QByteArray operations;
        for (int index = 0; index < 257; ++index) {
            if (!operations.isEmpty()) {
                operations.append(',');
            }
            operations.append(R"({"type":"retime_range","start_frame":0,"end_frame":2,"target_duration_frames":1})");
        }
        REQUIRE_FALSE(parseEditPlan(QByteArray("{\"version\":1,\"operations\":[") + operations + "]}").isValid());
    }

    SECTION("Frame values must be bounded integers with a valid interval")
    {
        REQUIRE_FALSE(
            parseEditPlan(R"({"version":1,"operations":[{"type":"retime_range","start_frame":-1,"end_frame":20,"target_duration_frames":10}]})").isValid());
        REQUIRE_FALSE(
            parseEditPlan(R"({"version":1,"operations":[{"type":"retime_range","start_frame":1.5,"end_frame":20,"target_duration_frames":10}]})").isValid());
        REQUIRE_FALSE(
            parseEditPlan(R"({"version":1,"operations":[{"type":"retime_range","start_frame":20,"end_frame":20,"target_duration_frames":10}]})").isValid());
        REQUIRE_FALSE(
            parseEditPlan(R"({"version":1,"operations":[{"type":"retime_range","start_frame":0,"end_frame":20,"target_duration_frames":0}]})").isValid());
        REQUIRE_FALSE(parseEditPlan(R"({"version":1,"operations":[{"type":"retime_range","start_frame":0,"end_frame":2147483648,"target_duration_frames":1}]})")
                          .isValid());
    }

    SECTION("Pitch preservation must be boolean")
    {
        REQUIRE_FALSE(
            parseEditPlan(
                R"({"version":1,"operations":[{"type":"retime_range","start_frame":0,"end_frame":20,"target_duration_frames":10,"preserve_pitch":"yes"}]})")
                .isValid());
    }

    SECTION("Overlapping operations are rejected")
    {
        REQUIRE_FALSE(
            parseEditPlan(
                R"({"version":1,"operations":[{"type":"mute_range","start_frame":10,"end_frame":30},{"type":"retime_range","start_frame":20,"end_frame":40,"target_duration_frames":5,"preserve_pitch":true}]})")
                .isValid());
    }
}
