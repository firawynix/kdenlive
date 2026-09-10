# AI Editing Assistant

This fork adds a review-first AI workflow to Kdenlive. An external model turns
a natural-language instruction into a small JSON edit plan; Kdenlive validates
and previews the plan locally, and only mutates the timeline after the user
presses **Apply**.

## Supported providers

| Provider | Environment variable | Default model (editable in the dock) |
| --- | --- | --- |
| OpenRouter | `OPENROUTER_API_KEY` | `openai/gpt-5-mini` |
| OpenAI | `OPENAI_API_KEY` | `gpt-5-mini` |
| Anthropic Claude | `ANTHROPIC_API_KEY` | `claude-sonnet-5` |

Model availability changes over time. Enter another model identifier in the
dock if the provider no longer offers the default.

On Windows, set one key in a PowerShell window, then completely close and
reopen Kdenlive. For example:

```powershell
setx OPENROUTER_API_KEY "your-key-here"
```

Use `OPENAI_API_KEY` or `ANTHROPIC_API_KEY` instead for the direct providers.
Never commit a key to this repository or put it in a Kdenlive project.

## Manual acceptance test

1. Start `C:\CraftRoot\bin\kdenlive.exe`.
2. Create a 25 fps project and place one audio/video clip across 00:10–02:10.
3. Avoid subtitles, compositions, same-track mixes, and locked tracks in or
   after this range for the first-version test.
4. Open **View → AI Editing Assistant**.
5. Choose the configured provider. The dock should report that its environment
   key was loaded.
6. Enter: `From 00:10 to 02:10, speed it up so it lasts exactly 40 seconds.`
7. Press **Generate plan**. Confirm that the preview says 3.000x, 120 seconds
   original duration, and 40 seconds new duration. The timeline must still be
   unchanged.
8. Press **Apply**. The range becomes 40 seconds, linked audio/video remain
   aligned, and later clips move left by 80 seconds.
9. Press Undo once. The original cuts, speeds, groups, and positions must all
   return. Redo once to apply the same result again.

## Safety and privacy

- Only the instruction, project FPS, and total timeline frame count are sent.
  Media and project files are not uploaded.
- Provider responses are untrusted. A versioned, closed parser rejects invalid
  or unsupported plans before any timeline API is called.
- Timeline preflight rejects cases that could partially mutate or desynchronize
  unsupported structures.
- Cancellation, timeout, network failure, provider HTTP failure, and invalid
  output leave the timeline unchanged.

## Current scope and extension contract

Version 1 applies one `retime_range` operation that shortens/speeds up a
continuous range. It intentionally refuses range expansion, subtitles,
compositions, mixes, locked content, internal clip boundaries, and unrelated
overlaps. This is not yet arbitrary video editing.

Every future edit type must add all five pieces: JSON schema, typed parser,
human-readable preview, preflight/executor, and automated tests. This keeps the
provider replaceable and prevents an AI response from receiving direct control
of Kdenlive internals.

## Developer verification

From the KDE Craft environment, run:

```powershell
ctest --test-dir C:\_\3377f5a\build -R "^(aieditorplannertest|aiproviderclienttest|retimerangeexecutortest)$" --output-on-failure
```

The acceptance test uses frames 250–3250 at 25 fps (120 seconds) and targets
1000 frames (40 seconds), verifying the 3x multiplier and an 80-second ripple.
