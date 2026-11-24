# SM-A226B-SukiSU-Ultra-Kernel

> ## ⬆ Major Update: V28 — 2026-11-24
> **The KPM shim has been removed completely.** SELinux hiding is now implemented
> **in-kernel with the same logic as the LKM (main branch) tree**:
> clean policydb snapshot + self-answered `access/context` + `policy` blob +
> `setprocattr` semantics + fake status page.
> Verified on device: Maple Ultra **ALL CLEAR** (SELinux side-channel item gone),
> Hunter 6.72 **no risk found**.
> See [CHANGELOG V28](CHANGELOG.en.md) / [release notes](RELEASE-v28.en.md).

[![Build kernel](https://github.com/kazaf080726/SM-A226B-SukiSU-Ultra-Kernel/actions/workflows/build.yml/badge.svg)](https://github.com/kazaf080726/SM-A226B-SukiSU-Ultra-Kernel/actions/workflows/build.yml)

> Samsung Galaxy A22 5G (SM-A226B / a22x / MediaTek MT6833), Linux 4.14.186
> kernel source and flashable builds
>
> **Kernel-level root: SukiSU-Nova** (formerly SukiSU-Ultra; builtin, v3.0.0 /
> KSU_VERSION 35081, tracking upstream main)
>
> 🌐 [中文](README.md) | **English**

This repository contains the **complete kernel source for the SM-A226B** (based on
Samsung's official `SM-A226B_13_Opensource` drop) with SukiSU-Ultra, SUSFS,
NoMount and PathMask integrated. Starting with V28, SELinux hiding is an
**in-kernel implementation shared with the LKM tree** (clean policydb snapshot +
self-answered queries); KPM / KPatch-Next have been removed entirely.

All source code and build artifacts are permanently free and public: build it,
modify it, redistribute it (under the GPL terms of each component).

---

## 1. Current version (V28 · Major Update)

| Component | Version / notes |
|---|---|
| Kernel | Linux 4.14.186, Samsung `SM-A226B_13_Opensource` |
| Root | SukiSU-Nova `builtin` (formerly SukiSU-Ultra), v3.0.0, KSU_VERSION **35081** (overridden at build time to match the manager) |
| SUSFS | **v2.4.0** (Non-GKI 4.14 port) |
| NoMount | **v2.3.1** (built into the kernel driver) |
| PathMask | 4.14 port (`pathmask.ko` + `procguard.ko`, KSU module) |
| SELinux hiding | **In-kernel LKM-equivalent engine (V28)**: clean policydb snapshot / self-answered access·context / policy blob / setprocattr semantics; **no KPM, no selinux_hook** |
| Anti-detection | clean-policy engine, app-side fake status, hidden-context `EINVAL` semantics, app-uid supercall isolation |
| KernelPatch / kpimg | **None** (zero traces, the KPatch-Next module is not needed) |

The device has only a `boot` partition (no `init_boot`); AnyKernel3 writes to
`/dev/block/by-name/boot` with `do.devicecheck=0`.

---

## 2. Version history (Releases)

| Version | Highlights | Result |
|---|---|---|
| **V1** | Initial integration of SukiSU + SUSFS 2.4.0 + NoMount 2.2.0 + PathMask | Boots |
| **V2** | Fixed the SukiSU manager "hide SELinux modifications" toggle on 4.14; shipped KPM (kpimg) and standard builds | Boots; NativeCheck reported `FolkPatch/APatch` + `SELinux Policy` |
| **V3** | Removed KPatch-Next / kpimg completely | `FolkPatch/APatch` gone, `SELinux Policy` remained |
| **V4** | Status spoofing (sequence=4 / policyload=1) + always-on native SELinux filter layer | Prepared the way for the V5 clean-policy engine |
| **V5** | selinux_hook 1.1.6.1 compiled in from full source, 12 hooks wired, clean-policy snapshot/redirect | `SELinux Policy` gone; `FolkPatch/APatch` intermittently returned |
| **V6** | Fixed the fixed-response fingerprint of `/sys/fs/selinux/access` (KPM + native filter) | **Both false positives gone, fully functional** |
| **V7** | NoMount 2.2.0 → **2.3.0** (inode rules, pre-opened cache, OOB/UAF fixes, hardening, faster boot) | Stable baseline |
| **V8** | zram `is_bdev_avail()` NULL bdev fix; PathMask 4.14 write hook `SyS_*` adaptation | Fixed random reboots |
| **V9** | Hide stock metadata/cache/efs/omr and loop5 mounts from app UIDs | XiaoGu MountInfo/Loop passed |
| **V10** | Keep the full mount view for 春秋 (UID 10296), fixing the "mount gap" | NativeCheck passed |
| **V11** | Inject `androidboot.vbmeta.digest` in `init/main.c` | Bootloader/TEE risk items gone |
| **V12** | Exclude Hunter (UID 10272) from mount hiding | Hunter red screen gone |
| **V13** | Rebuild only: KSU version advanced to **41183** with upstream main, same source as V12; added full repo audit and overview docs | Previous release |
| **V14** | SukiSU renamed **SukiSU-Nova**: builtin source upgraded to `builtin@4bec532d`, package renamed `com.sukisu.nova`, version string `v3.0.0`; all five 4.14 compatibility changes kept | Previous release |
| **V15** | SUSFS 4.14 patches synced upstream (SUS_PATH `i_state` fixes); kernel version formula aligned with the manager, outputs **35040** | Previous release |
| **V16** | NoMount upgraded to **2.3.1** (KEYS select, export_operations proxy, boot optimisation); version aligned to manager **35049** | Previous release |
| **V17** | AVC log spoofing enabled by default in the kernel (manager independent); version aligned to 35061 | Previous release |
| **V18** | Added the KSU `avc_spoof` feature (ID 10003) so the manager can toggle AVC spoofing | Previous release |
| **V19** | Fixed the 春秋 "mount gap": UID becomes 10265/10267 after a reset, exemption table updated | Previous release |
| **V20** | NoMount synced to upstream 6849f00 (virtual backing path resolution); manager r1 (version still 35061) | Previous release |
| **V21** | Aligned to Telegram-channel manager 35070 (source unchanged, version only) | Previous release |
| **V22** | Aligned to Telegram-channel manager 35073 (source unchanged, version only) | Previous release |
| **V23** | Aligned to manager 35081 (translation commits, source unchanged, version only) | Previous release |
| **V25** | Kernel-level dynamic mount exemption: parse `packages.list` for the current 春秋/Hunter UIDs, fixing the post-reset "mount gap" | Previous release |
| **V26** | Fix kernel panic caused by the selinux_hook clean-policy redirect for the "app UID + root domain" combination | Previous release |
| **V27** | Align SELinux hiding with stock: status page back to 0/0, app-side fake status page, remove the clean-policy redirect | Previous release |
| **V28** | Remove KPM, build LKM-equivalent SELinux hiding in-tree (clean policydb snapshot + self-answered access/context + policy blob + setprocattr semantics) | **Current recommended release** |

Every release ships its own flashable package and update notes.

> The V7 AnyKernel3 package was verified on a real SM-A226B and boots normally.

---

## 3. Feature overview

### 3.1 SukiSU-Nova (`CONFIG_KSU`, formerly SukiSU-Ultra)
- `builtin` branch (SukiSU-Nova), manager v3.0.0 (versionCode 35081), APK
  `SukiSU_Nova_v3.0.0_35081-arm64-v8a-release.apk`; package name
  `com.sukisu.nova` (the old Ultra package is no longer recognised);
- Source lives in `KernelSU/`; `drivers/kernelsu -> ../KernelSU/kernel`;
- 4.14 compatibility layer: `static_call.h` version guards, `ksu_kvcalloc`,
  `strncpy_from_user_nofault` aliases, KPM extension compiled out below 5.10, etc.

### 3.2 SUSFS v2.4.0 (`CONFIG_KSU_SUSFS`)
- `fs/susfs.c` + `include/linux/susfs.h` + `include/linux/susfs_def.h`;
- 18 kernel files merged (NonGKI 4.14 port + BakaSU 4.14.186 fixes);
- Enabled: SUS_PATH, SUS_MOUNT, SUS_KSTAT, SPOOF_UNAME, SPOOF_CMDLINE,
  OPEN_REDIRECT, SUS_MAP, HIDE_KSU_SUSFS_SYMBOLS, PRIORITY_UID_FILTER, ENABLE_LOG.

### 3.3 NoMount v2.3.1 (`CONFIG_NOMOUNT`)
- Source lives in `NoMount/`; `fs/nomount -> ../NoMount/kernel/src`, built-in driver;
- 2.3.0/2.3.1: inode baseline rules, pre-opened backing-file cache, O(1) revalidate,
  readdir concurrency fixes, OOB/UAF fixes, xattr LSM checks, cross-sb proxy inodes,
  boot-time speedups.

### 3.4 PathMask 4.14
- `pathmask/`: `pathmask.ko` + `procguard.ko`, KSU module WebUI to configure hidden
  paths / UIDs;
- Built in the same pass as the kernel, vermagic matches.

### 3.5 SELinux hiding (V28 in-kernel LKM-equivalent engine)
- Starting with V28 **KPM / selinux_hook are gone entirely**, replaced by an
  in-kernel 4.14 implementation with the same logic as the LKM (main branch) tree:
  - `security/selinux/ksu_selinux_hide.h`: public interface;
  - `security/selinux/ss/services.c`: `ksu_selinux_hide_capture_clean_policy()`
    serialises the pristine policydb **before** KernelSU patches the live policy,
    re-parses it into `ksu_clean_policy{policydb, sidtab}` and keeps the original
    policy blob; provides clean-policy
    `context_to_sid / sid_to_context / compute_av(seqno=1)`;
  - `security/selinux/selinuxfs.c`: `SEL_ACCESS/SEL_CONTEXT` write nodes are
    answered by the clean engine; app policy reads (`/sys/fs/selinux/policy`,
    including mmap) return the clean blob;
  - `security/selinux/hooks.c`: app writes of hidden contexts to
    `/proc/self/attr/current` return `-EINVAL` (matching the clean-policy parse
    failure semantics of the LKM `my_setprocattr`);
  - `security/selinux/ss/status.c`: keeps Samsung's stock 0/0 behaviour; app
    observers get a fake status page (`version=1, sequence=0, enforcing=1,
    policyload=0, deny_unknown=1`);
  - `kernel/reboot.c`, `KernelSU/kernel/supercall/dispatch.c`: app-side reboot
    supercall behaves exactly like stock while the root/ksud path is preserved.

### 3.6 Anti-detection details
- **clean-policy engine**: the pristine policy is snapshotted at boot into a
  separate `policydb`; every hidden-domain query (`access`/`context`/`policy`) is
  answered from the clean data and hidden types return `EINVAL`;
- **status spoofing**: app view `1 0 1 0 1` (root/manager still read the real page);
- **policy blob hiding**: app reads of `/sys/fs/selinux/policy` or mmap return the
  clean blob;
- **setprocattr semantics**: hidden-context writes return `-EINVAL`, matching the
  DirtySepolicy `NORMAL_EINVAL` expectation;
- **supercall isolation**: reboot supercalls from app UIDs (>=10000) are
  indistinguishable from stock, avoiding APatch/KernelSU timing/trait detection.

---

## 4. Repository layout

```
.
├── arch/arm64/configs/a22x_defconfig   # default configuration
├── KernelSU/                           # SukiSU-Nova source (builtin)
├── NoMount/                            # NoMount v2.3.1 (kernel + module)
├── pathmask/                           # PathMask 4.14 LKM and KSU module
├── security/selinux/ksu_selinux_hide.h # V28 in-kernel SELinux hiding interface
├── security/selinux/ss/services.c      # clean policydb snapshot + answer engine
├── security/selinux/selinuxfs.c        # access/context/policy wrappers
├── security/selinux/hooks.c            # setprocattr hiding semantics
├── security/selinux/ss/status.c        # status page (stock values + app fake page)
├── anykernel/                          # AnyKernel3 template
├── ci/                                 # build/package scripts
├── docs/                               # build, features, changelog, architecture, repo overview
└── out/                                # build output (not committed)
```

`drivers/kernelsu` and `fs/nomount` are symlinks into the two subprojects above.

---

## 5. Building (WSL / Ubuntu)

Dependencies:

```bash
sudo apt-get update
sudo apt-get install -y build-essential bc bison flex libssl-dev libelf-dev \
    lz4 device-tree-compiler python3 zip unzip cpio rsync curl git zlib1g-dev
```

Toolchain (AOSP clang-r383902 + GCC 4.9 binutils):

```bash
bash ci/fetch_toolchain.sh          # downloads to ./toolchain (~330 MB)
```

Build:

```bash
export ARCH=arm64
export PATH="$PWD/toolchain/gcc/linux-x86/aarch64/aarch64-linux-android-4.9/bin:$PWD/toolchain/clang/host/linux-x86/clang-r383902/bin:$PATH"
export CROSS_COMPILE=aarch64-linux-androidkernel-
export CLANG_TRIPLE=aarch64-linux-gnu-

make O=out a22x_defconfig
make O=out -j"$(nproc)" KCFLAGS=-w \
     CONFIG_SECTION_MISMATCH_WARN_ONLY=y LDFLAGS_BUILD_ID=
```

Artifacts: `out/arch/arm64/boot/Image`, `Image.gz`.

Or simply:

```bash
bash ci/build.sh      # reproducible build
bash ci/package.sh    # produces dist/A226B-...zip (AnyKernel3)
bash pathmask/build.sh
```

Optional: ccache to speed up repeated builds (only reuses translation units with
identical inputs):

```bash
export USE_CCACHE=1
bash ci/build.sh
```

---

## 6. Flashing

1. **Back up boot first**: TWRP → Backup → Boot;
2. TWRP → Install → flash the `A226B-...zip` from the release;
3. Reboot and install the **SukiSU-Nova v3.0.0** manager APK (versionCode 35081,
   package `com.sukisu.nova`); uninstall the old `com.sukisu.ultra` first;
4. If you need path hiding, flash `A226B-PathMask-4.14.zip` and configure it in
   the KSU manager.

> This device has no `init_boot`; never flash a partition other than `boot`.

---

## 7. Verification

```bash
# build time
grep -E 'CONFIG_(KSU|NOMOUNT)=' out/.config
nm out/vmlinux | grep -E 'ksu_selinux_hide_capture_clean_policy|ksu_selinux_hide_compute_av'
nm out/vmlinux | grep -E 'kp_hook_before|selinux_hook_kpm_builtin_init' && echo LEGACY || echo "KPM removed"

# full feature check (same script as CI, 65 checks)
bash ci/verify-features.sh

# CI build log must have zero warnings / errors
bash ci/verify-log.sh build.log

# on device
dmesg | grep -i ksu_selinux_hide
dmesg | grep -i nomount
```

---

## 8. Languages / Translations

Documentation is bilingual (Chinese is the primary language):

| Chinese | English |
|---|---|
| [README.md](README.md) | [README.en.md](README.en.md) |
| [docs/CHANGELOG.md](docs/CHANGELOG.md) | [docs/CHANGELOG.en.md](docs/CHANGELOG.en.md) |
| [docs/ARCHITECTURE-V12.md](docs/ARCHITECTURE-V12.md) | [docs/ARCHITECTURE-V12.en.md](docs/ARCHITECTURE-V12.en.md) |
| [docs/REPO-MASTER.md](docs/REPO-MASTER.md) | [docs/REPO-MASTER.en.md](docs/REPO-MASTER.en.md) |
| [docs/BUILD.md](docs/BUILD.md) | [docs/BUILD.en.md](docs/BUILD.en.md) |
| [docs/FEATURES.md](docs/FEATURES.md) | [docs/FEATURES.en.md](docs/FEATURES.en.md) |
| [docs/RELEASE-v28.md](docs/RELEASE-v28.md) | [docs/RELEASE-v28.en.md](docs/RELEASE-v28.en.md) |
| [NOTICE.md](NOTICE.md) | [NOTICE.en.md](NOTICE.en.md) |
| [pathmask/README.md](pathmask/README.md) | [pathmask/README.en.md](pathmask/README.en.md) |
| [patches/README.md](patches/README.md) | [patches/README.en.md](patches/README.en.md) |

GitHub Releases contain both Chinese and English notes; the NoMount module WebUI
ships its own `en.json` / `zh.json` (and more) locales.

---

## 9. License and credits

- Samsung 4.14.186 kernel source: GPL-2.0;
- SukiSU-Ultra: GPL-2.0; SUSFS: GPL-2.0; NoMount: GPL-2.0; PathMask: GPL-2.0;
  AnyKernel3: see upstream.
- Full third-party notices: [NOTICE.md](NOTICE.md) / [NOTICE.en.md](NOTICE.en.md).

This project is free and public: no paid builds, no paywall. Flashing is at your
own risk.
