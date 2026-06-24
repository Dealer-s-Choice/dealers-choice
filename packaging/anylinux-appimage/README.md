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
3. Downloads `quick-sharun` from pkgforge-dev (not vendored here).
4. Bundles the installed binary and packs the AppImage and `.zsync` file.

The AppImage and its `.zsync` file are written to `out/` at the repository
root, the same place the traditional AppImage uses.

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
