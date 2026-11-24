# Build & reproducibility

> [中文](BUILD.md) | **English**

This repository already **bundles** the SukiSU (`KernelSU/`) and NoMount (`NoMount/`) source,
so there is no need to clone the subprojects; `drivers/kernelsu` and `fs/nomount` are symbolic links.

## 1. Dependencies

Ubuntu 22.04 / 24.04 / 26.04 (WSL2 also works):

```bash
sudo apt-get update
sudo apt-get install -y build-essential bc bison flex libssl-dev libelf-dev \
    lz4 device-tree-compiler python3 zip unzip cpio rsync curl git zlib1g-dev
```

## 2. Toolchain

The `CC` path in the top-level `Makefile` is hard-coded, so the toolchain must be placed in `toolchain/` at the repository root:

```bash
bash ci/fetch_toolchain.sh     # AOSP clang-r383902 + GCC 4.9 binutils, about 330 MB
```

The two directories required:

```
toolchain/clang/host/linux-x86/clang-r383902/bin/clang
toolchain/gcc/linux-x86/aarch64/aarch64-linux-android-4.9/bin/aarch64-linux-androidkernel-
```

## 3. Building

```bash
export ARCH=arm64
export PATH="$PWD/toolchain/gcc/linux-x86/aarch64/aarch64-linux-android-4.9/bin:$PWD/toolchain/clang/host/linux-x86/clang-r383902/bin:$PATH"
export CROSS_COMPILE=aarch64-linux-androidkernel-
export CLANG_TRIPLE=aarch64-linux-gnu-

make O=out a22x_defconfig
make O=out -j"$(nproc)" KCFLAGS=-w \
     CONFIG_SECTION_MISMATCH_WARN_ONLY=y LDFLAGS_BUILD_ID=
```

Outputs:

```
out/arch/arm64/boot/Image
out/arch/arm64/boot/Image.gz
```

You can also use the scripts directly:

```bash
bash ci/build.sh          # pins KBUILD_BUILD_USER/HOST/TIMESTAMP, reproducible
bash ci/package.sh        # AnyKernel3 zip -> dist/
bash pathmask/build.sh    # PathMask LKM + KSU module zip -> dist/
```

`JOBS=n` controls parallelism; `OUT=...` changes the output directory.

## 4. Key configuration

In `arch/arm64/configs/a22x_defconfig`:

```
CONFIG_KSU=y
CONFIG_KSU_SUSFS=y
CONFIG_KSU_SUSFS_SUS_PATH=y
CONFIG_KSU_SUSFS_SUS_MOUNT=y
CONFIG_KSU_SUSFS_SUS_KSTAT=y
CONFIG_KSU_SUSFS_SPOOF_UNAME=y
CONFIG_KSU_SUSFS_SPOOF_CMDLINE_OR_BOOTCONFIG=y
CONFIG_KSU_SUSFS_OPEN_REDIRECT=y
CONFIG_KSU_SUSFS_SUS_MAP=y
CONFIG_KSU_SUSFS_HIDE_KSU_SUSFS_SYMBOLS=y
CONFIG_KSU_SUSFS_PRIORITY_UID_FILTER=y
CONFIG_NOMOUNT=y
CONFIG_SECURITY_SELINUX_QUERY_FILTER=y
CONFIG_SECURITY_SELINUX_HOOK_KPM=y
CONFIG_KALLSYMS_ALL=y
```

## 5. Reproducible build

`ci/build.sh` pins:

- `KBUILD_BUILD_USER=A226B`, `KBUILD_BUILD_HOST=a22x`, `KBUILD_BUILD_VERSION=1`;
- `KBUILD_BUILD_TIMESTAMP` is taken from `ci/repro-date.txt` (incremented with each release);
- `LOCALVERSION=""`, to prevent `setlocalversion` from appending `+`.

The same source + the same `repro-date.txt` should yield a byte-for-byte identical `Image`.

## 6. Common issues

- **`linux/static_call.h: No such file`**: 4.14 has no such header, and a version guard already exists on the SukiSU side;
  if you get this error after updating KernelSU, check `KernelSU/kernel/kernel_includes.h`.
- **`access_ok()` argument count**: 4.14 takes 2 arguments; SukiSU's KPM extension is already compiled out entirely below 5.10.
- **`kvcalloc` / `strncpy_from_user_nofault` undefined**: 4.14 does not have them; use
  the `ksu_kvcalloc` / `ksu_strncpy_from_user_nofault` compatibility layer.
- **Broken symbolic links**: cloning on Windows requires symlink support to be enabled, or just build inside WSL/Linux.

## 7. ccache (optional)

`ci/build.sh` supports `USE_CCACHE=1` to wrap clang with ccache:

```bash
export USE_CCACHE=1
export CCACHE_DIR="$PWD/.ccache"   # default is .ccache under the repository
bash ci/build.sh
```

ccache is addressed by preprocessed content and only reuses compilation units whose inputs are exactly identical; source changes invalidate entries as expected.
It is enabled by default in GitHub Actions, which caches `.ccache` via `actions/cache` (limit 1.5 GB per run).

## 8. GitHub Actions

The repository ships `.github/workflows/build.yml`, which uses the same `ci/` scripts as local builds:

- push to `main` (source/config changes) → automatically build, package, and upload Actions artifacts;
- push a `v*` tag → additionally create/update a GitHub Release with the AnyKernel3 and PathMask packages attached;
- `workflow_dispatch` → manually trigger a single run;
- the toolchain is cached with `actions/cache`, so the second build does not download it again;
- at the end of the build, `ci/verify-features.sh` is forced to run (65 checks: configs, symbols, 12 hooks,
  Image markers, PathMask, AnyKernel3); if any single check is missing the run fails, no artifacts are uploaded and no Release is published;
- the build log is scanned by `ci/verify-log.sh`; any `warning:` / `error:` /
  `Syntax error` makes CI fail, guaranteeing every build log is clean.

Artifacts are on the Actions run page under Artifacts, already separated by type:

- `A226B-Image-<run>`: `Image.gz`
- `A226B-AnyKernel3-<run>`: kernel flashable zip
- `A226B-Modules-<run>`: kernel module package (if any)
- `A226B-PathMask-<run>`: PathMask module package
- `A226B-verify-logs-<run>`: `build.log`, `feature-verify.txt`, `ccache-stats.txt`


---

## SukiSU version pinning (`ci/manager-version.txt`)

The manager's versionCode can no longer be derived from the public repository: builds from the Telegram channel come from unpublished CI commits, and the commit counts
of the public `main`/`builtin` cannot produce the corresponding number.

Therefore, for each release write the target versionCode into `ci/manager-version.txt` (a single line of plain digits);
when the `KSU_VERSION_OVERRIDE` environment variable is not set, `ci/build.sh` reads that file and passes it to
`KernelSU/kernel/Makefile`; an explicit environment variable still takes precedence.

GitHub Actions also supports temporary overrides via the `ksu_version` input on manual triggers.
