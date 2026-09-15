#!/usr/bin/env bash
# Build reprodutível do AppImage sobre a base mínima oficialmente suportada.
set -euo pipefail

export DEBIAN_FRONTEND=noninteractive
export APPIMAGE_EXTRACT_AND_RUN=1
JOBS=${JOBS:-$(nproc)}
SOURCE_DIR=${SOURCE_DIR:-/work/kdenlive}
OUTPUT_DIR=${OUTPUT_DIR:-/output}

sed -i 's/^Types: deb$/Types: deb deb-src/' /etc/apt/sources.list.d/ubuntu.sources
apt-get update
apt-get build-dep -y kdenlive mlt
apt-get install -y --no-install-recommends \
  ca-certificates curl file git ninja-build libsecret-tools libflite1 libspeechd2 \
  patchelf xdg-utils

rm -rf /tmp/mlt
git clone --depth 1 --branch v7.38.0 --recurse-submodules \
  https://github.com/mltframework/mlt.git /tmp/mlt
cmake -S /tmp/mlt -B /tmp/mlt/build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr/local \
  -DMOD_QT=OFF \
  -DMOD_QT6=ON \
  -DMOD_GLAXNIMATE=OFF \
  -DMOD_GLAXNIMATE_QT6=ON
cmake --build /tmp/mlt/build --parallel "$JOBS"
cmake --install /tmp/mlt/build
ldconfig

JOBS="$JOBS" SOURCE_DIR="$SOURCE_DIR" OUTPUT_DIR="$OUTPUT_DIR" \
  "$SOURCE_DIR/packaging/linux/build-appimage.sh"
