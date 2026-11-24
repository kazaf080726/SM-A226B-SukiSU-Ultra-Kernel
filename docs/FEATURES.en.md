# Feature inventory

> [中文](FEATURES.md) | **English**

This document explains what this kernel changed, where each feature lives, and how to verify it.
Baseline: Samsung official `SM-A226B_13_Opensource` (Linux 4.14.186, MT6833 / a22x).

```
Layer 0  Samsung stock 4.14.186
Layer 1  SUSFS v2.4.0 (Non-GKI 4.14 port)        fs/susfs.c + 18 kernel files
Layer 2  SukiSU-Nova builtin 30007              KernelSU/ -> drivers/kernelsu
Layer 3  NoMount v2.3.0 (VFS redirection)        NoMount/ -> fs/nomount
Layer 4  PathMask 4.14 (LKM)                     pathmask/
Layer 5  selinux_hook 1.1.7 (source built in)    security/selinux/selinux_hook_kpm/
Layer 6  native anti-detection layer             selinux_query_filter.c + ss/status.c
```

## 1. SUSFS v2.4.0 (`CONFIG_KSU_SUSFS`)

- Version: `include/linux/susfs.h` → `SUSFS_VERSION "v2.4.0"`, `NON-GKI` for 4.14;
- Implementation: `fs/susfs.c`, `include/linux/susfs.h`, `include/linux/susfs_def.h`;
- Enabled: SUS_PATH, SUS_MOUNT, SUS_KSTAT, SPOOF_UNAME,
  SPOOF_CMDLINE_OR_BOOTCONFIG, OPEN_REDIRECT, SUS_MAP,
  HIDE_KSU_SUSFS_SYMBOLS, PRIORITY_UID_FILTER, ENABLE_LOG;
- Userspace interface: `CMD_SUSFS_*` ioctl (`susfs_def.h`), issued by the susfs4ksu module.

## 2. SukiSU-Nova (`CONFIG_KSU`, formerly SukiSU-Ultra)

- `KernelSU/` source, `drivers/kernelsu -> ../KernelSU/kernel`;
- Version: v3.0.0 / KSU_VERSION 30007 (`30000+main-4001`);
- 4.14 compatibility: `static_call.h` guards, `ksu_kvcalloc`, `strncpy_from_user_nofault`,
  the KPM extension compiled out below 5.10, `selinux_hide_compat.c`.

## 3. NoMount v2.3.0 (`CONFIG_NOMOUNT`)

- `NoMount/`, `fs/nomount -> ../NoMount/kernel/src`;
- Version: `NOMOUNT_BASE_VERSION "23"`;
- 2.3.0 features: inode baseline rules, pre-opened backing-file cache, O(1) revalidate,
  readdir concurrency fixes, OOB/UAF fixes, xattr LSM checks, cross-sb proxy inodes, faster boot.

## 4. PathMask 4.14 (`pathmask/`)

- `pathmask.ko`: path hiding;
- `procguard.ko`: process/UID protection;
- built in the same build as the kernel with matching vermagic; KSU module WebUI configuration.

## 5. selinux_hook 1.1.7 (built in)

- Source: `security/selinux/selinux_hook_kpm/`
  (`selinux_hook.c` 4521 lines + `kpm_hooks.c` registry + self-contained headers);
- Parameter block: `include/linux/selinux_hook_kpm.h`;
- Entry: `late_initcall(selinux_hook_kpm_builtin_init)`;
- Wiring: original functions renamed to `__kp_*`, same-name static wrappers, dispatched by `kp_hook_before/after`;
- The 12 hook points are listed in `docs/CHANGELOG.md` V5.

### clean-policy engine

- At startup `snapshot_clean_policy()`: reads the clean policy → `policydb_read` into an independent policydb;
- `/sys/fs/selinux/policy` returns the clean blob;
- `context_struct_compute_av` recomputes AV with the clean policydb;
- `string_to_context_struct` / context existence checks go through clean data;
- the `selinux_complete_init` wrapper serves as a lifecycle fallback.

## 6. Native anti-detection layer

- `security/selinux/selinux_query_filter.c` (`CONFIG_SECURITY_SELINUX_QUERY_FILTER`):
  access/context filtering, seqno probes, procattr, always on;
- `security/selinux/ss/status.c`: `sequence=4`, `policyload=1`;
- `/access` reply: `0 ffffffff 0 ffffffff 1 0` (real-kernel shape, since V6);
- the v4 filter and the KPM layer are two independent layers that do not depend on each other.

## 7. No KPatch / kpimg

- In the image, `strings Image | grep -icE 'kpimg|KernelPatch'` = 0;
- no KPatch-Next, `kpatch`, or `.kpm` loader is required;
- `selinux_status_update_seqlock` / `selinux_policy_commit`, which do not exist in 4.14,
  are skipped with `if (addr)` in the KPM source (N/A, not a removed feature).

## 8. Build switches and verification

```bash
grep -E 'CONFIG_(KSU|NOMOUNT|SECURITY_SELINUX)' out/.config
nm out/vmlinux | grep -E 'hook_wrap|snapshot_clean_policy|selinux_hook_kpm_builtin_init'
nm out/vmlinux | grep -w nomount_init
strings out/arch/arm64/boot/Image | grep -icE 'kpimg|KernelPatch'   # 0
```
