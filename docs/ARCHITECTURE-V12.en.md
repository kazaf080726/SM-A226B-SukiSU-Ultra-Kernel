# Architecture (V12 baseline): SM-A226B Kernel Architecture Audit (read-only full-diff edition)

> [中文](ARCHITECTURE-V12.md) | **English**

- Audit target: `/home/kazaf/a226b/gh-repo`, tag `v12` = commit `9877efbd4`
- Baseline: Samsung official `SM-A226B_13_Opensource.zip` → `Kernel.tar.gz` (Linux 4.14.186, a22x, no KernelSU)
- Baseline extraction path: `/home/kazaf/a226b/stock-v12`
- Audit date: 2026-11-15
- Method: full `diff -rq` + per-file `diff -up` + comparison against upstream repositories (KernelSU / NoMount)
- Constraints: no flashing, no source changes, no touching `/data`; this file was the only newly added file

---

## 0. Fact levels (annotated throughout)

| Level | Meaning |
|---|---|
| **A** | Directly confirmed by file contents / build artifacts / device output |
| **B** | Only existence or direction known, not confirmed function by function, or depends on runtime state |
| **C** | Deprecated, rolled back, must not be reused |

> All A-level conclusions in this file can be reproduced in Section 12 with the same set of commands.
> Narrative text without a level annotation is background by default and is not a conclusion.

---

## 1. Summary of conclusions

1. **The change surface is fully enumerated** (A): relative to stock 4.14.186, **47 files are modified**, **0 files deleted**, and 9 new code/documentation subtrees + several single files are added. There are no hidden changes that are "documented but missing from the code" or "present in the code but not mentioned in the docs".
2. **The six-layer architecture holds** (A): KernelSU, SUSFS, NoMount, PathMask, anti-detection, and KPM/selinux_hook can all be mapped to concrete files and functions; see Sections 5 and 6.
3. **The built-in KernelSU is traced to upstream** (A): `KernelSU/` = SukiSU-Ultra `builtin` branch `f9aa2fec` (2026-11-12) **verbatim + 5 Linux 4.14 compatibility changes**; `uapi/` is fully identical to upstream. The earlier judgment of "a bare snapshot with unverifiable provenance" no longer holds.
4. **NoMount is upstream as-is** (A): `NoMount/kernel/src` is byte-for-byte identical to `maxsteeel/nomount` HEAD `7914458` (diff: 0 lines).
5. **KPM is not an empty shell** (A): `selinux_hook_kpm/` is an **in-kernel shim** of the KernelPatch API; 12 SELinux functions were renamed + wrapped under the same names + call `kp_hook_before/after` directly; it is compiled into V12.
6. **Earlier verbal conclusions need correction** (A): `fs/proc/task_mmu.c` and `fs/open.c` **both have** hooks (SUSFS SUS_MAP / KSU faccessat); see §6.5.6 and §6.2.
7. **Build artifacts are untrustworthy** (A): the repository's `dist/` and `/home/kazaf/a226b/v9-out` were built during the shield-branch era; the images contain the `a226b_shield` string and are **not V12**; flashing them is forbidden.
8. **The version number is computed online at compile time** (A): `KSU_VERSION = 40000 + main commit count - 2815`; V12 = 41179 (3994 at the time); rebuilding today = 41183 (3998).
9. **The device currently has no boot backup** (A): before any flashing, back up first and verify by reading it back.

---

## 2. Full-diff method and results

### 2.1 Commands (reproducible)

```bash
# baseline
mkdir -p /home/kazaf/a226b/stock-v12
unzip -p SM-A226B_13_Opensource.zip Kernel.tar.gz | tar -xz -C /home/kazaf/a226b/stock-v12

# full comparison (excluding build artifacts / version-control data / large binaries)
diff -rq --no-dereference \
  --exclude=.git --exclude=dist --exclude=oldrepo --exclude=toolchain --exclude=out \
  --exclude='*.o' --exclude='*.ko' --exclude='*.cmd' --exclude='*.mod' --exclude='*.mod.c' \
  --exclude=modules.order --exclude=Module.symvers --exclude='*.a' --exclude='*.so' \
  --exclude=generated --exclude='.config' --exclude='*.orig' --exclude='*.rej' \
  --exclude='*.zip' --exclude='*.gz' --exclude='*.tar' --exclude='*.apk' \
  --exclude='*.jpg' --exclude='*.png' --exclude='*.bin' --exclude='*.dtb' --exclude='*.dtbo' \
  --exclude=vmlinux --exclude=vmlinux.o --exclude=System.map \
  /home/kazaf/a226b/stock-v12 /home/kazaf/a226b/gh-repo
```

`--no-dereference` is mandatory: `drivers/kernelsu` and `fs/nomount` are two symlinks; following them would traverse the entire subtree twice.

### 2.2 Results (A)

| Category | Count |
|---|---|
| Present on both sides but contents differ (modified) | **47** |
| Present only in stock (deleted) | **0** |
| Present only in repo (added, counted by line) | 25 lines (9 directories + several files) |

**Key point: no stock file was deleted.** All changes are "additions + in-place patches"; rollback only needs a `checkout` of the tag.

### 2.3 Audit artifacts

| Path | Content |
|---|---|
| `/home/kazaf/a226b/audit/rq.txt` | raw `diff -rq` output |
| `/home/kazaf/a226b/audit/modified.txt` | list of the 47 modified files |
| `/home/kazaf/a226b/audit/stats.tsv` | per-file hunks / +lines / -lines / layer tag |
| `/home/kazaf/a226b/audit/perfile/*.diff` | per-file unified diff |
| `/home/kazaf/a226b/audit/v12.patch` | merged patch (3960 lines, with function context) |
| `/home/kazaf/a226b/audit/ksu_builtin_diff.txt` | built-in KSU vs upstream builtin branch |
| `/home/kazaf/a226b/audit/nomount_diff.txt` | NoMount vs upstream (0 lines) |
| Windows copy | `D:\...\SM-A226B\.audit\out\` |

---

## 3. Complete list of changed files (47, A)

Sorted by owning layer; "hunks/+/-" come from `stats.tsv`.

### 3.1 KernelSU integration (8)

| File | hunks | +/- | Injected function |
|---|---|---|---|
| `fs/exec.c` | 4 | +32/-0 | `do_execveat_common()`: `ksu_handle_execveat()` / `_sucompat()`, on exit `ksu_handle_post_execveat_sucompat()` |
| `fs/open.c` | 4 | +27/-0 | `SYSCALL_DEFINE3(faccessat)`: `ksu_handle_faccessat()` + `filename_lookup()` |
| `fs/stat.c` | 5 | +84/-0 | `vfs_statx()`: `ksu_handle_stat()`; `vfs_statx_fd()`: `ksu_handle_vfs_fstat()` |
| `fs/read_write.c` | 1 | +10/-0 | `SYSCALL_DEFINE3(read)`: `ksu_handle_sys_read()` |
| `kernel/sys.c` | 8 | +54/-0 | `setresuid`: `ksu_handle_setresuid()`; `getpriority/setpriority`: `ksu_hide_app_uid()` |
| `kernel/reboot.c` | 1 | +8/-0 | `SYSCALL_DEFINE4(reboot)`: `ksu_handle_sys_reboot()` |
| `drivers/input/input.c` | 1 | +8/-0 | `input_handle_event()`: `ksu_handle_input_handle_event()` (trigger key) |
| `drivers/{Kconfig,Makefile}` | 1/1 | +1/+1 | `source "drivers/kernelsu/Kconfig"`, `obj-$(CONFIG_KSU) += kernelsu/` |

### 3.2 SUSFS 2.4.0 (16)

| File | hunks | +/- | Injection point |
|---|---|---|---|
| `fs/namei.c` | 27 | +259/-1 | `lookup_dcache/__lookup_hash/lookup_fast/lookup_slow/walk_component/link_path_walk/lookup_open/do_tmpfile/do_o_path/path_openat/generic_readlink/vfs_readlink`; `filename_lookup()` made non-static |
| `fs/readdir.c` | 26 | +190/-0 | `fillonedir/filldir/filldir64/compat_*` each filtered through `ilookup()+susfs_is_inode_sus_path()`; the four `*getdents*` entry points add FUSE `susfs_get_data_path()` |
| `fs/namespace.c` | 11 | +224/-0 | `mnt_free_id/mnt_alloc_group_id/__lookup_mnt/vfs_kern_mount/clone_mnt/copy_mnt_ns`; adds `susfs_alloc_unshare_ksu_vfsmnt()`, `susfs_alloc_non_unshare_ksu_vfsmnt()`, `susfs_get_non_sus_mnt_id_from_mnt()`, `susfs_get_non_sus_vfsmnt_from_vfsmnt()` |
| `fs/stat.c` | (same file as above) | — | `vfs_getattr_nosec()`: `susfs_sus_kstat_spoof_generic_fillattr()` |
| `fs/statfs.c` | 2 | +73/-0 | `vfs_statfs()`: `susfs_statfs_by_dentry()` + non-sus mount replacement |
| `fs/super.c` | 2 | +22/-0 | `get_anon_bdev()`: the KSU domain uses the `DEFAULT_KSU_MNT_MINOR_DEV` range |
| `fs/proc/task_mmu.c` | 6 | +52/-0 | `show_map_vma/smaps/pagemap_read` + `mm/memory.c:__access_remote_vm`: `SUSFS_IS_INODE_SUS_MAP()` hiding |
| `fs/proc/fd.c` | 2 | +65/-0 | `seq_show()`: sus_kstat spoofing + non-sus mnt_id |
| `fs/proc/base.c` | 4 | +20/-0 | `do_proc_readlink()` + `proc_map_files_readdir()` |
| `fs/notify/fdinfo.c` | 4 | +74/-1 | `inotify_fdinfo()`: sus_kstat / sus_mount spoofing |
| `fs/proc/cmdline.c` | 1 | +11/-0 | `cmdline_proc_show()`: `susfs_spoof_cmdline_or_bootconfig()` |
| `fs/proc_namespace.c` | 6 | +205/-0 | insert `a226b_hide_mount_line()` before the three `show_*`; switch `*_open()` to `susfs_show_*` |
| `kernel/kallsyms.c` | 2 | +46/-0 | `s_show()`: drop ksu_/susfs_ symbols by prefix/substring |
| `security/selinux/avc.c` | 3 | +19/-0 | `avc_dump_query()`: spoof as `priv_app` when tsid == `susfs_ksu_sid` |
| `mm/memory.c` | 2 | +10/-0 | `__access_remote_vm()`: break out directly on sus_map |
| `fs/Makefile` | 2 | +2/-0 | `obj-$(CONFIG_KSU_SUSFS) += susfs.o` |

Added: `fs/susfs.c` (2217 lines), `include/linux/susfs.h` (260 lines), `include/linux/susfs_def.h` (238 lines).

### 3.3 NoMount (3 + entire subtree)

| File | hunks | +/- | Content |
|---|---|---|---|
| `fs/Makefile` | (same as above) | +1 | `obj-$(CONFIG_NOMOUNT) += nomount/` |
| `fs/Kconfig` | 1 | +1 | `source "fs/nomount/Kconfig"` |
| `fs/nomount` | — | symlink | → `../NoMount/kernel/src` (3 files: `nomount.c` 2234 lines, `nomount.h`, `Kconfig`/`Makefile`) |

### 3.4 PathMask / procguard (2)

| File | hunks | +/- | Content |
|---|---|---|---|
| `kernel/module.c` | 3 | +29/-0 | adds `ksu_hide_lkm_name()`; `m_show()` hides `pathmask`/`procguard` lines from app UIDs |
| entire `pathmask/` | — | — | `src/{pm_main.c,procguard.c,compat.c,core.h,hint.h}` (LKM, kretprobe + kallsyms resolution), `ksu-module/` (configuration and service.sh) |

### 3.5 Anti-detection + boot integrity (12)

| File | hunks | +/- | Content |
|---|---|---|---|
| `fs/proc_namespace.c` | (see 3.2) | +205 | `a226b_hide_mount_line()`: 6 devnames; UID 10296/10272 exempt |
| `fs/proc/root.c` | 1 | +11/-1 | `proc_mkdir("fs")` changed to `S_IFDIR|0700` |
| `kernel/configs.c` | 2 | +15/-1 | `/proc/config.gz` changed to `0640 root:system` (cannot be 0600, otherwise Vintf fails) |
| `security/selinux/ss/status.c` | 6 | +25/-13 | status page `sequence=4`, `policyload=1`; self-increment removed |
| `fs/proc/version.c` | 1 | +20/-0 | `/proc/version` prints the uname spoofed by SUSFS |
| `kernel/utsname_sysctl.c` | 2 | +30/-0 | `/proc/sys/kernel/{osrelease,version}` spoofed the same way |
| `init/main.c` | 1 | +9/-0 | `setup_command_line()` appends `androidboot.vbmeta.digest=d259db70…` |
| `block/ioprio.c` | 4 | +43/-0 | `ioprio_set/get`: `ksu_hide_app_uid()` + `susfs_is_priority_uid_filtered()` |
| `kernel/sys.c` | (see 3.1) | +54 | `getpriority/setpriority` same as above |
| `drivers/block/zram/zram_drv.c` | 1 | +5/-1 | `is_bdev_avail()` adds `!zram->bdev` |
| `security/selinux/hooks.c` | 4 | +56/-0 | `check_nnp_nosuid()`: allow `u:r:init:s0` → `u:r:su:s0` |
| `arch/arm64/configs/a22x_defconfig` | 3 | +17/-2 | `DMESG_RESTRICT=y`, `SECURITY_DEFEX` changed from y to off, 14 KSU/SUSFS/NoMount/filter switches appended |

### 3.6 KPM / selinux_hook / query filtering (6 + subtree)

| File | hunks | +/- | Wrapped function |
|---|---|---|---|
| `security/security.c` | 2 | +21/-0 | `security_setprocattr()` → `__kp_security_setprocattr()` |
| `fs/libfs.c` | 2 | +27/-0 | `simple_read_from_buffer()` → `__kp_simple_read_from_buffer()` |
| `security/selinux/hooks.c` | (see 3.5) | +56 | `selinux_setprocattr()`, `selinux_complete_init()` |
| `security/selinux/selinuxfs.c` | 7 | +92/-2 | `sel_read_handle_status()`, `sel_mmap_handle_status()`, `sel_write_context()`, `sel_write_access()` |
| `security/selinux/ss/services.c` | 5 | +81/-4 | `context_struct_compute_av()`, `string_to_context_struct()`, `security_read_policy()` |
| `security/selinux/ss/status.c` | (see 3.5) | +25/-13 | `selinux_status_update_policyload()` |
| `security/selinux/{Kconfig,Makefile}` | 1/2 | +20/+2 | adds `SECURITY_SELINUX_QUERY_FILTER`, `SECURITY_SELINUX_HOOK_KPM` |

Added: `security/selinux/selinux_hook_kpm/` (10 files, 4754 lines), `security/selinux/selinux_query_filter.c/.h`, `include/linux/selinux_hook_kpm.h`.

### 3.7 Build/reproducibility (5, not part of the security layers)

| File | hunks | Content |
|---|---|---|---|
| `arch/arm64/kernel/vdso/Makefile` | 1 | `--build-id` → `--build-id=none` |
| `kernel/gen_kheaders.sh` | 1 | tar with fixed mtime/owner/sort order |
| `scripts/extract-cert.c` | 1 | `OPENSSL_SUPPRESS_DEPRECATED` (OpenSSL 3) |
| `scripts/mod/modpost.c` | 1 | `const char *here` (GCC 12 warning) |
| `scripts/kconfig/symbol.c` | 2 | `cv_stack` promoted to static (GCC 12 dangling-pointer warning) |

### 3.8 Overview of added subtrees (A)

| Path | Files | Lines | Nature |
|---|---|---|---|
| `KernelSU/` | 177 | 30,495 | upstream builtin@f9aa2fec + 5 changes |
| `NoMount/` | 56 | 12,587 | upstream maxsteeel/nomount, kernel/src byte-identical |
| `pathmask/` | 54 | 16,224 | local implementation (no upstream) |
| `security/selinux/selinux_hook_kpm/` | 10 | 4,754 | local KernelPatch shim port |
| `anykernel/` | 11 | 58,008 | AK3 template (tools are binaries) |
| `patches/` | 3 | 4,755 | `susfs_patch_to_4.14.patch`, `susfs_inline_hook_patches.sh` |
| `ci/` | 7 | 501 | build/package/verify scripts |
| `docs/` | 3 | 384 | BUILD/FEATURES/CHANGELOG |
| `.github/workflows/` | 1 | 140 | CI |
| top level | 6 | — | README ×2, NOTICE ×2, LICENSE, .gitignore |

---

## 4. Six-layer architecture diagram (A)

```
                    ┌────────────────────────────────────────────────────┐
  userspace         │ SukiSU manager com.sukisu.ultra (external APK)     │
                    │ susfs4ksu / nomount / pathmask (KSU modules)       │
                    └───────────────┬────────────────────────────────────┘
                                    │ supercall / ioctl / feature
 ┌──────────────────────────────────┴───────────────────────────────────────┐
 │ Layer 1  KernelSU  SukiSU builtin@f9aa2fec                               │
 │   drivers/kernelsu -> ../KernelSU/kernel                                 │
 │   supercall dispatch → ksu_handle_{execveat,faccessat,stat,sys_read,     │
 │   setresuid,sys_reboot,input_handle_event}                               │
 ├──────────────────────────────────────────────────────────────────────────┤
 │ Layer 2  SUSFS 2.4.0   fs/susfs.c + 16 host files                        │
 │   SUS_PATH / SUS_MOUNT / SUS_KSTAT / SUS_MAP / OPEN_REDIRECT /           │
 │   SPOOF_UNAME / SPOOF_CMDLINE / HIDE_SYMBOLS / PRIORITY_UID_FILTER       │
 ├──────────────────────────────────────────────────────────────────────────┤
 │ Layer 3  NoMount 2.3.0   fs/nomount -> ../NoMount/kernel/src             │
 │   VFS path redirection (fs_initcall); unlike SUS_MOUNT:                  │
 │   SUSFS fakes a "mount view"; NoMount redirects "path resolution"        │
 ├──────────────────────────────────────────────────────────────────────────┤
 │ Layer 4  PathMask / procguard (LKM, loaded at runtime)                   │
 │   kretprobe + kallsyms; getattr/getdents entry; not in kernel build      │
 ├──────────────────────────────────────────────────────────────────────────┤
 │ Layer 5  Native anti-detection (local policy, A226B-only)                │
 │   a226b_hide_mount_line (6 devnames / UID 10296+10272 exempt)            │
 │   /proc/fs 0700, /proc/config.gz 0640, sequence=4, ioprio hiding,        │
 │   /proc/modules LKM hiding, /proc/version aligned with sysctl            │
 │   uname, vbmeta.digest injection, zram NULL fix, nnp_nosuid allow        │
 ├──────────────────────────────────────────────────────────────────────────┤
 │ Layer 6  KPM / selinux_hook 1.1.7 (built into kernel, not .kpm)          │
 │   kp_hook_before/after shim + 12 wrappers + clean policydb engine        │
 │   selinux_query_filter (CONFIG_ always on) = standalone 2nd layer        │
 └──────────────────────────────────────────────────────────────────────────┘
                    ┌──────────────────────────────────────────────────┐
                    │ Layer 0  Samsung 4.14.186 (stock-v12 baseline)   │
                    └──────────────────────────────────────────────────┘
```

---

## 5. Layer-by-layer function-level description

### 5.1 Layer 1 — KernelSU (SukiSU builtin)

**Provenance (A)**: `KernelSU/` vs upstream `builtin` branch `f9aa2fec` (2026-11-12 21:07 +0800):

- `uapi/`: **all 5 files identical**
- `kernel/`: **4 files with different contents + 1 new file**, all other files byte-identical (`diff -rq` output is only 5 lines)

**5 local changes (A, item by item)**:

| File | Change | Reason |
|---|---|---|
| `kernel/extension/kpm/kpm.c` | entire file wrapped in `#if defined(CONFIG_KPM) && LINUX_VERSION_CODE >= 5.10` | 4.14 has no KernelPatch runtime; avoids compilation |
| `kernel/kernel_includes.h` | `<linux/static_call.h>` included only on ≥5.10 | 4.14 lacks this header |
| `kernel/kernel_compat.h` | adds `ksu_kvcalloc()` (<4.18) and `strncpy_from_user_nofault` alias (<5.8) | 4.14 lacks these two APIs |
| `kernel/ksu_main.c` | `selinux_hide` now compiled for all versions; <5.10 goes through `selinux_hide_compat.c` | gives the manager's "hide SELinux modifications" switch a real backend on 4.14 |
| `kernel/feature/selinux_hide_compat.c` | new: registers only the feature handler; the actual hiding is delegated to KPM | 4.14 has no `struct selinux_policy` |

**supercall wiring (A, 7 host-side points)**:

```
do_execveat_common ──► ksu_handle_execveat / _sucompat / _post_execveat_sucompat
faccessat          ──► ksu_handle_faccessat
vfs_statx          ──► ksu_handle_stat
vfs_statx_fd       ──► ksu_handle_vfs_fstat
sys_read           ──► ksu_handle_sys_read
setresuid          ──► ksu_handle_setresuid
sys_reboot         ──► ksu_handle_sys_reboot
input_handle_event ──► ksu_handle_input_handle_event (static key controlled)
```

**Version number mechanism (A, key point)**: `KernelSU/kernel/Makefile`

```make
KSU_GITHUB_VER := curl releases/latest | tag_name        # fetch latest release online
GITHUB_COMMITS := curl commits?sha=main&per_page=1 | page=last  # fetch main commit count online
LOCAL_COUNT    := GITHUB_COMMITS ?: git rev-list --count main
KSU_VERSION    := 40000 + LOCAL_COUNT - 2815
```

- At V12 build time main = 3994 → **41179** (matches the device manager versionCode, A)
- Measured on 2026-11-15: main = 3998 → rebuild yields **41183** (A)
- When offline, `LOCAL_COUNT` falls back to the **21 commits of this kernel repository** → **37206** (A)
- The sha in `KSU_VERSION_FULL` is also taken from this repository's HEAD (`KernelSU/.git` does not exist; git searches upward): the V12 image carries `v4.3.0-r2-90f5352c@main`, while `90f5352c` is a **v11 commit** (A)

**Risk**: the version number is decoupled from the code; offline builds silently become 37206; `41179` in `build-info.txt` is hardcoded by `ci/package.sh` (A).

### 5.2 Layer 2 — SUSFS 2.4.0

**Provenance (A)**: `include/linux/susfs.h` → `SUSFS_VERSION "v2.4.0"`; `fs/susfs.c` 2217 lines; `patches/susfs_patch_to_4.14.patch` (144 KB) records the porting baseline.

**Function-level distribution (A)**:

| Sub-feature | Entry (susfs.c) | Host injection point |
|---|---|---|
| SUS_PATH | `susfs_add_sus_path()`, `susfs_is_inode_sus_path()`, `susfs_run_sus_path_loop()`, fake qstr `"..5.u.S"` | 8 lookup paths in `namei.c` + all filldir in `readdir.c` |
| SUS_MOUNT | `susfs_add_sus_mount()`, `susfs_set_hide_sus_mnts_for_non_su_procs()`, `susfs_is_current_proc_umounted()` | 6 places in `namespace.c` + `susfs_show_*` in `proc_namespace.c` |
| SUS_KSTAT | `susfs_add_sus_kstat()`, `susfs_sus_kstat_spoof_generic_fillattr()` | `stat.c`, `statfs.c`, `proc/fd.c`, `notify/fdinfo.c`, `task_mmu.c` |
| SUS_MAP | `SUSFS_IS_INODE_SUS_MAP()` macro | `task_mmu.c` (maps/smaps/pagemap), `mm/memory.c`, `proc/base.c` |
| OPEN_REDIRECT | `susfs_add_open_redirect()`, `susfs_open_redirect_spoof_do_sys_openat()` | `namei.c` (do_tmpfile/do_o_path/path_openat/readlink) |
| SPOOF_UNAME | `susfs_spoof_uname()` + static key | `kernel/sys.c:newuname`, `proc/version.c`, `utsname_sysctl.c` (the latter two are local hardening) |
| SPOOF_CMDLINE | `susfs_spoof_cmdline_or_bootconfig()` | `proc/cmdline.c` |
| HIDE_SYMBOLS | — | `kallsyms.c:s_show()` prefix/substring filtering |
| PRIORITY_UID_FILTER | `susfs_is_priority_uid_filtered()` | `kernel/sys.c`, `block/ioprio.c` |
| AVC log spoofing | `susfs_ksu_sid`, `susfs_priv_app_sid` | `avc.c:avc_dump_query()` |

**Call chain (A, hiding mounts as the example)**:
`open("/proc/self/mountinfo")` → `mountinfo_open()` → if `susfs_is_hide_sus_mnts_for_non_su_procs_enabled` and the current context is not the KSU domain → `susfs_show_mountinfo()` → first pass through `a226b_hide_mount_line()` → then drop entries by `mnt_id >= DEFAULT_KSU_MNT_ID` → output.

**Dependencies**: `CONFIG_KSU_SUSFS` must be enabled together with `CONFIG_KSU` (susfs.c directly externs KSU symbols); `SUSFS_VERSION` is asserted by `ci/verify-features.sh` (A).

### 5.3 Layer 3 — NoMount 2.3.0

**Provenance (A)**: byte-for-byte identical to `maxsteeel/nomount` HEAD `7914458` ("bump v2.2.0 → v2.3.0") under `kernel/src`; `NOMOUNT_BASE_VERSION "23"`.

**Structure (A)**: `nomount.c` 2234 lines, sole entry point `fs_initcall(nomount_init)`; no exported symbols; works through fs-layer redirection + inode proxying.
Symbols asserted by `ci/verify-features.sh`: `nomount_init`, `nm_hijacked_lookup`, `nm_d_revalidate`, `nm_dir_lookup`, `nm_clear_state` (A).

**Boundary with SUS_MOUNT (A/B)**:
- A: SUSFS SUS_MOUNT fakes `mnt_id` and the mount table in `fs/namespace.c`, belonging to the **view layer**;
- A: NoMount redirects dentry/inode at the VFS layer without creating real mounts, belonging to the **resolution layer**;
- B: the priority when both hit the same path (NoMount's `nm_hijacked_lookup` vs `susfs_is_inode_sus_path`) has not been tested; test cases need to be constructed.

### 5.4 Layer 4 — PathMask / procguard

**Nature (A)**: two **LKMs**, not in the kernel image; `lsmod` on the device shows `pathmask` and `procguard`; only 6 source files under `pathmask/src` are tracked by git; `.o/.ko/.cmd` are untracked leftovers (`git ls-files` count is 0).

**Mechanism (A)**: `kallsyms_name_to_addr()` resolves VFS symbols + kretprobe:
- `getattr_entry()` (`pathmask.ko`) — stat view of hidden paths;
- `getdents_entry()` — directory entry hiding;
- `procguard.c` — proc view protection;
- `verify-features.sh` checks with `nm pathmask.ko | grep getattr_entry/getdents_entry` (A).

**Risk (A/B)**: LKMs are tightly bound to the kernel vermagic; any kernel change requires a rebuild (`pathmask/build.sh`); B: the concrete deny/allow rules come from `ksu-module/*.conf`, and the actually effective set has not been checked one by one.

### 5.5 Layer 5 — Anti-detection (local policy)

This is the **only "non-upstream" layer**; all of it is A226B-specific logic (A):

| # | File:function | Behavior | Remark |
|---|---|---|---|
| 1 | `fs/proc_namespace.c:a226b_hide_mount_line()` | hides the six devnames `metadata`/`cache`/`sec_efs`/`efs`/`omr`/`loop5` | `uid<10000 \|\| uid==10296 \|\| uid==10272 \|\| !dev` passes through directly; all other UIDs are hidden |
| 2 | `fs/proc/root.c:proc_root_init()` | `/proc/fs` lowered to `0700` | prevents Chunqiu (春秋) from reading `/proc/fs/ext4`, `jbd2` to determine loop |
| 3 | `kernel/configs.c:ikconfig_init()` | `/proc/config.gz` `0640 root:system` | 0600 would make the Vintf check fail and pop up "internal error" |
| 4 | `security/selinux/ss/status.c` | `sequence=4`, `policyload=1` fixed | matches the "clean device" fingerprint |
| 5 | `kernel/sys.c` + `block/ioprio.c:ksu_hide_app_uid()` | apps calling `getpriority/ioprio` to enumerate other app UIDs get `-ESRCH` | only effective for target UIDs 10000–19999 / 90000–99999 |
| 6 | `kernel/module.c:ksu_hide_lkm_name()` | `/proc/modules` hides `pathmask`/`procguard` from app UIDs | the two names are hardcoded; renaming makes it ineffective |
| 7 | `fs/proc/version.c` + `kernel/utsname_sysctl.c` | `/proc/version`, `osrelease`, `version` spoofed consistently with `uname(2)` | prevents "the two views contradicting each other" |
| 8 | `init/main.c:setup_command_line()` | appends `androidboot.vbmeta.digest=d259db70…` | hardcoded; ineffective as soon as the boot image changes |
| 9 | `drivers/block/zram/zram_drv.c:is_bdev_avail()` | adds `!zram->bdev` | root cause of the V8 random reboots |
| 10 | `security/selinux/hooks.c:check_nnp_nosuid()` | allow the `init` → `su` domain transition | works together with su startup |

**Boundary (A)**: every filter is decided by `uid`; `uid<10000` is always allowed (fail-open); `system_server`/`zygote`/`app_zygote` are not touched.

### 5.6 Layer 6 — KPM / selinux_hook + query filtering

**Nature (A)**: **compiled into the kernel**, not loaded as a `.kpm`; both `CONFIG_SECURITY_SELINUX_HOOK_KPM=y` and `CONFIG_SECURITY_SELINUX_QUERY_FILTER=y` are in the V12 defconfig.

**Working mechanism (A)**: a comment in `kpm_hooks.c` states — "Compiled into the kernel we cannot patch text, so `hook_wrap()` only records the callbacks and the wrapped kernel functions call `kp_hook_before()/kp_hook_after()` directly."
That is: the original function is renamed to `__kp_*`, a same-name function performs the wrapping and calls the hook table directly; this is not an inline hook and does not require KernelPatch.

**12 wiring points (A, consistent with `ci/verify-features.sh`)**:

```
security_setprocattr        selinux_setprocattr        selinux_complete_init
sel_read_handle_status      sel_mmap_handle_status     sel_write_context
sel_write_access            context_struct_compute_av  string_to_context_struct
security_read_policy        selinux_status_update_policyload
simple_read_from_buffer
```

**clean-policy engine (A/B)**: `snapshot_clean_policy()` reads a clean policydb into an independent copy at boot; `/sys/fs/selinux/policy` returns the clean blob; `context_struct_compute_av()` recomputes AV using the clean policydb.
A: the code path exists and is compiled; B: whether `late_initcall(selinux_hook_kpm_builtin_init)` actually loaded the clean policy on the current device (needs dmesg `[selinux_hook]` log verification).

**Second-layer filtering (A)**: `selinux_query_filter.c` intercepts `access`/`context`/`procattr` queries at the three entry points `sel_write_access()` / `sel_write_context()` / `selinux_setprocattr()`, returning `-EINVAL` (equivalent to "this type does not exist in the clean policy"). Since V6, the `/access` response changed from `0 0 0 0 1 0` to `0 ffffffff 0 ffffffff 1 0` (A, code + verify assertion).

**Version annotation conflict (A)**: `KPM_VERSION(SELINUX_VERSION)` in `selinux_hook.c`, with `SELINUX_VERSION "1.1.7"`; but `docs/FEATURES.md` says `1.1.6.1`. **The source's 1.1.7 prevails**; the documentation needs correction.

---

## 6. Cross dependencies and callback order

```
boot
 └─ selinux_init ...................... load SELinux policy
     └─ selinux_complete_init ......... KPM wrap point (clean policy snapshot timing)
         └─ late_initcall(selinux_hook_kpm_builtin_init)
             └─ hook_wrap() × N ....... register callbacks into kp_hooks[48]
 └─ fs_initcall(nomount_init) ......... install path redirection
 └─ kernelsu_init() ................... KSU init → register feature handler
     └─ ksu_selinux_hide_init() ....... 4.14 goes through the compat version
 └─ user space ksud / susfs4ksu / nomount / pathmask push rules in order
```

Dependency constraints (A):
1. `CONFIG_KSU_SUSFS*` depends on `CONFIG_KSU` (`fs/susfs.c` externs KSU symbols);
2. `CONFIG_SECURITY_SELINUX_HOOK_KPM` depends on `KALLSYMS_ALL` (stated explicitly in Kconfig, A): `select`/`depends on SECURITY_SELINUX && KALLSYMS_ALL`;
3. the PathMask LKM depends on the kernel vermagic; reuse across kernels is forbidden;
4. the `susfs4ksu` module requires `susfs` kernel support, and the `nomount` module requires `CONFIG_NOMOUNT` (A: modules and same-named kernel capabilities correspond).

---

## 7. Known issues / unresolved items (A)

| # | Issue | Evidence | Impact |
|---|---|---|---|
| 1 | `dist/` and `v9-out` are shield-era builds | images contain `a226b_shield: proc_create failed`, `KSU_VERSION_FULL=86c40b44`; the V12 package has `90f5352c` and no shield string | **flashing them reinstalls the already-rolled-back shield** |
| 2 | the device has no boot backup | `/data/local/tmp` and `/cache` have been cleared | no way back if the flash bricks the device |
| 3 | the version number drifts with upstream | rebuild today = 41183 ≠ the manager's 41179 | manager version prompts/gating |
| 4 | the version number collapses in offline builds | falls back to the 21 commits of this repository → 37206 | manager judges the kernel as "too old" |
| 5 | `KSU_VERSION_FULL` sha points to this repository | `KernelSU/.git` is missing; git searches upward | the version string cannot represent the SukiSU source |
| 6 | `build-info.txt` hardcodes 41179 | `ci/package.sh:40` | metadata lies after a rebuild |
| 7 | KPM version annotations inconsistent | source 1.1.7 vs FEATURES.md 1.1.6.1 | documentation cannot be fully trusted |
| 8 | `.o/.ko` leftovers in `pathmask/src` | untracked files (`git ls-files` = 0) | old `.ko` may be packed by mistake |
| 9 | `ccache` not in PATH | `command -v ccache` is empty | `USE_CCACHE=1` silently has no effect |
| 10 | DirtySepolicy not really eliminated | Duck still reports 1 risk item (A: measured on device) | known unresolved; do not globally mask selinuxfs again |
| 11 | duplicate extern block in `fs/stat.c` | the same `extern susfs_is_inode_sus_kstat` declaration appears twice | harmless, code smell |
| 12 | layers without an upstream anchor | `pathmask/`, `selinux_hook_kpm/`, the anti-detection layer | can only be traced through this file + tag |

---

## 8. Build and flashing procedure (A, scripts read)

### 8.1 Build

```bash
cd /home/kazaf/a226b/gh-repo
OUT=/home/kazaf/a226b/v9-out bash ci/build.sh       # produce Image / Image.gz
OUT=/home/kazaf/a226b/v9-out bash ci/package.sh     # produce AK3 + modules + dist/Image.gz
OUT=/home/kazaf/a226b/v9-out bash pathmask/build.sh # produce pathmask.ko / procguard.ko
bash ci/verify-features.sh                           # per-item gate
```

Key points (A):
- `ci/build.sh` uses `KBUILD_BUILD_TIMESTAMP` (from `ci/repro-date.txt`) + `LOCALVERSION=` to guarantee reproducibility;
- a from-scratch build deletes `arch/arm64/include/generated` and `include/generated`, but **reusing `OUT` keeps the old `.o` files**;
- the gates of `ci/verify-features.sh`: 14 CONFIGs, KSU/SUSFS/NoMount/KPM symbols, 12 hook targets, query-filter symbols, no `kpimg`/`KernelPatch` allowed in the image, the old `/access` fingerprint, PathMask vermagic.

### 8.2 Flashing (not performed)

```bash
# 1) Back up first (the current device has no backup)
adb shell su -c 'dd if=/dev/block/by-name/boot of=/data/local/tmp/boot-v12-backup.img bs=4096'
adb shell su -c 'sha256sum /data/local/tmp/boot-v12-backup.img'
adb pull /data/local/tmp/boot-v12-backup.img
Get-FileHash .\boot-v12-backup.img -Algorithm SHA256   # compare with the local file

# 2) Recommended: flash AK3 via TWRP; if using dd:
#    dd if=Image of=/dev/block/by-name/boot bs=4096 && sync
# 3) Read back and verify the sha256 after flashing
```

Key AK3 attributes (A): `do.devicecheck=0`, `BLOCK=/dev/block/by-name/boot`, `do.modules=1`, `do.systemless=1`.

### 8.3 Rollback

- with a backup: `dd if=boot-v12-backup.img of=/dev/block/by-name/boot bs=4096 && sync`;
- without a backup: re-flash the same version of the official boot (Odin/AP), then flash the V12 zip again;
- source-level rollback: `git checkout v12` (no files were deleted; a checkout fully restores the tree).

---

## 9. Unconfirmed items (B list)

1. **Whether KPM actually installed the clean policy at runtime**: requires `dmesg | grep selinux_hook` and `cat /sys/fs/selinux/policy | head -c 32`.
2. **Which of the 12 hook points actually registered successfully on 4.14**: `hook_wrap()` returns -1 on failure; runtime logs are needed.
3. **Actual SUSFS rule set**: which sus_path/sus_kstat were pushed by `susfs4ksu` on the device has not been checked one by one.
4. **NoMount rule set**: the `/data/adb/modules/nomount` configuration has not been read line by line.
5. **PathMask deny/allow rules**: `pathmask/ksu-module/*.conf` has not been compared with the actual files on the device.
6. **Side effects of disabling `CONFIG_SECURITY_DEFEX`**: the stock defex LSM is turned off; whether this affects some Samsung security features has not been evaluated.
7. **Hardcoded UIDs in `a226b_hide_mount_line()`**: 10296/10272 are the currently installed UIDs; they change after uninstalling and reinstalling the detectors, and there is no automatic discovery mechanism.
8. **Behavior of `KSU_VERSION_FULL` after `KernelSU/.git` is restored**: if someone puts `.git` back, the sha becomes a real KSU commit and no longer matches historical builds.
9. **Whether the upstream builtin branch has functional commits after f9aa2fec**: only that snapshot was compared this time.
10. **Whether the nnp_nosuid allowance in `security/selinux/hooks.c`** expands the attack surface: semantically it is "allow the init→su exec", but no threat modeling was done.

---

## 10. Things that must not be done (carried over and confirmed in code)

1. Do not globally mask selinuxfs — KPM/query_filter both filter **by query type**, not by disabling the filesystem;
2. Do not touch UID 1000 / system_server / zygote / app_zygote — all existing filters let `uid<10000` through;
3. Do not modify the PackageManager database;
4. Do not modify system apps;
5. Do not globally spoof uname — SUSFS has one place; `proc/version`/sysctl are "the same spoofed value in the image", not a second spoofing layer;
6. Do not reintroduce `a226b_shield` / LSPosed companion modules (level C, rolled back, grep across the whole tree finds 0 hits);
7. Must back up boot before flashing;
8. All filters are fail-open (`a226b_hide_mount_line()` returns `uid<10000 → false` on its first line);
9. Read the code before changing it.

---

## 11. Master table of fact levels

| Conclusion | Level |
|---|---|
| 47 modified / 0 deleted / list of added subtrees | A |
| which files and functions each of the six layers lands in | A |
| KernelSU = builtin@f9aa2fec + 5 compatibility changes | A |
| NoMount = byte-for-byte identical to upstream | A |
| KPM is an in-kernel shim with 12 wiring points | A |
| `task_mmu.c`/`open.c` have hooks | A |
| `dist/`+`v9-out` are shield leftovers | A |
| version number formula and drift | A |
| KPM clean policy effective at runtime | B |
| actual rule sets of SUSFS/NoMount/PathMask | B |
| side effects of disabling DEFEX | B |
| impact of later upstream builtin commits | B |

---

## 12. Reproducing this audit

```bash
# 1) extract the baseline
unzip -p SM-A226B_13_Opensource.zip Kernel.tar.gz | tar -xz -C /home/kazaf/a226b/stock-v12
# 2) full diff (see §2.1)
diff -rq --no-dereference ... /home/kazaf/a226b/stock-v12 /home/kazaf/a226b/gh-repo > audit/rq.txt
# 3) per-file patches
bash .audit/04_patch.sh        # generate audit/perfile/*.diff and audit/v12.patch
# 4) KSU identity
git clone --depth 1 --branch builtin https://github.com/SukiSU-Ultra/SukiSU-Ultra /tmp/k
diff -rq /tmp/k/kernel /home/kazaf/a226b/gh-repo/KernelSU/kernel
# 5) NoMount identity
git clone --depth 50 https://github.com/maxsteeel/nomount /tmp/n
diff -rq /tmp/n/kernel/src /home/kazaf/a226b/gh-repo/NoMount/kernel/src   # expect 0 lines
```

---

## 13. Appendix: corrections this audit makes to existing documentation

| Old claim | Correction | Level |
|---|---|---|
| `fs/proc/task_mmu.c` has no maps filtering | It does: SUSFS SUS_MAP hides the vma/smaps/pagemap of marked inodes | A |
| `fs/open.c` has no hook | It does: `faccessat` hooks KSU + `filename_lookup()` | A |
| the kernel source is a "bare snapshot with no anchor" | the anchor was found: builtin@f9aa2fec | A |
| selinux_hook 1.1.6.1 | the source labels it 1.1.7 | A |
| `docs/README-V12.md` | does not exist; the repo `docs/` only has BUILD/FEATURES/CHANGELOG | A |
| the repo has `ksu-module/` | does not exist (`ls` reports No such file) | A |
| `KernelSU` is a submodule | it is not: a normal directory committed directly into the repository | A |
