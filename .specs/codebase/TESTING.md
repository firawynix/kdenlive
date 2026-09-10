# Testing Infrastructure

## Frameworks

- Catch-style C++ tests are registered individually by `tests/CMakeLists.txt`.
- Each test executable links the full `kdenliveLib` and shares `tests/TestMain.cpp`.
- Appium tests cover GUI behavior separately.

## Relevant Existing Coverage

- `tests/timewarptest.cpp`: speed factor, duration changes, undo/redo, and one-frame lower bound.
- `tests/trimmingtest.cpp`: boundary cuts and resizes.
- `tests/spacertest.cpp`: ripple/space behavior.
- `tests/groupstest.cpp`: linked/grouped item behavior.

## Execution

- Configure with `BUILD_TESTING=ON`.
- Build the desired test target, then run it directly or through CTest.
- A complete build requires Qt 6.10+, KF 6.21+, MLT 7.38+, FFmpeg, and OpenTimelineIO.

## Limitation

- The current Windows machine has not yet been confirmed to contain the KDE Craft dependency environment, so source-level checks may precede compiled tests.

