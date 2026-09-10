# AI Editor Design

**Spec:** `.specs/features/ai-editor/spec.md`
**Context:** `.specs/features/ai-editor/context.md`
**Status:** Implemented

## Architecture Overview

The provider and UI never give an external model mutation-capable timeline objects. OpenRouter, OpenAI, or Anthropic returns untrusted bytes; a strict parser converts them to a small typed model. Preview and execution consume only this typed model. See `architecture-flow.mmd` and its rendered SVG.

## Code Reuse Analysis

| Existing component | Location | Use |
| --- | --- | --- |
| Speed mutation | `TimelineModel::requestClipTimeWarp()` | Apply multiplier through an undo-composable overload |
| Boundary cuts | `TimelineFunctions::requestClipCut()` | Split clips at range boundaries |
| Range query | `TimelineModel::getItemsInRange()` | Resolve affected unlocked items |
| Ripple movement | `TimelineFunctions::removeSpace()` | Reuse or mirror collision-aware movement |
| Dock registration | `MainWindow::addDock()` | Host assistant widget |
| HTTP pattern | `ProviderModel::slotStartSearch()` | Async Qt network request lifecycle |

## Components

### EditPlan

- **Location:** `src/aieditor/editplan.hpp`, `src/aieditor/editplan.cpp`
- **Purpose:** Parse and validate the versioned provider response.
- **Interface:** `EditPlanParseResult parseEditPlan(const QByteArray &json)`.
- **Output:** Typed `RetimeRangeOperation` with canonical integer frames.

### RetimeRangeExecutor

- **Location:** `src/aieditor/retimerangeexecutor.*`
- **Purpose:** Preflight and execute a range retime as one composed undo transaction.
- **Dependencies:** `TimelineModel`, `TimelineFunctions`, group model, Core undo stack.

### AiProviderClient

- **Location:** `src/aieditor/aiproviderclient.*`
- **Purpose:** Send prompt plus minimal timeline context to OpenRouter, OpenAI,
  or Anthropic and return locally validated plan bytes.
- **Dependencies:** Qt Network; provider-specific environment key.

### AssistantDock

- **Location:** `src/aieditor/assistantdock.*`
- **Purpose:** Prompt, progress, plan preview, Apply, Cancel, and errors.
- **Reuses:** `MainWindow::addDock()` and KDE localization.

## Data Model

```text
EditPlan
  version: integer (= 1)
  operations: 1..64 typed operations

RetimeRangeOperation
  startFrame: integer >= 0
  endFrame: integer > startFrame
  targetDurationFrames: integer >= 1
  preservePitch: boolean (default true)
  speedMultiplier: (endFrame - startFrame) / targetDurationFrames
```

## Error Handling

| Scenario | Handling | User impact |
| --- | --- | --- |
| Invalid/oversized JSON | Reject before timeline lookup | Validation message |
| Unsupported operation/version | Reject entire plan | No partial plan |
| Unsafe compound range | Fail preflight | Timeline unchanged |
| Provider/network failure | Async error and retry option | Timeline unchanged |
| Mutation failure | Run composed undo | Original timeline restored |

## Technical Decisions

| Decision | Choice | Rationale |
| --- | --- | --- |
| Canonical time unit | Integer project frames | Avoid locale/timecode/FPS ambiguity |
| Interval convention | Half-open `[start, end)` | Duration is exactly `end - start` |
| Parser strictness | Closed version and operation set | Model output is untrusted |
| Initial secret source | Environment variable | Avoid insecure persistence before a credential-store design |
| Timeline API | Undo-composable overloads | One user-visible undo operation |
