#!/bin/bash
# Build the SM-A226B (a22x) 4.14.186 kernel with SukiSU + SUSFS + NoMount.
# Assumes toolchain/ and the KernelSU/ + NoMount/ clones already exist.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

export ARCH=arm64
export SUBARCH=arm64
export PATH="$ROOT/toolchain/gcc/linux-x86/aarch64/aarch64-linux-android-4.9/bin:$ROOT/toolchain/clang/host/linux-x86/clang-r383902/bin:$PATH"
export CROSS_COMPILE="aarch64-linux-androidkernel-"
export CLANG_TRIPLE="aarch64-linux-gnu-"
unset CC LD AR NM STRIP OBJCOPY OBJDUMP READELF HOSTCC HOSTCXX || true

# --- optional ccache wrapper -------------------------------------------------
# USE_CCACHE=1 wraps the target compiler with ccache. The cache is content
# addressed, so only translation units whose preprocessed input is unchanged
# are reused; this never serves a stale object for changed source.
CC_BIN="$ROOT/toolchain/clang/host/linux-x86/clang-r383902/bin/clang"
CC_CMD="$CC_BIN"
if [ "${USE_CCACHE:-0}" = "1" ]; then
    if command -v ccache >/dev/null 2>&1; then
        export CCACHE_DIR="${CCACHE_DIR:-$ROOT/.ccache}"
        export CCACHE_NOHASHDIR=true
        export CCACHE_COMPILERCHECK=content
        export CCACHE_MAXSIZE="${CCACHE_MAXSIZE:-1500M}"
        ccache --max-size="$CCACHE_MAXSIZE" >/dev/null 2>&1 || true
        CC_CMD="ccache $CC_BIN"
        echo "=== ccache enabled: dir=$CCACHE_DIR max=$CCACHE_MAXSIZE"
    else
        echo "=== USE_CCACHE=1 but ccache not found; building without ccache"
    fi
fi

JOBS="${JOBS:-$(nproc)}"
OUT="${OUT:-$ROOT/out}"
DEFCONFIG="${DEFCONFIG:-a22x_defconfig}"

# --- reproducible build metadata -------------------------------------------
# Everything below gets compiled into Image: the Linux banner carries
# "<user>@<host>" plus the UTS timestamp, and the built-in initramfs takes its
# cpio mtimes from KBUILD_BUILD_TIMESTAMP (scripts/gen_initramfs_list.sh).
# Leaving them unset makes two builds of the same commit differ, so pin all of
# them. Defaults are stable per commit (committer date), which keeps CI and
# local builds byte-identical. Set REPRO=0 to disable.
REPRO="${REPRO:-1}"
if [ "$REPRO" != "0" ]; then
    export KBUILD_BUILD_USER="${KBUILD_BUILD_USER:-A226B}"
    export KBUILD_BUILD_HOST="${KBUILD_BUILD_HOST:-a22x}"
    export KBUILD_BUILD_VERSION="${KBUILD_BUILD_VERSION:-1}"
    if [ -z "${KBUILD_BUILD_TIMESTAMP:-}" ]; then
        # The timestamp must derive from file *content*, not from the clock or
        # from commit metadata: otherwise a build done before committing (dirty
        # tree, old commit date) can never match the CI build of the pushed
        # commit. ci/repro-date.txt is one line, bumped per release.
        if [ -f "$ROOT/ci/repro-date.txt" ]; then
            KBUILD_BUILD_TIMESTAMP="$(head -n1 "$ROOT/ci/repro-date.txt")"
        fi
        [ -n "${KBUILD_BUILD_TIMESTAMP:-}" ] || KBUILD_BUILD_TIMESTAMP="Thu Jan  1 00:00:00 UTC 2026"
        export KBUILD_BUILD_TIMESTAMP
    fi
    echo "=== reproducible: user=$KBUILD_BUILD_USER host=$KBUILD_BUILD_HOST version=$KBUILD_BUILD_VERSION ts=$KBUILD_BUILD_TIMESTAMP"
    # scripts/setlocalversion appends "+" whenever LOCALVERSION is unset and HEAD
    # has no annotated tag, which is always the case here (the repo uses
    # lightweight tags). That made the release string depend on the git state,
    # so a local build could never match CI. Setting LOCALVERSION (even empty)
    # suppresses it; an explicit LOCALVERSION is still honoured.
    : "${LOCALVERSION=}"
    export LOCALVERSION
    echo "=== reproducible: LOCALVERSION='$LOCALVERSION' (suppresses the '+' suffix)"
else
    echo "=== reproducible: DISABLED (REPRO=0)"
fi

mkdir -p "$OUT"

# This Samsung tree also writes arch/$ARCH/include/generated and include/generated
# into the *source* tree. When out/ has been wiped but those survive, kbuild
# considers the uapi asm headers already handled, skips regenerating them under
# $OUT, and the build dies with "asm/types.h: file not found". A fresh clone has
# none of them, which is why CI never hits this. Drop them for from-scratch
# builds so local and CI start from the same state; incremental builds keep them.
if [ ! -f "$OUT/.config" ]; then
    rm -rf "$ROOT/arch/$ARCH/include/generated" "$ROOT/include/generated"
fi

# A "+" suffix on the release string means "built from a tree that differs from
# HEAD"; it comes from scripts/setlocalversion. Say so instead of shipping a
# mystery suffix.
if ! git -C "$ROOT" diff-index --quiet HEAD -- 2>/dev/null; then
    echo "=== WARNING: working tree differs from HEAD; the kernel release will carry a '+' suffix"
fi

echo "=== [$DEFCONFIG] SukiSU: $(git -C KernelSU rev-parse --short=12 HEAD 2>/dev/null || echo '?')"
echo "=== [$DEFCONFIG] NoMount: $(git -C NoMount rev-parse --short=12 HEAD 2>/dev/null || echo '?')"
# Reproducibility: the linker otherwise stamps a build-id note whose value is
# not stable between links (GNU ld treats a bare --build-id as a generated id).
# With everything else pinned, that note was the only remaining difference
# between a local and a CI build: 39 bytes across the vmlinux and vDSO notes out
# of 29,894,672. The vDSO side is handled in arch/arm64/kernel/vdso/Makefile;
# this clears it for the vmlinux and module links.
make -C "$ROOT" O="$OUT" ARCH=arm64 CC="$CC_CMD" "$DEFCONFIG"
make -C "$ROOT" O="$OUT" ARCH=arm64 -j"$JOBS" \
    CC="$CC_CMD" KCFLAGS=-w CONFIG_SECTION_MISMATCH_WARN_ONLY=y LDFLAGS_BUILD_ID=

ls -lh "$OUT/arch/arm64/boot/Image" "$OUT/arch/arm64/boot/Image.gz"
