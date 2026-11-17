# Simulator Raspberry Pi pentru serverul DoIP/UDS.
#
# Etapa 1 (build): compilează serverul C++ din ./rpi și rulează testele GoogleTest.
# Etapa 2 (runtime): un Ubuntu minimal care rulează doar executabilul, ca pe Pi.

ARG UBUNTU_VERSION=24.04

# ---------------------------------------------------------------------------
FROM ubuntu:${UBUNTU_VERSION} AS build

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        ninja-build \
        libssl-dev \
        libgtest-dev \
        libgmock-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY rpi/ ./rpi/

# RUN_TESTS=0 sare peste teste pentru un build mai rapid.
ARG RUN_TESTS=1
RUN cmake -S rpi -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --parallel \
    && if [ "$RUN_TESTS" = "1" ]; then ctest --test-dir build --output-on-failure; fi

# ---------------------------------------------------------------------------
FROM ubuntu:${UBUNTU_VERSION} AS runtime

ENV DEBIAN_FRONTEND=noninteractive
# Doar biblioteca OpenSSL de runtime (libcrypto). Pe Ubuntu 24.04 pachetul se numește libssl3t64.
RUN apt-get update \
    && (apt-get install -y --no-install-recommends libssl3t64 \
        || apt-get install -y --no-install-recommends libssl3) \
    && rm -rf /var/lib/apt/lists/*

# Utilizatorul "pi" imită contul de pe Raspberry Pi. Imaginea de 24.04 are deja
# utilizatorul "ubuntu" cu UID 1000, așa că îl redenumim în loc să creăm unul nou.
RUN if id ubuntu >/dev/null 2>&1; then \
        usermod -l pi -d /home/pi -m ubuntu && groupmod -n pi ubuntu; \
    else \
        useradd -m -u 1000 -s /bin/bash pi; \
    fi \
    && mkdir -p /home/pi/UpdMgr/image \
    && chown -R pi:pi /home/pi

COPY --from=build /src/build/rpi_doip_server /usr/local/bin/rpi_doip_server
COPY docker/entrypoint.sh /usr/local/bin/entrypoint.sh
RUN chmod 0755 /usr/local/bin/entrypoint.sh

USER pi
ENV HOME=/home/pi
WORKDIR /home/pi

# Portul DoIP standard (TCP).
EXPOSE 13400/tcp

ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]
