# 更新记录 / Changelog

## V7 后续修复 — PathMask 4.14 写类 hook

修复 PathMask 4.14 移植里两个会导致内核 panic 的问题：

- `sys_openat_entry` 在 `write_op_policy=eacces` 时把 `regs[0]` 当成
  `struct pt_regs *` 解引用；4.14 的 `SyS_openat` 参数是真实的
  `(dfd, filename, flags, mode)`，dfd=-100 时会访问 `0xffffffac` 并 oops。
  现在 4.14 直接读 `regs[2]` 的 `O_CREAT`。
- 写类 syscall hook（mkdirat/unlinkat/renameat/renameat2/linkat/symlinkat）
  仍硬编码 `__arm64_sys_*`，4.14 上注册失败且参数按 5.x pt_regs 读取。
  现在统一使用 `SyS_*`，并按真实寄存器取路径。

已实机验证：`touch` / `mkdir` 在隐藏目标上返回 `EACCES`，无 panic；
`rmmod` 正常。


本文件记录 V1–V7 每个版本**更新了什么、修复了什么**。Release 页面有对应卡刷包。

---

## V7 — 2026-11-14

**主题：NoMount 2.2.0 → 2.3.0**

更新：

- NoMount 内核驱动升级到上游 `v2.3.0-720`（commit `ab93ee3`），`NOMOUNT_BASE_VERSION "23"`；
- 规则改为 inode 基准：目标 rename/move 后仍指向同一数据，unlink 后自动重绑；
- 预打开 backing file 缓存：只读重定向 open+read+close 接近原生性能；
- 虚拟 inode 常驻 dcache，revalidate 改 O(1)；
- readdir 游标带哈希片段，并发增删规则不跳项/不重复。

修复：

- 单规则 + `--uid-range` 时 `nm_select_rule` 的 OOB 读；
- leaf RCU 退休、虚拟 dentry 恢复 `s_op` 顺序、`nm clear rules` 阻塞等 UAF/卡死问题；
- xattr 走 LSM 检查、虚拟 inode 清 SUID/SGID、虚拟 dentry 的 unlink/rename/rmdir 由 NoMount 接管；
- 跨 sb 代理 inode，修 FUSE / user-namespace bind mount 崩溃与空 dentry 解引用；
- kernfs 超级块不再劫持 `s_op`，修 `cgroup2` panic。

其余组件（SukiSU 41179、SUSFS 2.4.0、PathMask、selinux_hook、V6 的 `/access` 修复）不变。

---

## V6 — 2026-11-14

**主题：修掉 `/sys/fs/selinux/access` 固定应答指纹**

修复：

- KPM 1.1.6.1 的 `write_clean_access_seqno_response()` 原先把应答写死为
  `0 0 0 0 1 0`，这是公开资料记录过的旧版 selinux_hook 指纹，
  native 检测器会把它判为 FolkPatch/APatch；
- v4 原生过滤层 `security/selinux/selinux_query_filter.c` 里也有一份同样字符串；
- 两处统一改为真内核形状：

  ```
  0 ffffffff 0 ffffffff 1 0
  ```

  （allowed、0xffffffff、auditallow、auditdeny、seqno=1、flags）
- seqno 仍固定为干净值，DirtySepolicy 的 seqno 判据继续被隐藏。

结果：NativeCheck 的 `FolkPatch/APatch` 与 `检测SELinux Policy时发现可疑问题`
全部消失，KPM 全功能保留。

---

## V5 — 2026-11-14

**主题：selinux_hook 1.1.6.1 全量源码编入**

更新：

- 把 4521 行 / 124 函数的 KPM 源码编进内核（`security/selinux/selinux_hook_kpm/`）；
- 新增 `hook_wrap/unhook` 注册表垫片、`hook_fargsN_t` 参数块；
- 12 个真实 hook 接线：`sel_write_access`、`sel_write_context`、
  `security_read_policy`、`simple_read_from_buffer`、`sel_read_handle_status`、
  `sel_mmap_handle_status`、`selinux_status_update_policyload`、
  `security_setprocattr`、`selinux_setprocattr`、`context_struct_compute_av`、
  `string_to_context_struct`、`selinux_complete_init`；
- 启用 clean-policy 引擎：启动快照、独立 policydb、policy/AV/context 重定向；
- 4.14 无 `selinux_status_update_seqlock` / `selinux_policy_commit`，
  按源码 `if (addr)` 跳过（N/A）。

结果：`检测SELinux Policy时发现可疑问题` 消失；但 `/access` 固定应答
导致 `FolkPatch/APatch` 间歇回归（V6 修）。

---

## V4 — 2026-11-14

**主题：status 伪造 + 常开原生过滤层**

更新：

- `security/selinux/ss/status.c`：`sequence` 固定 4、`policyload` 固定 1；
- 新增 `security/selinux/selinux_query_filter.c`：常开过滤 access/context、
  seqno 探针、procattr（硬编码 su/apd/apatch/kpatch/kpm/kernelsu/susfs）；
- defconfig 增加 `CONFIG_SECURITY_SELINUX_QUERY_FILTER=y`。

结果：为 V5 的 clean-policy 引擎铺路；单独 v4 仍未彻底解决 SELinux 检测。

---

## V3 — 2026-11-14

**主题：移除 KPatch-Next / kpimg**

修复：

- 镜像内不再追加 KPatch-Next `kpimg`；
- 不再需要 KPatch-Next 模块 / `kpatch` 命令 / selinux_hook `.kpm` 加载器；
- `FolkPatch/APatch` 检测消失。

遗留：`检测SELinux Policy时发现可疑问题` 仍在。

---

## V2 — 2026-11-13

**主题：修“隐藏 SELinux 修改”开关 + KPM 双版本**

更新：

- 修 SukiSU 管理器“隐藏 SELinux 修改”开关在 4.14 上不可用、开启后不联动 KPM 的问题；
- 提供两个 AK3 包：标准版（无 KPM）与 KPM 版（kpimg + KPatch-Next + selinux_hook 模块）；
- 新增原生 UID 隐藏、兼容层修复（`ksu_kvcalloc`、`strncpy_from_user_nofault` 等）。

结果：可开机；NativeCheck 报 `FolkPatch/APatch`（kpimg）与 `SELinux Policy`。

---

## V1 — 2026-11-13

**主题：初始集成**

- Samsung `SM-A226B_13_Opensource` 4.14.186 基线；
- SukiSU-Ultra `builtin` v4.3.0-r2 / 41179；
- SUSFS v2.4.0（NonGKI 4.14 移植 + BakaSU 4.14.186 修复，18 个文件）；
- NoMount 2.2.0 内置；
- PathMask 4.14 LKM（`pathmask.ko` + `procguard.ko`）；
- AnyKernel3：`do.devicecheck=0`、`BLOCK=/dev/block/by-name/boot`、无 init_boot。

结果：可开机，基础功能可用；后续版本在此之上做 SELinux 反检测。
