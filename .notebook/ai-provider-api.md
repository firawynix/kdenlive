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
