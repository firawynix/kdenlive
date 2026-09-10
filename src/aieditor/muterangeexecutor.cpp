/*
    SPDX-FileCopyrightText: 2026 Kdenlive AI Editor contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "muterangeexecutor.hpp"

#include "timeline2/model/timelinefunctions.hpp"
#include "timeline2/model/timelineitemmodel.hpp"

#include <algorithm>
#include <unordered_set>

namespace Kdenlive {
namespace AiEditor {

bool MuteRangeResult::isValid() const
{
    return error.isEmpty();
}

MuteRangeResult MuteRangeExecutor::preflight(const std::shared_ptr<TimelineItemModel> &timeline, const MuteRangeOperation &operation)
{
    MuteRangeResult result;
    if (!timeline) {
        result.error = QStringLiteral("No active timeline is available.");
        return result;
    }
    if (operation.startFrame < 0 || operation.endFrame <= operation.startFrame || operation.endFrame > timeline->duration()) {
        result.error = QStringLiteral("The mute range is outside the active timeline.");
        return result;
    }

    std::unordered_set<int> affected;
    for (int trackId : timeline->getAllTracksIds()) {
        const auto items = timeline->getItemsInRange(trackId, operation.startFrame, operation.endFrame - 1, true);
        if (timeline->trackIsLocked(trackId) && !items.empty()) {
            result.error = QStringLiteral("A locked track contains content in the mute range.");
            return result;
        }
        for (int itemId : items) {
            if (!timeline->isClip(itemId)) {
                result.error = QStringLiteral("The mute range contains an unsupported timeline item.");
                return result;
            }
            if (timeline->getItemPosition(itemId) > operation.startFrame || timeline->getItemEnd(itemId) < operation.endFrame) {
                result.error = QStringLiteral("The mute range must be covered by one continuous clip on each affected track.");
                return result;
            }
            affected.insert(itemId);
        }
    }
    if (affected.empty()) {
        result.error = QStringLiteral("No clip covers the mute range.");
        return result;
    }

    const int representative = *affected.begin();
    const auto group = timeline->getGroupElements(representative);
    if (group.size() != affected.size()) {
        result.error = QStringLiteral("All clips covering the mute range must belong to the same aligned group.");
        return result;
    }
    for (int id : affected) {
        if (group.count(id) == 0) {
            result.error = QStringLiteral("All clips covering the mute range must belong to the same aligned group.");
            return result;
        }
        const auto state = timeline->getClipState(id).first;
        if (state == PlaylistState::AudioOnly) {
            result.mutedClipIds.push_back(id);
        }
    }
    if (result.mutedClipIds.isEmpty()) {
        result.error = QStringLiteral("No audio clip covers the mute range.");
        return result;
    }
    std::sort(result.mutedClipIds.begin(), result.mutedClipIds.end());
    return result;
}

MuteRangeResult MuteRangeExecutor::execute(const std::shared_ptr<TimelineItemModel> &timeline, const MuteRangeOperation &operation, Fun &undo, Fun &redo)
{
    auto result = preflight(timeline, operation);
    if (!result.isValid()) {
        return result;
    }

    const int firstAudio = result.mutedClipIds.constFirst();
    const int representativeTrack = timeline->getClipTrackId(firstAudio);
    int segmentId = timeline->getClipByPosition(representativeTrack, operation.startFrame);
    auto rollBack = [&]() {
        const bool restored = undo();
        Q_ASSERT(restored);
        undo = []() { return true; };
        redo = []() { return true; };
    };

    if (timeline->getItemPosition(segmentId) < operation.startFrame) {
        if (!TimelineFunctions::requestClipCut(timeline, segmentId, operation.startFrame, undo, redo)) {
            rollBack();
            result.error = QStringLiteral("Could not cut the clips at the mute range start.");
            return result;
        }
        segmentId = timeline->getClipByPosition(representativeTrack, operation.startFrame);
    }
    if (segmentId < 0 || timeline->getItemPosition(segmentId) != operation.startFrame) {
        rollBack();
        result.error = QStringLiteral("The mute range start did not produce an exact clip boundary.");
        return result;
    }
    if (timeline->getItemEnd(segmentId) > operation.endFrame && !TimelineFunctions::requestClipCut(timeline, segmentId, operation.endFrame, undo, redo)) {
        rollBack();
        result.error = QStringLiteral("Could not cut the clips at the mute range end.");
        return result;
    }

    segmentId = timeline->getClipByPosition(representativeTrack, operation.startFrame);
    const auto segments = timeline->getGroupElements(segmentId);
    result.mutedClipIds.clear();
    for (int clipId : segments) {
        if (!timeline->isClip(clipId) || timeline->getItemPosition(clipId) != operation.startFrame || timeline->getItemEnd(clipId) != operation.endFrame) {
            rollBack();
            result.error = QStringLiteral("The mute range did not isolate an aligned clip group.");
            return result;
        }
        const auto state = timeline->getClipState(clipId).first;
        PlaylistState::ClipState target = state;
        if (state == PlaylistState::AudioOnly) {
            target = PlaylistState::Disabled;
        }
        if (target != state) {
            if (!TimelineFunctions::changeClipState(timeline, clipId, target, undo, redo)) {
                rollBack();
                result.mutedClipIds.clear();
                result.error = QStringLiteral("Kdenlive could not mute every audio clip in the range.");
                return result;
            }
            result.mutedClipIds.push_back(clipId);
        }
    }
    std::sort(result.mutedClipIds.begin(), result.mutedClipIds.end());
    return result;
}

} // namespace AiEditor
} // namespace Kdenlive
