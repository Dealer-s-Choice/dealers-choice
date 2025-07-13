FROM alpine AS build

RUN apk update && apk add --no-cache \
    build-base \
    cmake \
    meson \
    ninja \
    git \
    pkgconf \
    protobuf-c-dev \
    sdl2-dev \
    sdl2_image-dev \
    sdl2_net-dev \
    sdl2_ttf-dev \
    musl-dev

# Copy source
WORKDIR /dealer
COPY . .

RUN meson setup builddir --buildtype=release -Db_sanitize=none --prefix=/usr && \
    meson compile -C builddir -v && \
    DESTDIR=/tmp/dealer_install meson install -C builddir

FROM alpine

# Install runtime dependencies
RUN apk add --no-cache \
    sdl2 \
    sdl2_image \
    sdl2_net \
    sdl2_ttf \
    protobuf-c \
    libstdc++

# Copy installed files from build stage
COPY --from=build /tmp/dealer_install/ /

CMD ["/bin/sh","-l"]
