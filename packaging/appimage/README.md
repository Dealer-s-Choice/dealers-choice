# AppImage

This directory builds a truly portable AppImage of Dealer's Choice.

It uses the [pkgforge-dev](https://github.com/pkgforge-dev/Anylinux-AppImages)
method: `sharun` + `uruntime` + DwarFS. This bundles the C library and the
dynamic linker inside the AppImage. The result runs on any Linux distro,
including musl systems (Alpine) and very old glibc systems.

## How to build

The build runs on an Arch Linux base. The GitHub Actions workflow
`../../.github/workflows/appimage.yml` installs the build
dependencies with `pacman` and then runs `build-appimage.sh`.

The script:

1. Builds Dealer's Choice from source with Meson.
2. Installs it into the system `/usr` (see "Data file lookup" below).
3. Downloads `get-debloated-pkgs` from pkgforge-dev (not vendored here) and
   swaps the stock mesa/LLVM/icu for the smaller debloated builds (see "Size"
   below).
4. Downloads `quick-sharun` from pkgforge-dev (not vendored here).
5. Bundles the installed binary and packs the AppImage and `.zsync` file.

The AppImage and its `.zsync` file are written to `out/` at the repository
root.

## Size

DC needs accelerated rendering (`SDL_RENDERER_ACCELERATED` in `src/main.c`),
so OpenGL is bundled. Arch's stock mesa drags in a very large LLVM library and
a large `libicudata`. Before bundling, the build runs
`get-debloated-pkgs --add-common --prefer-nano`, which replaces those with
pkgforge-dev's debloated builds: an LLVM-free mesa and an `icu` stub. This
brings the AppImage from about 87 MiB down to about 31 MiB.

The remaining large item that is NOT needed by DC is the AV1 and JXL codec
stack (`libaom`, `libSvtAv1Enc`, `librav1e`, `libjxl`, `libdav1d`, about
23 MiB total). These are pulled in because Arch's `sdl2_image` package hard
depends on `libavif` and `libjxl` for AVIF and JXL image support. DC only
loads PNG assets, so it never uses them. There is no debloated `sdl2_image`
(or `libavif` / `libjxl`) in the pkgforge-dev repo at this time, and the Arch
dependency cannot be removed without rebuilding `sdl2_image` from source. They
are left in the bundle for now. If a leaner `sdl2_image` becomes available, or
if DC builds `sdl2_image` itself without AVIF/JXL, the AppImage could shrink by
roughly another 23 MiB.

## Data file lookup and relocatability

Dealer's Choice does not read `XDG_DATA_DIRS` to find its data files. It looks
for them in this order (see `get_data_dir()` in `src/util.c` and the locale
lookup in `src/main.c`):

1. The `DEALERSCHOICE_DATADIR` environment variable.
2. A relative `../data` directory.
3. The path compiled in at build time, for example
   `/usr/share/dealers-choice`.

This works with `sharun` without any source change, env-var seam, or manual
copy, as long as DC is installed into the real `/usr` before bundling (the
build script does a plain `meson install`, not a `DESTDIR` install). The reason
is how `quick-sharun` handles hardcoded paths:

- It scans the host `/usr/share`, finds `dealers-choice` (matched against the
  binary name) and `locale`, and bundles them into the AppDir.
- It then patches the binary in place, rewriting the literal `/usr/share` to
  `/tmp/<token>` (an equal-length string, so the patch fits), and records a
  path mapping.
- At runtime its `uruntime`/`bwrap` launcher bind-mounts the bundled data onto
  `/tmp/<token>`, so the binary's now-`/tmp/<token>/dealers-choice` path is a
  real directory.

DC bakes `DEALERSCHOICE_DATADIR` and `DEALERSCHOICE_LOCALEDIR` in as
`/usr/share/dealers-choice` and `/usr/share/locale` (`meson.build`), and
`get_data_dir()` only `stat()`s that compiled-in path. Because `quick-sharun`
makes the patched path resolve, lookup step 3 succeeds with no help from us.
(Step 1's env var is unset and step 2's `../data` does not exist at runtime, so
step 3 is the one that fires.)

If DC were installed with `--destdir=$APPDIR` instead, the host `/usr/share`
scan would find nothing and the data would not be bundled — that was the cause
of an earlier, more convoluted version of this script.
