# AI semantic cleanup

## Flow

- `LocalTimelineTranscriber` serializes the active timeline, then uses the
  configured `melt` executable to render a temporary mono 16 kHz WAV.
- Kdenlive's existing `SpeechToTextWhisper` paths and virtual environment run
  `whispertosrt.py`; SRT timestamps are mapped to project frames.
- Media stays local. `AiProviderClient` receives only prompt, timing metadata,
  and `[start-end] text` transcript lines.
- Plans may contain up to 256 non-overlapping `mute_range` and `retime_range`
  operations.
- `EditPlanExecutor` applies them in descending start-frame order and composes
  one undo action.

## Windows setup

- Python environment: `C:\Users\Hugo\AppData\Local\kdenlive\venv`
- Model cache: `C:\Users\Hugo\.cache\whisper\base.pt`
- Settings: Whisper, model `base`, language Portuguese, device CPU, FP16 off.

## Constraints

- Mute ranges must isolate aligned linked clips and contain at least one
  audio-only clip.
- Retime retains its existing restrictions on subtitles, compositions, mixes,
  locked tracks, and clip boundaries.
- Do not add raw provider actions: each operation requires schema, parser,
  preview, preflight, executor, and tests.

## Next improvements

1. Add a configurable 10–100% performance budget (80% default) for local audio
   export and Whisper, with safe CPU-thread, GPU/device, and memory/cache
   controls. Treat the percentage as best effort rather than an exact hardware
   utilization guarantee.
2. Add phase-aware progress for every long-running local operation. The
   assistant must show the current phase, percentage, elapsed time, and rolling
   estimated time remaining for audio preparation and Whisper transcription.
   Provider requests must show elapsed time and an indeterminate state when the
   provider cannot report measurable progress.
3. Keep Cancel available throughout every phase and guarantee that cancellation
   leaves the timeline unchanged and removes temporary files.
4. Add in-app credential management for OpenRouter, direct OpenAI, and direct
   Anthropic Claude. Store secrets only through the operating system's secure
   credential facility, retain environment variables as a compatible fallback,
   and support test/remove actions without displaying or logging a key.
5. Brand the fork's own user-facing surfaces as `Firawynix - Kdenlive` while
   retaining clear upstream Kdenlive attribution, copyright notices, and GPL
   licensing. Audit window titles, About data, package metadata, documentation,
   and generated installers instead of performing an unsafe global rename.
