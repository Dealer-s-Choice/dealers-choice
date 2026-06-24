# Anylinux AppImage

This directory builds a truly portable "Anylinux" AppImage of Dealer's Choice.

It uses the [pkgforge-dev](https://github.com/pkgforge-dev/Anylinux-AppImages)
method: `sharun` + `uruntime` + DwarFS. This bundles the C library and the
dynamic linker inside the AppImage. The result runs on any Linux distro,
including musl systems (Alpine) and very old glibc systems. The traditional
AppImage in `../appimage` (built with linuxdeploy) does not do this, so it
needs a recent enough glibc on the host.

## How to build

The build runs on an Arch Linux base. The GitHub Actions workflow
`../../.github/workflows/anylinux-appimage.yml` installs the build
dependencies with `pacman` and then runs `build-anylinux-appimage.sh`.

The script:

1. Builds Dealer's Choice from source with Meson.
2. Installs it into an `AppDir` under `/usr`.
3. Copies the installed game data and locale into the AppDir's top-level
   `share/` (see "Data file lookup" below for why).
4. Downloads `get-debloated-pkgs` from pkgforge-dev (not vendored here) and
   swaps the stock mesa/LLVM/icu for the smaller debloated builds (see "Size"
   below).
5. Downloads `quick-sharun` from pkgforge-dev (not vendored here).
6. Bundles the installed binary and packs the AppImage and `.zsync` file.

The AppImage and its `.zsync` file are written to `out/` at the repository
root, the same place the traditional AppImage uses.

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

`sharun` sets `XDG_DATA_DIRS` and leaves the working directory at the launch
location, so neither the compiled-in `/usr/share/...` path nor `../data` will
exist on the user's machine. Without help the bundled binary would not find
its data.

We do not patch the source for this. DC already has the environment-variable
seam we need. The build script writes a `.env` file next to `sharun` in the
AppDir:

```
DEALERSCHOICE_DATADIR=${SHARUN_DIR}/share/dealers-choice
DEALERSCHOICE_LOCALEDIR=${SHARUN_DIR}/share/locale
```

`sharun` expands `${SHARUN_DIR}` to the AppImage mount point at runtime. This
is the same approach the traditional `../appimage/AppRun` uses with its `$HERE`
variable, so behaviour matches between the two AppImage variants.

For this to work the data must actually live at `share/dealers-choice` inside
the AppDir. `quick-sharun` has its own data-directory deployment, but it only
scans the host's `/usr/share`, and DC is installed into the AppDir, not onto
the host. So the build script copies the data and locale from the AppDir's
`usr/share` into the AppDir's top-level `share/` itself, and sets
`DEPLOY_DATADIR=0` to turn off the host scan. Skipping this step produces an
AppImage that starts but then fails to find its fonts and config files.
