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
- KDE Craft/MinGW environment is installed at `C:\CraftRoot` with Qt 6.11.1,
  KDE Frameworks 6.29.0, MLT 7.41.0, FFmpeg, CMake 4.1.4, Ninja 1.13.2,
  and GCC 14.2.0.
- The local fork builds successfully with `BUILD_TESTING=ON`, and
  `aieditorplannertest` passes.
- Next task: T3 retime preflight.

## Deferred Ideas

- Direct provider integrations other than OpenRouter.
- Speech-to-text-driven silence removal.
- Vision analysis of thumbnails or proxy media.
- MCP/REST control of a running Kdenlive instance.

## Blockers

- None for the current foundation work.
