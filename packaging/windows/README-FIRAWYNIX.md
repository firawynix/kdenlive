# Firawynix Kdenlive for Windows

The Windows installer supports x64 only. It includes an offline KDE Craft
runtime, the Firawynix editor, local AI components, and a launcher. The
launcher checks the same update feed that Firawynix Center uses:

`https://jogos.firawynix.com.br/api/games/kdenlive/windows/atualizacao.json`

When a newer version is available, the launcher asks before downloading it to
`%LOCALAPPDATA%\Firawynix-Kdenlive\updates`. It accepts only the expected
HTTPS package URL, size, SHA-256 hash, and Firawynix signer certificate. If
the check fails or the user declines, the installed editor opens normally.
Store-packaged installations skip this external updater and rely on Store
updates.

The individual website installer and Firawynix Center use the same Windows
x64 package URL. Keep the package and catalog version in sync when publishing
an update. Do not publish an x86 package: this distribution has no x86 build.

To build the signed local package, compile the `kdenlive` target and its local
AI helper, then run `packaging/windows/build-installer.ps1` with the matching
KDE Craft archive and local model dependencies. The script writes the installer
and its hash to `dist/windows`. It requires the Firawynix signing certificate
in the current user's certificate store. Never commit the private key.

The project remains under the GPL and retains Kdenlive's credits. The
[upstream project](https://github.com/KDE/kdenlive) and
[Firawynix fork](https://github.com/firawynix/kdenlive) publish their source.

## Public, verifiable build

The `Build Windows installer` workflow builds the fork on a clean GitHub
Actions Windows runner with KDE Craft. It pins the blueprint to the triggering
commit, compiles from source, and publishes an unsigned installer artifact.
Its `provenance.json` identifies the repository, commit, workflow run, version,
and SHA-256. Production signing through SignPath is a separate step; the
private signing key does not belong in this repository or on the runner.
