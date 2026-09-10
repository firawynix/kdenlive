# AI Editor Specification

## Problem Statement

Precise timeline edits often require several mechanical operations even when the intended result is simple to describe. The assistant must translate that intent into deterministic Kdenlive operations without giving an external model direct control over project files.

## Goals

- [ ] Turn natural-language requests into a versioned, validated edit plan.
- [ ] Preview and apply an exact-duration range retime as one undoable edit.
- [ ] Establish an operation registry that can safely grow to other edit types.

## Out of Scope

| Feature | Reason |
| --- | --- |
| Direct `.kdenlive` XML editing | Bypasses model invariants and undo/redo |
| Uploading full media automatically | Privacy, bandwidth, and cost risk |
| Every editing operation in v1 | Each operation needs explicit validation and tests |
| Generated video or images | Separate product capability |

## User Stories

### P1: Exact-duration range retime

**User Story:** As an editor, I want to say “from X to Y, make it N seconds” so that Kdenlive performs the required speed and ripple edits precisely.

**Acceptance Criteria:**

1. WHEN a plan contains valid frame boundaries and target duration THEN the system SHALL calculate the same speed multiplier on every run.
2. WHEN the requested range intersects supported unlocked clips THEN the system SHALL preserve relative timing and linked A/V synchronization.
3. WHEN the user applies a previewed plan THEN the system SHALL record one undoable operation.
4. WHEN the range cannot be edited safely THEN the system SHALL make no timeline change and report the reason.

**Independent Test:** A 120-second range converted to 40 seconds produces a 3× multiplier and a timeline shortened by 80 seconds, with undo restoring the original state.

### P1: Safe AI plan ingestion

**User Story:** As an editor, I want AI suggestions validated before execution so that malformed or surprising responses cannot corrupt my project.

**Acceptance Criteria:**

1. WHEN JSON is malformed, oversized, or has an unsupported version THEN the system SHALL reject it without accessing the timeline.
2. WHEN an operation name or required field is unsupported THEN the system SHALL reject the entire plan.
3. WHEN numeric frame values are negative, fractional, non-finite, out of range, or contradictory THEN the system SHALL return an actionable validation error.
4. WHEN a plan is valid THEN the system SHALL expose a typed operation rather than raw JSON to the executor.

**Independent Test:** Parser unit tests cover valid input and each rejection category without constructing an MLT project.

### P1: Review before Apply

**User Story:** As an editor, I want to see the proposed actions before they run so that I retain creative control.

**Acceptance Criteria:**

1. WHEN OpenRouter returns a valid plan THEN the assistant SHALL display a human-readable summary.
2. WHEN the user cancels THEN the system SHALL leave the timeline unchanged.
3. WHEN the user selects Apply THEN the validated plan SHALL be executed exactly once.

**Independent Test:** A valid plan appears in the dock and only changes the timeline after Apply.

### P2: Expandable editing vocabulary

**User Story:** As an editor, I want additional editing commands to use the same safe workflow.

**Acceptance Criteria:**

1. WHEN a new operation is added THEN it SHALL define its schema, preview description, validator, executor, and tests.

## Edge Cases

- Empty operation arrays and more than 64 operations are rejected.
- The plan payload is capped at 256 KiB.
- Frame intervals are half-open: `[start_frame, end_frame)`.
- Target duration must be at least one frame.
- Unknown fields may be preserved by providers but cannot change executor behavior.
- Locked tracks are not modified; a plan that would desynchronize them is rejected.
- Unsupported mixes, compositions, or subtitle transformations fail before mutation in the first executor version.

## Requirement Traceability

| Requirement ID | Story | Status |
| --- | --- | --- |
| AIE-01 | Safe AI plan ingestion | Implemented; build verification pending |
| AIE-02 | Deterministic retime math | Implemented; build verification pending |
| AIE-03 | Exact-duration range execution | Pending |
| AIE-04 | Atomic undo/redo | Pending |
| AIE-05 | Review before Apply | Pending |
| AIE-06 | OpenRouter structured response | Pending |
| AIE-07 | Expandable operation registry | Pending |

**Coverage:** 7 requirements, 7 mapped to tasks, 0 unmapped.

## Success Criteria

- [ ] A 120-second range targeting 40 seconds yields exactly 3× before frame rounding.
- [ ] Invalid model output cannot invoke timeline APIs.
- [ ] The complete P1 flow is previewable, cancellable, and undoable.
