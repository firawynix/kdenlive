# AI Editor Context

**Gathered:** 2026-09-10
**Spec:** `.specs/features/ai-editor/spec.md`
**Status:** Approved through the user's confirmation of the proposed MVP plan.

## Feature Boundary

The first vertical slice accepts a natural-language exact-duration retime request, obtains a structured plan through OpenRouter, previews it, and applies it safely through Kdenlive timeline APIs.

## Implementation Decisions

### Interaction

- Use a docked “AI Assistant” experience inside Kdenlive.
- Always show a plan preview with explicit Apply and Cancel actions.
- Report unsupported timeline situations instead of applying a partial edit.

### Retime Behavior

- Interpret intervals as `[start, end)` in canonical project frames.
- Preserve audio pitch by default.
- Preserve linked audio/video synchronization.
- Close the saved duration using ripple movement after the edited range.

### Provider and Privacy

- OpenRouter is the first provider.
- Send timeline metadata and the user's prompt, not source media, by default.
- Use an environment variable for the first API-key integration; persistent secure storage is deferred.

### Agent's Discretion

- Internal class and file names that follow Kdenlive conventions.
- Exact preview wording and non-destructive error presentation.
- Which unsupported compound-edit cases are rejected in the first executor version.

## Specific References

- Primary example: “from time X to Y, accelerate it so the result is 40 seconds.”
- Long-term intent: support any edit through an explicitly implemented operation vocabulary.

## Deferred Ideas

- Transcription, silence removal, visual media analysis, and generative media.

