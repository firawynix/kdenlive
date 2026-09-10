# Timewarp Flow
> Existing speed-change and range-edit building blocks

Entry: `src/timeline2/view/timelinecontroller.cpp:TimelineController::changeItemSpeed()`

Flow: controller → `TimelineModel::requestClipTimeWarp()` → clip `useTimewarpProducer()` → track reinsert → Core undo stack

- UI-facing speed uses percent; inserted-clip invokable overload divides by 100.
- Undo-composable overload consumes multiplier directly (`1.2` = 120%).
- Linked A/V partner is discovered through `m_groups->getSplitPartner()`.
- `TimelineFunctions::liftZone()` shows boundary cut + range enumeration.
- `TimelineFunctions::removeSpace()` shows ripple movement after a zone.
- `tests/timewarptest.cpp` verifies duration math, undo/redo, and one-frame minimum.

Updated: 2026-09-10
