# Firawynix Windows distribution

The Windows target keeps the internal CMake target name `kdenlive` but emits
`firawynix-kdenlive.exe`. `src/CMakeLists.txt` configures the Win32 version
resource from `data/windows/firawynix-kdenlive-version.rc.in`, while the normal
ECM app-icon pipeline embeds the Firawynix/Kdenlive icon generated in every
required size under `data/icons`.

`src/main.cpp` registers the executable-local `data/locale` directory for the
`kdenlive` translation domain on Windows. This is required because a Craft
installation may otherwise resolve an older global catalog before the fork's
updated Brazilian Portuguese catalog.

`packaging/windows/build-installer.ps1` publishes the self-contained .NET 8
launcher, stages the KDE Craft portable dependencies, replaces the upstream
editor binary, PT-BR catalog, and installed Firawynix splash QML with the current
build, then invokes Inno Setup.
Generated stage, launcher, installer, checksum, and update metadata remain local
build artifacts under `dist/windows`.

The launcher checks the latest `firawynix/kdenlive` GitHub release. It accepts
only an installer asset named `Firawynix-Kdenlive-Setup*.exe` with a sibling
asset whose name is exactly `<installer>.sha256`. It verifies SHA-256 before
starting the silent installer, and opens the locally installed editor whenever
the update service is offline, unavailable, or skipped with `--no-update`.

The static demonstration site is in `website/`. It uses its own Sites manifest
and build output for publication, while the page credits and links both the
Firawynix fork and the KDE/Kdenlive upstream project.

The startup window is implemented in `src/dialogs/Splash.qml`. Firawynix visual
identity is embedded through `src/icons.qrc`; the window keeps the KDE/Kdenlive
credit and offers separate support links for the original project and the fork.
It uses a dedicated Firawynix background asset while keeping the original
upstream Kdenlive background untouched in the repository.
