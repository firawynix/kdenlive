# Firawynix Linux distribution

The Firawynix fork is built natively on Linux with `packaging/linux/build-appimage.sh`.
The script expects a current Arch Linux environment because this branch requires Qt
6.10+, KDE Frameworks 6.21+ and MLT 7.38+. It installs the fork into an AppDir and
uses linuxdeploy with its Qt plugin, explicitly carrying MLT/Frei0r modules, Melt,
FFmpeg and `secret-tool`.

On Linux, AI provider credentials use the desktop Secret Service through
`secret-tool`. Environment variables remain the fallback when Secret Service is
not installed or no keyring is unlocked.

The release artifact is named
`Firawynix-Kdenlive-26.11.70-firaw.5-x86_64.AppImage`, accompanied by a SHA-256
file. The Center catalog must pin its exact size and digest.
