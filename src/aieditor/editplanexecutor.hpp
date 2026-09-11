/*
    SPDX-FileCopyrightText: 2026 Kdenlive AI Editor contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "editplan.hpp"

#include <QString>
#include <memory>

class TimelineItemModel;

namespace Kdenlive {
namespace AiEditor {

struct EditPlanExecutionResult
{
    QString error;
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
    static EditPlanExecutionResult preflight(const std::shared_ptr<TimelineItemModel> &timeline, const EditPlan &plan);
    static CompatibleEditPlan compatiblePlan(const std::shared_ptr<TimelineItemModel> &timeline, const EditPlan &plan);
    static EditPlanExecutionResult apply(const std::shared_ptr<TimelineItemModel> &timeline, const EditPlan &plan);
};

} // namespace AiEditor
} // namespace Kdenlive
