# AI provider API

The AI editor uses a provider-neutral Qt network client with three adapters:

- OpenRouter and OpenAI use the Chat Completions-compatible envelope and
  `response_format.type = json_schema`.
- Anthropic uses Messages with `output_config.format.type = json_schema`.
- Each response is extracted into the same plan JSON and passed through the
  strict local v1 parser before it can reach the UI.

Keys are read from `OPENROUTER_API_KEY`, `OPENAI_API_KEY`, or
`ANTHROPIC_API_KEY`. They are never written to project files or included in
logs. Requests contain the user's instruction, FPS, and total timeline frame
count—not media files or clip contents.

The shared completion classifier keeps cancellation separate from timeout,
network errors, HTTP errors, and invalid/unsafe plan payloads. The live request
has a 60-second transfer timeout and can be aborted by the user.
