# V28 Release Notes — Major Update

**Date:** 2026-11-24  
**Kernel:** Linux 4.14.186 (Samsung SM-A226B / Galaxy A22 5G)  
**Root:** SukiSU-Nova builtin · KSU_VERSION 35081  
**Tag:** `v28`

> [中文](RELEASE-v28.md) | **English**

---

## ⬆ Why this is a major update

V28 replaces the project's most critical layer — SELinux hiding — from a **KPM
hot-patch shim** with **an in-kernel implementation shared with the LKM (main
branch) tree**. This is an architectural change, not a small patch:

- removed the KPM shim (10 files, ~4.7k lines) and the old native query filter layer;
- restored the 12 kernel functions that had been rewritten by inline patching;
- added a 4.14 built-in clean-policydb engine (policy snapshot, self-answered
  queries, policy blob, setprocattr semantics);
- hiding behaviour now matches the OnePlus Ace 5 (5.10+ LKM) `selinux_hide.c`.

## Changes

### Removed
- `security/selinux/selinux_hook_kpm/`
- `include/linux/selinux_hook_kpm.h`
- `security/selinux/selinux_query_filter.{c,h}`
- `CONFIG_SECURITY_SELINUX_HOOK_KPM` / `CONFIG_SECURITY_SELINUX_QUERY_FILTER`

### Added (4.14 builtin, LKM semantics)
- `ksu_selinux_hide_capture_clean_policy()`: snapshots the pristine policydb
  before KernelSU patches the live policy and produces an independent
  `policydb + sidtab + policy blob`;
- clean-policy `context_to_sid / sid_to_context / compute_av(seqno=1)`;
- `selinuxfs`: `SEL_ACCESS/SEL_CONTEXT` write nodes are answered by the clean
  engine; app reads of `/sys/fs/selinux/policy` (including mmap) return the clean
  blob;
- `hooks.c`: app writes of hidden contexts to `/proc/self/attr/current` return
  `-EINVAL`;
- `kernel/reboot.c` + `supercall/dispatch.c`: app-side reboot supercall behaves
  exactly like stock;
- `status.c`: stock 0/0 behaviour + app-side fake status page `1 0 1 0 1`.

## Verification (real SM-A226B)

| Item | Result |
|---|---|
| Maple Ultra 2.8.2 | **Perfect! ALL CLEAR** (283/283 and 284/284 perfect, 0 dangerous / 0 suspicious) |
| Maple SELinux side-channel item | **Gone in every run** after the fix |
| Hunter 6.72 | **No risk found** (108 items, 0 high risk / 0 unavailable / 0 timeouts) |
| SukiSU Nova manager | Working (12 superusers / 12 modules, Built-in) |
| Status page | root sees the real page; apps see `1 0 1 0 1` |
| Stability | Boots normally, no panics; boot backups kept |

> Note: on the A22, Maple's APatch/KernelSU **timing** checks can appear
> intermittently under load (Maple's own evidence states it may be unstable due
> to performance fluctuation). This is unrelated to SELinux hiding; re-test on an
> idle device.

## Artifacts

- `A226B-SukiSU-SUSFS240-NoMount-v28-20261114.zip` (AnyKernel3)
- `A226B-SukiSU-SUSFS240-NoMount-v28-modules-20261114.zip` (modules)
- `A226B-PathMask-4.14-20261114.zip`
- `Image.gz` / `SHA256SUMS.txt`

SHA256 sums are in `SHA256SUMS.txt` next to the artifacts.

## Upgrade steps

1. Back up the current boot partition first (TWRP or `dd if=/dev/block/by-name/boot`);
2. Flash `A226B-...-v28-*.zip` (AnyKernel3, `do.devicecheck=0`);
3. The first boot captures the clean policy once — this is expected;
4. Open the manager and confirm "Working / Built-in / 35081".
