# Tech Stack

**Analyzed:** 2026-09-10

## Core

- Application: Kdenlive 26.11.70 development tree (`CMakeLists.txt`).
- Language: C++ with Qt types and QML for timeline/monitor views.
- Build: CMake 3.16+ with KDE Extra CMake Modules.
- UI: Qt 6.10+, KDE Frameworks 6.21+, KDDockWidgets 2.4+.
- Media: MLT 7.38+, FFmpeg, frei0r, LADSPA, SoX, libsamplerate.
- Interchange: OpenTimelineIO.

## Testing

- Unit/integration runner: bundled Catch implementation via `tests/TestMain.cpp`.
- GUI automation: `appiumtests/`.
- Optional fuzzing: `fuzzer/` with Clang.

## Development Tools

- Formatting: `.clang-format` and KDE git pre-commit hook.
- Static configuration: KConfig/KConfigXT.
- Localization: KDE `i18n` functions and gettext tooling.

