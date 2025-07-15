# Building

You can see how it's built and which dependencies are needed by looking at
[the
workflows](https://github.com/Dealer-s-Choice/dealers_choice/tree/trunk/.github/workflows)
or the
[Dockerfile](https://github.com/Dealer-s-Choice/dealers_choice/blob/trunk/docker/Dockerfile)
that builds on Alpine Linux. There is also a [script that installs
dependencies](https://github.com/Dealer-s-Choice/dealers_choice/blob/trunk/.github/scripts/install_ubuntu_deps.sh)
on Ubuntu Linux. The FreeBSD dependencies are listed in
[cirrus.yml](https://github.com/Dealer-s-Choice/dealers_choice/tree/trunk/.cirrus.yml).

[Meson](https://mesonbuild.com/) is used for the build system.

```sh
meson setup _build
cd _build
ninja
```

To see various options that can be used for configuring and building, in the
build directory, use:

    meson configure

Periodically the build may break if changes were made to the subprojects. You
can update them by using:

    meson subprojects update

while in the repo root directory. If you encounter an error when updating,
it's usually simplest to remove the subproject directory associate with the
error (e.g. pokeval, deckhandler) and re-running `ninja` or `meson compile` in
the build directory.
