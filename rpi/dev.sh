#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
image_name="${RPI_DEV_IMAGE:-psi-rpi-dev:bookworm}"
arm_image_name="${RPI_DEV_ARM64_IMAGE:-psi-rpi-dev:bookworm-arm64}"

run_container() {
    docker run --rm \
        --user "$(id -u):$(id -g)" \
        --env HOME=/tmp \
        --volume "$repo_root:/workspace" \
        --workdir /workspace \
        "$image_name" "$@"
}

run_arm_container() {
    docker run --rm \
        --platform linux/arm64 \
        --user "$(id -u):$(id -g)" \
        --env HOME=/tmp \
        --volume "$repo_root:/workspace" \
        --workdir /workspace \
        "$arm_image_name" "$@"
}

case "${1:-}" in
    image)
        docker build --file "$repo_root/rpi/docker/Dockerfile" \
            --tag "$image_name" "$repo_root/rpi"
        ;;
    image-arm64)
        docker buildx build --platform linux/arm64 --load \
            --file "$repo_root/rpi/docker/Dockerfile" \
            --tag "$arm_image_name" "$repo_root/rpi"
        ;;
    shell)
        docker run --rm --interactive --tty \
            --user "$(id -u):$(id -g)" \
            --env HOME=/tmp \
            --env 'PS1=rpi-dev:\w\$ ' \
            --volume "$repo_root:/workspace" \
            --workdir /workspace \
            "$image_name" bash --noprofile --norc
        ;;
    shell-arm64)
        docker run --rm --interactive --tty \
            --platform linux/arm64 \
            --user "$(id -u):$(id -g)" \
            --env HOME=/tmp \
            --env 'PS1=rpi-arm64:\w\$ ' \
            --volume "$repo_root:/workspace" \
            --workdir /workspace \
            "$arm_image_name" bash --noprofile --norc
        ;;
    configure)
        run_container cmake --fresh -S rpi -B rpi/build -DCMAKE_BUILD_TYPE=Debug
        ;;
    build)
        run_container cmake --build rpi/build --parallel
        ;;
    test)
        run_container ctest --test-dir rpi/build --output-on-failure
        ;;
    coverage)
        run_container cmake -S rpi -B rpi/build-coverage \
            -DCMAKE_BUILD_TYPE=Debug -DPSI_ENABLE_COVERAGE=ON
        run_container cmake --build rpi/build-coverage --parallel
        run_container lcov --zerocounters --directory rpi/build-coverage
        run_container ctest --test-dir rpi/build-coverage --output-on-failure
        run_container lcov --capture --directory rpi/build-coverage \
            --rc lcov_branch_coverage=1 \
            --output-file rpi/build-coverage/coverage.raw.info
        run_container lcov --extract rpi/build-coverage/coverage.raw.info \
            '/workspace/rpi/*' --rc lcov_branch_coverage=1 \
            --output-file rpi/build-coverage/coverage.project.info
        run_container lcov --remove rpi/build-coverage/coverage.project.info \
            '/workspace/rpi/tests/*' --rc lcov_branch_coverage=1 \
            --output-file rpi/build-coverage/coverage.info
        run_container genhtml rpi/build-coverage/coverage.info \
            --branch-coverage --output-directory rpi/build-coverage/html
        run_container lcov --summary rpi/build-coverage/coverage.info \
            --rc lcov_branch_coverage=1
        ;;
    configure-arm64)
        run_arm_container cmake --fresh -S rpi -B rpi/build-arm64 -DCMAKE_BUILD_TYPE=Release
        ;;
    build-arm64)
        run_arm_container cmake --build rpi/build-arm64 --parallel
        ;;
    test-arm64)
        run_arm_container ctest --test-dir rpi/build-arm64 --output-on-failure
        ;;
    *)
        cat <<'EOF'
Usage: ./rpi/dev.sh {image|shell|configure|build|test|coverage|image-arm64|shell-arm64|configure-arm64|build-arm64|test-arm64}

Build the native image once with 'image', then configure and build locally.
The arm64 commands build a separate image and executable for the 64-bit Pi.
Set RPI_DEV_IMAGE or RPI_DEV_ARM64_IMAGE to use different local image tags.
EOF
        exit 2
        ;;
esac
