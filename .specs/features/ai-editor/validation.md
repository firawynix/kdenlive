# AI Editor Validation

**Date:** 2026-09-10
**Scope:** T1 through T8

## Results

| Check | Result |
| --- | --- |
| Strict v1 plan parser added to `kdenliveLib` sources | PASS (compiled in `kdenliveLib`) |
| 120 s → 40 s at 25 fps asserts a 3× multiplier | PASS (executed) |
| Malformed, oversized, unsupported, and unsafe plans covered | PASS (executed) |
| Diff whitespace validation | PASS |
| Mermaid architecture rendering | PASS |
| Full Kdenlive build with tests enabled | PASS (KDE Craft/MinGW) |
| `aieditorplannertest` | PASS — 1/1 tests, 0 failures |
| `aiproviderclienttest` | PASS — 1/1 tests, 0 failures |
| `retimerangeexecutortest` | PASS — 1/1 tests, 0 failures |
| Installed executable matches the tested build | PASS |

## Code Quality

- Changes are isolated under `src/aieditor/` plus explicit Kdenlive UI,
  CMake, test, and documentation registration.
- Parser returns typed data and rejects the entire plan on the first invalid
  operation.
- OpenRouter, OpenAI, and Anthropic use provider-specific HTTPS and structured
  response formats; keys are loaded only from environment variables.
- AI generation only creates a preview. Timeline mutation starts after Apply.
- Range cuts, linked A/V retime, ripple, and rollback compose into one undo.
- No new runtime dependency was introduced.

## Verified Environment

- KDE Craft root: `C:\CraftRoot`
- ABI: `windows-gcc-x86_64`
- Qt: 6.11.1
- KDE Frameworks: 6.29.0
- MLT: 7.41.0
- GCC: 14.2.0
- CMake: 4.1.4
- Ninja: 1.13.2
- Build directory: `C:\_\3377f5a\build`
- Installed executable: `C:\CraftRoot\bin\kdenlive.exe`

## Acceptance Scenario

At 25 fps, frames 250–3250 describe a 120-second interval. Retiming it to 1000
frames produces exactly 40 seconds at 3x speed. The executor test verifies the
new duration, linked audio/video alignment, the 80-second ripple, one-step
undo, and redo.

## Deliberately Deferred

Version 1 does not expand ranges and rejects subtitles, compositions, mixes,
locked content, internal clip boundaries, and unrelated overlaps. Additional
edit types must use the same schema, parser, preview, preflight/executor, and
test contract.
