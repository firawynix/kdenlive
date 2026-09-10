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
