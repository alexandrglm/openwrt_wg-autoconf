#!/bin/bash

set -e

CACHE_DIR="/home/alexander/Desktop/repo/ALPINE-SDK-DOCKER"
mkdir -p "$CACHE_DIR/apk-cache" "$CACHE_DIR/distfiles"


RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

log() { echo -e "${BLUE}[INFO]${NC} $1"; }
success() { echo -e "${GREEN}[OK]${NC} $1"; }
error() { echo -e "${RED}[ERROR]${NC} $1"; exit 1; }
warning() { echo -e "${YELLOW}[WARN]${NC} $1"; }
input_prompt() { echo -e "${CYAN}>>${NC} $1"; }

el_puto_slash() {
    echo "$1" | sed 's:/*$::'
}

clear
echo ""
echo "===[ Docker Alpine SDK Build v1.0 ]=================="
echo ""

BUILD_CONF_GLOBAL="./abuild-docker.conf"

if [ -f "$BUILD_CONF_GLOBAL" ]; then
    log "Found global abuild-docker.conf"
    source "$BUILD_CONF_GLOBAL"
    BUILD_DIR="$STORED_BUILD_DIR"
    DOCKER_IMAGE="$STORED_DOCKER_IMAGE"
    PACKAGER_PRIVKEY="$STORED_PACKAGER_PRIVKEY"
    PACKAGER_PUBKEY="$STORED_PACKAGER_PUBKEY"
    success "Configuration loaded automatically"
    echo ""
else
    input_prompt "Enter absolute path to build directory (where APKBUILD is):"
    read -r BUILD_DIR
    BUILD_DIR=$(el_puto_slash "$BUILD_DIR")

    if [ ! -f "$BUILD_DIR/APKBUILD" ]; then
        error "APKBUILD not found at: $BUILD_DIR/APKBUILD"
    fi

    if [ ! -d "$BUILD_DIR/source" ]; then
        error "source directory not found at: $BUILD_DIR/source"
    fi

    success "Build directory: $BUILD_DIR"
    echo ""

    input_prompt "Enter Docker image (default: alpine:latest):"
    read -r DOCKER_IMAGE
    DOCKER_IMAGE=${DOCKER_IMAGE:-alpine:latest}

    input_prompt "Enter absolute path to RSA private key:"
    read -r PACKAGER_PRIVKEY
    PACKAGER_PRIVKEY=$(el_puto_slash "$PACKAGER_PRIVKEY")

    if [ ! -f "$PACKAGER_PRIVKEY" ]; then
        error "Private key not found: $PACKAGER_PRIVKEY"
    fi

    input_prompt "Enter absolute path to RSA public key:"
    read -r PACKAGER_PUBKEY
    PACKAGER_PUBKEY=$(el_puto_slash "$PACKAGER_PUBKEY")

    if [ ! -f "$PACKAGER_PUBKEY" ]; then
        error "Public key not found: $PACKAGER_PUBKEY"
    fi

    echo ""
    echo "===[ Configuration Summary ]====================="
    echo "Build Dir:      $BUILD_DIR"
    echo "Docker Image:   $DOCKER_IMAGE"
    echo "Private Key:    $PACKAGER_PRIVKEY"
    echo "Public Key:     $PACKAGER_PUBKEY"
    echo "=================================================="
    echo ""

    printf "Save configuration? (Y/n): "
    read -r confirm
    case "$confirm" in
        n|N) ;;
        *)
            cat > "$BUILD_CONF_GLOBAL" << EOF
export STORED_BUILD_DIR="$BUILD_DIR"
export STORED_DOCKER_IMAGE="$DOCKER_IMAGE"
export STORED_PACKAGER_PRIVKEY="$PACKAGER_PRIVKEY"
export STORED_PACKAGER_PUBKEY="$PACKAGER_PUBKEY"
EOF
            success "Configuration saved to: $BUILD_CONF_GLOBAL"
            echo ""
            ;;
    esac
fi

success "APKBUILD found"
echo ""

log "Reading APKBUILD..."
PKG_NAME=$(grep "^pkgname=" "$BUILD_DIR/APKBUILD" | head -1 | cut -d= -f2 | tr -d '"'"'")
PKG_VERSION=$(grep "^pkgver=" "$BUILD_DIR/APKBUILD" | head -1 | cut -d= -f2 | tr -d '"'"'")
PKG_REL=$(grep "^pkgrel=" "$BUILD_DIR/APKBUILD" | head -1 | cut -d= -f2 | tr -d '"'"'")
PKG_DESC=$(grep "^pkgdesc=" "$BUILD_DIR/APKBUILD" | head -1 | cut -d= -f2 | tr -d '"'"'")
PKG_URL=$(grep "^url=" "$BUILD_DIR/APKBUILD" | head -1 | cut -d= -f2 | tr -d '"'"'")
PKG_ARCH=$(grep "^arch=" "$BUILD_DIR/APKBUILD" | head -1 | cut -d= -f2 | tr -d '"'"'")
LICENSE=$(grep "^license=" "$BUILD_DIR/APKBUILD" | head -1 | cut -d= -f2 | tr -d '"'"'")
DEPENDS=$(grep "^depends=" "$BUILD_DIR/APKBUILD" | head -1 | cut -d= -f2 | tr -d '"'"'")
MAINTAINER=$(head -1 "$BUILD_DIR/APKBUILD" | sed 's/# Maintainer: //')

success "Package: $PKG_NAME v$PKG_VERSION-r$PKG_REL"
success "Architecture: $PKG_ARCH"
success "Maintainer: $MAINTAINER"
echo ""

log "Cleaning source binary..."
if [ -f "$BUILD_DIR/source/usr/bin/wg-autoconf.source" ]; then
    cp "$BUILD_DIR/source/usr/bin/wg-autoconf.source" "$BUILD_DIR/source/usr/bin/wg-autoconf.clean"
    sed -i '/^[[:space:]]*$/d' "$BUILD_DIR/source/usr/bin/wg-autoconf.clean"
    sed -i '/^[[:space:]]*#/ { /^[[:space:]]*# wg-autoconf/!d }' "$BUILD_DIR/source/usr/bin/wg-autoconf.clean"
    success "Source cleaned"
else
    warning "wg-autoconf.source not found, skipping clean"
fi
echo ""

log "Creating source.tar.gz..."
cd "$BUILD_DIR"
tar czf source.tar.gz source/
success "source.tar.gz created"
ls -lh source.tar.gz
echo ""

log "Building APK with Docker..."
docker run --rm \
  -v "$BUILD_DIR:/src" \
  -v "$PACKAGER_PRIVKEY:/home/builder/.abuild/wg-autoconf-100r6-DEBUG.rsa:ro" \
  -v "$PACKAGER_PUBKEY:/home/builder/.abuild/wg-autoconf-100r6-DEBUG.rsa.pub:ro" \
  -v "$CACHE_DIR/apk-cache:/var/cache/apk" \
  -v "$CACHE_DIR/distfiles:/home/builder/.abuild/distfiles" \
  -w /src \
  "$DOCKER_IMAGE" \
  sh -c "
    apk add abuild alpine-sdk
    adduser -D -s /bin/sh builder
    addgroup builder abuild
    chown -R builder:builder /src
    mkdir -p /home/builder/.abuild
    cat > /home/builder/.abuild/abuild.conf << 'CONFEOF'
PACKAGER_PRIVKEY=/home/builder/.abuild/wg-autoconf-100r6-DEBUG.rsa
PACKAGER_PUBKEY=/home/builder/.abuild/wg-autoconf-100r6-DEBUG.rsa.pub
DISTFILES=/home/builder/.abuild/distfiles
CONFEOF
    chown -R builder:builder /home/builder/.abuild

    su builder -c '
      cd /src
      abuild checksum
      abuild -P /src/packages -r
    '
  "

success "Build completed"
echo ""

log "APK files generated:"
find "$BUILD_DIR/packages" -name "*.apk" -type f -exec ls -lh {} \;

echo ""
echo "===[ Build Complete ]================================"
echo ""
