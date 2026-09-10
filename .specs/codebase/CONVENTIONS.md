# Code Conventions

## Naming

- Classes: PascalCase (`TimelineModel`, `ProviderModel`).
- Methods and variables: camelCase (`requestClipTimeWarp`, `pitchCompensate`).
- Members: `m_` prefix (`m_model`, `m_networkManager`).
- Files: lowercase, normally matching the contained class.

## Organization

- Headers use `#pragma once` and SPDX headers.
- Implementation includes the matching header first, followed by project and framework headers.
- Source lists are maintained in module `CMakeLists.txt` files.
- User-visible strings use KDE localization helpers.

## Error Handling

- Recoverable UI errors use signals or `pCore->displayMessage()`.
- Mutation functions return success/failure and roll back composed local operations on failure.
- Assertions protect internal invariants; external data must be validated without assertions.

## Tests

- Tests use descriptive `TEST_CASE`, `SECTION`, `REQUIRE`, and `REQUIRE_FALSE` statements.
- Timeline tests build an in-memory document and assert `checkConsistency()` around mutations.

