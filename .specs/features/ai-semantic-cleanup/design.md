# Design

## Pipeline

1. `AssistantDock` selects a preset and accepts an editable instruction.
2. Semantic presets invoke `LocalTimelineTranscriber`.
3. The transcriber serializes the current timeline, renders its mixed audio to a
   temporary WAV, and runs Kdenlive's configured Whisper subtitle script.
4. Parsed SRT cues are converted to frame-aligned transcript segments.
5. `AiProviderClient` sends those segments with the instruction and a strict
   version-1 edit-plan schema.
6. `EditPlanExecutor` validates and executes operations in descending start-frame
   order, composing all undo/redo functions into one action.

## Operations

- `retime_range`: existing exact-duration range retime.
- `mute_range`: split linked clips at range boundaries and disable only isolated
  audio clips; video clips remain enabled.

## Safety

- Plans are bounded to 256 operations and 256 KiB.
- All ranges must be inside the timeline and may not overlap.
- Each executor performs preflight checks before mutation.
- Batch execution rolls back on the first failure.
- Transcript text is escaped by JSON serialization and media is never attached.
