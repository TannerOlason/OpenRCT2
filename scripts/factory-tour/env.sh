#!/usr/bin/env bash
# Factory Tour toolchain environment. Dot-source this file: `source scripts/factory-tour/env.sh`.
# The machine has no sudo for apt, so every build dependency comes from a conda environment.

FT_TOOLCHAIN="${FT_TOOLCHAIN:-/home/user/miniconda3/envs/factory-tour}"
FT_RCT2_DATA="${FT_RCT2_DATA:-/media/user/D/rct2-data/app}"
# Build tree lives on the root disk: the NTFS D drive (ntfs3) hangs in uninterruptible sleep on heavy
# unlink/truncate traffic (seen 2026-10-03 while clearing CMakeFiles). D is used only for read-only data.
FT_BUILD_DIR="${FT_BUILD_DIR:-/home/user/Documents/Projects/factory-tour-build}"

if [[ ! -d "$FT_TOOLCHAIN/bin" ]]; then
    echo "factory-tour: toolchain not found at $FT_TOOLCHAIN" >&2
    echo "  create it with scripts/factory-tour/create-toolchain" >&2
    return 1 2>/dev/null || exit 1
fi

if [[ ! -d "$FT_RCT2_DATA" && -b /dev/sdb1 ]]; then
    udisksctl mount -b /dev/sdb1 >/dev/null 2>&1 || true
fi

export FT_TOOLCHAIN FT_RCT2_DATA FT_BUILD_DIR
export PATH="$FT_TOOLCHAIN/bin:$PATH"
export CC="$FT_TOOLCHAIN/bin/x86_64-conda-linux-gnu-gcc"
export CXX="$FT_TOOLCHAIN/bin/x86_64-conda-linux-gnu-g++"
export CMAKE_PREFIX_PATH="$FT_TOOLCHAIN${CMAKE_PREFIX_PATH:+:$CMAKE_PREFIX_PATH}"
export PKG_CONFIG_PATH="$FT_TOOLCHAIN/lib/pkgconfig:$FT_TOOLCHAIN/share/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
export CCACHE_DIR="${CCACHE_DIR:-$FT_BUILD_DIR/.ccache}"
export CCACHE_MAXSIZE="${CCACHE_MAXSIZE:-3G}"
# Runtime: the conda libstdc++ is newer than the system one, so binaries built here need the conda lib dir.
export LD_LIBRARY_PATH="$FT_TOOLCHAIN/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
# Do not let conda's activation CFLAGS leak in; upstream's CMake owns the warning and optimisation flags.
unset CFLAGS CXXFLAGS CPPFLAGS LDFLAGS DEBUG_CFLAGS DEBUG_CXXFLAGS
