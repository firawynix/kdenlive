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

## Runtime controls and credentials

- `ResourceBudget` converts the selected percentage into bounded CPU and memory
  values. The transcriber applies environment-level thread limits plus native
  Windows affinity/job limits to each child process; CUDA is never offered for
  the detected AMD-only system.
- `SecureCredentialStore` uses provider-specific Windows Credential Manager
  generic credentials. `AssistantDock` prefers that store and falls back to the
  existing environment variable.
- Connection tests use read-only authenticated API endpoints and never send a
  model prompt or expose a key in a URL, status message, or log.
- Branding changes only user-facing identity. The `kdenlive` application id and
  upstream project/legal metadata remain intact for compatibility and proper
  attribution.
