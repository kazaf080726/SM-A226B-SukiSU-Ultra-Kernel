# PathMask 4.14 port notes

> [中文](README.md) | **English**

This directory is a 4.14 Non-GKI port of
[Andrea-lyz/LKM-PathMask](https://github.com/Andrea-lyz/LKM-PathMask) v2.8.3. Its
KernelSU module shell (service.sh, WebUI, settings) is kept; only the kernel-side
`.ko` modules were replaced.

## What changed

The upstream module targets GKI 5.10+ (DDK/KMI) and relies on KMSDK + KallRecon to
recover kallsyms from the stripped Image, hooking `__arm64_sys_*` entry points that
only exist on 5.10+. For 4.14:

1. **Dropped KallRecon/KMSDK**
   - On 4.14, `kallsyms_lookup_name`, `kern_path` and `path_put` are exported
     symbols, so they are used directly instead of scanning the kallsyms table.
2. **Hooks moved to the real 4.14 functions**
   - `inode_permission` is only a thin wrapper on 4.14 and calls
     `__inode_permission`; the hook targets `__inode_permission`.
   - `vfs_getattr` calls `vfs_getattr_nosec`; the hook targets `vfs_getattr_nosec`.
   - `SyS_getdents64` replaces `__arm64_sys_getdents64`.
3. **Syscall fallback prefix adaptation**
   - `__arm64_sys_*` → `SyS_*`; `faccessat2` / `openat2` do not exist on 4.14 and
     registration failure is skipped automatically; `openat` is skipped at
     registration time because `close_fd` is missing, avoiding fd leaks.
   - 4.14 has no ThinLTO, so `__inode_permission` / `vfs_getattr_nosec` are not
     inlined away and the main hiding path does not depend on syscall hooks.
4. **Small 4.14 API compatibility**
   - Added `regs_set_return_value` (arm64 `regs->regs[0]`).
   - Added `<linux/ratelimit.h>`.
5. **procguard ported as well**: it only needs the `in_group_p` kretprobe and works
   directly.

## Preserved features

- `target_paths` / `target_path` path hiding
- `scope_mode=global/deny/allow`, `deny_uids`, `hide_isolated`
- `hide_dirents` (getdents64 filtering)
- `write_op_policy` (passthrough/eacces/enoent)
- `procguard.ko` (gid 3009 / AID_READPROC isolated-process fix)
- Upstream KernelSU module shell: persistent config, WebUI, hot reload, diagnostics

## Limitations

- 4.14 has no `faccessat2` / `openat2`; those two syscall hooks are not registered.
- The `openat` hook registers normally; the `write_op_policy=eacces` O_CREAT check
  was adapted to the 4.14 `SyS_*` argument layout (the old code dereferenced
  `regs[0]` as pt_regs and panicked with an invalid fd of -100).
- Write-side syscall hooks (mkdirat/unlinkat/renameat/renameat2/linkat/symlinkat)
  use the 4.14 `SyS_*` entries with real register arguments.
- Module vermagic must match the kernel exactly, so the `.ko` files are produced in
  the same CI pass as the kernel; do not mix versions.
- PathMask and SUSFS/NoMount all change VFS visibility; hiding the same path with
  several of them at once is stacked and has not been combination-tested.
