# AI Editing Assistant

This fork adds a review-first AI workflow to Kdenlive. An external model turns
a natural-language instruction into a typed JSON edit plan; Kdenlive validates
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

The assistant also accepts a key directly in the dock. **Save securely** stores
it in Windows Credential Manager under a provider-specific target. The input is
masked, the secret is never copied into Kdenlive settings, projects, logs, or
the repository, and **Remove saved key** deletes only that secure-store entry.
Environment variables remain a compatible fallback. **Test connection** uses a
read-only authenticated provider endpoint and does not request a model
completion.

## Performance budget

The **Performance** section has a 10–100% best-effort resource budget and
defaults to 80%. It determines the automatic CPU thread count and memory ceiling
for the local audio-export and Whisper child processes. An advanced CPU-thread
override is available and zero means automatic. Settings persist between runs.

On Windows the selected CPU count is enforced with process affinity and the
memory ceiling with a child-process job object where Windows permits it. Whisper
also receives PyTorch/OpenMP thread limits; CUDA runs receive a per-process GPU
memory fraction. This is a safe resource budget, not a promise that CPU, GPU,
and RAM will all remain exactly at the selected percentage.

This workstation has an AMD Radeon RX 9060 XT. The installed OpenAI Whisper
PyTorch runtime is CPU-only and does not support that GPU on Windows, so the UI
honestly exposes CPU processing. NVIDIA CUDA is offered only when compatible
hardware is detected.

## Fork identity

The user-facing product name is **Firawynix - Kdenlive**. The stable internal
application identifier remains `kdenlive` so existing settings, projects, file
associations, and plugins continue working. The About data clearly identifies
this as an independently maintained fork and preserves Kdenlive attribution,
copyright notices, homepage, authors, and GPL licensing.

## Ready prompts

The **Ready prompt** selector fills the instruction field and leaves it
editable. It includes:

- Clean work meeting: mute off-topic dialogue and compress silent gaps.
- Mute off-topic dialogue.
- Compress silent gaps longer than two seconds to 0.5 seconds.
- Exact-duration speed change.

The first three automatically enable local audio analysis. Custom instructions
can enable or disable it with the checkbox.

## Semantic cleanup test

1. Start `C:\CraftRoot\bin\kdenlive.exe` and open a project containing an
   audio/video clip on the timeline.
2. Open **View → AI Editing Assistant**.
3. Choose **Clean work meeting** under **Ready prompt**. The instruction remains
   editable and local audio analysis is selected automatically.
4. Choose the provider/model and press **Generate plan**. Kdenlive exports the
   current timeline audio to a temporary local WAV and transcribes it locally
   with Whisper. For a long video this can take several minutes on CPU.
5. Review every proposed mute or speed operation. The timeline has not changed
   yet.
6. Press **Apply**, then use Undo once to restore the complete previous
   timeline.

This machine is configured with the multilingual Whisper `base` model,
Portuguese language, CPU processing, and FP16 disabled.

## Exact-duration acceptance test

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

- Without local audio analysis, only the instruction, project FPS, and total
  timeline frame count are sent.
- With local audio analysis, media stays on the computer. Only the timestamped
  transcript text is additionally sent to the selected provider.
- Provider responses are untrusted. A versioned, closed parser rejects invalid
  or unsupported plans before any timeline API is called.
- Timeline preflight rejects cases that could partially mutate or desynchronize
  unsupported structures.
- Cancellation, timeout, network failure, provider HTTP failure, and invalid
  output leave the timeline unchanged.

## Current scope and extension contract

Version 1 supports up to 256 non-overlapping `retime_range` and `mute_range`
operations. Plans are applied from the end toward the beginning as one undoable
action. Range expansion, subtitles during retime, compositions, mixes, locked
content, internal clip boundaries, and unrelated overlaps remain intentionally
unsupported.

Every future edit type must add all five pieces: JSON schema, typed parser,
human-readable preview, preflight/executor, and automated tests. This keeps the
provider replaceable and prevents an AI response from receiving direct control
of Kdenlive internals.

## Developer verification

From the KDE Craft environment, run:

```powershell
ctest --test-dir C:\_\3377f5a\build -R "^(aieditorconfigurationtest|aieditorplannertest|aiproviderclienttest|retimerangeexecutortest|aieditorsemanticexecutortest)$" --output-on-failure
```

The acceptance test uses frames 250–3250 at 25 fps (120 seconds) and targets
1000 frames (40 seconds), verifying the 3x multiplier and an 80-second ripple.
