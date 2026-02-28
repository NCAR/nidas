# Debian 12 (Bookworm) native amd64 + one selectable cross target
# Build examples:
#   docker build -t xdev:arm64 --build-arg HOST_ARCH=arm64 .
#   docker build -t xdev:armhf --build-arg HOST_ARCH=armhf .
#   docker build -t xdev:i386  --build-arg HOST_ARCH=i386  .
#   docker build -t xdev:amd64 --build-arg HOST_ARCH=amd64 .
#
# Or with podman, using the tag name conventions in cnidas.sh:
#
#   podman build -t nidas-build-debian-arm64:bookworm --build-arg HOST_ARCH=arm64 -f Dockerfile
#

FROM debian:12-slim

# Ensure all packages are up-to-date to reduce vulnerabilities
RUN apt-get update && apt-get upgrade -y && rm -rf /var/lib/apt/lists/*

ENV DEBIAN_FRONTEND=noninteractive

# ---- Select cross target (or amd64 native-only) ----
ARG HOST_ARCH=arm64
ENV HOST_ARCH=${HOST_ARCH}


# ---- Base (native) toolchain, build tools, Python, Boost headers, Flex/Bison ----
RUN apt-get update \
  && apt-get install -y --no-install-recommends \
      ca-certificates gnupg dirmngr wget curl git \
      build-essential pkg-config ninja-build make cmake scons \
      gdb gdb-multiarch file patchelf rsync gawk reprepro \
      python3 python3-pip python3-venv python3-setuptools python3-wheel \
      flex bison libfl-dev \
      libboost-dev \
      libboost-system-dev libboost-filesystem-dev libboost-program-options-dev \
      libboost-thread-dev libboost-chrono-dev libboost-regex-dev \
       # Debian packaging
      devscripts quilt debhelper dpkg-dev fakeroot lintian dh-make equivs \
  && rm -rf /var/lib/apt/lists/*

# ---- Install cross toolchain + per-arch dev libraries if HOST_ARCH != amd64 ----
RUN set -eu; \
  case "$HOST_ARCH" in \
    arm64) DEBARCH=arm64; TRIPLE=aarch64-linux-gnu;  LIBDIR=aarch64-linux-gnu ;; \
    armhf) DEBARCH=armhf; TRIPLE=arm-linux-gnueabihf; LIBDIR=arm-linux-gnueabihf ;; \
    i386)  DEBARCH=i386;  TRIPLE=i686-linux-gnu;      LIBDIR=i386-linux-gnu ;; \
    amd64) DEBARCH="";    TRIPLE="";                  LIBDIR="";; \
    *) echo "Unsupported HOST_ARCH: $HOST_ARCH (use arm64|armhf|i386|amd64)"; exit 1;; \
  esac; \
  if [ -n "$DEBARCH" ]; then \
    echo "Installing cross toolchain for $HOST_ARCH ($TRIPLE)"; \
    dpkg --add-architecture "$DEBARCH"; \
    apt-get update; \
    apt-get install -y --no-install-recommends \
      "gcc-${TRIPLE}" "g++-${TRIPLE}" \
      "libc6-${DEBARCH}-cross" "libc6-dev-${DEBARCH}-cross" \
      "libboost-system-dev:${DEBARCH}" \
      "libboost-filesystem-dev:${DEBARCH}" \
      "libboost-program-options-dev:${DEBARCH}" \
      "libboost-thread-dev:${DEBARCH}" \
      "libboost-chrono-dev:${DEBARCH}" \
      "libboost-regex-dev:${DEBARCH}" \
      "libfl-dev:${DEBARCH}" \
      "libboost-serialization-dev:${DEBARCH}" \
      "libboost-iostreams-dev:${DEBARCH}" ; \
    rm -rf /var/lib/apt/lists/* ; \
  fi

# ---- Workspace + ccache ----
 RUN mkdir -p /workspace


#custom additions below
RUN dpkg --add-architecture ${HOST_ARCH} && \
    apt-get -y update && \
    apt-get -y --no-install-recommends \
    -o Dpkg::Options::="--force-overwrite" install \
    libxerces-c-dev libjsoncpp-dev libxml2 \
    libxerces-c-dev:${HOST_ARCH} \
    libcap-dev:${HOST_ARCH} libxerces-c-dev:${HOST_ARCH} libbluetooth-dev:${HOST_ARCH} \
    libnetcdf-dev:${HOST_ARCH} libjsoncpp-dev:${HOST_ARCH} libftdi1-dev:${HOST_ARCH} \
    crossbuild-essential-${HOST_ARCH} libbz2-dev:${HOST_ARCH} libgsl0-dev:${HOST_ARCH} \
    libcap-dev:${HOST_ARCH} libxerces-c-dev:${HOST_ARCH} libbluetooth-dev:${HOST_ARCH} \
    libnetcdf-dev:${HOST_ARCH}


RUN mkdir -p /third-party
WORKDIR /third-party


# Local packages
RUN mkdir -p ublox
COPY ./scripts/docker/build-ublox.sh ublox
RUN cd ublox && ./build-ublox.sh

RUN mkdir -p xmlrpc-build
COPY ./scripts/docker/build-xmlrpc.sh xmlrpc-build
RUN cd xmlrpc-build && ./build-xmlrpc.sh ${HOST_ARCH}

RUN mkdir -p ~/.scons/site_scons && \
    git -C ~/.scons/site_scons clone https://github.com/ncar/eol_scons

WORKDIR /workspace


# ---- Default: print versions and open a shell ----
CMD bash -lc '\
  echo "HOST_ARCH=${HOST_ARCH}"; \
  echo "Native GCC:"; gcc -v || true; \
  echo "CMake:"; cmake --version; echo "SCons:"; scons --version; \
  echo "Python:"; python3 --version; \
  if [ "${HOST_ARCH}" != "amd64" ]; then \
    case "${HOST_ARCH}" in \
      arm64) T=aarch64-linux-gnu ;; armhf) T=arm-linux-gnueabihf ;; i386) T=i686-linux-gnu ;; esac; \
    echo "Cross GCC (${HOST_ARCH}):"; ${T}-g++ -v || true; \
    echo "Toolchain file: /opt/toolchains/${T}.cmake"; \
  fi; \
  exec bash'
