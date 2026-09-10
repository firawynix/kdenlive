/*
    SPDX-FileCopyrightText: 2026 Kdenlive AI Editor contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "editplan.hpp"
#include "undohelper.hpp"

#include <QString>
#include <QVector>
#include <memory>

class TimelineItemModel;

namespace Kdenlive {
namespace AiEditor {

struct RetimeRangePreflightResult
{
    QVector<int> clipIds;
    QVector<int> trackIds;
    QString error;

    bool isValid() const;
};

struct RetimeRangeExecutionResult
{
    QVector<int> retimedClipIds;
    QString error;

    bool isValid() const;
};

/**
 * Validates whether a retime operation can be applied without partially
 * changing unsupported timeline structures.
 */
class RetimeRangeExecutor
{
public:
    static RetimeRangePreflightResult preflight(const std::shared_ptr<TimelineItemModel> &timeline, const RetimeRangeOperation &operation);
    static RetimeRangeExecutionResult execute(const std::shared_ptr<TimelineItemModel> &timeline, const RetimeRangeOperation &operation, Fun &undo, Fun &redo);
    static RetimeRangeExecutionResult apply(const std::shared_ptr<TimelineItemModel> &timeline, const RetimeRangeOperation &operation);
};

} // namespace AiEditor
} // namespace Kdenlive
