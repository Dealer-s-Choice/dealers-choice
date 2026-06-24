#!/bin/sh

# Build a truly-portable "Anylinux" AppImage of Dealer's Choice using
# sharun + uruntime + DwarFS (pkgforge-dev method). Unlike the traditional
# linuxdeploy AppImage in ../appimage, this bundles the libc and dynamic
# linker, so the result runs on any Linux distro (musl, very old glibc, ...).
#
# Meant to run on an Arch base (see ../../.github/workflows/anylinux-appimage.yml).
# Build deps are installed by the workflow via pacman; this script builds DC
# from source, installs it to a DESTDIR, then bundles the installed binary.

set -eux

ARCH="$(uname -m)"

# VERSION is exported by CI (tag name or "snapshot"); fall back for local runs.
VERSION="${VERSION:-snapshot}"

# quick-sharun and get-debloated-pkgs are fetched from pkgforge-dev rather than
# vendored, so we always track the upstream bundling logic.
SHARUN="https://raw.githubusercontent.com/pkgforge-dev/Anylinux-AppImages/refs/heads/main/useful-tools/quick-sharun.sh"
DEBLOAT_PKGS="https://raw.githubusercontent.com/pkgforge-dev/Anylinux-AppImages/refs/heads/main/useful-tools/get-debloated-pkgs.sh"

# Source root is two levels up from this script (packaging/anylinux-appimage/).
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
SOURCE_ROOT=$(CDPATH= cd -- "$SCRIPT_DIR/../.." && pwd)
test -f "$SOURCE_ROOT/meson.build"

WORKSPACE="${WORKSPACE:-$SOURCE_ROOT}"
BUILD_DIR="$SOURCE_ROOT/_build_anylinux_appdir"
APPDIR="${APPDIR:-/tmp/dealers-choice-anylinux-AppDir}"
OUTPATH="$WORKSPACE/out"
WORKDIR="$SOURCE_ROOT/packaging/anylinux-appimage"

rm -rf "$APPDIR" "$BUILD_DIR"
mkdir -p "$APPDIR" "$OUTPATH"

# --- build DC from source and install into the AppDir under /usr -----------
meson setup "$BUILD_DIR" \
  -Dbuildtype=release \
  -Dstrip=true \
  -Db_sanitize=none \
  -Dprefix=/usr

ninja -C "$BUILD_DIR"
meson test -C "$BUILD_DIR" -v
meson install -C "$BUILD_DIR" --destdir="$APPDIR" --skip-subprojects

# --- place data and locale where the bundled binary will look --------------
# DC's data/locale lookup is env-var driven (DEALERSCHOICE_DATADIR /
# DEALERSCHOICE_LOCALEDIR), NOT XDG_DATA_DIRS aware (see README.md). The .env
# below points those at ${SHARUN_DIR}/share/..., which sharun expands to the
# AppImage mountpoint at runtime. quick-sharun's own datadir auto-detection
# only scans the host /usr, but DC is installed into the AppDir (not the host),
# so we copy the data and locale into the AppDir's top-level share/ ourselves
# and turn that auto-detection off.
mkdir -p "$APPDIR/share"
cp -a "$APPDIR/usr/share/dealers-choice" "$APPDIR/share/dealers-choice"
if [ -d "$APPDIR/usr/share/locale" ]; then
  cp -a "$APPDIR/usr/share/locale" "$APPDIR/share/locale"
fi

cat > "$APPDIR/.env" <<'EOF'
DEALERSCHOICE_DATADIR=${SHARUN_DIR}/share/dealers-choice
DEALERSCHOICE_LOCALEDIR=${SHARUN_DIR}/share/locale
EOF

# --- bundle with sharun and pack the AppImage ------------------------------
export APPDIR
export ICON="$SOURCE_ROOT/icons/dealers-choice_128x128.png"
export DESKTOP="$SOURCE_ROOT/dealers-choice.desktop"
export OUTPATH
export OUTNAME="dealers_choice-$VERSION-anylinux-$ARCH.AppImage"
export VERSION
export MAIN_BIN=dealers-choice
export DEPLOY_OPENGL=1
# We populate share/ ourselves above; quick-sharun's host-/usr datadir scan
# would find nothing for DC and is not needed.
export DEPLOY_DATADIR=0

# Update info for gh-releases-zsync. Tagged builds track the "latest" release,
# snapshot builds track the rolling "snapshot" prerelease (matches appimage.yml).
if [ "$VERSION" = "snapshot" ]; then
  TAG="snapshot"
else
  TAG="latest"
fi
export UPINFO="gh-releases-zsync|dealer-s-choice|dealers_choice|$TAG|*$ARCH.AppImage.zsync"

cd "$WORKDIR"

# Replace the stock Arch mesa/LLVM/icu with pkgforge-dev's debloated builds
# BEFORE bundling. DC needs accelerated rendering (SDL_RENDERER_ACCELERATED),
# so OpenGL stays, but stock mesa drags in a 164 MiB libLLVM, a 52 MiB
# libgallium and a 32 MiB libicudata. --add-common (implies --add-mesa) swaps
# in the nano mesa/LLVM and the icu stub, which quick-sharun then bundles
# instead. This is what keeps the AppImage small.
wget --retry-connrefused --tries=30 "$DEBLOAT_PKGS" -O "$WORKDIR/get-debloated-pkgs"
chmod +x "$WORKDIR/get-debloated-pkgs"
./get-debloated-pkgs --add-common --prefer-nano

wget --retry-connrefused --tries=30 "$SHARUN" -O "$WORKDIR/quick-sharun"
chmod +x "$WORKDIR/quick-sharun"

# Deploy the installed binary, then turn the AppDir into a DwarFS AppImage.
./quick-sharun "$APPDIR/usr/bin/dealers-choice"
./quick-sharun --make-appimage

ls -lh "$OUTPATH"
