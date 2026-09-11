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
launcher, stages the KDE Craft `PortablePackager` output (not the application-only
`SevenZipPackager` output), replaces the upstream
editor binary, PT-BR catalog, and installed Firawynix splash QML with the current
build, then invokes Inno Setup. The script fails closed unless the staged `bin`
directory contains both the FFmpeg avcodec runtime and Qt6Core.
Generated stage, launcher, installer, checksum, and update metadata remain local
build artifacts under `dist/windows`.

The launcher checks the latest `firawynix/kdenlive` GitHub release. It accepts
only an installer asset named `Firawynix-Kdenlive-Setup*.exe` with a sibling
asset whose name is exactly `<installer>.sha256`. It verifies SHA-256 before
starting the silent installer, and opens the locally installed editor whenever
the update service is offline, unavailable, or skipped with `--no-update`.

The static demonstration site is in `website/` (`dist/index.html` is an exact
copy of `index.html`). The page credits and links both the Firawynix fork and the
KDE/Kdenlive upstream project; its sticky footer carries the download button
(`releases/latest/download/Firawynix-Kdenlive-Setup-x64.exe`), both repositories
and the support link (`firawynix.com.br/apoie?de=kdenlive`).

Hosting (see `website/deploy/README.md`): the container runs on `10.81.66.10`,
but the files live on `10.81.66.7` in `/opt/firawynix-kdenlive-site/`. The
`sync-standby` job mirrors `/opt/` with `rsync --delete` every 15 minutes, so a
folder created only on `.10` is wiped (that took the first publication down).
nginx answers `POST` with the page (`error_page 405 =200 $uri`): after the
Cloudflare challenge the browser comes back with a POST, and a static site
returned `405 Not Allowed`.

Every channel downloads the same release asset — there is no separate update
server: the site button, the launcher's own updater, and the Firawynix Center
(the Firawynix games/projects launcher). The Center checks size and SHA-256, so
its catalog entry holds the pinned URL of the latest tag plus that tag's hash,
refreshed every 15 minutes by `sync-kdenlive-center.sh` on `10.81.66.7` (source
in the `firawynix/firawynix-center` repository, `tools/`). A release therefore
needs the `<installer>.sha256` asset, and its hash must match GitHub's asset
digest, or the Center keeps the previous version. Center 1.1.3+ compares every
numeric group, so `26.11.70-firaw.4` is newer than `firaw.3`.

The startup window is implemented in `src/dialogs/Splash.qml`. Firawynix visual
identity is embedded through `src/icons.qrc`; the window keeps the KDE/Kdenlive
credit and offers separate support links for the original project and the fork.
It uses a dedicated Firawynix background asset while keeping the original
upstream Kdenlive background untouched in the repository.
