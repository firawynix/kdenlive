/*
    SPDX-FileCopyrightText: 2026 Kdenlive AI Editor contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "editplan.hpp"

#include <QString>
#include <functional>
#include <memory>

class TimelineItemModel;

namespace Kdenlive {
namespace AiEditor {

struct AppliedEditOperation
{
    EditOperation operation;
    std::function<bool()> undo;
    std::function<bool()> redo;
    std::function<bool()> isApplied;
};

struct EditPlanExecutionResult
{
    QString error;
    QVector<AppliedEditOperation> appliedOperations;
    bool isValid() const;
};

struct CompatibleEditPlan
{
    EditPlan plan;
    int skippedOperations{0};
    QString firstSkippedReason;
};

class EditPlanExecutor
{
public:
    using ProgressCallback = std::function<void(int completed, int total)>;
    using StateChangedCallback = std::function<void()>;

    static EditPlanExecutionResult preflight(const std::shared_ptr<TimelineItemModel> &timeline, const EditPlan &plan);
    static CompatibleEditPlan compatiblePlan(const std::shared_ptr<TimelineItemModel> &timeline, const EditPlan &plan);
    static EditPlanExecutionResult apply(const std::shared_ptr<TimelineItemModel> &timeline, const EditPlan &plan, const ProgressCallback &progress = {},
                                         const StateChangedCallback &stateChanged = {});
};

} // namespace AiEditor
} // namespace Kdenlive
