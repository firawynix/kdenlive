# AI provider API

The AI editor uses a provider-neutral Qt network client with four adapters:

- OpenRouter and OpenAI use the Chat Completions-compatible envelope and
  `response_format.type = json_schema`.
- Anthropic uses Messages with `output_config.format.type = json_schema`.
- Ollama uses the local `/api/chat` endpoint with the plan schema in `format`,
  streaming disabled, and a 30-minute local inference timeout.
- Each response is extracted into the same plan JSON and passed through the
  strict local v1 parser before it can reach the UI.

Keys are read from `OPENROUTER_API_KEY`, `OPENAI_API_KEY`, or
`ANTHROPIC_API_KEY`. They are never written to project files or included in
logs. Requests contain the user's instruction, FPS, and total timeline frame
count—not media files or clip contents.

Ollama requires no credential and is hard-coded to the loopback address
`127.0.0.1:11434`. `LocalAiManager` owns hardware recommendation, explicit
Windows runtime installation, local-server probing, and streamed model pulls.

The shared completion classifier keeps cancellation separate from timeout,
network errors, HTTP errors, and invalid/unsafe plan payloads. The live request
has a three-minute cloud transfer timeout and can be aborted by the user.

## Output-limit recovery

Transcript requests start at 4,000 characters per segment. A provider response
whose finish reason is `length` or `max_tokens` is classified separately from
ordinary provider failures. The assistant automatically halves the segment
budget down to 1,000 characters, rebuilds only the provider-analysis segments,
and retries without rerunning local Whisper transcription.

The chosen segment budget and reduction count are stored in the credential-free
session checkpoint. Legacy checkpoints with no completed provider segment adopt
the safer 4,000-character budget; legacy checkpoints already in progress retain
their former 16,000-character mapping so completed segment indexes remain valid.
Automatic subdivision is bounded to four reductions to prevent retry loops. If
the minimum still fails, the UI asks for a model with a larger output allowance.

## Provider handoff

`AiSessionStore::loadMostAdvancedCompatible()` chooses resume progress by the
latest completed timeline frame, not merely by segment number. A checkpoint may
be copied to another provider/model only when timeline fingerprint, normalized
prompt, FPS, duration, transcript, and chunk mapping are compatible. The source
checkpoint remains intact. `AssistantDock::updateProvider()` clears only the
in-memory preview when switching providers so an interrupted checkpoint is not
deleted; explicit Discard retains its destructive meaning for the active plan.

Before starting the expensive local audio export, the dock searches for a
unique saved request with the same normalized prompt, timeline duration, and
FPS. When exactly one timeline fingerprint matches, its embedded transcript is
reused and provider handoff begins immediately. If different timelines match
those coarse fields, transcription runs normally so separate media is not mixed.

For local chunk responses only, deterministic normalization swaps a strictly
reversed frame pair and removes zero-length operations before the complete plan
is parsed again by the same strict safety validator. Ollama reasoning output is
disabled for this schema-constrained classification task to reduce per-segment
latency; the final plan still requires local validation and explicit Apply.
Overlapping local mute ranges are merged. A retime range that crosses muted
speech, or a later retime that conflicts with an accepted one, is omitted as
ambiguous; the strict parser then validates the normalized fragment again.
The same normalization is repeated when saved fragments are combined, which
also handles cross-segment conflicts created before this recovery existed.
The strict combined-plan ceiling is 4,096 operations (still bounded by the
256-KiB payload limit), while each provider response remains schema-limited to
256. This supports long recordings without weakening the per-request boundary.

Before preview, each normalized operation is preflighted independently against
the active timeline. Operations that cannot preserve existing aligned groups,
clip boundaries, locks, mixes, or subtitles are omitted; compatible operations
remain reviewable and applicable as one undo action. The UI reports the skipped
count and first reason rather than weakening executor invariants.

Large plans keep per-operation undo/redo closures in vectors and expose one
iterative aggregate undo action. This avoids recursive closure depth growing
with every edit. The apply callback is throttled to roughly ten UI updates per
second, allowing the dock to show percentage and ETA while excluding user input
until the atomic application succeeds or rolls back.

## Prompt library and transcript suggestions

The assistant stores user-named prompts in the application's KConfig under the
`AiEditorPrompts` group. Names, prompt text, and the associated local-audio
analysis choice are persisted together. Built-in prompts cannot be deleted;
saved prompts can be updated by saving the same name or removed explicitly.

Transcript-driven prompt suggestions use a separate structured-output schema
in `aiproviderclient.cpp`. `AssistantDock` divides the cached local transcript
into larger analysis segments, collects suggestions from every segment, and
asks the selected provider to consolidate them into a short final list. The
suggestions are temporary until the user chooses Save prompt. This flow does
not create or apply timeline operations.

Only timestamped speech is available to this suggestion flow. The system prompt
therefore forbids claims about frozen frames, missing screen sharing, or other
visual states. Those require a future local frame-analysis pipeline rather than
guessing from silence.

## Local model recommendation

On Windows, `LocalAiManager::detectHardware()` uses DXGI to read the active
adapter name and dedicated video memory in addition to CPU threads and system
memory. The recommended Qwen model is selected from GPU memory when available,
with CPU/RAM fallbacks for integrated or unreported adapters. The existing
Prepare action installs Ollama through Windows Package Manager, starts its
loopback service, downloads the recommendation with progress/ETA, and verifies
the model without handling project media.

All AI-editor user-facing strings and executor/parser errors are translatable.
The fork's `po/pt_BR/kdenlive.po` includes the Brazilian Portuguese catalog for
the complete AI workflow.
