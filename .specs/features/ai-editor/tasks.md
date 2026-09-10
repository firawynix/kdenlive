# AI Editor Tasks

**Design:** `.specs/features/ai-editor/design.md`
**Status:** In Progress

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

### T2: Test the edit-plan contract ⚠️

**What:** Add parser contract tests to the existing Catch suite.
**Where:** `tests/aieditorplannertest.cpp`, `tests/CMakeLists.txt`.
**Depends on:** T1.
**Requirement:** AIE-01, AIE-02.

**Done when:**

- Tests cover the valid 120s→40s case at 25 fps and all specified rejection categories.
- Test target configures in a supported build environment.

**Current:** Test source is complete; execution awaits a Qt/KDE/MLT build environment.

### T3: Implement retime preflight

**What:** Resolve and validate affected timeline items without mutation.
**Where:** `src/aieditor/retimerangeexecutor.*`, corresponding tests.
**Depends on:** T2.
**Requirement:** AIE-03.

**Done when:** unsupported or desynchronizing ranges fail before mutation.

### T4: Execute boundary cuts and proportional retime

**What:** Apply the validated operation through undo-composable timeline APIs.
**Depends on:** T3.
**Requirement:** AIE-03, AIE-04.

**Done when:** linked A/V remains synchronized and mutation failure rolls back.

### T5: Ripple and one-step undo

**What:** Close or open the duration delta and expose one undo entry.
**Depends on:** T4.
**Requirement:** AIE-04.

**Done when:** undo and redo restore both exact timeline states.

### T6: Add OpenRouter client

**What:** Request a structured edit plan using minimal timeline context.
**Depends on:** T2.
**Requirement:** AIE-06.

**Done when:** success, timeout, cancellation, HTTP failure, and invalid response tests pass.

### T7: Add assistant dock and preview

**What:** Implement prompt, plan preview, Apply, Cancel, and error states.
**Depends on:** T5, T6.
**Requirement:** AIE-05.

**Done when:** no mutation occurs before Apply and the dock is registered in Kdenlive.

### T8: End-to-end acceptance and documentation

**What:** Validate the complete example and document setup/privacy behavior.
**Depends on:** T7.
**Requirement:** AIE-01 through AIE-07.

**Done when:** the 120s→40s scenario is previewable, applied once, and fully undoable.

## Tool Choice

- Code navigation: CodeNavi workflow and local source search.
- Diagram: Mermaid Studio validation/rendering.
- Provider reference: official OpenRouter documentation.
- Source control: Git and connected GitHub repository.
