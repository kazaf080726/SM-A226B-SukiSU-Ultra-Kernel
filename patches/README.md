# 集成改动说明 / Integration notes

本仓库基于三星官方开源包 `SM-A226B_13_Opensource` 中的 `Kernel.tar.gz`
（Linux 4.14.186，MT6833，a22x_defconfig）。

相对原厂源码的改动分为三部分：

## 0. 现状（V13）

本目录记录的是**历史移植来源**；当前仓库实际组件版本以 defconfig 与 `docs/ARCHITECTURE-V12.md` 为准：

| 组件 | 当前版本 |
|---|---|
| SukiSU-Ultra | builtin 分支，KSU_VERSION 41183（编译期计算） |
| SUSFS | v2.4.0（10 个 feature 全开） |
| NoMount | v2.3.0（NOMOUNT_BASE_VERSION 23） |
| selinux_hook | 1.1.7（编入内核，非 .kpm） |
| PathMask | 4.14 移植（LKM） |

## 1. SUSFS（4.14 Non-GKI 移植，基线补丁 v2.3.0，现为 v2.4.0）

基线补丁来自（GPL-2.0）：

- https://github.com/JackA1ltman/NonGKI_Kernel_Build_2nd
  文件：`Patches/Patch/susfs_patch_to_4.14.patch`（SUSFS v2.3.0）
- 该补丁由 simonpunk 的 `gki-android12-5.10` 分支深度移植到 4.14

应用方式：在 4.14.186 源码根目录 `patch -p1 < susfs_patch_to_4.14.patch`，
随后对 4 个文件（fs/notify/fdinfo.c、fs/proc/task_mmu.c、fs/super.c、
fs/namespace.c）的拒绝 hunk 做了 4.14.186 适配。

## 2. KernelSU/BakaSU 内联钩子

使用同一项目提供的 `Patches/susfs_inline_hook_patches.sh`（SUSFS v2.3 版）
给以下文件加入 BakaSU 需要的钩子：

- fs/exec.c            ksu_handle_execveat / _sucompat / post
- fs/open.c            ksu_handle_faccessat
- fs/read_write.c      ksu_handle_sys_read
- fs/stat.c            ksu_handle_stat / ksu_handle_vfs_fstat
- fs/namei.c           filename_lookup 导出
- drivers/input/input.c ksu_handle_input_handle_event
- kernel/reboot.c      ksu_handle_sys_reboot
- security/selinux/hooks.c  init -> su domain 放行

额外手工修改：

- kernel/sys.c：`ksu_handle_setresuid()` 钩子（脚本因当时未 clone
  `drivers/kernelsu` 而跳过，这里补上）
- fs/read_write.c：BakaSU 的 `ksu_handle_sys_read()` 是 3 参数版本，
  调用处补 `&buf, &count`
- fs/proc/task_mmu.c：void 函数中的 `return 0` 改为 `return`
- fs/notify/fdinfo.c：`mask` 声明移到函数开头（C 语言 label 后不能跟声明）

## 3. NoMount v2.3.0

使用上游 `kernel/setup.sh` 集成：`fs/nomount -> ../NoMount/kernel/src`，
并在 `fs/Kconfig`、`fs/Makefile` 中加入 `CONFIG_NOMOUNT`。

## 4. defconfig

在 `arch/arm64/configs/a22x_defconfig` 末尾追加：

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

## 后续补齐

- priority UID filter：**V12 起已启用**（`CONFIG_KSU_SUSFS_PRIORITY_UID_FILTER=y`，`susfs_is_priority_uid_filtered()` 接入 `getpriority/setpriority/ioprio`）；
- SELinux 策略级 selinux_hide：4.14 无 `struct selinux_policy`，改由内置 selinux_hook clean-policy 引擎承担（`feature/selinux_hide_compat.c` 只提供 feature 开关）。

其余路径/挂载/stat/maps/uname/cmdline/open_redirect/avc 日志隐藏均已生效。
