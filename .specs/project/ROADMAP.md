# Roadmap

## M1 — Safe edit-plan foundation

- Versioned JSON contract.
- `retime_range` parsing and validation.
- Unit tests for valid, malformed, unsupported, and unsafe plans.

## M2 — Timeline execution

- Resolve frame ranges against unlocked tracks.
- Cut boundary clips and preserve linked audio/video.
- Apply proportional speed changes and ripple the remaining timeline.
- Execute as one undoable command with collision checks.

## M3 — OpenRouter integration

- Provider client with model selection and environment-based secret input.
- Minimal timeline context serialization.
- Structured-output request and response handling.
- Timeouts, cancellation, and privacy messaging.

## M4 — Assistant experience

- Docked prompt interface.
- Human-readable plan preview and explicit Apply/Cancel.
- Settings and error states.
- Interactive acceptance testing on Windows.

## Later operations

- Trim, split, move, ripple delete, effects, transitions, captions, audio levels, and render presets.

