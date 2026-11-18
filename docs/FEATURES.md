# 功能清单 / Feature inventory

本文件说明这个内核改了什么、每个功能落在哪里、怎么验证。
基线：三星官方 `SM-A226B_13_Opensource`（Linux 4.14.186，MT6833 / a22x）。

```
Layer 0  三星原厂 4.14.186
Layer 1  SUSFS v2.4.0（Non-GKI 4.14 移植）        fs/susfs.c + 18 个内核文件
Layer 2  SukiSU-Nova builtin 30007              KernelSU/ -> drivers/kernelsu
Layer 3  NoMount v2.3.0（VFS 重定向）             NoMount/ -> fs/nomount
Layer 4  PathMask 4.14（LKM）                     pathmask/
Layer 5  selinux_hook 1.1.7（源码内置）          security/selinux/selinux_hook_kpm/
Layer 6  原生反检测层                              selinux_query_filter.c + ss/status.c
```

## 1. SUSFS v2.4.0（`CONFIG_KSU_SUSFS`）

- 版本：`include/linux/susfs.h` → `SUSFS_VERSION "v2.4.0"`，4.14 为 `NON-GKI`；
- 实现：`fs/susfs.c`、`include/linux/susfs.h`、`include/linux/susfs_def.h`；
- 已开启：SUS_PATH、SUS_MOUNT、SUS_KSTAT、SPOOF_UNAME、
  SPOOF_CMDLINE_OR_BOOTCONFIG、OPEN_REDIRECT、SUS_MAP、
  HIDE_KSU_SUSFS_SYMBOLS、PRIORITY_UID_FILTER、ENABLE_LOG；
- 用户态接口：`CMD_SUSFS_*` ioctl（`susfs_def.h`），由 susfs4ksu 模块下发。

## 2. SukiSU-Nova（`CONFIG_KSU`，原 SukiSU-Ultra）

- `KernelSU/` 源码，`drivers/kernelsu -> ../KernelSU/kernel`；
- 版本：v3.0.0 / KSU_VERSION 30007（`30000+main-4001`）；
- 4.14 兼容：`static_call.h` 守卫、`ksu_kvcalloc`、`strncpy_from_user_nofault`、
  KPM 扩展在 <5.10 编出、`selinux_hide_compat.c`。

## 3. NoMount v2.3.0（`CONFIG_NOMOUNT`）

- `NoMount/`，`fs/nomount -> ../NoMount/kernel/src`；
- 版本：`NOMOUNT_BASE_VERSION "23"`；
- 2.3.0 特性：inode 基准规则、预打开 backing file 缓存、O(1) revalidate、
  readdir 并发修复、OOB/UAF 修复、xattr LSM 检查、跨 sb 代理 inode、启动提速。

## 4. PathMask 4.14（`pathmask/`）

- `pathmask.ko`：路径隐藏；
- `procguard.ko`：进程/UID 保护；
- 与内核同一次构建，vermagic 匹配；KSU 模块 WebUI 配置。

## 5. selinux_hook 1.1.7（内置）

- 源码：`security/selinux/selinux_hook_kpm/`
  （`selinux_hook.c` 4521 行 + `kpm_hooks.c` 注册表 + 自包含头文件）；
- 参数块：`include/linux/selinux_hook_kpm.h`；
- 入口：`late_initcall(selinux_hook_kpm_builtin_init)`；
- 接线方式：原函数改名 `__kp_*`，同名静态包装，`kp_hook_before/after` 分发；
- 12 个 hook 点见 `docs/CHANGELOG.md` V5。

### clean-policy 引擎

- 启动时 `snapshot_clean_policy()`：读取干净策略 → `policydb_read` 到独立 policydb；
- `/sys/fs/selinux/policy` 返回 clean blob；
- `context_struct_compute_av` 用 clean policydb 重算 AV；
- `string_to_context_struct` / context 存在性走 clean 数据；
- `selinux_complete_init` 包装作为生命周期兜底。

## 6. 原生反检测层

- `security/selinux/selinux_query_filter.c`（`CONFIG_SECURITY_SELINUX_QUERY_FILTER`）：
  access/context 过滤、seqno 探针、procattr，常开；
- `security/selinux/ss/status.c`：`sequence=4`、`policyload=1`；
- `/access` 应答：`0 ffffffff 0 ffffffff 1 0`（真内核形状，V6 起）；
- v4 过滤器与 KPM 双层，互不依赖。

## 7. 无 KPatch / kpimg

- 镜像中 `strings Image | grep -icE 'kpimg|KernelPatch'` = 0；
- 不需要 KPatch-Next、`kpatch`、`.kpm` 加载器；
- 4.14 不存在的 `selinux_status_update_seqlock` / `selinux_policy_commit`
  在 KPM 源码里按 `if (addr)` 跳过（N/A，非删功能）。

## 8. 构建开关与验证

```bash
grep -E 'CONFIG_(KSU|NOMOUNT|SECURITY_SELINUX)' out/.config
nm out/vmlinux | grep -E 'hook_wrap|snapshot_clean_policy|selinux_hook_kpm_builtin_init'
nm out/vmlinux | grep -w nomount_init
strings out/arch/arm64/boot/Image | grep -icE 'kpimg|KernelPatch'   # 0
```
