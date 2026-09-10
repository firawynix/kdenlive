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

struct MuteRangeResult
{
    QVector<int> mutedClipIds;
    QString error;

    bool isValid() const;
};

class MuteRangeExecutor
{
public:
    static MuteRangeResult preflight(const std::shared_ptr<TimelineItemModel> &timeline, const MuteRangeOperation &operation);
    static MuteRangeResult execute(const std::shared_ptr<TimelineItemModel> &timeline, const MuteRangeOperation &operation, Fun &undo, Fun &redo);
};

} // namespace AiEditor
} // namespace Kdenlive
