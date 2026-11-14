# SM-A226B-Kernel

> Full Linux 4.14.186 kernel source for the Samsung Galaxy A22 5G
> (SM-A226B / a22x / MediaTek MT6833), with SukiSU-Ultra, SUSFS, NoMount,
> PathMask and selinux_hook compiled in.

🌐 [中文](README.md) | English

This repository is the complete kernel source derived from Samsung's
`SM-A226B_13_Opensource` package. The selinux_hook KPM is compiled **into the
kernel** — no KPatch-Next, no kpimg, no text patching.

Everything here is free and public; build it yourself, modify it, redistribute
it under the respective GPL licenses.

---

## Current version (V7)

| Component | Version |
|---|---|
| Kernel | Linux 4.14.186 (Samsung SM-A226B source) |
| Root | SukiSU-Ultra `builtin`, v4.3.0-r2, KSU_VERSION **41179** |
| SUSFS | **v2.4.0** (Non-GKI 4.14 port) |
| NoMount | **v2.3.0** (built-in driver) |
| PathMask | 4.14 port (`pathmask.ko` + `procguard.ko`, KSU module) |
| SELinux hiding | selinux_hook **1.1.6.1 full source** (4521 lines, 124 functions, 12 hooks) |
| Anti-detection | clean-policy engine, status spoof, realistic `/access` reply, v4 native filter |
| KernelPatch / kpimg | **none** |

The device has only a `boot` partition (no `init_boot`); AnyKernel3 writes to
`/dev/block/by-name/boot` with `do.devicecheck=0`.

---

## Releases

| Version | Summary | Result |
|---|---|---|
| **V1** | Initial SukiSU + SUSFS 2.4.0 + NoMount 2.2.0 + PathMask | booted |
| **V2** | Fixed the SukiSU "hide SELinux modifications" toggle on 4.14; KPM (kpimg) and standard builds | booted; NativeCheck flagged `FolkPatch/APatch` + `SELinux Policy` |
| **V3** | Removed KPatch-Next / kpimg entirely | `FolkPatch/APatch` gone, `SELinux Policy` remained |
| **V4** | Status spoof (sequence=4 / policyload=1) + always-on native SELinux filter | groundwork for V5 |
| **V5** | selinux_hook 1.1.6.1 compiled in, 12 hooks, clean-policy snapshot/redirect | `SELinux Policy` gone; `FolkPatch/APatch` intermittently returned |
| **V6** | Fixed the `/sys/fs/selinux/access` fixed-response fingerprint (KPM + native filter) | **both false positives gone; all features working** |
| **V7** | NoMount 2.2.0 → **2.3.0** (inode rules, pre-opened cache, OOB/UAF fixes, hardening, boot speedup) | current recommended build |

Each release ships the matching AnyKernel3 zip and notes.

---

## Features

- **SukiSU-Ultra** (`KernelSU/`, `CONFIG_KSU`): builtin branch, 41179, 4.14 compat
  shims (`ksu_kvcalloc`, `strncpy_from_user_nofault`, static_call guards).
- **SUSFS v2.4.0** (`fs/susfs.c` + headers, 18 files): SUS_PATH, SUS_MOUNT,
  SUS_KSTAT, SPOOF_UNAME, SPOOF_CMDLINE, OPEN_REDIRECT, SUS_MAP,
  HIDE_KSU_SUSFS_SYMBOLS, PRIORITY_UID_FILTER.
- **NoMount v2.3.0** (`NoMount/`, `fs/nomount` symlink, `CONFIG_NOMOUNT`):
  inode-based rules, pre-opened backing-file cache, O(1) revalidate, OOB/UAF fixes.
- **PathMask 4.14** (`pathmask/`): `pathmask.ko` + `procguard.ko`.
- **selinux_hook 1.1.6.1** (`security/selinux/selinux_hook_kpm/`): full KPM source
  compiled in; 12 static hook points wired through `hook_fargsN_t` wrappers.
- **Anti-detection**: clean-policy snapshot, status spoof, realistic
  `/access` reply `0 ffffffff 0 ffffffff 1 0`, plus the v4 native filter in
  `security/selinux/selinux_query_filter.c`.

---

## Build (WSL / Ubuntu)

```bash
sudo apt-get install -y build-essential bc bison flex libssl-dev libelf-dev \
    lz4 device-tree-compiler python3 zip unzip cpio rsync curl git zlib1g-dev

bash ci/fetch_toolchain.sh          # AOSP clang-r383902 + GCC 4.9 binutils

export ARCH=arm64
export PATH="$PWD/toolchain/gcc/linux-x86/aarch64/aarch64-linux-android-4.9/bin:$PWD/toolchain/clang/host/linux-x86/clang-r383902/bin:$PATH"
export CROSS_COMPILE=aarch64-linux-androidkernel-
export CLANG_TRIPLE=aarch64-linux-gnu-

make O=out a22x_defconfig
make O=out -j"$(nproc)" KCFLAGS=-w CONFIG_SECTION_MISMATCH_WARN_ONLY=y LDFLAGS_BUILD_ID=
```

Or use `bash ci/build.sh`, `bash ci/package.sh`, `bash pathmask/build.sh`.

---

## Flash

1. Back up `boot` in TWRP.
2. Flash the AnyKernel3 zip from Releases.
3. Reboot and install the matching SukiSU manager (41179).
4. Optionally flash `A226B-PathMask-4.14.zip` for path hiding.

> No `init_boot` on this device.

---

## Verification

```bash
grep -E 'CONFIG_(KSU|NOMOUNT|SECURITY_SELINUX_HOOK_KPM)=' out/.config
nm out/vmlinux | grep -E 'hook_wrap|snapshot_clean_policy|selinux_hook_kpm_builtin_init'
strings out/arch/arm64/boot/Image | grep -icE 'kpimg|KernelPatch'   # 0
```

On device: `dmesg | grep -iE 'selinux_hook|nomount'`.

---

## License & credits

Samsung 4.14.186 source: GPL-2.0. SukiSU-Ultra, SUSFS, NoMount, PathMask,
selinux_hook: GPL-2.0. AnyKernel3: see upstream. See [NOTICE.md](NOTICE.md).

Free and public; flashing is at your own risk.
