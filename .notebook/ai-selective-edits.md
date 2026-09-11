# Selective AI edits and iterative requests

## Undo model

`EditPlanExecutor::apply` still pushes one aggregate undo command for the whole
plan, but every applied operation now owns a shared `AppliedOperationState`.
The aggregate command and the per-operation controls call the same state. This
makes each action idempotent: global Undo skips an operation that the user has
already undone individually, and global Redo restores every operation.

The dock stores the public `AppliedEditOperation` handles only while the same
timeline model is active. A selective toggle is also pushed onto Kdenlive's
normal undo stack, so Ctrl+Z can reverse that toggle.

## Iterative workflow

After a successful plan, the prompt stays available and the main action becomes
“Generate another adjustment”. A new request reads the currently modified
timeline, so follow-up instructions are planned against the result already on
screen. Applied operations are grouped by request number in the dock.

## Local AI setup

The Ollama setup action is state-aware. When Ollama is absent it explicitly
offers to install Ollama and download the hardware-recommended model. When the
runtime is present but the model is absent it offers only model preparation;
when both are ready it reports the ready model.
