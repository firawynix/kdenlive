# AI Semantic Cleanup

## Goal

Extend the AI Editing Assistant so a user can ask, in natural language, to mute
off-topic dialogue and compress silent gaps in the active Kdenlive timeline.
Audio analysis must run locally; only timestamped transcript text may be sent to
the selected AI provider.

## Requirements

- ASC-01: Provide selectable, editable prompt presets for common operations.
- ASC-02: Export the current timeline audio and transcribe it locally with
  Kdenlive's configured Whisper installation.
- ASC-03: Send the instruction, FPS, duration, and timestamped transcript to the
  configured provider; never upload media.
- ASC-04: Support plans containing multiple `mute_range` and `retime_range`
  operations.
- ASC-05: Validate the complete plan before application and display a readable
  preview of every operation.
- ASC-06: Apply operations from the end of the timeline toward the beginning so
  earlier frame coordinates remain stable.
- ASC-07: Apply the entire plan as one undoable Kdenlive action.
- ASC-08: Allow cancellation while transcribing or waiting for the provider.
- ASC-09: Reject overlapping, out-of-bounds, structurally unsupported, or
  unrepresentable operations before changing the timeline.

## Acceptance criteria

- A preset can fill the instruction field without preventing manual edits.
- Off-topic speech ranges can be disabled on audio clips while video remains.
- Silent ranges longer than two seconds can be shortened to 0.5 seconds while
  preserving audio pitch.
- A mixed plan previews and applies several operations with one Undo restoring
  the prior timeline.
- A missing Whisper setup produces actionable guidance and no timeline change.
