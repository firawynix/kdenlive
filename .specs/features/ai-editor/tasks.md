# AI Editor Tasks

**Design:** `.specs/features/ai-editor/design.md`
**Status:** Complete

## Execution Plan

### Phase 1 — Foundation

T1 → T2 → T3

### Phase 2 — Timeline execution

T3 → T4 → T5

### Phase 3 — Provider and interface

T5 → T6 → T7 → T8

## Task Breakdown

### T1: Define and parse the edit-plan contract ✅

**What:** Add the typed v1 `retime_range` plan model and strict JSON parser.
**Where:** `src/aieditor/editplan.hpp`, `src/aieditor/editplan.cpp`, module CMake files.
**Depends on:** None.
**Requirement:** AIE-01, AIE-02.
**Tools:** local filesystem; Qt official documentation for JSON semantics.

**Done when:**

- Valid plans return typed operations and deterministic speed math.
- Invalid size, JSON, version, operation count/type, and numeric fields return actionable errors.
- Source is registered in `kdenliveLib`.

### T2: Test the edit-plan contract ✅

**What:** Add parser contract tests to the existing Catch suite.
**Where:** `tests/aieditorplannertest.cpp`, `tests/CMakeLists.txt`.
**Depends on:** T1.
**Requirement:** AIE-01, AIE-02.

**Done when:**

- Tests cover the valid 120s→40s case at 25 fps and all specified rejection categories.
- Test target configures in a supported build environment.

**Verified:** Built from the local fork with KDE Craft/MinGW and
`BUILD_TESTING=ON`; `ctest -R ^aieditorplannertest$ --output-on-failure`
passes (1/1).

### T3: Implement retime preflight ✅

**What:** Resolve and validate affected timeline items without mutation.
**Where:** `src/aieditor/retimerangeexecutor.*`, corresponding tests.
**Depends on:** T2.
**Requirement:** AIE-03.

**Done when:** unsupported or desynchronizing ranges fail before mutation.

**Verified:** The focused executor test accepts one continuous clip and rejects
empty ranges, internal clip boundaries, locked ripple content, subtitles,
compositions, mixes, and unrelated overlapping clips before mutation;
`ctest -R ^retimerangeexecutortest$ --output-on-failure` passes (1/1).

### T4: Execute boundary cuts and proportional retime ✅

**What:** Apply the validated operation through undo-composable timeline APIs.
**Depends on:** T3.
**Requirement:** AIE-03, AIE-04.

**Done when:** linked A/V remains synchronized and mutation failure rolls back.

**Verified:** The executor isolates both boundaries, applies the exact 3x speed
to a linked audio/video pair, preserves its group, and its composed undo/redo
restores both exact states in `retimerangeexecutortest`.

### T5: Ripple and one-step undo ✅

**What:** Close or open the duration delta and expose one undo entry.
**Depends on:** T4.
**Requirement:** AIE-04.

**Done when:** undo and redo restore both exact timeline states.

**Verified:** The retimed range closes its 80-frame delta across all unlocked
tracks, the following linked segments move from frame 140 to frame 60, and one
undo-stack entry restores cuts, speed, groups, and positions exactly.

### T6: Add OpenRouter, OpenAI, and Claude clients ✅

**What:** Request a structured edit plan from OpenRouter, OpenAI, or Anthropic
Claude using minimal timeline context.
**Depends on:** T2.
**Requirement:** AIE-06.

**Done when:** success, timeout, cancellation, HTTP failure, and invalid response tests pass.

**Verified:** `aiproviderclienttest` validates HTTPS endpoints, provider-specific
authentication and structured-output envelopes, valid responses from both API
families, missing credentials, timeout, cancellation, HTTP errors, and unsafe
plans (1/1 passing).

### T7: Add assistant dock and preview ✅

**What:** Implement prompt, plan preview, Apply, Cancel, and error states.
**Depends on:** T5, T6.
**Requirement:** AIE-05.

**Done when:** no mutation occurs before Apply and the dock is registered in Kdenlive.

**Verified:** The full `kdenlive.exe` target builds with the registered AI
Editing Assistant dock. Provider/model selection, credential state,
generate/cancel, human-readable preview, preflight, Apply, and Discard are
separate states; only Apply invokes the timeline executor.

### T8: End-to-end acceptance and documentation ✅

**What:** Validate the complete example and document setup/privacy behavior.
**Depends on:** T7.
**Requirement:** AIE-01 through AIE-07.

**Done when:** the 120s→40s scenario is previewable, applied once, and fully undoable.

**Verified:** The acceptance test uses the literal 25 fps interval from frame
250 to 3250 (120 seconds), produces a 3x retime to 1000 frames (40 seconds),
ripples following material by 80 seconds, and restores the original timeline
with one undo. Provider setup, manual testing, privacy, safety, and current
limitations are documented in `dev-docs/ai-editor.md`.

## Tool Choice

- Code navigation: CodeNavi workflow and local source search.
- Diagram: Mermaid Studio validation/rendering.
- Provider reference: official OpenRouter, OpenAI, and Anthropic documentation.
- Source control: Git and connected GitHub repository.
