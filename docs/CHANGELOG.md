# 更新记录 / Changelog

## V22 — 2026-11-22

**主题：对齐 Telegram 渠道管理器 35073（源码无更新）**

- 管理器 versionCode **35073**（= 30000 + 提交数 4077 + 996），比 35070 多 3 个构建提交；
- 公开仓库未变：`main`=`1439545f`、`builtin`=`4bec532d`、`dev`=`1439545f`、
  `manager-ci` 标签=`c60f21b9`；35070/35073 的构建提交均不在公开分支，属 CI 私有提交；
- 二进制层面（对 35070 包）uapi 6、feature 8 项、SUSFS 命令集一致，无需内核改动；
- 本版仅重编：`KSU_VERSION_OVERRIDE=35073`。

验证：`VERIFY_OK`、65/65 门禁通过，版本串 `v3.0.0-26ba349c@main`。

## V21 — 2026-11-22

**主题：对齐 Telegram 渠道的新管理器 35070（源码无更新）**

- 新管理器：`SukiSU_Nova_v3.0.0-r1_35070`（ksud `manager-ci-1-g33f33d36`，uapi 仍 6）；
  已装旧包是 `35061`（ksud `g08145d34`）；
- 与公开仓库对比：`main`=`1439545f`、`builtin`=`4bec532d`、`dev`=`1439545f` 均未变；
  新包的构建 commit `33f33d36` 在公开仓库查不到（`manager-ci` 标签现指向 `c60f21b9`），
  属未公开的 CI 提交，**没有内核源码可同步**；
- 二进制对比：uapi 6、feature 8 项（含 `avc_spoof`）、SUSFS 命令集与旧包完全一致，
  因此只需把内核 `KSU_VERSION` 对齐到 **35070**；
- 插件：KernelSU / SUSFS / NoMount / PathMask 均无更新（V20 已是最新）。

验证：`VERIFY_OK`、65/65 门禁通过，镜像版本串 `v3.0.0-ee9421d4@main`。

## V20 — 2026-11-22

**主题：NoMount 上游新修复 + 管理器 v3.0.0-r1**

- 管理器升级到 **v3.0.0-r1**，但 versionCode 仍是 **35061**（构建号未变），内核侧不需要改版本；
- NoMount 内核驱动同步到上游 `6849f00`（*kernel: resolve virtual backing paths*）：
  新增 `nm_unwrap_virtual_backing()`，在解析 backing 路径时解开嵌套的虚拟 inode，
  避免规则指向另一个 NoMount 虚拟路径时校验失败；只改 `nomount.c`（41 行），
  `NoMount/` 现与上游逐字节一致；
- 插件盘点：KernelSU `builtin@4bec532d`、SUSFS 4.14 补丁（md5 未变）、PathMask v2.8.3
  **均无更新**；
- 本版包含 V19 的挂载豁免 UID 修复（春秋 10265 / Hunter 10267）。

验证：`VERIFY_OK`、65/65 feature 检查通过，镜像版本串 `v3.0.0-b3b854eb@main`。

## V19 — 2026-11-21

**主题：修春秋“挂载间隙”——出厂重置后 UID 变了**

原因：`fs/proc_namespace.c` 的挂载隐藏按 UID 豁免春秋和 Hunter，但豁免值是硬编码的
（10296 / 10272）。2026-11-21 出厂重置后两个应用重新分配了 UID：
`com.chunqiunativecheck` = **10265**、`com.zhenxi.hunter` = **10267**。
于是春秋不再被豁免，`metadata`/`cache`/`efs`/`omr`/`loop5` 被隐藏，它比对原厂布局后
报“挂载间隙”。实测（同一台设备）：

| UID | 6 个目标挂载可见数 |
|---|---|
| 0 | 6 |
| 10265（春秋实际） | 0 |
| 10267（Hunter 实际） | 0 |
| 10296 / 10272（旧值） | 6 |

修复：豁免表改为四个常量（新旧 UID 都保留，恢复旧备份也能用），并把判断拆成
`uid < 10000 || !dev` 的 fail-open 与豁免判断两段。

验证：`VERIFY_OK`、65/65 门禁通过；刷入后用
`su 10265 -c 'grep -cE "by-name/(metadata|cache|sec_efs|efs|omr)|loop5" /proc/self/mountinfo'`
应返回 6。

## V18 — 2026-11-21

**主题：补上 KSU feature `avc_spoof`（ID 10003），修“内核不支持此特性”**

根因：AVC 日志伪装在 SukiSU-Nova 里是 **KernelSU 的 feature**（`avc_spoof`，ID 10003），
不是 SUSFS 的功能。管理器按 ID 查询；我们的 `builtin@4bec532d` 快照里没有这个 feature，
`ksud feature list` 返回 `[NOT_SUPPORTED] avc_spoof (ID=10003)`，UI 自然显示“不支持”。
V17 之前把 SUSFS 的静态键默认打开只是让内核侧代码可执行，并没有让管理器识别到 feature。

修复：
- `KernelSU/uapi/feature.h` + `kernel/include/uapi/feature.h`：加入
  `KSU_FEATURE_AVC_SPOOF = 10003`（与上游 ID 一致）；
- 新增 `kernel/feature/avc_spoof_compat.c` + `avc_spoof.h`：注册 feature 处理器，
  get/set 直接开关 `susfs_is_avc_log_spoofing_enabled`（真正的改写已在
  `security/selinux/avc.c` 里，等价于上游的 `slow_avc_audit` kprobe 方案）；
- `policy/feature.c`：为 10003 注册名字 `avc_spoof`（否则按名字查询失败）；
- `extension/extension.c`：`forced_mask`/`forced_pending` 改名 `ext_*`，避免
  `KSU_FEATURE_MAX` 增大后与 feature.c 的同名位图在同一编译单元冲突；
- `selinux.c` 里 V17 的“启动即启用”已移除，改由 feature 管理（默认 enabled=1，
  仍可用管理器开关关闭）。

验证：镜像内含 `avc_spoof` 符号与字符串，65/65 门禁通过；刷入后
`ksud feature list` 应显示 `[ENABLED] avc_spoof (ID=10003)`。

## V17 — 2026-11-21

**主题：AVC 日志伪装默认启用 + 版本对齐 35061**

- `KernelSU/kernel/selinux/selinux.c`：在 `susfs_set_batch_sid()` 里，sid 解析成功后
  自动 `static_branch_enable(&susfs_is_avc_log_spoofing_enabled)`。
  上游只在用户态发 `CMD_SUSFS_ENABLE_AVC_LOG_SPOOFING` 时才打开；本机 KSU 模块已按用户
  要求删除，没人再发这个 ioctl，导致 `security/selinux/avc.c` 的伪装分支永远不执行。
  现在改为内核启动时自启用，仍可用 ioctl 关闭；
- 该功能作用：AVC 审计日志里若 `tcontext` 是 `u:r:ksu:s0`，替换为
  `u:r:priv_app:s0:c512,c768`（`tsid` 也替换为 priv_app 的 sid），避免日志暴露 root 域；
- 管理器 versionCode 到 **35061**，构建用 `KSU_VERSION_OVERRIDE=35061`；uapi 仍 6；
- 插件盘点：Nova `builtin@4bec532d`、SUSFS 4.14 补丁（md5 未变）、NoMount `dcd77d03`、
  PathMask v2.8.3 **均无更新**。

验证：`VERIFY_OK`、65/65 feature 检查通过，镜像内含
`avc log spoofing enabled at boot` 字符串。刷入后可用
`dmesg | grep -i "avc log spoofing"` 确认。

## V16 — 2026-11-21

**主题：NoMount 2.3.1 + 内核版本号对齐管理器 35049**

- NoMount 内核驱动由 v2.3.0 升级到上游 **v2.3.1**（`dcd77d0`）：Kconfig `select KEYS`、
  export_operations 代理、UID bloom 扩到 16 words、启动与规则建立路径精简；
  `NoMount/kernel/src` 与上游逐字节一致（本地无改动）；
- 管理器 versionCode 已到 **35049**（其版本号随发布递增，不再等于 Nova 仓库提交数公式）；
  内核通过 `KSU_VERSION_OVERRIDE=35049` 对齐，`uapi` 仍为 6；
- SUSFS：上游 4.14 补丁与 V15 相同（md5 一致），无更新；
- PathMask：上游 `Andrea-lyz/LKM-PathMask` 最新仍为 v2.8.3，无更新；
- KernelSU：Nova `builtin` 仍为 `4bec532d`，无代码更新。

验证：`VERIFY_OK`、65/65 feature 检查通过，`build-info` 显示 NoMount v2.3.1 / KSU 35049。

## V15 — 2026-11-19

**主题：SUSFS 4.14 补丁同步上游 + 内核版本号对齐管理器**

- SUSFS 4.14 补丁同步到上游 `NonGKI_Kernel_Build_2nd` 2026-11-18 状态（版本号仍为 v2.4.0，
  但包含实质修复）：SUS_PATH 改用 `i_state` 标记、`susfs_kvmalloc`、`kernel_read` 签名检测、
  移除 `susfs_get_data_path` 等；
- `fs/susfs.c`、`fs/namei.c`、`fs/readdir.c`、`include/linux/susfs{,_def}.h` 换为上游新版；
  保留本地改动：`susfs_contains()` 辅助函数（`kernel/kallsyms.c` 符号隐藏使用）、
  `filename_lookup()` 导出（KSU faccessat/stat 使用）、`fs/Makefile` 的 NoMount 行；
- `KernelSU/kernel/Makefile` 版本公式改为与管理器一致：`30000 + 上游 main 提交数 + 996`
  （原来是 `- 4001`，导致内核 30007 与管理器 35000+ 永远对不上）；
- 新增构建期覆盖开关 `KSU_VERSION_OVERRIDE`，本次按当前管理器对齐为 **35040**；
- 其余插件无更新：NoMount `master` 仍为 `7914458`（v2.3.0），PathMask 为本地实现无上游。

实机待验证；`ksud susfs features` 应仍为 10 项。

## V14 — 2026-11-18

**主题：跟随 SukiSU 更名 SukiSU-Nova（完全适配）**

- 上游 `SukiSU-Ultra` 更名 **`SukiSU-Nova`**，内置源码同步到 `builtin@4bec532d`；
- 包名：`com.sukisu.ultra` → **`com.sukisu.nova`**（内核 `KSU_PACKAGE_NAME` 默认值），
  旧 Ultra 管理器不再被识别；
- 版本公式：`40000+N-2815` → **`30000+N-4001`**；上游 main 当前 4008 提交 →
  `KSU_VERSION` **30007**，完整版本串 `v3.0.0-<sha>@main`（管理器 v3.0.0 的最低要求即 `v3.0.0`）；
- uapi 版本仍为 **6**，与管理器 v3.0.0 一致，无需移植新接口；
- `main` 分支新增的 701 行 uapi（manager / sulog / selinux 新命令）属 GKI 线，
  非 GKI 4.14 走 `builtin`，本次不引入；
- 保留全部 5 处 4.14 兼容改动（KPM 编译排除、`static_call.h` 守卫、`ksu_kvcalloc`、
  `strncpy_from_user_nofault` 别名、`selinux_hide_compat.c`）；
- `ci/verify-features.sh` 的版本串断言由 `v4.3.0-r2` 更新为 `v3.0.0`。

## V13 — 2026-11-15

**主题：跟随上游重编（内核源码无改动）**

- SukiSU 版本号随上游 `main` 前进（3994 → 3998 提交），`KSU_VERSION` 41179 → **41183**，
  与设备管理器 versionCode 41183 精确配对；
- 内核源码与 V12 完全相同（同一提交 `9877efbd4`），功能集合无变化；
- 修复 `ci/package.sh` 把 `KSU_VERSION` 硬编码为 41179 的问题：改由 `ci/build.sh`
  从编译日志提取真实值写入 `$OUT/ksu-version.txt`，打包时读取；
- 新增 `docs/ARCHITECTURE-V12.md`（全量 diff 与函数级审计）与 `docs/REPO-MASTER.md`（仓库总览）；
- 修正 README / FEATURES / patches 中过期的版本号与"未移植"描述。

实机：已刷入并正常开机；`ksud debug version` = 41183，SUSFS 10 项 feature 全开。


## V8 — 2026-11-14

### 修复

- **zram LRU writeback NULL deref（随机重启根因）**：
  `is_bdev_avail()` 只检查了 `zram->bdev->bd_disk`，没有检查 `zram->bdev`。
  三星 LRU writeback 在未配置 backing device 时仍会调用
  `zram_add_to_writeback_list()`，`zram->bdev == NULL` 时访问 `NULL + 0x80`
  直接 Oops。现增加 `!zram->bdev` 判断，未配置 writeback 时安全跳过。
- **PathMask 4.14 写类 hook**：
  - `write_op_policy=eacces` 的 `O_CREAT` 判断改用 4.14 的 `regs[2]`，
    不再把 `dfd` 当 `pt_regs` 解引用（`0xffffffac` panic）；
  - 写类 syscall hook 改用 `SyS_*` 入口并按真实寄存器取路径；
  - 已实机验证 `touch`/`mkdir` 返回 `EACCES`，无 panic。

其余与 V7 相同：SukiSU 41179、SUSFS 2.4.0、NoMount 2.3.0、PathMask 4.14、
selinux_hook 内置 KPM、无 KPatch/kpimg。


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
