# Integration notes

> [中文](README.md) | **English**

This repository is based on `Kernel.tar.gz` from Samsung's official
`SM-A226B_13_Opensource` package (Linux 4.14.186, MT6833, `a22x_defconfig`).

Changes relative to the stock source fall into three parts:

## 0. Current state (V28)

This directory documents the **historical porting sources**. The actual component
versions of the current repository are defined by the defconfig and by
`docs/ARCHITECTURE-V12.md` (and its English version):

| Component | Current version |
|---|---|
| SukiSU-Nova | builtin branch, KSU_VERSION 35081 (overridden at build time, matches the manager) |
| SUSFS | v2.4.0 (all 10 features enabled) |
| NoMount | v2.3.1 (NOMOUNT_BASE_VERSION 23) |
| SELinux hiding | V28 in-kernel LKM-equivalent engine (clean policydb snapshot / self-answered queries; no KPM/selinux_hook) |
| PathMask | 4.14 port (LKM) |

## 1. SUSFS (4.14 Non-GKI port, baseline patch v2.3.0, now v2.4.0)

The baseline patch comes from (GPL-2.0):

- https://github.com/JackA1ltman/NonGKI_Kernel_Build_2nd
  file: `Patches/Patch/susfs_patch_to_4.14.patch` (SUSFS v2.3.0)
- that patch is a deep 4.14 port of simonpunk's `gki-android12-5.10` branch

How it was applied: `patch -p1 < susfs_patch_to_4.14.patch` in the 4.14.186 source
root, then the rejected hunks in four files (fs/notify/fdinfo.c,
fs/proc/task_mmu.c, fs/super.c, fs/namespace.c) were adapted to 4.14.186.

## 2. KernelSU/BakaSU inline hooks

Using the same project's `Patches/susfs_inline_hook_patches.sh` (SUSFS v2.3
version), the hooks required by BakaSU were added to:

- fs/exec.c            ksu_handle_execveat / _sucompat / post
- fs/open.c            ksu_handle_faccessat
- fs/read_write.c      ksu_handle_sys_read
- fs/stat.c            ksu_handle_stat / ksu_handle_vfs_fstat
- fs/namei.c           export filename_lookup
- drivers/input/input.c ksu_handle_input_handle_event
- kernel/reboot.c      ksu_handle_sys_reboot
- security/selinux/hooks.c  allow init -> su domain

Additional manual edits:

- kernel/sys.c: `ksu_handle_setresuid()` hook (the script skipped it because
  `drivers/kernelsu` was not cloned at that time);
- fs/read_write.c: BakaSU's `ksu_handle_sys_read()` takes three arguments, so the
  call site passes `&buf, &count`;
- fs/proc/task_mmu.c: `return 0` in a void function changed to `return`;
- fs/notify/fdinfo.c: `mask` declaration moved to the top of the function (C does
  not allow declarations after a label).

## 3. NoMount v2.3.1

Integrated through upstream's `kernel/setup.sh`:
`fs/nomount -> ../NoMount/kernel/src`, with `CONFIG_NOMOUNT` added to `fs/Kconfig`
and `fs/Makefile`.

## 4. defconfig

Appended to `arch/arm64/configs/a22x_defconfig`:

```
CONFIG_KSU=y
CONFIG_KSU_SUSFS=y
CONFIG_KSU_SUSFS_SUS_PATH=y
CONFIG_KSU_SUSFS_SUS_MOUNT=y
CONFIG_KSU_SUSFS_SUS_KSTAT=y
CONFIG_KSU_SUSFS_SPOOF_UNAME=y
CONFIG_KSU_SUSFS_ENABLE_LOG=y
CONFIG_KSU_SUSFS_HIDE_KSU_SUSFS_SYMBOLS=y
CONFIG_KSU_SUSFS_SPOOF_CMDLINE_OR_BOOTCONFIG=y
CONFIG_KSU_SUSFS_OPEN_REDIRECT=y
CONFIG_KSU_SUSFS_SUS_MAP=y
CONFIG_NOMOUNT=y
```

## Follow-ups completed

- priority UID filter: **enabled since V12**
  (`CONFIG_KSU_SUSFS_PRIORITY_UID_FILTER=y`, `susfs_is_priority_uid_filtered()`
  wired into `getpriority/setpriority/ioprio`);
- policy-level SELinux hiding: since V28 handled by the in-kernel LKM-equivalent
  engine (`security/selinux/ksu_selinux_hide.h` + `ss/services.c` + `selinuxfs.c` +
  `hooks.c`); `feature/selinux_hide_compat.c` only keeps the feature switch and
  the app-side fake status page.

All other path/mount/stat/maps/uname/cmdline/open_redirect/AVC log hiding remains
active.
