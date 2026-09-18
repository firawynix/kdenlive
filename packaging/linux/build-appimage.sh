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

install -Dm755 "$(command -v melt)" "$APPDIR/usr/bin/melt"
install -Dm755 "$(command -v ffmpeg)" "$APPDIR/usr/bin/ffmpeg"
MLT_LIB_DIR=${MLT_LIB_DIR:-$(find /usr/local/lib /usr/lib -type d -name mlt-7 -print -quit 2>/dev/null)}
MLT_SHARE_DIR=${MLT_SHARE_DIR:-$(find /usr/local/share /usr/share -type d -name mlt-7 -print -quit 2>/dev/null)}
FREI0R_LIB_DIR=${FREI0R_LIB_DIR:-$(find /usr/local/lib /usr/lib -type d -name frei0r-1 -print -quit 2>/dev/null || true)}
test -n "$MLT_LIB_DIR" && test -n "$MLT_SHARE_DIR"
cp -a "$MLT_LIB_DIR" "$APPDIR/usr/lib/"
cp -a "$MLT_SHARE_DIR" "$APPDIR/usr/share/"
if [[ -n "$FREI0R_LIB_DIR" ]]; then cp -a "$FREI0R_LIB_DIR" "$APPDIR/usr/lib/"; fi
if command -v secret-tool >/dev/null; then install -Dm755 "$(command -v secret-tool)" "$APPDIR/usr/bin/secret-tool"; fi

TOOLS="$SOURCE_DIR/.cache/linuxdeploy"
mkdir -p "$TOOLS"
curl -fsSL -o "$TOOLS/linuxdeploy" https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
curl -fsSL -o "$TOOLS/linuxdeploy-plugin-qt" https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage
chmod +x "$TOOLS/linuxdeploy" "$TOOLS/linuxdeploy-plugin-qt"

libraries=()
while IFS= read -r library; do libraries+=(--library "$library"); done < <(find "$MLT_LIB_DIR" "$FREI0R_LIB_DIR" -type f -name '*.so*' 2>/dev/null)
for pattern in libmovit.so libexif.so librnnoise.so librtaudio.so libsox_ng.so libjack.so libasound.so libusb-1.0.so; do
  library=$(find /usr/lib -type f -name "$pattern.*" -print -quit 2>/dev/null || true)
  if [[ -n "$library" ]]; then libraries+=(--library "$library"); fi
done
# linuxdeploy copia os arquivos reais destas bibliotecas, mas em algumas bases
# omite os links de SONAME que o FFmpeg abre em tempo de execução.
for soname in libjack.so.0 libasound.so.2 libusb-1.0.so.0 \
  libOpenGL.so.0 libGLX.so.0 libGLdispatch.so.0 libharfbuzz.so.0 \
  libfribidi.so.0; do
  library=$(ldconfig -p | awk -v soname="$soname" '$1 == soname { print $NF; exit }')
  if [[ -n "$library" ]]; then
    real_library=$(readlink -f "$library")
    install -Dm644 "$real_library" "$APPDIR/usr/lib/$(basename "$real_library")"
    ln -sfn "$(basename "$real_library")" "$APPDIR/usr/lib/$soname"
  fi
done
export EXTRA_QT_PLUGINS="iconengines;imageformats;platforminputcontexts;platforms;styles;wayland-decoration-client;wayland-graphics-integration-client;wayland-shell-integration"
export QML_SOURCES_PATHS="$SOURCE_DIR/src"
if [[ -x /usr/lib/qt6/bin/qmake ]]; then export QMAKE=/usr/lib/qt6/bin/qmake; fi
export OUTPUT="$OUTPUT_DIR/Firawynix-Kdenlive-26.11.70-firaw.7-x86_64.AppImage"
install -Dm644 "$SOURCE_DIR/data/icons/256-apps-kdenlive.png" "$OUTPUT_DIR/kdenlive.png"
"$TOOLS/linuxdeploy" --appimage-extract-and-run \
  --appdir "$APPDIR" \
  --executable "$APPDIR/usr/bin/kdenlive" \
  --executable "$APPDIR/usr/bin/kdenlive_render" \
  --executable "$APPDIR/usr/bin/ffmpeg" \
  --executable "$APPDIR/usr/bin/melt" \
  --executable "$APPDIR/usr/bin/secret-tool" \
  --desktop-file "$APPDIR/usr/share/applications/org.kde.kdenlive.desktop" \
  --icon-file "$OUTPUT_DIR/kdenlive.png" \
  "${libraries[@]}" \
  --plugin qt \
  --output appimage

sha256sum "$OUTPUT" > "$OUTPUT.sha256"
