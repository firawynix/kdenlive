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
#include <utility>

namespace Kdenlive {
namespace AiEditor {

namespace {
EditPlanExecutionResult preflightOperation(const std::shared_ptr<TimelineItemModel> &timeline, const EditOperation &operation)
{
    EditPlanExecutionResult result;
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
        result.error = validation.error;
    } else {
        const auto validation = MuteRangeExecutor::preflight(timeline, operation.muteRange);
        result.error = validation.error;
    }
    return result;
}
} // namespace

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
        result = preflightOperation(timeline, operation);
        if (!result.isValid()) {
            return result;
        }
    }
    return result;
}

CompatibleEditPlan EditPlanExecutor::compatiblePlan(const std::shared_ptr<TimelineItemModel> &timeline, const EditPlan &plan)
{
    CompatibleEditPlan result;
    result.plan.version = plan.version;
    if (!timeline) {
        result.skippedOperations = int(plan.operations.size());
        result.firstSkippedReason = QStringLiteral("No active timeline is available.");
        return result;
    }
    result.plan.operations.reserve(plan.operations.size());
    for (const EditOperation &operation : plan.operations) {
        const auto validation = preflightOperation(timeline, operation);
        if (validation.isValid()) {
            result.plan.operations.push_back(operation);
        } else {
            ++result.skippedOperations;
            if (result.firstSkippedReason.isEmpty()) {
                result.firstSkippedReason = validation.error;
            }
        }
    }
    return result;
}

EditPlanExecutionResult EditPlanExecutor::apply(const std::shared_ptr<TimelineItemModel> &timeline, const EditPlan &plan, const ProgressCallback &progress)
{
    auto result = preflight(timeline, plan);
    if (!result.isValid()) {
        return result;
    }

    QVector<EditOperation> operations = plan.operations;
    std::sort(operations.begin(), operations.end(),
              [](const EditOperation &left, const EditOperation &right) { return left.startFrame() > right.startFrame(); });
    QVector<Fun> undoSteps;
    QVector<Fun> redoSteps;
    undoSteps.reserve(operations.size());
    redoSteps.reserve(operations.size());
    const auto undoCompletedSteps = [&undoSteps]() {
        bool restored = true;
        for (auto it = undoSteps.rbegin(); it != undoSteps.rend(); ++it) {
            const bool stepRestored = (*it)();
            restored = stepRestored && restored;
        }
        return restored;
    };
    for (qsizetype index = 0; index < operations.size(); ++index) {
        const EditOperation &operation = operations.at(index);
        Fun operationUndo = []() { return true; };
        Fun operationRedo = []() { return true; };
        QString error;
        if (operation.type == EditOperationType::RetimeRange) {
            error = RetimeRangeExecutor::execute(timeline, operation.retimeRange, operationUndo, operationRedo).error;
        } else {
            error = MuteRangeExecutor::execute(timeline, operation.muteRange, operationUndo, operationRedo).error;
        }
        if (!error.isEmpty()) {
            const bool restored = undoCompletedSteps();
            Q_ASSERT(restored);
            result.error = error;
            return result;
        }
        undoSteps.push_back(std::move(operationUndo));
        redoSteps.push_back(std::move(operationRedo));
        if (progress) {
            progress(int(index + 1), int(operations.size()));
        }
    }
    Fun undo = [steps = std::move(undoSteps)]() mutable {
        bool restored = true;
        for (auto it = steps.rbegin(); it != steps.rend(); ++it) {
            const bool stepRestored = (*it)();
            restored = stepRestored && restored;
        }
        return restored;
    };
    Fun redo = [steps = std::move(redoSteps)]() mutable {
        bool restored = true;
        for (Fun &step : steps) {
            const bool stepRestored = step();
            restored = stepRestored && restored;
        }
        return restored;
    };
    pCore->pushUndo(undo, redo, i18np("AI: Apply edit plan", "AI: Apply edit plan (%1 operations)", operations.size()));
    return result;
}

} // namespace AiEditor
} // namespace Kdenlive
