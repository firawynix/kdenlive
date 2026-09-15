/*
    SPDX-FileCopyrightText: 2026 Kdenlive AI Editor contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "editplan.hpp"

#include <QString>
#include <QVector>
#include <memory>

class TimelineItemModel;

namespace Kdenlive {
namespace AiEditor {

struct VisualFrameRange
{
    int startFrame{0};
    int endFrame{0};
};

struct PersonRetimePlanResult
{
    EditPlan plan;
    QString error;
    int preservedFrames{0};
    int acceleratedFrames{0};
    int minimumDurationFrames{0};

    bool isValid() const;
};

class PersonRetimePlanner
{
public:
    static int targetDurationFrames(const QString &instruction, double fps);
    static QVector<VisualFrameRange> normalizeDetections(const QVector<int> &detectedFrames, int sampleStepFrames, int paddingFrames, int mergeGapFrames,
                                                         int timelineFrames);
    static int minimumTargetDurationFrames(int timelineFrames, const QVector<int> &editableDurations);
    static QVector<int> allocateTargetDurations(const QVector<int> &sourceDurations, int requiredReduction, QString *error = nullptr);
    static PersonRetimePlanResult build(const std::shared_ptr<TimelineItemModel> &timeline, const QVector<VisualFrameRange> &personRanges, int targetFrames);
};

} // namespace AiEditor
} // namespace Kdenlive
