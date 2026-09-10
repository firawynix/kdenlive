/*
    SPDX-FileCopyrightText: 2026 Kdenlive AI Editor contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QByteArray>
#include <QString>
#include <QVector>

namespace Kdenlive {
namespace AiEditor {

enum class EditOperationType { RetimeRange, MuteRange };

struct RetimeRangeOperation
{
    int startFrame{-1};
    int endFrame{-1};
    int targetDurationFrames{-1};
    bool preservePitch{true};

    double speedMultiplier() const;
};

struct MuteRangeOperation
{
    int startFrame{-1};
    int endFrame{-1};
};

struct EditOperation
{
    EditOperationType type{EditOperationType::RetimeRange};
    RetimeRangeOperation retimeRange;
    MuteRangeOperation muteRange;

    int startFrame() const;
    int endFrame() const;
};

struct EditPlan
{
    int version{0};
    QVector<EditOperation> operations;
};

struct EditPlanParseResult
{
    EditPlan plan;
    QString error;

    bool isValid() const;
};

EditPlanParseResult parseEditPlan(const QByteArray &json, bool allowEmpty = false);

} // namespace AiEditor
} // namespace Kdenlive
