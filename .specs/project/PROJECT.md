# Kdenlive AI Editor

**Vision:** Extend Kdenlive with a safe, provider-neutral assistant that turns natural-language requests into reviewable, undoable timeline edits.
**For:** Video editors who know the result they want but do not want to perform every mechanical timeline operation manually.
**Solves:** Repetitive editing work such as compressing a time range to a precise duration while preserving project integrity.

## Goals

- Convert a Portuguese or English instruction into a schema-validated edit plan.
- Preview every plan before execution and apply it as one undoable transaction.
- Deliver a first end-to-end `retime_range` operation with deterministic frame math.
- Keep media local unless the user explicitly opts into sending derived media to a provider.

## Tech Stack

- C++ and Qt 6.10+
- KDE Frameworks 6.21+
- MLT 7.38+
- CMake and Catch2-style tests already bundled by Kdenlive
- OpenRouter through its OpenAI-compatible HTTPS API

## Scope

**v1 includes:**

- An AI assistant dock in Kdenlive.
- OpenRouter model and endpoint configuration.
- A strict, versioned JSON edit-plan contract.
- Plan preview, explicit Apply, validation, and actionable errors.
- Retime a bounded timeline range to an exact target duration.
- Linked audio/video handling, pitch preservation, ripple movement, and one-step undo.

**Explicitly out of scope:**

- Letting a model edit `.kdenlive` XML directly.
- Uploading full source media by default.
- Generative video/image features.
- Claiming support for every Kdenlive operation in the first release.

## Constraints

- GPL licensing and existing KDE/Kdenlive coding conventions must be preserved.
- Windows builds use KDE Craft; the feature must remain cross-platform.
- Model output is untrusted input and must be validated before it reaches timeline APIs.
- The upstream `master` branch remains untouched; development occurs on `feature/ai-editor-openrouter`.

