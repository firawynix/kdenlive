/*
    SPDX-FileCopyrightText: 2026 Kdenlive AI Editor contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "retimerangeexecutor.hpp"

#include "bin/model/subtitlemodel.hpp"
#include "timeline2/model/timelineitemmodel.hpp"

#include <algorithm>
#include <unordered_set>

namespace Kdenlive {
namespace AiEditor {

bool RetimeRangePreflightResult::isValid() const
{
    return error.isEmpty();
}

RetimeRangePreflightResult RetimeRangeExecutor::preflight(const std::shared_ptr<TimelineItemModel> &timeline,
                                                          const RetimeRangeOperation &operation)
{
    RetimeRangePreflightResult result;
    if (!timeline) {
        result.error = QStringLiteral("No active timeline is available.");
        return result;
    }
    if (operation.startFrame < 0 || operation.endFrame <= operation.startFrame || operation.targetDurationFrames < 1) {
        result.error = QStringLiteral("The retime range is invalid.");
        return result;
    }

    // Subtitle ripple is deliberately not part of the first executor. Reject it
    // before any cuts can leave captions out of sync.
    const auto subtitleModel = timeline->getSubtitleModel();
    const auto subtitles = subtitleModel ? subtitleModel->getItemsInRange(-1, operation.startFrame, -1) : std::unordered_set<int>{};
    if (!subtitles.empty()) {
        result.error = QStringLiteral("The range cannot be retimed while subtitles exist at or after its start.");
        return result;
    }

    std::unordered_set<int> affectedClips;
    std::unordered_set<int> affectedTracks;
    const auto allTracks = timeline->getAllTracksIds();
    for (int trackId : allTracks) {
        const auto laterItems = timeline->getItemsInRange(trackId, operation.startFrame, -1, true);
        if (timeline->trackIsLocked(trackId) && !laterItems.empty()) {
            result.error = QStringLiteral("A locked track contains content that would be affected by the retime.");
            return result;
        }

        for (int itemId : laterItems) {
            if (timeline->isComposition(itemId)) {
                result.error = QStringLiteral("Compositions at or after the range start are not supported yet.");
                return result;
            }
        }

        const auto rangeItems = timeline->getItemsInRange(trackId, operation.startFrame, operation.endFrame - 1, true);
        for (int itemId : rangeItems) {
            if (!timeline->isClip(itemId)) {
                result.error = QStringLiteral("The selected range contains an unsupported timeline item.");
                return result;
            }
            if (timeline->getItemPosition(itemId) > operation.startFrame || timeline->getItemEnd(itemId) < operation.endFrame) {
                result.error = QStringLiteral("The selected range must be covered by one continuous clip on each affected track.");
                return result;
            }

            const auto mixRange = timeline->getMixInOut(itemId);
            if (mixRange.first >= 0 || mixRange.second >= 0 || timeline->getMixDuration(itemId) > 0) {
                result.error = QStringLiteral("Clips participating in a same-track mix cannot be retimed yet.");
                return result;
            }
            affectedClips.insert(itemId);
            affectedTracks.insert(trackId);
        }
    }

    if (affectedClips.empty()) {
        result.error = QStringLiteral("No clip covers the selected range.");
        return result;
    }

    const int representative = *affectedClips.begin();
    const auto groupElements = timeline->getGroupElements(representative);
    if (groupElements.size() != affectedClips.size()) {
        result.error = QStringLiteral("All clips covering the range must belong to the same aligned group.");
        return result;
    }
    for (int clipId : affectedClips) {
        if (groupElements.count(clipId) == 0) {
            result.error = QStringLiteral("All clips covering the range must belong to the same aligned group.");
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

} // namespace AiEditor
} // namespace Kdenlive
