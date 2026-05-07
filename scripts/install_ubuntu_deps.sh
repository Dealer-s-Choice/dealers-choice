#!/bin/sh

set -e

sudo apt update
sudo apt-get install --no-install-recommends -y \
  cmake \
  gettext \
  meson \
  libprotobuf-c-dev \
  libsdl2-dev \
  libsdl2-image-dev \
  libsdl2-net-dev \
  libsdl2-ttf-dev \
  libsodium-dev
