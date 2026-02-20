#!/bin/sh

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
# Number of parallel jobs for make
MAKE_JOBS=$(sysctl -n hw.ncpu) # Use number of CPU cores

echo "--- egorbsd ISO Build Script ---"
echo "Source root: ${SRC_ROOT}"
echo "Distribution output directory: ${DIST_DIR}"
echo "Kernel configuration: ${KERN_CONF}"
echo "Architecture: ${KERN_ARCH}"
echo "Parallel jobs: ${MAKE_JOBS}"

# Ensure we are in the source root
cd "${SRC_ROOT}"

# --- Step 1: Clean up previous build artifacts (optional but recommended) ---
echo ""
echo "--- Cleaning up previous build artifacts ---"
# make cleanworld is dangerous, so buildworld will implicitly clean and rebuild
# make cleankernel is safer
# If you want to force a clean build, uncomment these:
# make cleanworld
# make cleankernel KERNCONF=${KERN_CONF}


# --- Step 2: Build world (userland) ---
echo ""
echo "--- Building world (userland) ---"
# -j ${MAKE_JOBS} for parallel compilation
make -j ${MAKE_JOBS} buildworld

# --- Step 3: Build kernel ---
echo ""
echo "--- Building kernel ---"
# KERNCONF specifies which kernel configuration file to use (e.g., sys/ARCH/conf/KERN_CONF)
make -j ${MAKE_JOBS} buildkernel KERNCONF=${KERN_CONF}

# --- Step 4: Create release and ISO image ---
echo ""
echo "--- Creating release and ISO image ---"
# Define DESTDIR to stage the release artifacts
# For simplicity, we'll build a release in a temporary directory and then copy the ISO
RELEASE_BUILD_DIR=$(mktemp -d -t egorbsd_release)
echo "Staging release in temporary directory: ${RELEASE_BUILD_DIR}"

# Build the release image (this includes everything, including ISO generation)
# By default, make release creates output in /usr/obj/usr/src/release
# The ISO will usually be in ${RELEASE_BUILD_DIR}/R/usr/freebsd-RELEASE-amd64.iso or similar
make release DESTDIR="${RELEASE_BUILD_DIR}" KERNCONF=${KERN_CONF}

# Find the generated ISO image and copy it to the dist directory
# The exact path can vary based on FreeBSD version and architecture,
# so we'll search for it.
mkdir -p "${DIST_DIR}"
ISO_PATH=$(find "${RELEASE_BUILD_DIR}" -name "*.iso" -print -quit)

if [ -f "${ISO_PATH}" ]; then
    echo "Found ISO: ${ISO_PATH}"
    cp "${ISO_PATH}" "${DIST_DIR}/egorbsd-${KERN_ARCH}.iso"
    echo "ISO image copied to ${DIST_DIR}/egorbsd-${KERN_ARCH}.iso"
else
    echo "ERROR: ISO image not found in ${RELEASE_BUILD_DIR}"
    exit 1
fi

# --- Cleanup temporary release build directory ---
echo ""
echo "--- Cleaning up temporary release build directory ---"
rm -rf "${RELEASE_BUILD_DIR}"

echo ""
echo "--- egorbsd ISO Build Completed Successfully ---"
echo "Your ISO image should be available at ${DIST_DIR}/egorbsd-${KERN_ARCH}.iso"