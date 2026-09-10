# Project State

**Updated:** 2026-09-10
**Branch:** `feature/ai-editor-openrouter`

## Decisions

- OpenRouter is the first provider; the wire format remains OpenAI-compatible.
- AI responses are treated as untrusted data and parsed into a closed operation set.
- Plans are previewed before explicit application.
- Timeline changes must be atomic and compatible with Kdenlive undo/redo.
- Frame positions are canonical inside the plan; timecodes are converted using the project profile.

## Current

- Fork and local partial clone created.
- Upstream remote points to `KDE/kdenlive`.
- Brownfield mapping, specification, design, and task plan are committed.
- Typed `retime_range` edit-plan parser and contract tests are committed and pushed.
- Next task after build verification: T3 retime preflight.

## Deferred Ideas

- Direct provider integrations other than OpenRouter.
- Speech-to-text-driven silence removal.
- Vision analysis of thumbnails or proxy media.
- MCP/REST control of a running Kdenlive instance.

## Blockers

- This Windows machine has no CMake, C++ compiler, Qt, KDE Frameworks, or KDE Craft environment, so the new C++ tests cannot yet run locally.

