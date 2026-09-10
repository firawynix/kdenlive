# AI retime execution

## Stable integration points

- Use the six-argument `TimelineModel::requestClipTimeWarp` overload when
  composing undo. Its speed argument is a multiplier (`3.0` means 3x), unlike
  the controller-facing percentage overload.
- `TimelineFunctions::requestClipCut` cuts all leaves of an aligned group and
  rebuilds the group hierarchy on each side of the boundary.
- After shortening the isolated segment, close the delta with
  `TimelineFunctions::removeSpace(QPoint(newEnd, oldEnd), ..., allTracks,
  false)`. Use a local undo pair for ripple because `removeSpace` invokes its
  supplied undo itself on failure.
- Accumulate both boundary cuts, every linked timewarp, and the ripple move in
  one `Fun` pair, then call `pCore->pushUndo` once.

## Current safety boundary

The v1 executor refuses subtitles, compositions, same-track mixes, locked
content, internal clip boundaries, and overlapping clips that are not members
of the same aligned group. It currently supports shortening a range; expansion
needs an insert-space phase before timewarp.
