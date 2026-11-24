# Changelog

> [中文](CHANGELOG.md) | **English**

Chinese is the primary documentation language; this is the full English translation.

> ## ⬆ Major Update: V28 — 2026-11-24
> **Remove the KPM shim and implement deep SELinux hiding inside the kernel with the same logic as LKM (main branch).**
> Maple Ultra: ALL CLEAR (the SELinux side-channel item disappeared entirely); Hunter 6.72: no risk found.
> See [V28](#v28--2026-11-24----major-update) / [RELEASE-v28](RELEASE-v28.md).

## V28 — 2026-11-24 · ⬆ Major Update


**Topic (major update): remove the KPM shim and implement deep SELinux hiding in the 4.14 kernel with the same logic as LKM (main branch); this is the largest refactor of the hiding layer since this project's V12 architecture.**

V27's "pass" was an illusion: Maple's oracle is briefly "clean" when the carrier 10s binding times out,
but the real probes still identify the device as Root. V28 redoes it following the LKM implementation:

### Removed
- Delete `security/selinux/selinux_hook_kpm/` (the KPM hot-patch shim, 10 files, 4.7k lines),
  `include/linux/selinux_hook_kpm.h`, and `security/selinux/selinux_query_filter.{c,h}`;
- Revert the 12 kernel functions rewritten by the KPM inline patches (selinuxfs/services/status/hooks/
  security/libfs), and remove the `__kp_*` wrappers and the `CONFIG_SECURITY_SELINUX_HOOK_KPM/
  QUERY_FILTER` options;
- `status.c` keeps Samsung stock's 0/0 behavior + the fake status page from the App's perspective (V27 semantics, retained).

### Added (LKM semantics, built into 4.14)
- `security/selinux/ksu_selinux_hide.h` + the `ss/services.c` engine:
  - `ksu_selinux_hide_capture_clean_policy()`: **before** KernelSU rewrites the live policy
    (at the `apply_kernelsu_rules()` entry), serialize the stock policydb and independently parse out
    `ksu_clean_policy{policydb, sidtab}` and the original policy blob;
  - `ksu_selinux_hide_context_to_sid/sid_to_context/compute_av()`: resolve contexts and compute AVs
    entirely on the clean policydb (`seqno=1`), equivalent to the main branch's
    `my_write_context/my_write_access`;
- `selinuxfs.c`: at compile time, replace `write_op[SEL_ACCESS/SEL_CONTEXT]` with
  `ksu_sel_write_access/ksu_sel_write_context`; when an App (uid≥10000) reads
  `/sys/fs/selinux/policy`, return the clean blob (same source for mmap/read);
- `hooks.c`: for an App writing `/proc/self/attr/current`, `selinux_setprocattr()` handles the
  hidden context by returning **-EINVAL** (that type does not exist in the clean policydb, matching the
  parse-failure semantics of LKM `my_setprocattr`).

### Key evidence (Maple process heap + kernel query loop)
- When the carrier probe writes `/proc/self/attr/current` and gets `EPERM(1)`, DirtySepolicy records it
  as `INCONCLUSIVE` → "SELinux policy access anomaly"; after changing it to **EINVAL(22)** it becomes
  `NORMAL_EINVAL`.
- Previously KSU/Magisk/LSPosed types were detected because the live policy and Samsung stock types such as
  `su` were directly exposed in policy/context queries; once the clean policydb answers, they all return `EINVAL`.

### Test results (final clean build, already flashed to the device)
- Maple Ultra 2.8.2: **Perfect! ALL CLEAR, 282/282 items perfect, 0 dangerous / 0 suspicious** (34.8s);
- Hunter 6.72: **No risk found, no problems were found on your device!** (108 items, high risk 0 / not evaluated 4 /
  unavailable 0 / timed out 0);
- `dmesg`: `ksu_selinux_hide: captured clean policy (1522149 bytes)`;
- `/sys/fs/selinux/status`: root = the real page, App = `1 0 1 0 1`;
- No diagnostic nodes such as `/proc/ksu_hide` (the clean build has removed the debug hooks).

### Addendum: supercall side-channel convergence (second commit the same day)

- `KernelSU/kernel/supercall/dispatch.c`: an app (uid≥10000) call to `reboot(0xdeadbeef, ...)`
  returns `-EINVAL` directly, consistent with stock;
- `kernel/reboot.c`: an app (uid≥10000) never enters `ksu_handle_sys_reboot()`,
  the SUSFS/supercall paths for root/ksud are preserved (manager and module functionality verified working);
- Conclusion: Maple's "APatch supercall key-length timing ratio" and "kernel supercall trait
  kernelsu_traits_count" come from timing/behavior observation of the reboot supercall hook; bypassing it on the app side
  reduces exposure, but on the A22 this item itself, per Maple's own documentation, "may appear unstably due to
  performance fluctuations" — testing showed the same kernel alternating between ALL CLEAR in consecutive runs and an occasional 1 item, unrelated to SELinux hiding.

## V27 — 2026-11-24

**Topic: eliminate Maple/DuckDetector's "SELinux policy compatibility/query service anomaly" (contingency plan one)**

Investigation conclusions (cross-checked against the Samsung stock `t24` source + DirtySepolicy criteria + Maple process-heap evidence):

1. Since V7, `security/selinux/ss/status.c` has been changed to "modern-kernel clean values"
   (`sequence=4`, `policyload=1`, with the seqlock increment frozen), whereas **Samsung stock 4.14 uses
   `sequence=0` and `policyload=0`**. DuckDetector's clean baseline for kernels `<6.10` is exactly 0/0,
   so the status page was judged as "Modern initial status counters".
2. KPM `selinux_hook` runs the original functions under the clean policydb for access/context writes,
   which also changes **the caller's permission-check order**: malformed queries return `EINVAL(22)`, while stock returns
   `EACCES(13)` (recorded by Maple's native oracle as `control0..3 malformed vs stock`).

Fix:
- `status.c` restores the Samsung stock logic (initial 0/0 + normal seqlock increments + policyload=seqno);
- `selinuxfs.c`'s `sel_open_handle_status()` returns, for **App (uid≥10000)**, a
  fake status page: `version=1, sequence=0, enforcing=1, policyload=0, deny_unknown=1`,
  with read and mmap seeing the same page; root/manager still reads the real page;
- KPM's `before_sel_write_access()` / `before_sel_write_context()` remove the
  clean-policydb redirect (the dirty-context/pairing filter and seqno fix are kept), restoring stock's
  permission-check and parse order.

Test results (after flashing V27):
- `/sys/fs/selinux/status`: both root and App views show `1 0 1 0 1`;
- Maple Ultra daily run: 285 items, **SELinux side-channel item gone** (283 perfect);
- The remaining APatch supercall timing and KernelSU side-channel items are historically known fluctuating items, unrelated to this fix.

Follow-up (plan two): deprecate KPM and port the complete `selinux_hide.c` from the main/LKM branch
(self-answered write_op + fake status page + LSM hook) into the 4.14 builtin.

## V26 — 2026-11-23

**Topic: fix the kernel panic caused by the `selinux_hook` clean-policy redirect (BUG at services.c:686)**

Symptom: the device rebooted by itself around 2026-11-23 20:39, `sys.boot.reason=kernel_panic`.
pstore / `last_kmsg` capture:

```
kernel BUG at security/selinux/ss/services.c:686!
context_struct_compute_av+0x570  (BUG_ON(!sattr))
security_compute_av -> avc_compute_av -> avc_has_perm
sel_write_access+0xb8
pid 17728 comm=sh uid=10285
```

Root cause:
- The built-in `selinux_hook` (`security/selinux/selinux_hook_kpm/selinux_hook.c`) decided by
  **current UID** whether to switch AV computation to the clean policydb;
- `su <app UID>` (KernelSU's uid switch) creates a process with **app UID + ksu domain**;
- This combination has no corresponding ksu type in the clean snapshot, so `flex_array_get(type_attr_map_array,
  scontext->type - 1)` returns NULL and the `BUG_ON(!sattr)` in stock code panics immediately.

Fix (two layers):
1. `before_context_struct_compute_av_policydb()`: before redirecting to the clean policydb,
   use the new `clean_policydb_contexts_valid()` to check that the source/target context type IDs are within
   the clean snapshot's `nprim` and `type_attr_map_array`; otherwise fall back to the live policydb;
2. `security/selinux/ss/services.c`: change `BUG_ON(!sattr)/BUG_ON(!tattr)` to
   range checks + `pr_warn_ratelimited` + fail-closed return, so any out-of-range context only causes
   this AV computation to be rejected, and no longer reboots the whole device.

Verification: the fix markers are in `vmlinux` (`clean redirect refused` / `compute_av context type out of range`),
`VERIFY_OK`, 65/65 gates passed. Re-test command after flashing:
`su 10285 -c 'echo u:r:untrusted_app:s0 u:object_r:ksu_file:s0 file read > /sys/fs/selinux/access'`
should return EINVAL instead of panicking.

## V25 — 2026-11-23

**Topic: kernel-level dynamic mount exemption (definitive fix for the "mount gap")**

Cause (measured): the mount hiding in `fs/proc_namespace.c` exempted 春秋/Hunter by UID, while the UIDs are hard-coded.
The device had the detectors reinstalled on 2026-11-22/23:

| App | Old UID | Measured UID |
|---|---|---|
| com.chunqiunativecheck | 10265 | **10303** |
| com.zhenxi.hunter | 10267 | **10281** |

On the same device, the number of visible stock mounts among the 6 read from `/proc/self/mountinfo` by different UIDs:
root=6, **10303=0**, 10265=6, **10281=0** — 春秋 saw 0, so it reported a "mount gap".

Fix (kernel-level, no longer relying on compile-time UIDs):
- Add `a226b_refresh_exempt_uids()`: the kernel reads `/data/system/packages.list` directly,
  parses the current UIDs by package-name prefix `com.chunqiu` / `com.zhenxi`, caching up to 8;
- Refresh it when `mounts_open_common()` opens the mount table, throttled to 30 seconds; normal reads only use the in-memory cache;
- `a226b_hide_mount_line()` now looks up the cache; the old UIDs (10296/10272/10265/10267/10303/10281/
  10298/10310) are kept as a fallback for when `/data` is not yet readable;
- This Samsung 4.14 tree has been backfilled with the newer `kernel_read()` signature (`ssize_t kernel_read(file, buf, count, &pos)`),
  and the code calls it accordingly.

Verification: `VERIFY_OK`, 65/65 gates passed; after flashing
`su 10303 -c 'grep -cE "by-name/(metadata|cache|sec_efs|efs|omr)|loop5" /proc/self/mountinfo'`
should return 6 (currently 0).

## V23 — 2026-11-22

**Topic: align with manager 35081 (translation commit, kernel unchanged)**

- Manager **35081** (= 30000 + 4085 + 996), whose commit completes the pt-BR translation
  (SukiSU-Nova/SukiSU-Nova#1060), changing only resource files under `manager/`;
- The public repositories are still unchanged: `main`/`dev`=`1439545f`, `builtin`=`4bec532d`,
  `manager-ci` tag=`c60f21b9`;
- This release only rebuilds: `KSU_VERSION_OVERRIDE=35081`; the kernel code is exactly the same as V22.

Verification: `VERIFY_OK`, 65/65 gates passed, version string `v3.0.0-7f22b889@main`.

## V22 — 2026-11-22

**Topic: align with the Telegram-channel manager 35073 (no source update)**

- Manager versionCode **35073** (= 30000 + 4077 commits + 996), 3 build commits more than 35070;
- The public repositories are unchanged: `main`=`1439545f`, `builtin`=`4bec532d`, `dev`=`1439545f`,
  `manager-ci` tag=`c60f21b9`; the build commits for 35070/35073 are not on public branches and are CI-private commits;
- At the binary level (against the 35070 package) uapi 6, the 8 features, and the SUSFS command set are identical, so no kernel change is needed;
- This release only rebuilds: `KSU_VERSION_OVERRIDE=35073`.

Verification: `VERIFY_OK`, 65/65 gates passed, version string `v3.0.0-26ba349c@main`.

## V21 — 2026-11-22

**Topic: align with the new Telegram-channel manager 35070 (no source update)**

- New manager: `SukiSU_Nova_v3.0.0-r1_35070` (ksud `manager-ci-1-g33f33d36`, uapi still 6);
  the previously installed package is `35061` (ksud `g08145d34`);
- Compared with the public repositories: `main`=`1439545f`, `builtin`=`4bec532d`, `dev`=`1439545f` are all unchanged;
  the new package's build commit `33f33d36` cannot be found in the public repositories (`manager-ci` now points to `c60f21b9`),
  and is an unreleased CI commit, so **there is no kernel source to sync**;
- Binary comparison: uapi 6, the 8 features (including `avc_spoof`), and the SUSFS command set completely match the old package,
  so only the kernel `KSU_VERSION` needs to be aligned to **35070**;
- Plugins: KernelSU / SUSFS / NoMount / PathMask all have no updates (V20 is already the latest).

Verification: `VERIFY_OK`, 65/65 gates passed, image version string `v3.0.0-ee9421d4@main`.

## V20 — 2026-11-22

**Topic: new NoMount upstream fix + manager v3.0.0-r1**

- Manager upgraded to **v3.0.0-r1**, but the versionCode is still **35061** (build number unchanged), so the kernel side needs no version change;
- NoMount kernel driver synced to upstream `6849f00` (*kernel: resolve virtual backing paths*):
  add `nm_unwrap_virtual_backing()`, which unwraps nested virtual inodes while resolving backing paths,
  avoiding validation failures when a rule points at another NoMount virtual path; only `nomount.c` changed (41 lines),
  and `NoMount/` is now byte-identical to upstream;
- Plugin audit: KernelSU `builtin@4bec532d`, the SUSFS 4.14 patch (md5 unchanged), and PathMask v2.8.3
  **all have no updates**;
- This release includes V19's mount-exemption UID fix (春秋 10265 / Hunter 10267).

Verification: `VERIFY_OK`, 65/65 feature checks passed, image version string `v3.0.0-b3b854eb@main`.

## V19 — 2026-11-21

**Topic: fix 春秋's "mount gap" — the UIDs changed after a factory reset**

Cause: the mount hiding in `fs/proc_namespace.c` exempted 春秋 and Hunter by UID, but the exemption values were hard-coded
(10296 / 10272). After the factory reset on 2026-11-21 the two apps were assigned new UIDs:
`com.chunqiunativecheck` = **10265** and `com.zhenxi.hunter` = **10267**.
So 春秋 was no longer exempted, `metadata`/`cache`/`efs`/`omr`/`loop5` were hidden, and after comparing with the stock layout
it reported a "mount gap". Measured (same device):

| UID | Visible count of the 6 target mounts |
|---|---|
| 0 | 6 |
| 10265 (春秋's actual) | 0 |
| 10267 (Hunter's actual) | 0 |
| 10296 / 10272 (old values) | 6 |

Fix: change the exemption table to four constants (keeping both old and new UIDs so restoring an old backup still works),
and split the check into the fail-open `uid < 10000 || !dev` part and the exemption check.

Verification: `VERIFY_OK`, 65/65 gates passed; after flashing, run
`su 10265 -c 'grep -cE "by-name/(metadata|cache|sec_efs|efs|omr)|loop5" /proc/self/mountinfo'`
and it should return 6.

## V18 — 2026-11-21

**Topic: add the KSU feature `avc_spoof` (ID 10003) and fix "kernel does not support this feature"**

Root cause: AVC log spoofing is a **KernelSU feature** in SukiSU-Nova (`avc_spoof`, ID 10003),
not a SUSFS feature. The manager queries it by ID; our `builtin@4bec532d` snapshot does not contain this feature,
so `ksud feature list` returns `[NOT_SUPPORTED] avc_spoof (ID=10003)`, and the UI naturally shows "not supported".
The earlier V17 enable-by-default of the SUSFS static key only made the kernel-side code executable; it did not make the manager recognize the feature.

Fix:
- `KernelSU/uapi/feature.h` + `kernel/include/uapi/feature.h`: add
  `KSU_FEATURE_AVC_SPOOF = 10003` (same ID as upstream);
- Add `kernel/feature/avc_spoof_compat.c` + `avc_spoof.h`: register the feature handler,
  get/set directly toggles `susfs_is_avc_log_spoofing_enabled` (the actual rewriting is already in
  `security/selinux/avc.c`, equivalent to upstream's `slow_avc_audit` kprobe approach);
- `policy/feature.c`: register the name `avc_spoof` for 10003 (otherwise name-based lookup fails);
- `extension/extension.c`: rename `forced_mask`/`forced_pending` to `ext_*` to avoid
  a conflict with feature.c's same-named bitmap in the same compilation unit after `KSU_FEATURE_MAX` grows;
- The V17 "enable at boot" in `selinux.c` has been removed and is now managed by the feature (default enabled=1,
  and can still be switched off from the manager).

Verification: the image contains the `avc_spoof` symbol and strings, 65/65 gates passed; after flashing
`ksud feature list` should show `[ENABLED] avc_spoof (ID=10003)`.

## V17 — 2026-11-21

**Topic: AVC log spoofing enabled by default + version alignment to 35061**

- `KernelSU/kernel/selinux/selinux.c`: in `susfs_set_batch_sid()`, after sid resolution succeeds,
  automatically call `static_branch_enable(&susfs_is_avc_log_spoofing_enabled)`.
  Upstream enables it only when userspace sends `CMD_SUSFS_ENABLE_AVC_LOG_SPOOFING`; the local KSU module has been
  removed at the user's request, so nobody sends this ioctl anymore, leaving the spoofing branch in `security/selinux/avc.c` never executed.
  It is now self-enabled at kernel boot and can still be turned off via ioctl;
- What it does: if `tcontext` in an AVC audit log is `u:r:ksu:s0`, replace it with
  `u:r:priv_app:s0:c512,c768` (and replace `tsid` with priv_app's sid too), preventing the log from exposing the root domain;
- Manager versionCode is now **35061**; the build uses `KSU_VERSION_OVERRIDE=35061`; uapi is still 6;
- Plugin audit: Nova `builtin@4bec532d`, the SUSFS 4.14 patch (md5 unchanged), NoMount `dcd77d03`,
  and PathMask v2.8.3 **all have no updates**.

Verification: `VERIFY_OK`, 65/65 feature checks passed; the image contains the
`avc log spoofing enabled at boot` string. After flashing, confirm with
`dmesg | grep -i "avc log spoofing"`.

## V16 — 2026-11-21

**Topic: NoMount 2.3.1 + kernel version aligned with manager 35049**

- NoMount kernel driver upgraded from v2.3.0 to upstream **v2.3.1** (`dcd77d0`): Kconfig `select KEYS`,
  export_operations proxy, UID bloom widened to 16 words, boot and rule-setup paths streamlined;
  `NoMount/kernel/src` is byte-identical to upstream (no local changes);
- Manager versionCode is now **35049** (its version number increments with releases and no longer equals the Nova repo commit-count formula);
  the kernel aligns via `KSU_VERSION_OVERRIDE=35049`, and `uapi` is still 6;
- SUSFS: the upstream 4.14 patch is the same as V15 (identical md5), no update;
- PathMask: upstream `Andrea-lyz/LKM-PathMask` is still at v2.8.3, no update;
- KernelSU: Nova `builtin` is still `4bec532d`, no code update.

Verification: `VERIFY_OK`, 65/65 feature checks passed; `build-info` shows NoMount v2.3.1 / KSU 35049.

## V15 — 2026-11-19

**Topic: SUSFS 4.14 patch synced with upstream + kernel version aligned with the manager**

- The SUSFS 4.14 patch is synced to the upstream `NonGKI_Kernel_Build_2nd` state as of 2026-11-18 (the version number is still v2.4.0,
  but it contains substantive fixes): SUS_PATH now uses an `i_state` marker, `susfs_kvmalloc`, `kernel_read` signature detection,
  removal of `susfs_get_data_path`, etc.;
- Replace `fs/susfs.c`, `fs/namei.c`, `fs/readdir.c`, `include/linux/susfs{,_def}.h` with the new upstream versions;
  keep the local changes: the `susfs_contains()` helper (used for symbol hiding in `kernel/kallsyms.c`),
  the `filename_lookup()` export (used by KSU faccessat/stat), and the NoMount line in `fs/Makefile`;
- Change the version formula in `KernelSU/kernel/Makefile` to match the manager: `30000 + upstream main commit count + 996`
  (it used to be `- 4001`, so kernel 30007 and manager 35000+ could never match);
- Add the build-time override switch `KSU_VERSION_OVERRIDE`; this time it is aligned to the current manager as **35040**;
- Other plugins have no updates: NoMount `master` is still `7914458` (v2.3.0); PathMask is a local implementation with no upstream.

Pending on-device verification; `ksud susfs features` should still show 10 items.

## V14 — 2026-11-18

**Topic: follow SukiSU's rename to SukiSU-Nova (full adaptation)**

- Upstream `SukiSU-Ultra` was renamed **`SukiSU-Nova`**, and the built-in source is synced to `builtin@4bec532d`;
- Package name: `com.sukisu.ultra` → **`com.sukisu.nova`** (kernel `KSU_PACKAGE_NAME` default);
  the old Ultra manager is no longer recognized;
- Version formula: `40000+N-2815` → **`30000+N-4001`**; upstream main currently has 4008 commits →
  `KSU_VERSION` **30007**, full version string `v3.0.0-<sha>@main` (the manager v3.0.0 minimum requirement is exactly `v3.0.0`);
- uapi version is still **6**, matching manager v3.0.0, so no new interfaces need porting;
- The 701 lines of new uapi on the `main` branch (new manager / sulog / selinux commands) belong to the GKI line,
  non-GKI 4.14 uses `builtin`, so they are not introduced this time;
- Keep all 5 4.14 compatibility changes (KPM build exclusion, `static_call.h` guard, `ksu_kvcalloc`,
  `strncpy_from_user_nofault` alias, `selinux_hide_compat.c`);
- Update the version-string assertion in `ci/verify-features.sh` from `v4.3.0-r2` to `v3.0.0`.

## V13 — 2026-11-15

**Topic: rebuild following upstream (no kernel source changes)**

- The SukiSU version number advances with upstream `main` (3994 → 3998 commits), `KSU_VERSION` 41179 → **41183**,
  exactly matching device manager versionCode 41183;
- The kernel source is exactly the same as V12 (same commit `9877efbd4`), and the feature set is unchanged;
- Fix `ci/package.sh` hard-coding `KSU_VERSION` to 41179: `ci/build.sh` now extracts the real value from the build log
  and writes it to `$OUT/ksu-version.txt`, which packaging reads;
- Add `docs/ARCHITECTURE-V12.md` (full diff and function-level audit) and `docs/REPO-MASTER.md` (repository overview);
- Correct outdated version numbers and "not ported" descriptions in README / FEATURES / patches.

On-device: flashed and boots normally; `ksud debug version` = 41183, all 10 SUSFS features enabled.


## V8 — 2026-11-14

### Fixes

- **zram LRU writeback NULL deref (root cause of random reboots)**:
  `is_bdev_avail()` only checked `zram->bdev->bd_disk` and not `zram->bdev`.
  Samsung's LRU writeback still calls it when no backing device is configured, and
  `zram_add_to_writeback_list()` is reached with `zram->bdev == NULL`, so accessing `NULL + 0x80`
  Oopses directly. A `!zram->bdev` check was added to skip safely when writeback is not configured.
- **PathMask 4.14 write-class hooks**:
  - The `O_CREAT` check for `write_op_policy=eacces` now uses 4.14's `regs[2]`,
    no longer dereferencing `dfd` as `pt_regs` (the `0xffffffac` panic);
  - Write-class syscall hooks now use the `SyS_*` entry points and read paths from the real registers;
  - Verified on-device that `touch`/`mkdir` return `EACCES` with no panic.

Everything else is the same as V7: SukiSU 41179, SUSFS 2.4.0, NoMount 2.3.0, PathMask 4.14,
selinux_hook with built-in KPM, and no KPatch/kpimg.


## V7 Follow-up Fix — PathMask 4.14 write-class hooks

Fix two problems in the PathMask 4.14 port that can cause kernel panics:

- With `write_op_policy=eacces`, `sys_openat_entry` dereferenced `regs[0]` as a
  `struct pt_regs *`; 4.14's `SyS_openat` arguments are the real
  `(dfd, filename, flags, mode)`; with dfd=-100 it accessed `0xffffffac` and oopsed.
  4.14 now reads `O_CREAT` directly from `regs[2]`.
- The write-class syscall hooks (mkdirat/unlinkat/renameat/renameat2/linkat/symlinkat)
  still hard-coded `__arm64_sys_*`, failed to register on 4.14, and read their arguments as 5.x pt_regs.
  They now uniformly use `SyS_*` and read paths from the real registers.

Verified on-device: `touch` / `mkdir` return `EACCES` on hidden targets with no panic;
`rmmod` works normally.


This file records **what was updated and what was fixed** in each version from V1 to V7. The Release page has the corresponding flashable packages.

---

## V7 — 2026-11-14

**Topic: NoMount 2.2.0 → 2.3.0**

Updates:

- NoMount kernel driver upgraded to upstream `v2.3.0-720` (commit `ab93ee3`), `NOMOUNT_BASE_VERSION "23"`;
- Rules are now inode-based: they still point to the same data after the target is renamed/moved, and rebind automatically after unlink;
- Pre-opened backing file cache: read-only redirected open+read+close is close to native performance;
- Virtual inodes stay resident in the dcache, and revalidate is now O(1);
- The readdir cursor carries a hash fragment, so concurrent rule additions/removals do not skip or duplicate entries.

Fixes:

- OOB read in `nm_select_rule` with a single rule + `--uid-range`;
- UAF/hang issues such as leaf RCU retirement, virtual dentry `s_op` restore ordering, and `nm clear rules` blocking;
- xattr processing goes through LSM checks, virtual inodes clear SUID/SGID, and unlink/rename/rmdir on virtual dentries are taken over by NoMount;
- Cross-sb proxy inodes, fixing FUSE / user-namespace bind mount crashes and NULL dentry dereferences;
- The kernfs superblock no longer hijacks `s_op`, fixing the `cgroup2` panic.

Other components (SukiSU 41179, SUSFS 2.4.0, PathMask, selinux_hook, V6's `/access` fix) are unchanged.

---

## V6 — 2026-11-14

**Topic: remove the fixed-response fingerprint of `/sys/fs/selinux/access`**

Fixes:

- KPM 1.1.6.1's `write_clean_access_seqno_response()` used to hard-code the response as
  `0 0 0 0 1 0`; this is an old selinux_hook fingerprint documented in public material,
  and native detectors judge it as FolkPatch/APatch;
- The v4 native filter layer `security/selinux/selinux_query_filter.c` also contained the same string;
- Both were changed to the real kernel shape:

  ```
  0 ffffffff 0 ffffffff 1 0
  ```

  (allowed, 0xffffffff, auditallow, auditdeny, seqno=1, flags)
- seqno is still fixed at the clean value, and DirtySepolicy's seqno criterion remains hidden.

Result: NativeCheck's `FolkPatch/APatch` and `Suspicious issue found when detecting SELinux Policy`
both disappeared, with all KPM features retained.

---

## V5 — 2026-11-14

**Topic: selinux_hook 1.1.6.1 full source compiled in**

Updates:

- Compile the 4521-line / 124-function KPM source into the kernel (`security/selinux/selinux_hook_kpm/`);
- Add the `hook_wrap/unhook` registry shim and the `hook_fargsN_t` argument block;
- Wire up the 12 real hooks: `sel_write_access`, `sel_write_context`,
  `security_read_policy`, `simple_read_from_buffer`, `sel_read_handle_status`,
  `sel_mmap_handle_status`, `selinux_status_update_policyload`,
  `security_setprocattr`, `selinux_setprocattr`, `context_struct_compute_av`,
  `string_to_context_struct`, `selinux_complete_init`;
- Enable the clean-policy engine: boot-time snapshot, independent policydb, policy/AV/context redirection;
- 4.14 has no `selinux_status_update_seqlock` / `selinux_policy_commit`,
  skipped via `if (addr)` as in the source (N/A).

Result: `Suspicious issue found when detecting SELinux Policy` disappeared; however, the fixed `/access` response
caused `FolkPatch/APatch` to regress intermittently (fixed in V6).

---

## V4 — 2026-11-14

**Topic: status spoofing + always-on native filter layer**

Updates:

- `security/selinux/ss/status.c`: `sequence` fixed at 4, `policyload` fixed at 1;
- Add `security/selinux/selinux_query_filter.c`: always-on filtering of access/context,
  seqno probes, and procattr (hard-coded su/apd/apatch/kpatch/kpm/kernelsu/susfs);
- Add `CONFIG_SECURITY_SELINUX_QUERY_FILTER=y` to defconfig.

Result: paved the way for the V5 clean-policy engine; v4 alone still did not fully solve SELinux detection.

---

## V3 — 2026-11-14

**Topic: remove KPatch-Next / kpimg**

Fixes:

- The image no longer appends the KPatch-Next `kpimg`;
- The KPatch-Next module / `kpatch` command / selinux_hook `.kpm` loader are no longer needed;
- The `FolkPatch/APatch` detection disappeared.

Remaining: `Suspicious issue found when detecting SELinux Policy` is still present.

---

## V2 — 2026-11-13

**Topic: fix the "hide SELinux modifications" switch + KPM dual builds**

Updates:

- Fix the SukiSU manager's "hide SELinux modifications" switch being unusable on 4.14 and not linking to KPM when enabled;
- Provide two AK3 packages: the standard build (no KPM) and the KPM build (kpimg + KPatch-Next + selinux_hook module);
- Add native UID hiding and compatibility-layer fixes (`ksu_kvcalloc`, `strncpy_from_user_nofault`, etc.).

Result: boots; NativeCheck reports `FolkPatch/APatch` (kpimg) and `SELinux Policy`.

---

## V1 — 2026-11-13

**Topic: initial integration**

- Samsung `SM-A226B_13_Opensource` 4.14.186 baseline;
- SukiSU-Ultra `builtin` v4.3.0-r2 / 41179;
- SUSFS v2.4.0 (NonGKI 4.14 port + BakaSU 4.14.186 fixes, 18 files);
- NoMount 2.2.0 built in;
- PathMask 4.14 LKM (`pathmask.ko` + `procguard.ko`);
- AnyKernel3: `do.devicecheck=0`, `BLOCK=/dev/block/by-name/boot`, no init_boot.

Result: boots, basic functionality works; later versions build SELinux anti-detection on top of this.
