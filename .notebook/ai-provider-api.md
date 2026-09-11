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
