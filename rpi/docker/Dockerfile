FROM debian:bookworm-slim

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        gdb \
        git \
        libgmock-dev \
        libgtest-dev \
        lcov \
        libssl-dev \
        make \
        ninja-build \
        pkg-config \
        python3 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace
CMD ["bash"]
