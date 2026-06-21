FROM alpine AS build

RUN apk update && apk add --no-cache \
    build-base \
    cmake \
    libsodium-dev \
    meson \
    ninja \
    git \
    pkgconf \
    protobuf-c-dev \
    sdl2-dev \
    sdl2_image-dev \
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
    libsodium \
    sdl2 \
    sdl2_image \
    sdl2_ttf \
    protobuf-c \
    libstdc++

# Copy installed files from build stage
COPY --from=build /tmp/dealer_install/ /

# Point the server at the installed data directory so it isn't confused by
# Docker creating /data as an empty mount-point directory.  Users can still
# override this with DEALERSCHOICE_DATADIR in EXTRA_ARGS.
ENV DEALERSCHOICE_DATADIR=/usr/share/dealers-choice

# Run as a non-root user (uid/gid 1000) so a compromised parser is contained to
# an unprivileged account, not container-root. The binaries bind only high ports
# (>1024), so no extra capabilities are needed. Bind-mounted host dirs
# (DC_OUTPUT_DIR / DC_REGISTRY_DIR) must be writable by this uid; the compose
# `user:` knob (DC_UID/DC_GID) lets operators match their host owner.
RUN addgroup -g 1000 dc && adduser -D -u 1000 -G dc dc \
    && mkdir -p /dc_config /dc_output /dc_registry \
    && chown dc:dc /dc_config /dc_output /dc_registry
USER dc
