# Codebase Concerns

## High — Ambiguous speed units across overloads

- `TimelineModel::requestClipTimeWarp(..., Fun&, Fun&)` consumes a multiplier such as `1.2`.
- The four-argument invokable overload divides its `speed` parameter by 100 for inserted clips.
- An AI executor calling the wrong overload could produce a 100× error.
- Mitigation: keep plan speed math in one typed adapter and test the exact called overload.

## High — Multi-track range compression is a compound edit

- Boundary cuts, linked A/V groups, gaps, mixes, subtitles, locked tracks, collisions, and ripple movement interact.
- Existing building blocks are spread across `TimelineFunctions` and `TimelineModel`.
- Mitigation: validate the range first, compose one undo transaction, and initially reject unsupported mixes rather than partially modifying a project.

## Medium — Model output is external input

- JSON from a provider can be malformed, oversized, semantically invalid, or request unsupported operations.
- Mitigation: closed operation registry, version check, numeric bounds, request-size limit, and no direct XML access.

## Medium — Secret storage is not established

- The inspected source does not currently use KWallet or QtKeychain.
- Mitigation for the first integration: read the API key from process environment; design persistent secure storage separately.

## Medium — Windows build cost

- Official documentation recommends KDE Craft and MinGW on Windows.
- Mitigation: keep the first parser tests dependent only on QtCore and validate full integration in a Craft environment.

