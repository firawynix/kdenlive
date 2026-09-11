# Design

## Pipeline

1. `AssistantDock` selects a preset and accepts an editable instruction.
2. Semantic presets invoke `LocalTimelineTranscriber`.
3. The transcriber serializes the current timeline, renders its mixed audio to a
   temporary WAV, and runs Kdenlive's configured Whisper subtitle script.
4. Parsed SRT cues are converted to frame-aligned transcript segments.
5. `AiProviderClient` sends those segments with the instruction and a strict
   version-1 edit-plan schema.
6. Long transcripts are divided into bounded, slightly overlapping segments.
   Each segment owns a disjoint frame interval so boundary context remains
   available without duplicating edits.
7. `AiSessionStore` saves the local transcript and every completed provider
   fragment atomically. It derives identifiers from timeline content and request
   settings and never stores a credential.
8. `EditPlanExecutor` validates and executes operations in descending start-frame
   order, composing all undo/redo functions into one action.
9. `LocalAiManager` inventories the host, recommends a bounded Ollama model,
   installs Ollama only after an explicit button press, and streams model-pull
   progress into the dock.

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
- Provider requests use a 16,000-character transcript budget per segment and a
  provider-appropriate 16,384-token output ceiling. OpenRouter routing requires
  support for the requested structured-output parameters.
- Transcript checkpoints are stored only in the application-local data folder,
  expire after seven days, and are reused only when the serialized timeline,
  FPS, Whisper model, and language produce the same fingerprint.
- `LocalTimelineTranscriber` consumes Melt and Whisper progress output while
  retaining a bounded diagnostic log. The dock shows phase, percentage, and an
  elapsed-rate ETA; indeterminate work remains animated.
- Ollama traffic is fixed to `127.0.0.1:11434`, requires no credential, uses the
  same edit-plan JSON schema, and receives only the prompt/transcript context.
- Memory-based recommendations favor Qwen3 30B at 48 GiB or more, 14B at 24
  GiB, 8B at 12 GiB, and 4B below that. They are recommendations rather than a
  promise of GPU fit or speed.
