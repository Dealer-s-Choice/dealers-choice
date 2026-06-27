#!/bin/sh

# Build a truly-portable AppImage of Dealer's Choice using sharun + uruntime +
# DwarFS (pkgforge-dev method). It bundles the libc and dynamic linker, so the
# result runs on any Linux distro (musl, very old glibc, ...).
#
# Meant to run on an Arch base (see ../../.github/workflows/appimage.yml).
# Build deps are installed by the workflow via pacman; this script builds DC
# from source, installs it into the system /usr, then bundles the installed
# binary with quick-sharun.

set -eux

ARCH="$(uname -m)"

# VERSION is exported by CI (tag name or "snapshot"); fall back for local runs.
VERSION="${VERSION:-snapshot}"

# quick-sharun and get-debloated-pkgs are fetched from pkgforge-dev rather than
# vendored, so we always track the upstream bundling logic.
SHARUN="https://raw.githubusercontent.com/pkgforge-dev/Anylinux-AppImages/refs/heads/main/useful-tools/quick-sharun.sh"
DEBLOAT_PKGS="https://raw.githubusercontent.com/pkgforge-dev/Anylinux-AppImages/refs/heads/main/useful-tools/get-debloated-pkgs.sh"

# Source root is two levels up from this script (packaging/appimage/).
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
SOURCE_ROOT=$(CDPATH= cd -- "$SCRIPT_DIR/../.." && pwd)
test -f "$SOURCE_ROOT/meson.build"

WORKSPACE="${WORKSPACE:-$SOURCE_ROOT}"
BUILD_DIR="$SOURCE_ROOT/_build_appdir"
APPDIR="${APPDIR:-/tmp/dealers-choice-AppDir}"
OUTPATH="$WORKSPACE/out"
WORKDIR="$SOURCE_ROOT/packaging/appimage"

rm -rf "$APPDIR" "$BUILD_DIR"
mkdir -p "$APPDIR" "$OUTPATH"

# --- build DC and install into the system /usr -----------------------------
# Install to the real /usr (no DESTDIR). quick-sharun then deploys DC the way
# it deploys any normal /usr-prefixed app: its datadir auto-detection scans the
# host /usr/share and bundles /usr/share/dealers-choice + /usr/share/locale,
# and its path patcher rewrites the binary's hardcoded /usr/share/... strings
# (equal-length, to /tmp/<token>) and symlinks the bundled data there at
# runtime (a plain symlink, not a bind-mount -- no FUSE/userns needed). DC
# bakes its lookup dirs in as /usr/share/dealers-choice and
# /usr/share/locale (DEALERSCHOICE_DATADIR/LOCALEDIR in meson.build), so this
# patching makes its compiled-in lookup resolve with no env seam or manual copy.
meson setup "$BUILD_DIR" \
  -Dbuildtype=release \
  -Dstrip=true \
  -Db_sanitize=none \
  -Dprefix=/usr

ninja -C "$BUILD_DIR"
meson test -C "$BUILD_DIR" -v
meson install -C "$BUILD_DIR" --skip-subprojects

# --- bundle with sharun and pack the AppImage ------------------------------
export APPDIR
export ICON="$SOURCE_ROOT/icons/dealers-choice_128x128.png"
export DESKTOP="$SOURCE_ROOT/dealers-choice.desktop"
export OUTPATH
export OUTNAME="dealers_choice-$VERSION-$ARCH.AppImage"
export VERSION
export DEPLOY_OPENGL=1

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

# Deploy the installed binary -- quick-sharun auto-bundles the data and locale
# and patches the hardcoded /usr paths -- then turn the AppDir into a DwarFS
# AppImage.
./quick-sharun /usr/bin/dealers-choice
./quick-sharun --make-appimage

ls -lh "$OUTPATH"
