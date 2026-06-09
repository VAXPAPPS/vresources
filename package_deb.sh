#!/bin/bash
set -e

# Make sure we build the latest version
echo "Building project..."
make clean
make

# Variables
PKG_NAME="vresources"
PKG_VERSION="0.1.0"
PKG_ARCH="amd64"
PKG_DIR="build/${PKG_NAME}_${PKG_VERSION}_${PKG_ARCH}"

echo "Creating package directory structure in ${PKG_DIR}..."
rm -rf "${PKG_DIR}"
mkdir -p "${PKG_DIR}/DEBIAN"
mkdir -p "${PKG_DIR}/usr/bin"
mkdir -p "${PKG_DIR}/usr/share/${PKG_NAME}"
mkdir -p "${PKG_DIR}/usr/share/applications"
mkdir -p "${PKG_DIR}/usr/share/icons/hicolor/scalable/apps"

# Copy binary and assets
cp vresources "${PKG_DIR}/usr/bin/"
cp style.css "${PKG_DIR}/usr/share/${PKG_NAME}/"
cp logo.svg "${PKG_DIR}/usr/share/icons/hicolor/scalable/apps/${PKG_NAME}.svg"
cp logo.svg "${PKG_DIR}/usr/share/${PKG_NAME}/"

# Create Debian control file
cat <<EOF > "${PKG_DIR}/DEBIAN/control"
Package: ${PKG_NAME}
Version: ${PKG_VERSION}
Section: utils
Priority: optional
Architecture: ${PKG_ARCH}
Maintainer: VAXP OS Suite <info@vaxp.org>
Depends: libgtk-4-1, libc6
Description: Premium System Resource Monitor for VAXP OS
 VResources is a dark glassmorphic system resources and processes monitor
 built in C using GTK4 and Cairo.
EOF

# Create desktop launcher file
cat <<EOF > "${PKG_DIR}/usr/share/applications/${PKG_NAME}.desktop"
[Desktop Entry]
Version=0.1.0
Type=Application
Name=VResources
Comment=System Resource Monitor
Exec=${PKG_NAME}
Icon=${PKG_NAME}
Categories=System;Monitor;Utility;
Terminal=false
StartupNotify=true
EOF

# Set proper permissions for Debian package build
chmod 755 "${PKG_DIR}/DEBIAN"
chmod 755 "${PKG_DIR}/usr/bin/vresources"
chmod 644 "${PKG_DIR}/usr/share/applications/${PKG_NAME}.desktop"
chmod 644 "${PKG_DIR}/usr/share/icons/hicolor/scalable/apps/${PKG_NAME}.svg"

echo "Building debian package..."
dpkg-deb --build "${PKG_DIR}"

echo "Debian package created successfully: build/${PKG_NAME}_${PKG_VERSION}_${PKG_ARCH}.deb"
