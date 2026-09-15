/*
    SPDX-FileCopyrightText: 2026 Kdenlive AI Editor contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "personretimeplanner.hpp"

#include "retimerangeexecutor.hpp"
#include "timeline2/model/timelineitemmodel.hpp"

#include <KLocalizedString>
#include <QRegularExpression>
#include <QSet>
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <utility>

namespace Kdenlive {
namespace AiEditor {

bool PersonRetimePlanResult::isValid() const
{
    return error.isEmpty();
}

int PersonRetimePlanner::targetDurationFrames(const QString &instruction, double fps)
{
    if (fps <= 0.0) {
        return -1;
    }
    const QString text = instruction.simplified();
    const QRegularExpression clock(QStringLiteral(R"((?:para|dure|duração(?:\s+final)?(?:\s+de)?|to|last)\s*(\d{1,2}):(\d{2})(?::(\d{2}))?)"),
                                   QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
    const auto clockMatch = clock.match(text);
    if (clockMatch.hasMatch()) {
        const int first = clockMatch.captured(1).toInt();
        const int second = clockMatch.captured(2).toInt();
        const int third = clockMatch.captured(3).isEmpty() ? -1 : clockMatch.captured(3).toInt();
        const qint64 seconds = third < 0 ? qint64(first) * 60 + second : qint64(first) * 3600 + qint64(second) * 60 + third;
        return seconds > 0 ? int(qRound64(double(seconds) * fps)) : -1;
    }

    const QRegularExpression amount(
        QStringLiteral(
            R"((?:para|dure|duração(?:\s+final)?(?:\s+de)?|reduz(?:a|ir)?(?:\s+o\s+v[ií]deo)?\s+(?:para|a)|to|last)\s*(\d+(?:[\.,]\d+)?)\s*(horas?|hours?|minutos?|minutes?|mins?|segundos?|seconds?|secs?))"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
    const auto match = amount.match(text);
    if (!match.hasMatch()) {
        return -1;
    }
    QString number = match.captured(1);
    number.replace(QLatin1Char(','), QLatin1Char('.'));
    bool ok = false;
    const double value = number.toDouble(&ok);
    if (!ok || value <= 0.0) {
        return -1;
    }
    const QString unit = match.captured(2).toLower();
    const double seconds = unit.startsWith(QLatin1String("h")) ? value * 3600.0 : (unit.startsWith(QLatin1String("m")) ? value * 60.0 : value);
    const qint64 frames = qRound64(seconds * fps);
    return frames > 0 && frames <= std::numeric_limits<int>::max() ? int(frames) : -1;
}

QVector<VisualFrameRange> PersonRetimePlanner::normalizeDetections(const QVector<int> &detectedFrames, int sampleStepFrames, int paddingFrames,
                                                                   int mergeGapFrames, int timelineFrames)
{
    if (timelineFrames <= 0 || detectedFrames.isEmpty()) {
        return {};
    }
    QVector<int> sorted = detectedFrames;
    std::sort(sorted.begin(), sorted.end());
    sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());
    const int halfStep = qMax(1, sampleStepFrames / 2);
    QVector<VisualFrameRange> result;
    for (int frame : std::as_const(sorted)) {
        VisualFrameRange current{qMax(0, frame - halfStep - paddingFrames), qMin(timelineFrames, frame + halfStep + paddingFrames + 1)};
        if (current.endFrame <= current.startFrame) {
            continue;
        }
        if (!result.isEmpty() && current.startFrame <= result.last().endFrame + mergeGapFrames) {
            result.last().endFrame = qMax(result.last().endFrame, current.endFrame);
        } else {
            result.push_back(current);
        }
    }
    return result;
}

QVector<int> PersonRetimePlanner::allocateTargetDurations(const QVector<int> &sourceDurations, int requiredReduction, QString *error)
{
    if (error) {
        error->clear();
    }
    if (sourceDurations.isEmpty() || requiredReduction <= 0) {
        if (error) {
            *error = i18n("There is no editable fast-motion duration to distribute.");
        }
        return {};
    }
    qint64 maximumReduction = 0;
    qint64 totalSource = 0;
    for (int duration : sourceDurations) {
        if (duration < 2) {
            if (error) {
                *error = i18n("A visual edit segment is too short to accelerate safely.");
            }
            return {};
        }
        maximumReduction += duration - 1;
        totalSource += duration;
    }
    if (requiredReduction > maximumReduction) {
        if (error) {
            *error = i18n("The requested final duration is too short while keeping detected people at normal speed.");
        }
        return {};
    }

    QVector<int> reductions(sourceDurations.size(), 0);
    struct Remainder
    {
        int index;
        double value;
    };
    QVector<Remainder> remainders;
    int assigned = 0;
    for (qsizetype index = 0; index < sourceDurations.size(); ++index) {
        const double exact = double(requiredReduction) * double(sourceDurations.at(index)) / double(totalSource);
        const int reduction = qMin(sourceDurations.at(index) - 1, int(std::floor(exact)));
        reductions[index] = reduction;
        assigned += reduction;
        remainders.push_back({int(index), exact - std::floor(exact)});
    }
    std::sort(remainders.begin(), remainders.end(), [](const Remainder &left, const Remainder &right) { return left.value > right.value; });
    int cursor = 0;
    while (assigned < requiredReduction) {
        bool changed = false;
        for (int count = 0; count < remainders.size() && assigned < requiredReduction; ++count) {
            const int index = remainders.at((cursor + count) % remainders.size()).index;
            if (reductions.at(index) < sourceDurations.at(index) - 1) {
                ++reductions[index];
                ++assigned;
                changed = true;
            }
        }
        if (!changed) {
            if (error) {
                *error = i18n("The requested duration could not be distributed across the safe clip segments.");
            }
            return {};
        }
        cursor = (cursor + 1) % remainders.size();
    }

    QVector<int> targets;
    targets.reserve(sourceDurations.size());
    for (qsizetype index = 0; index < sourceDurations.size(); ++index) {
        targets.push_back(sourceDurations.at(index) - reductions.at(index));
    }
    return targets;
}

PersonRetimePlanResult PersonRetimePlanner::build(const std::shared_ptr<TimelineItemModel> &timeline, const QVector<VisualFrameRange> &personRanges,
                                                  int targetFrames)
{
    PersonRetimePlanResult result;
    result.plan.version = 1;
    if (!timeline || timeline->duration() < 2 || targetFrames < 1 || targetFrames >= timeline->duration()) {
        result.error = i18n("The visual edit target duration is invalid for the active timeline.");
        return result;
    }

    const int duration = timeline->duration();
    QSet<int> boundarySet{0, duration};
    for (int trackId : timeline->getAllTracksIds()) {
        const auto items = timeline->getItemsInRange(trackId, 0, duration - 1, true);
        for (int itemId : items) {
            if (timeline->isClip(itemId)) {
                boundarySet.insert(qBound(0, timeline->getItemPosition(itemId), duration));
                boundarySet.insert(qBound(0, timeline->getItemEnd(itemId), duration));
            }
        }
    }
    QVector<int> boundaries(boundarySet.begin(), boundarySet.end());
    std::sort(boundaries.begin(), boundaries.end());

    QVector<VisualFrameRange> candidates;
    int cursor = 0;
    auto appendFastRange = [&](int start, int end) {
        if (end - start < 2) {
            return;
        }
        auto boundary = std::upper_bound(boundaries.begin(), boundaries.end(), start);
        int pieceStart = start;
        while (boundary != boundaries.end() && *boundary < end) {
            if (*boundary - pieceStart >= 2) {
                candidates.push_back({pieceStart, *boundary});
            }
            pieceStart = *boundary;
            ++boundary;
        }
        if (end - pieceStart >= 2) {
            candidates.push_back({pieceStart, end});
        }
    };
    for (const VisualFrameRange &range : personRanges) {
        const int start = qBound(0, range.startFrame, duration);
        const int end = qBound(start, range.endFrame, duration);
        appendFastRange(cursor, start);
        cursor = qMax(cursor, end);
    }
    appendFastRange(cursor, duration);

    QVector<VisualFrameRange> editable;
    QVector<int> sourceDurations;
    for (const VisualFrameRange &range : std::as_const(candidates)) {
        RetimeRangeOperation probe{range.startFrame, range.endFrame, range.endFrame - range.startFrame - 1, true};
        if (RetimeRangeExecutor::preflight(timeline, probe).isValid()) {
            editable.push_back(range);
            sourceDurations.push_back(range.endFrame - range.startFrame);
        }
    }
    if (editable.isEmpty()) {
        result.error = i18n("No clip segment outside the detected-person ranges can be accelerated safely.");
        return result;
    }
    if (editable.size() > 4096) {
        result.error = i18n("The visual analysis produced too many edit segments. Increase the visual sampling interval and try again.");
        return result;
    }
    const int requiredReduction = duration - targetFrames;
    QString allocationError;
    const QVector<int> targets = allocateTargetDurations(sourceDurations, requiredReduction, &allocationError);
    if (targets.isEmpty()) {
        result.error = allocationError;
        return result;
    }
    for (qsizetype index = 0; index < editable.size(); ++index) {
        EditOperation operation;
        operation.type = EditOperationType::RetimeRange;
        operation.retimeRange = {editable.at(index).startFrame, editable.at(index).endFrame, targets.at(index), true};
        result.plan.operations.push_back(operation);
        result.acceleratedFrames += sourceDurations.at(index);
    }
    result.preservedFrames = duration - result.acceleratedFrames;
    return result;
}

} // namespace AiEditor
} // namespace Kdenlive
