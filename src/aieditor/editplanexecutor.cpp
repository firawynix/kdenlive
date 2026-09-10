/*
    SPDX-FileCopyrightText: 2026 Kdenlive AI Editor contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "editplanexecutor.hpp"

#include "core.h"
#include "muterangeexecutor.hpp"
#include "retimerangeexecutor.hpp"
#include "timeline2/model/timelineitemmodel.hpp"

#include <KLocalizedString>
#include <algorithm>

namespace Kdenlive {
namespace AiEditor {

bool EditPlanExecutionResult::isValid() const
{
    return error.isEmpty();
}

EditPlanExecutionResult EditPlanExecutor::preflight(const std::shared_ptr<TimelineItemModel> &timeline, const EditPlan &plan)
{
    EditPlanExecutionResult result;
    if (!timeline || plan.operations.isEmpty()) {
        result.error = QStringLiteral("No valid edit plan is available.");
        return result;
    }
    for (const EditOperation &operation : plan.operations) {
        if (operation.endFrame() > timeline->duration()) {
            result.error = QStringLiteral("An edit operation extends beyond the active timeline.");
            return result;
        }
        if (operation.type == EditOperationType::RetimeRange) {
            if (operation.retimeRange.targetDurationFrames > operation.retimeRange.endFrame - operation.retimeRange.startFrame) {
                result.error = QStringLiteral("Making a range longer is not supported yet.");
                return result;
            }
            const auto validation = RetimeRangeExecutor::preflight(timeline, operation.retimeRange);
            if (!validation.isValid()) {
                result.error = validation.error;
                return result;
            }
        } else {
            const auto validation = MuteRangeExecutor::preflight(timeline, operation.muteRange);
            if (!validation.isValid()) {
                result.error = validation.error;
                return result;
            }
        }
    }
    return result;
}

EditPlanExecutionResult EditPlanExecutor::apply(const std::shared_ptr<TimelineItemModel> &timeline, const EditPlan &plan)
{
    auto result = preflight(timeline, plan);
    if (!result.isValid()) {
        return result;
    }

    QVector<EditOperation> operations = plan.operations;
    std::sort(operations.begin(), operations.end(),
              [](const EditOperation &left, const EditOperation &right) { return left.startFrame() > right.startFrame(); });
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    for (const EditOperation &operation : operations) {
        QString error;
        if (operation.type == EditOperationType::RetimeRange) {
            error = RetimeRangeExecutor::execute(timeline, operation.retimeRange, undo, redo).error;
        } else {
            error = MuteRangeExecutor::execute(timeline, operation.muteRange, undo, redo).error;
        }
        if (!error.isEmpty()) {
            const bool restored = undo();
            Q_ASSERT(restored);
            result.error = error;
            return result;
        }
    }
    pCore->pushUndo(undo, redo, i18np("AI: Apply edit plan", "AI: Apply edit plan (%1 operations)", operations.size()));
    return result;
}

} // namespace AiEditor
} // namespace Kdenlive
