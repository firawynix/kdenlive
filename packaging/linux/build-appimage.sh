#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
set -euo pipefail

SOURCE_DIR=${SOURCE_DIR:-$(cd "$(dirname "$0")/../.." && pwd)}
BUILD_DIR=${BUILD_DIR:-$SOURCE_DIR/build-linux}
APPDIR=${APPDIR:-$SOURCE_DIR/dist/linux/AppDir}
OUTPUT_DIR=${OUTPUT_DIR:-$SOURCE_DIR/dist/linux}
JOBS=${JOBS:-$(nproc)}
REUSE_BUILD=${REUSE_BUILD:-0}
export APPIMAGE_EXTRACT_AND_RUN=1
# O binutils embarcado pelo linuxdeploy continuous ainda não entende seções
# RELR produzidas por distribuições rolling; o AppImage continua comprimido.
export NO_STRIP=1

if [[ "$REUSE_BUILD" != 1 ]]; then
  rm -rf -- "$BUILD_DIR" "$APPDIR"
fi
mkdir -p "$BUILD_DIR" "$APPDIR" "$OUTPUT_DIR"

if [[ "$REUSE_BUILD" != 1 ]]; then
  cmake -S "$SOURCE_DIR" -B "$BUILD_DIR" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DRELEASE_BUILD=ON \
    -DBUILD_TESTING=OFF \
    -DBUILD_QCH=OFF
  cmake --build "$BUILD_DIR" --parallel "$JOBS"
  DESTDIR="$APPDIR" cmake --install "$BUILD_DIR"
else
  test -x "$APPDIR/usr/bin/kdenlive"
fi

install -Dm755 /usr/bin/melt "$APPDIR/usr/bin/melt"
install -Dm755 /usr/bin/ffmpeg "$APPDIR/usr/bin/ffmpeg"
cp -a /usr/lib/mlt-7 "$APPDIR/usr/lib/"
cp -a /usr/share/mlt-7 "$APPDIR/usr/share/"
if [[ -d /usr/lib/frei0r-1 ]]; then cp -a /usr/lib/frei0r-1 "$APPDIR/usr/lib/"; fi
if command -v secret-tool >/dev/null; then install -Dm755 "$(command -v secret-tool)" "$APPDIR/usr/bin/secret-tool"; fi

TOOLS="$SOURCE_DIR/.cache/linuxdeploy"
mkdir -p "$TOOLS"
curl -fsSL -o "$TOOLS/linuxdeploy" https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
curl -fsSL -o "$TOOLS/linuxdeploy-plugin-qt" https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage
chmod +x "$TOOLS/linuxdeploy" "$TOOLS/linuxdeploy-plugin-qt"

libraries=()
while IFS= read -r library; do libraries+=(--library "$library"); done < <(find /usr/lib/mlt-7 /usr/lib/frei0r-1 -type f -name '*.so*' 2>/dev/null)
for pattern in libmovit.so libexif.so librnnoise.so librtaudio.so libsox_ng.so; do
  library=$(find /usr/lib -type f -name "$pattern.*" -print -quit 2>/dev/null || true)
  if [[ -n "$library" ]]; then libraries+=(--library "$library"); fi
done
export EXTRA_QT_PLUGINS="iconengines;imageformats;platforminputcontexts;platforms;styles;wayland-decoration-client;wayland-graphics-integration-client;wayland-shell-integration"
export QML_SOURCES_PATHS="$SOURCE_DIR/src"
export OUTPUT="$OUTPUT_DIR/Firawynix-Kdenlive-26.11.70-firaw.5-x86_64.AppImage"
install -Dm644 "$SOURCE_DIR/data/icons/256-apps-kdenlive.png" "$OUTPUT_DIR/kdenlive.png"
"$TOOLS/linuxdeploy" --appimage-extract-and-run \
  --appdir "$APPDIR" \
  --executable "$APPDIR/usr/bin/kdenlive" \
  --executable "$APPDIR/usr/bin/kdenlive_render" \
  --desktop-file "$APPDIR/usr/share/applications/org.kde.kdenlive.desktop" \
  --icon-file "$OUTPUT_DIR/kdenlive.png" \
  "${libraries[@]}" \
  --plugin qt \
  --output appimage

sha256sum "$OUTPUT" > "$OUTPUT.sha256"
