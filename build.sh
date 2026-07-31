#!/bin/sh
# Build the ACE OS ISO.
#
# Usage:
#   ./build.sh            # build inside a Docker container (recommended)
#   ./build.sh --native   # build directly on a Debian host (needs root + live-build)
#
# Output: ace-os-<date>-amd64.iso in the repository root.
set -e

cd "$(dirname "$0")"

build_native() {
    if [ "$(id -u)" -ne 0 ]; then
        echo "error: native builds must run as root (try: sudo $0 --native)" >&2
        exit 1
    fi
    command -v lb >/dev/null 2>&1 || {
        echo "error: live-build is not installed (apt install live-build)" >&2
        exit 1
    }
    lb clean
    lb config
    lb build
}

build_docker() {
    command -v docker >/dev/null 2>&1 || {
        echo "error: docker not found. Install Docker, or run on Debian with: sudo $0 --native" >&2
        exit 1
    }
    docker run --rm --privileged \
        -v "$(pwd)":/build -w /build \
        debian:trixie \
        sh -c "apt-get update && \
               apt-get install -y --no-install-recommends live-build ca-certificates && \
               lb clean && lb config && lb build"
}

case "${1:-}" in
    --native) build_native ;;
    *)        build_docker ;;
esac

if [ -f live-image-amd64.hybrid.iso ]; then
    iso="ace-os-$(date +%Y.%m.%d)-amd64.iso"
    mv live-image-amd64.hybrid.iso "$iso"
    echo ""
    echo "Done: $iso"
    echo "Flash it to a USB stick (8 GB or larger) with Rufus, balenaEtcher, or dd."
else
    echo "error: build finished but no ISO was produced — check build.log" >&2
    exit 1
fi
