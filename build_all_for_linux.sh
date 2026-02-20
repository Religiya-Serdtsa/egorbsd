#!/bin/bash

# Exit immediately if a command exits with a non-zero status.
set -e

# --- Configuration ---
# Assuming the build will happen in the current directory (workspace root)
SRC_ROOT=$(pwd)
# Output directory for the ISO and other release artifacts
DIST_DIR="${SRC_ROOT}/dist"
# Kernel configuration name. Assumes a file like sys/ARCH/conf/EGORBSD exists.
KERN_CONF="EGORBSD" # User can change this in their kernel config file to enable EGOR_TTAK
KERN_ARCH=$(uname -m) # Detect current machine architecture
if [ "${KERN_ARCH}" = "x86_64" ]; then
    KERN_ARCH="amd64"
fi

# Number of parallel jobs for make
# On Linux, use nproc.
if command -v nproc > /dev/null; then
    MAKE_JOBS=$(nproc)
else
    MAKE_JOBS=4
fi

# Set MAKEOBJDIRPREFIX to a local directory if not already set
export MAKEOBJDIRPREFIX="${MAKEOBJDIRPREFIX:-${SRC_ROOT}/obj}"
mkdir -p "${MAKEOBJDIRPREFIX}"

# Use the make.py wrapper for building on Linux
MAKE_CMD="python3 ${SRC_ROOT}/tools/build/make.py"

echo "--- egorbsd Linux Build Script ---"
echo "Source root: ${SRC_ROOT}"
echo "Object directory: ${MAKEOBJDIRPREFIX}"
echo "Distribution output directory: ${DIST_DIR}"
echo "Kernel configuration: ${KERN_CONF}"
echo "Architecture: ${KERN_ARCH}"
echo "Parallel jobs: ${MAKE_JOBS}"

# Ensure we are in the source root
cd "${SRC_ROOT}"

# --- Step 1: Build world (userland) ---
echo ""
echo "--- Building world (userland) ---"
# -j ${MAKE_JOBS} for parallel compilation
${MAKE_CMD} -j ${MAKE_JOBS} buildworld

# --- Step 2: Build kernel ---
echo ""
echo "--- Building kernel ---"
# KERNCONF specifies which kernel configuration file to use (e.g., sys/ARCH/conf/KERN_CONF)
${MAKE_CMD} -j ${MAKE_JOBS} buildkernel KERNCONF=${KERN_CONF}

# --- Step 3: Create release and ISO image ---
echo ""
echo "--- Creating release and ISO image ---"
# Define DESTDIR to stage the release artifacts
# For simplicity, we'll build a release in a temporary directory and then copy the ISO
RELEASE_BUILD_DIR=$(mktemp -d -p "${SRC_ROOT}" egorbsd_release.XXXXXX)
echo "Staging release in temporary directory: ${RELEASE_BUILD_DIR}"

# Build the release image (this includes everything, including ISO generation)
# Note: make release on Linux might require additional setup or might be limited
# depending on the tools available.
${MAKE_CMD} release DESTDIR="${RELEASE_BUILD_DIR}" KERNCONF=${KERN_CONF}

# Find the generated ISO image and copy it to the dist directory
mkdir -p "${DIST_DIR}"
ISO_PATH=$(find "${RELEASE_BUILD_DIR}" -name "*.iso" -print -quit)

if [ -n "${ISO_PATH}" ] && [ -f "${ISO_PATH}" ]; then
    echo "Found ISO: ${ISO_PATH}"
    cp "${ISO_PATH}" "${DIST_DIR}/egorbsd-${KERN_ARCH}.iso"
    echo "ISO image copied to ${DIST_DIR}/egorbsd-${KERN_ARCH}.iso"
else
    echo "ERROR: ISO image not found in ${RELEASE_BUILD_DIR}"
    # In some cross-build environments, release target might not produce an ISO automatically
    # or it might be in a different location.
    # Check MAKEOBJDIRPREFIX too
    ISO_PATH=$(find "${MAKEOBJDIRPREFIX}" -name "*.iso" -print -quit)
    if [ -n "${ISO_PATH}" ] && [ -f "${ISO_PATH}" ]; then
        echo "Found ISO in obj: ${ISO_PATH}"
        cp "${ISO_PATH}" "${DIST_DIR}/egorbsd-${KERN_ARCH}.iso"
    else
        exit 1
    fi
fi

# --- Cleanup temporary release build directory ---
echo ""
echo "--- Cleaning up temporary release build directory ---"
rm -rf "${RELEASE_BUILD_DIR}"

echo ""
echo "--- egorbsd Linux Build Completed Successfully ---"
echo "Your ISO image should be available at ${DIST_DIR}/egorbsd-${KERN_ARCH}.iso"
