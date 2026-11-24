# SM-A226B Repository master guide

> [中文](REPO-MASTER.md) | **English**

> For whoever takes over maintenance: one page to understand what the repository is, what it consists of, how to build it, how it works at runtime, and what must not be touched.
> Details (function-level descriptions of the 47 changed files) are in [ARCHITECTURE-V12.md](ARCHITECTURE-V12.md).
> Current baseline: **V13** (`v13` tag), kernel 4.14.186 / a22x / SM-A226B.

---

## 1. Identity

| Item | Value |
|---|---|
| Repository | `kazaf080726/SM-A226B-SukiSU-Ultra-Kernel` |
| HEAD | `v13` (source content identical to `v12` = `9877efbd4`) |
| Commits | 21 (`v1`–`v7` point to the same import commit `061b7fcef`) |
| Scale | 78,141 tracked files, 1.2 GB working tree, `.git` about 430 MB |
| Branch | `main`; `shield-archive-20261115` is abandoned history (grade C, do not use) |

From V8 onward there is one commit per release; V1–V6 can only be traced through `docs/CHANGELOG.md` and cannot be diffed.

---

## 2. Provenance

| Component | Upstream | Local changes |
|---|---|---|
| Kernel body | Samsung `SM-A226B_13_Opensource` 4.14.186 | 47 files, 0 deletions |
| KernelSU | SukiSU-Nova `builtin` @ `4bec532d` (originally SukiSU-Ultra) | 4 modified + 1 added, all 4.14 compatibility |
| NoMount | `maxsteeel/nomount` @ `7914458` (v2.3.0) | `kernel/src` byte-for-byte identical |
| SUSFS | NonGKI 4.14 port patches (`patches/`) | 10 configs + 4.14.186 adaptations |
| selinux_hook | Admire's KPM 1.1.7 source | compiled into the kernel + KernelPatch API shim |
| PathMask / anti-detection / CI | original to this project | — |

Re-verify upstream identity:

```bash
git clone --depth 1 --branch builtin https://github.com/SukiSU-Nova/SukiSU-Nova /tmp/k
diff -rq /tmp/k/kernel KernelSU/kernel                 # expect only 5 differing lines

git clone --depth 50 https://github.com/maxsteeel/nomount /tmp/n
diff -rq /tmp/n/kernel/src NoMount/kernel/src          # expect 0 lines
```

---

## 3. Directory map

```
arch/arm64/configs/a22x_defconfig   sole config entry (+15 switches; DMESG_RESTRICT=y; DEFEX off)
fs/ kernel/ mm/ security/ ...       47 host files (list in ARCHITECTURE-V12.md §3)
KernelSU/                           built-in SukiSU (drivers/kernelsu -> ../KernelSU/kernel)
NoMount/                            built-in NoMount (fs/nomount -> ../NoMount/kernel/src)
pathmask/                           PathMask/procguard LKM source + KSU module config
security/selinux/selinux_hook_kpm/  in-kernel implementation of the 12 hooks
security/selinux/selinux_query_filter.c  always-on v4 SELinux query filter
ci/                                 build / package / verify / fetch_toolchain / repro-date
anykernel/                          AnyKernel3 template (BLOCK=/dev/block/by-name/boot)
patches/                            port sources and historical patches (not part of the build)
docs/                               BUILD / FEATURES / CHANGELOG / ARCHITECTURE-V12 / this file
```

---

## 4. Six-layer runtime

| Layer | Entry | Key files | Switch |
|---|---|---|---|
| KernelSU | `kernelsu_init()` | `KernelSU/kernel/ksu_main.c` + 7 `ksu_handle_*` hosts | `CONFIG_KSU` |
| SUSFS 2.4.0 | `susfs_init` + `CMD_SUSFS_*` ioctl | `fs/susfs.c` + 16 hosts | `CONFIG_KSU_SUSFS*` |
| NoMount 2.3.0 | `fs_initcall(nomount_init)` | `NoMount/kernel/src/nomount.c` | `CONFIG_NOMOUNT` |
| PathMask | module load | `pathmask/src/*.c` (kretprobe + kallsyms) | loaded at runtime |
| Anti-detection | each syscall/proc entry | `fs/proc_namespace.c`, `ss/status.c`, `selinux_query_filter.c`, etc. | fixed at compile time |
| KPM / selinux_hook | `late_initcall(selinux_hook_kpm_builtin_init)` | `selinux_hook_kpm/` + 12 wrapper points | `CONFIG_SECURITY_SELINUX_HOOK_KPM` |

Runtime self-checks:

```bash
su -c 'ksud debug version'          # Kernel Version: 30007
su -c 'ksud susfs features'         # 10 items
su -c 'ksud feature list'           # su_compat/kernel_umount/adb_root/selinux_hide
su -c 'lsmod | grep -E "pathmask|procguard"'
su -c 'dmesg | grep -i selinux_hook'
```

---

## 5. Build and gates

```bash
OUT=/path/to/out bash ci/build.sh       # reproducible build; writes $OUT/build.log and $OUT/ksu-version.txt
OUT=/path/to/out bash ci/package.sh     # AK3 + modules + dist/Image.gz
OUT=/path/to/out bash pathmask/build.sh # PathMask LKM (vermagic must match)
bash ci/verify.sh && bash ci/verify-features.sh && bash ci/verify-log.sh build.log
```

- `ci/build.sh`: sanitizes PATH, pins `KBUILD_BUILD_*`, sets `LOCALVERSION=""`, cleans `generated` from the source tree, sets `LDFLAGS_BUILD_ID=`; extracts the SukiSU version number from the log for packaging.
- `ci/package.sh`: the `KSU_VERSION` in `build-info.txt` is taken from `$OUT/ksu-version.txt` (no longer hard-coded); entry mtimes and zip order are fixed.
- Gates: image ≤28 MiB, PathMask vermagic, 65 feature checks, zero `warning:`/`error:` in the build log.
- CI: triggered by pushes to `main` / `v*` tags; tag builds automatically publish a Release.

**Note**: `KSU_VERSION = 30000 + upstream main commit count + 996` (same formula as the manager versionCode;
can be overridden with `KSU_VERSION_OVERRIDE`), computed online at compile time by `KernelSU/kernel/Makefile`.
The same source compiled on different dates yields different version numbers; offline builds fall back to this repository's commit count (the value collapses).
This is the cost of "the kernel always aligning with the latest manager", not a bug.

---

## 6. Configuration matrix

| Capability | Switch | Dependency |
|---|---|---|
| Root | `CONFIG_KSU=y` | — |
| SUSFS | `CONFIG_KSU_SUSFS` + 9 sub-items | requires `CONFIG_KSU` |
| NoMount | `CONFIG_NOMOUNT=y` | — |
| Query filtering | `CONFIG_SECURITY_SELINUX_QUERY_FILTER=y` | `SECURITY_SELINUX` |
| Built-in KPM | `CONFIG_SECURITY_SELINUX_HOOK_KPM=y` | `SECURITY_SELINUX && KALLSYMS_ALL` |

---

## 7. Invariants (must be followed before changing code)

1. Filtering always fails open; `uid < 10000` is never filtered;
2. Never block selinuxfs globally, intercept only by query type;
3. Do not touch `system_server` / `zygote` / `app_zygote` / UID 1000;
4. Do not change the PackageManager database, do not change system apps;
5. Do not reintroduce `a226b_shield` or the LSPosed companion module;
6. Before flashing you must back up boot and read it back to verify;
7. When replacing the boot image, `vbmeta.digest` in `init/main.c` must be updated in sync;
8. When replacing the kernel you must rebuild PathMask (vermagic binding);
9. When updating the KernelSU source you must keep the 5 4.14 compatibility changes;
10. Detector UIDs (10296 Chunqiu / 10272 Hunter) are hard-coded and change on reinstall.

---

## 8. Known issues

| Issue | Description |
|---|---|
| sha of `KSU_VERSION_FULL` | `KernelSU/.git` does not exist, so git walks up to this repository's HEAD, which does not represent the SukiSU commit |
| Offline build version collapse | falls back to this repository's commit count (21), yielding values such as 37206 |
| `dist/` historical leftovers | shield-era builds were once put here; flashing them equals rolling back shield; they have been moved to `dist-shield-20261115` |
| `preinit` disabled | `usr/include` is not generated, so the KSU preinit stage is not compiled in (this has been the case since V12) |
| `qf_enabled()` always true | the query filter is effectively always on, which contradicts the comment "only when the feature is enabled" |
| `security/selinux/ss/status.c` | `sequence=4`/`policyload=1` are hard-coded clean values |

---

## 9. Troubleshooting quick reference

| Symptom | Check first |
|---|---|
| Build reports `static_call.h` / `kvcalloc` / `strncpy_from_user_nofault` | whether KernelSU's 4.14 compatibility layer was overwritten |
| Module fails to load | whether PathMask was rebuilt for the current kernel (vermagic) |
| Boot stuck at the logo | the initialization order of `selinux_hook` / `nomount` in `dmesg` |
| Detector reports a mount anomaly | whether that UID is in the 10296/10272 exemption table |
| Manager reports a version mismatch | whether `ksud debug version` matches the APK versionCode |

---

## 10. Documentation index

| File | Contents |
|---|---|
| `docs/ARCHITECTURE-V12.md` | full diff, function-level descriptions of the 47 files, A/B fact grades |
| `docs/BUILD.md` | dependencies, toolchain, reproducible build, CI |
| `docs/FEATURES.md` | feature list and verification methods |
| `docs/CHANGELOG.md` | change log for V1–V13 |
| `patches/README.md` | port sources and completed items |
