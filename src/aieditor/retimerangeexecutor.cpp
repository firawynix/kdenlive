/*
    SPDX-FileCopyrightText: 2026 Kdenlive AI Editor contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "retimerangeexecutor.hpp"

#include "bin/model/subtitlemodel.hpp"
#include "core.h"
#include "macros.hpp"
#include "timeline2/model/timelinefunctions.hpp"
#include "timeline2/model/timelineitemmodel.hpp"

#include <KLocalizedString>
#include <QPoint>
#include <algorithm>
#include <unordered_set>

namespace Kdenlive {
namespace AiEditor {

bool RetimeRangePreflightResult::isValid() const
{
    return error.isEmpty();
}

bool RetimeRangeExecutionResult::isValid() const
{
    return error.isEmpty();
}

RetimeRangePreflightResult RetimeRangeExecutor::preflight(const std::shared_ptr<TimelineItemModel> &timeline, const RetimeRangeOperation &operation)
{
    RetimeRangePreflightResult result;
    if (!timeline) {
        result.error = i18n("No active timeline is available.");
        return result;
    }
    if (operation.startFrame < 0 || operation.endFrame <= operation.startFrame || operation.targetDurationFrames < 1) {
        result.error = i18n("The retime range is invalid.");
        return result;
    }

    // Subtitle ripple is deliberately not part of the first executor. Reject it
    // before any cuts can leave captions out of sync.
    const auto subtitleModel = timeline->getSubtitleModel();
    const auto subtitles = subtitleModel ? subtitleModel->getItemsInRange(-1, operation.startFrame, -1) : std::unordered_set<int>{};
    if (!subtitles.empty()) {
        result.error = i18n("The range cannot be retimed while subtitles exist at or after its start.");
        return result;
    }

    std::unordered_set<int> affectedClips;
    std::unordered_set<int> affectedTracks;
    const auto allTracks = timeline->getAllTracksIds();
    for (int trackId : allTracks) {
        const auto laterItems = timeline->getItemsInRange(trackId, operation.startFrame, -1, true);
        if (timeline->trackIsLocked(trackId) && !laterItems.empty()) {
            result.error = i18n("A locked track contains content that would be affected by the retime.");
            return result;
        }

        for (int itemId : laterItems) {
            if (timeline->isComposition(itemId)) {
                result.error = i18n("Compositions at or after the range start are not supported yet.");
                return result;
            }
        }

        const auto rangeItems = timeline->getItemsInRange(trackId, operation.startFrame, operation.endFrame - 1, true);
        for (int itemId : rangeItems) {
            if (!timeline->isClip(itemId)) {
                result.error = i18n("The selected range contains an unsupported timeline item.");
                return result;
            }
            if (timeline->getItemPosition(itemId) > operation.startFrame || timeline->getItemEnd(itemId) < operation.endFrame) {
                result.error = i18n("The selected range must be covered by one continuous clip on each affected track.");
                return result;
            }

            const auto mixRange = timeline->getMixInOut(itemId);
            if (mixRange.first >= 0 || mixRange.second >= 0 || timeline->getMixDuration(itemId) > 0) {
                result.error = i18n("Clips participating in a same-track mix cannot be retimed yet.");
                return result;
            }
            affectedClips.insert(itemId);
            affectedTracks.insert(trackId);
        }
    }

    if (affectedClips.empty()) {
        result.error = i18n("No clip covers the selected range.");
        return result;
    }

    const int representative = *affectedClips.begin();
    const auto groupElements = timeline->getGroupElements(representative);
    if (groupElements.size() != affectedClips.size()) {
        result.error = i18n("All clips covering the range must belong to the same aligned group.");
        return result;
    }
    for (int clipId : affectedClips) {
        if (groupElements.count(clipId) == 0) {
            result.error = i18n("All clips covering the range must belong to the same aligned group.");
            return result;
        }
    }

    result.clipIds.reserve(qsizetype(affectedClips.size()));
    for (int clipId : affectedClips) {
        result.clipIds.push_back(clipId);
    }
    std::sort(result.clipIds.begin(), result.clipIds.end());

    result.trackIds.reserve(qsizetype(affectedTracks.size()));
    for (int trackId : affectedTracks) {
        result.trackIds.push_back(trackId);
    }
    std::sort(result.trackIds.begin(), result.trackIds.end());
    return result;
}

RetimeRangeExecutionResult RetimeRangeExecutor::execute(const std::shared_ptr<TimelineItemModel> &timeline, const RetimeRangeOperation &operation, Fun &undo,
                                                        Fun &redo)
{
    RetimeRangeExecutionResult result;
    const auto validation = preflight(timeline, operation);
    if (!validation.isValid()) {
        result.error = validation.error;
        return result;
    }
    if (operation.targetDurationFrames > operation.endFrame - operation.startFrame) {
        result.error = i18n("Making a range longer is not supported yet.");
        return result;
    }

    const int representativeTrack = timeline->getClipTrackId(validation.clipIds.constFirst());
    int segmentId = timeline->getClipByPosition(representativeTrack, operation.startFrame);
    if (segmentId < 0) {
        result.error = i18n("The clip at the range start could not be resolved.");
        return result;
    }

    auto rollBack = [&]() {
        const bool restored = undo();
        Q_ASSERT(restored);
        undo = []() { return true; };
        redo = []() { return true; };
    };

    if (timeline->getItemPosition(segmentId) < operation.startFrame) {
        if (!TimelineFunctions::requestClipCut(timeline, segmentId, operation.startFrame, undo, redo)) {
            rollBack();
            result.error = i18n("Could not cut the clips at the range start.");
            return result;
        }
        segmentId = timeline->getClipByPosition(representativeTrack, operation.startFrame);
    }
    if (segmentId < 0 || timeline->getItemPosition(segmentId) != operation.startFrame) {
        rollBack();
        result.error = i18n("The range start did not produce an exact clip boundary.");
        return result;
    }

    if (timeline->getItemEnd(segmentId) > operation.endFrame && !TimelineFunctions::requestClipCut(timeline, segmentId, operation.endFrame, undo, redo)) {
        rollBack();
        result.error = i18n("Could not cut the clips at the range end.");
        return result;
    }

    segmentId = timeline->getClipByPosition(representativeTrack, operation.startFrame);
    if (segmentId < 0) {
        rollBack();
        result.error = i18n("The isolated range could not be resolved.");
        return result;
    }

    const auto segments = timeline->getGroupElements(segmentId);
    for (int clipId : segments) {
        if (!timeline->isClip(clipId) || timeline->getItemPosition(clipId) != operation.startFrame || timeline->getItemEnd(clipId) != operation.endFrame) {
            rollBack();
            result.error = i18n("The range did not isolate an aligned clip group.");
            return result;
        }
    }

    for (int clipId : segments) {
        if (!timeline->requestClipTimeWarp(clipId, operation.speedMultiplier(), operation.preservePitch, true, undo, redo)) {
            rollBack();
            result.error = i18n("Kdenlive could not apply the requested speed to every clip.");
            return result;
        }
        if (timeline->getItemPlaytime(clipId) != operation.targetDurationFrames) {
            rollBack();
            result.error = i18n("The requested duration cannot be represented exactly in this project.");
            return result;
        }
        result.retimedClipIds.push_back(clipId);
    }

    const int retimedEnd = operation.startFrame + operation.targetDurationFrames;
    if (retimedEnd < operation.endFrame) {
        QVector<int> allTracks;
        const auto trackIds = timeline->getAllTracksIds();
        allTracks.reserve(qsizetype(trackIds.size()));
        for (int trackId : trackIds) {
            allTracks.push_back(trackId);
        }

        Fun rippleUndo = []() { return true; };
        Fun rippleRedo = []() { return true; };
        if (!TimelineFunctions::removeSpace(timeline, QPoint(retimedEnd, operation.endFrame), rippleUndo, rippleRedo, allTracks, false)) {
            rollBack();
            result.retimedClipIds.clear();
            result.error = i18n("The timeline could not close the space left by the retime.");
            return result;
        }
        UPDATE_UNDO_REDO_NOLOCK(rippleRedo, rippleUndo, undo, redo);
    }

    std::sort(result.retimedClipIds.begin(), result.retimedClipIds.end());
    return result;
}

RetimeRangeExecutionResult RetimeRangeExecutor::apply(const std::shared_ptr<TimelineItemModel> &timeline, const RetimeRangeOperation &operation)
{
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    auto result = execute(timeline, operation, undo, redo);
    if (result.isValid()) {
        pCore->pushUndo(undo, redo, i18n("AI: Retime range"));
    }
    return result;
}

} // namespace AiEditor
} // namespace Kdenlive
