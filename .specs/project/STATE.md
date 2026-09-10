# Project State

**Updated:** 2026-09-10
**Branch:** `feature/ai-editor-openrouter`

## Decisions

- OpenRouter, direct OpenAI, and direct Anthropic Claude are supported through
  one provider-neutral client.
- AI responses are treated as untrusted data and parsed into a closed operation set.
- Plans are previewed before explicit application.
- Timeline changes must be atomic and compatible with Kdenlive undo/redo.
- Frame positions are canonical inside the plan; timecodes are converted using the project profile.

## Current

- Fork and local partial clone created.
- Upstream remote points to `KDE/kdenlive`.
- Brownfield mapping, specification, design, and task plan are committed.
- Typed multi-operation `retime_range` / `mute_range` parser, atomic timeline
  executor, local Whisper transcription, three-provider HTTPS client, prompt
  presets, and review-first assistant dock are implemented.
- KDE Craft/MinGW environment is installed at `C:\CraftRoot` with Qt 6.11.1,
  KDE Frameworks 6.29.0, MLT 7.41.0, FFmpeg, CMake 4.1.4, Ninja 1.13.2,
  and GCC 14.2.0.
- The local fork builds successfully with `BUILD_TESTING=ON`. Parser, provider,
  and timeline executor test targets pass, including the 120s→40s acceptance
  case with linked A/V, ripple, one-step undo, and redo.
- The local machine has multilingual Whisper `base` installed and configured
  for Portuguese CPU transcription.
- Next task: add more operation types only when each receives a schema, typed
  parser, preview, safe executor, and automated tests.

## Deferred Ideas

- Configurable 10–100% performance budget, defaulting to 80%, for AI-local
  workloads with CPU-thread, GPU/device, and memory/cache controls.
- Progress UI for machine-intensive work: separate audio-export,
  transcription, and provider phases; show percentage where measurable,
  elapsed time, and a rolling ETA without blocking cancellation.
- Vision analysis of thumbnails or proxy media.
- MCP/REST control of a running Kdenlive instance.

## Blockers

- None for the current foundation work.
