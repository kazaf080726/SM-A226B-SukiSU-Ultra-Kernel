# SM-A226B V12 内核架构审计（只读全量 diff 版）

- 审计对象：`/home/kazaf/a226b/gh-repo`，tag `v12` = commit `9877efbd4`
- 基准：Samsung 官方 `SM-A226B_13_Opensource.zip` → `Kernel.tar.gz`（Linux 4.14.186，a22x，无 KernelSU）
- 基准解压路径：`/home/kazaf/a226b/stock-v12`
- 审计日期：2026-11-15
- 方法：全量 `diff -rq` + 逐文件 `diff -up` + 上游仓库对比（KernelSU / NoMount）
- 约束：未刷机、未改源码、未动 `/data`；本文件是唯一新增文件

---

## 0. 事实等级（全文标注）

| 级 | 含义 |
|---|---|
| **A** | 已用文件内容 / 构建产物 / 设备输出直接确认 |
| **B** | 只知道存在或方向，未逐函数确认，或依赖运行时状态 |
| **C** | 已废弃、已回滚，不得复用 |

> 本文件所有 A 级结论都可在第 12 节用同一组命令复现。
> 未标注的叙述性文字默认是背景，不作为结论。

---

## 1. 结论摘要

1. **改动面已穷举**（A）：相对原厂 4.14.186，修改 **47 个文件**，删除 **0 个文件**，新增 9 个代码/文档子树 + 若干单文件。不存在"文档说有、代码里没有"或"代码里有、文档没提"的隐藏改动。
2. **六层架构成立**（A）：KernelSU、SUSFS、NoMount、PathMask、反检测、KPM/selinux_hook 都能落到具体文件与函数，见第 5、6 节。
3. **内置 KernelSU 已定位到上游**（A）：`KernelSU/` = SukiSU-Ultra `builtin` 分支 `f9aa2fec`（2026-11-12）**原样 + 5 处 4.14 兼容改动**，`uapi/` 与上游完全一致。此前"裸快照、身份不可考"的判断不再成立。
4. **NoMount 是上游原样**（A）：`NoMount/kernel/src` 与 `maxsteeel/nomount` HEAD `7914458` 逐字节一致（diff 0 行）。
5. **KPM 不是空壳**（A）：`selinux_hook_kpm/` 是 KernelPatch API 的**内核内 shim**，12 个 SELinux 函数被改名 + 同名包装 + `kp_hook_before/after` 直调，已编入 V12。
6. **早期口头结论需要纠正**（A）：`fs/proc/task_mmu.c`、`fs/open.c` **都有** hook（SUSFS 的 SUS_MAP / KSU 的 faccessat），见 §6.5.6、§6.2。
7. **构建产物不可信**（A）：仓库 `dist/` 与 `/home/kazaf/a226b/v9-out` 是 shield 分支时期构建，镜像内含 `a226b_shield` 字符串，**不是 V12**，禁止刷入。
8. **版本号是编译期联网算的**（A）：`KSU_VERSION = 40000 + main 提交数 - 2815`，V12 = 41179（当时 3994），今天重编 = 41183（3998）。
9. **设备当前无 boot 备份**（A）：任何刷写前必须先备份并读回校验。

---

## 2. 全量 diff 方法与结果

### 2.1 命令（可复现）

```bash
# 基准
mkdir -p /home/kazaf/a226b/stock-v12
unzip -p SM-A226B_13_Opensource.zip Kernel.tar.gz | tar -xz -C /home/kazaf/a226b/stock-v12

# 全量比对（排除构建产物 / 版本库 / 大型二进制）
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

`--no-dereference` 是必须的：`drivers/kernelsu`、`fs/nomount` 是两个符号链接，跟随会重复遍历整棵子树。

### 2.2 结果（A）

| 类别 | 数量 |
|---|---|
| 两边都存在但内容不同（modified） | **47** |
| 只在原厂存在（deleted） | **0** |
| 只在仓库存在（added，按行计） | 25 行（9 个目录 + 若干文件） |

**关键点：没有任何原厂文件被删除。** 所有改动都是"新增 + 就地打补丁"，回滚只需 checkout tag。

### 2.3 审计产物

| 路径 | 内容 |
|---|---|
| `/home/kazaf/a226b/audit/rq.txt` | `diff -rq` 原始输出 |
| `/home/kazaf/a226b/audit/modified.txt` | 47 个改动文件清单 |
| `/home/kazaf/a226b/audit/stats.tsv` | 每文件 hunks / +行 / -行 / 层标记 |
| `/home/kazaf/a226b/audit/perfile/*.diff` | 逐文件 unified diff |
| `/home/kazaf/a226b/audit/v12.patch` | 合并 patch（3960 行，带函数上下文） |
| `/home/kazaf/a226b/audit/ksu_builtin_diff.txt` | 内置 KSU vs 上游 builtin 分支 |
| `/home/kazaf/a226b/audit/nomount_diff.txt` | NoMount vs 上游（0 行） |
| Windows 副本 | `D:\...\SM-A226B\.audit\out\` |

---

## 3. 完整改动文件清单（47 个，A）

按归属层排序；"hunks/+/-" 来自 `stats.tsv`。

### 3.1 KernelSU 接入（8）

| 文件 | hunks | +/- | 注入函数 |
|---|---|---|---|
| `fs/exec.c` | 4 | +32/-0 | `do_execveat_common()`：`ksu_handle_execveat()` / `_sucompat()`，出口 `ksu_handle_post_execveat_sucompat()` |
| `fs/open.c` | 4 | +27/-0 | `SYSCALL_DEFINE3(faccessat)`：`ksu_handle_faccessat()` + `filename_lookup()` |
| `fs/stat.c` | 5 | +84/-0 | `vfs_statx()`：`ksu_handle_stat()`；`vfs_statx_fd()`：`ksu_handle_vfs_fstat()` |
| `fs/read_write.c` | 1 | +10/-0 | `SYSCALL_DEFINE3(read)`：`ksu_handle_sys_read()` |
| `kernel/sys.c` | 8 | +54/-0 | `setresuid`：`ksu_handle_setresuid()`；`getpriority/setpriority`：`ksu_hide_app_uid()` |
| `kernel/reboot.c` | 1 | +8/-0 | `SYSCALL_DEFINE4(reboot)`：`ksu_handle_sys_reboot()` |
| `drivers/input/input.c` | 1 | +8/-0 | `input_handle_event()`：`ksu_handle_input_handle_event()`（触发密钥） |
| `drivers/{Kconfig,Makefile}` | 1/1 | +1/+1 | `source "drivers/kernelsu/Kconfig"`、`obj-$(CONFIG_KSU) += kernelsu/` |

### 3.2 SUSFS 2.4.0（16）

| 文件 | hunks | +/- | 注入点 |
|---|---|---|---|
| `fs/namei.c` | 27 | +259/-1 | `lookup_dcache/__lookup_hash/lookup_fast/lookup_slow/walk_component/link_path_walk/lookup_open/do_tmpfile/do_o_path/path_openat/generic_readlink/vfs_readlink`；`filename_lookup()` 去掉 static |
| `fs/readdir.c` | 26 | +190/-0 | `fillonedir/filldir/filldir64/compat_*` 逐个 `ilookup()+susfs_is_inode_sus_path()` 过滤；四个 `*getdents*` 入口加 FUSE `susfs_get_data_path()` |
| `fs/namespace.c` | 11 | +224/-0 | `mnt_free_id/mnt_alloc_group_id/__lookup_mnt/vfs_kern_mount/clone_mnt/copy_mnt_ns`；新增 `susfs_alloc_unshare_ksu_vfsmnt()`、`susfs_alloc_non_unshare_ksu_vfsmnt()`、`susfs_get_non_sus_mnt_id_from_mnt()`、`susfs_get_non_sus_vfsmnt_from_vfsmnt()` |
| `fs/stat.c` | （同上文件） | — | `vfs_getattr_nosec()`：`susfs_sus_kstat_spoof_generic_fillattr()` |
| `fs/statfs.c` | 2 | +73/-0 | `vfs_statfs()`：`susfs_statfs_by_dentry()` + 非 sus 挂载替换 |
| `fs/super.c` | 2 | +22/-0 | `get_anon_bdev()`：KSU 域用 `DEFAULT_KSU_MNT_MINOR_DEV` 段 |
| `fs/proc/task_mmu.c` | 6 | +52/-0 | `show_map_vma/smaps/pagemap_read` + `mm/memory.c:__access_remote_vm`：`SUSFS_IS_INODE_SUS_MAP()` 隐藏 |
| `fs/proc/fd.c` | 2 | +65/-0 | `seq_show()`：sus_kstat 伪装 + 非 sus mnt_id |
| `fs/proc/base.c` | 4 | +20/-0 | `do_proc_readlink()` + `proc_map_files_readdir()` |
| `fs/notify/fdinfo.c` | 4 | +74/-1 | `inotify_fdinfo()`：sus_kstat / sus_mount 伪装 |
| `fs/proc/cmdline.c` | 1 | +11/-0 | `cmdline_proc_show()`：`susfs_spoof_cmdline_or_bootconfig()` |
| `fs/proc_namespace.c` | 6 | +205/-0 | 三个 `show_*` 前插 `a226b_hide_mount_line()`；`*_open()` 切到 `susfs_show_*` |
| `kernel/kallsyms.c` | 2 | +46/-0 | `s_show()`：按前缀/子串丢弃 ksu_/susfs_ 符号 |
| `security/selinux/avc.c` | 3 | +19/-0 | `avc_dump_query()`：tsid == `susfs_ksu_sid` 时伪装成 `priv_app` |
| `mm/memory.c` | 2 | +10/-0 | `__access_remote_vm()`：sus_map 直接 break |
| `fs/Makefile` | 2 | +2/-0 | `obj-$(CONFIG_KSU_SUSFS) += susfs.o` |

新增：`fs/susfs.c`（2217 行）、`include/linux/susfs.h`（260 行）、`include/linux/susfs_def.h`（238 行）。

### 3.3 NoMount（3 + 整棵子树）

| 文件 | hunks | +/- | 内容 |
|---|---|---|---|
| `fs/Makefile` | （同上） | +1 | `obj-$(CONFIG_NOMOUNT) += nomount/` |
| `fs/Kconfig` | 1 | +1 | `source "fs/nomount/Kconfig"` |
| `fs/nomount` | — | 符号链接 | → `../NoMount/kernel/src`（3 个文件：`nomount.c` 2234 行、`nomount.h`、`Kconfig`/`Makefile`） |

### 3.4 PathMask / procguard（2）

| 文件 | hunks | +/- | 内容 |
|---|---|---|---|
| `kernel/module.c` | 3 | +29/-0 | 新增 `ksu_hide_lkm_name()`，`m_show()` 对 app UID 隐藏 `pathmask`/`procguard` 行 |
| 整棵 `pathmask/` | — | — | `src/{pm_main.c,procguard.c,compat.c,core.h,hint.h}`（LKM，kretprobe + kallsyms 解析）、`ksu-module/`（配置与 service.sh） |

### 3.5 反检测 + 启动完整性（12）

| 文件 | hunks | +/- | 内容 |
|---|---|---|---|
| `fs/proc_namespace.c` | （见 3.2） | +205 | `a226b_hide_mount_line()`：6 个 devname，UID 10296/10272 豁免 |
| `fs/proc/root.c` | 1 | +11/-1 | `proc_mkdir("fs")` 改为 `S_IFDIR|0700` |
| `kernel/configs.c` | 2 | +15/-1 | `/proc/config.gz` 改 `0640 root:system`（不能 0600，否则 Vintf 失败） |
| `security/selinux/ss/status.c` | 6 | +25/-13 | 状态页 `sequence=4`、`policyload=1`，去掉自增 |
| `fs/proc/version.c` | 1 | +20/-0 | `/proc/version` 输出 SUSFS 伪装后的 uname |
| `kernel/utsname_sysctl.c` | 2 | +30/-0 | `/proc/sys/kernel/{osrelease,version}` 同样伪装 |
| `init/main.c` | 1 | +9/-0 | `setup_command_line()` 追加 `androidboot.vbmeta.digest=d259db70…` |
| `block/ioprio.c` | 4 | +43/-0 | `ioprio_set/get`：`ksu_hide_app_uid()` + `susfs_is_priority_uid_filtered()` |
| `kernel/sys.c` | （见 3.1） | +54 | `getpriority/setpriority` 同上 |
| `drivers/block/zram/zram_drv.c` | 1 | +5/-1 | `is_bdev_avail()` 增加 `!zram->bdev` |
| `security/selinux/hooks.c` | 4 | +56/-0 | `check_nnp_nosuid()`：`u:r:init:s0` → `u:r:su:s0` 放行 |
| `arch/arm64/configs/a22x_defconfig` | 3 | +17/-2 | `DMESG_RESTRICT=y`、`SECURITY_DEFEX` 由 y 改 off、追加 14 个 KSU/SUSFS/NoMount/过滤器开关 |

### 3.6 KPM / selinux_hook / 查询过滤（6 + 子树）

| 文件 | hunks | +/- | 包装的函数 |
|---|---|---|---|
| `security/security.c` | 2 | +21/-0 | `security_setprocattr()` → `__kp_security_setprocattr()` |
| `fs/libfs.c` | 2 | +27/-0 | `simple_read_from_buffer()` → `__kp_simple_read_from_buffer()` |
| `security/selinux/hooks.c` | （见 3.5） | +56 | `selinux_setprocattr()`、`selinux_complete_init()` |
| `security/selinux/selinuxfs.c` | 7 | +92/-2 | `sel_read_handle_status()`、`sel_mmap_handle_status()`、`sel_write_context()`、`sel_write_access()` |
| `security/selinux/ss/services.c` | 5 | +81/-4 | `context_struct_compute_av()`、`string_to_context_struct()`、`security_read_policy()` |
| `security/selinux/ss/status.c` | （见 3.5） | +25/-13 | `selinux_status_update_policyload()` |
| `security/selinux/{Kconfig,Makefile}` | 1/2 | +20/+2 | 新增 `SECURITY_SELINUX_QUERY_FILTER`、`SECURITY_SELINUX_HOOK_KPM` |

新增：`security/selinux/selinux_hook_kpm/`（10 文件 4754 行）、`security/selinux/selinux_query_filter.c/.h`、`include/linux/selinux_hook_kpm.h`。

### 3.7 构建/可复现性（5，不属于安全层）

| 文件 | hunks | 内容 |
|---|---|---|
| `arch/arm64/kernel/vdso/Makefile` | 1 | `--build-id` → `--build-id=none` |
| `kernel/gen_kheaders.sh` | 1 | tar 固定 mtime/owner/排序 |
| `scripts/extract-cert.c` | 1 | `OPENSSL_SUPPRESS_DEPRECATED`（OpenSSL 3） |
| `scripts/mod/modpost.c` | 1 | `const char *here`（GCC 12 警告） |
| `scripts/kconfig/symbol.c` | 2 | `cv_stack` 提升为 static（GCC 12 悬垂指针警告） |

### 3.8 新增子树总览（A）

| 路径 | 文件数 | 行数 | 性质 |
|---|---|---|---|
| `KernelSU/` | 177 | 30,495 | 上游 builtin@f9aa2fec + 5 处改动 |
| `NoMount/` | 56 | 12,587 | 上游 maxsteeel/nomount，kernel/src 逐字节一致 |
| `pathmask/` | 54 | 16,224 | 本地实现（无上游） |
| `security/selinux/selinux_hook_kpm/` | 10 | 4,754 | 本地 KernelPatch shim 移植 |
| `anykernel/` | 11 | 58,008 | AK3 模板（工具为二进制） |
| `patches/` | 3 | 4,755 | `susfs_patch_to_4.14.patch`、`susfs_inline_hook_patches.sh` |
| `ci/` | 7 | 501 | build/package/verify 脚本 |
| `docs/` | 3 | 384 | BUILD/FEATURES/CHANGELOG |
| `.github/workflows/` | 1 | 140 | CI |
| 顶层 | 6 | — | README ×2、NOTICE ×2、LICENSE、.gitignore |

---

## 4. 六层架构图（A）

```
                    ┌──────────────────────────────────────────────┐
  userspace         │ SukiSU 管理器 com.sukisu.ultra（外部 APK）    │
                    │ susfs4ksu / nomount / pathmask（KSU 模块）    │
                    └───────────────┬──────────────────────────────┘
                                    │ supercall / ioctl / feature
 ┌──────────────────────────────────┴───────────────────────────────────┐
 │ Layer 1  KernelSU  SukiSU builtin@f9aa2fec                           │
 │   drivers/kernelsu -> ../KernelSU/kernel                             │
 │   supercall 分发 → ksu_handle_{execveat,faccessat,stat,sys_read,     │
 │   setresuid,sys_reboot,input_handle_event}                           │
 ├──────────────────────────────────────────────────────────────────────┤
 │ Layer 2  SUSFS 2.4.0   fs/susfs.c + 16 个宿主文件                    │
 │   SUS_PATH / SUS_MOUNT / SUS_KSTAT / SUS_MAP / OPEN_REDIRECT /       │
 │   SPOOF_UNAME / SPOOF_CMDLINE / HIDE_SYMBOLS / PRIORITY_UID_FILTER   │
 ├──────────────────────────────────────────────────────────────────────┤
 │ Layer 3  NoMount 2.3.0   fs/nomount -> ../NoMount/kernel/src         │
 │   VFS 路径重定向（fs_initcall），与 SUS_MOUNT 职责不同：              │
 │   SUSFS 伪造"挂载视图"，NoMount 重定向"路径解析"                     │
 ├──────────────────────────────────────────────────────────────────────┤
 │ Layer 4  PathMask / procguard（LKM，运行时加载）                      │
 │   kretprobe + kallsyms；getattr/getdents 入口；不参与内核编译         │
 ├──────────────────────────────────────────────────────────────────────┤
 │ Layer 5  原生反检测（本地策略，A226B 专属）                           │
 │   a226b_hide_mount_line（6 devname / UID 10296+10272 豁免）           │
 │   /proc/fs 0700、/proc/config.gz 0640、sequence=4、ioprio 枚举隐藏、  │
 │   /proc/modules 隐藏 LKM、/proc/version 与 sysctl uname 对齐、        │
 │   vbmeta.digest 注入、zram NULL 修复、nnp_nosuid 放行                 │
 ├──────────────────────────────────────────────────────────────────────┤
 │ Layer 6  KPM / selinux_hook 1.1.7（编入内核，非 .kpm 加载）           │
 │   kp_hook_before/after shim + 12 个包装函数 + clean policydb 引擎     │
 │   selinux_query_filter（CONFIG_ 常开）为独立第二层过滤                │
 └──────────────────────────────────────────────────────────────────────┘
                    ┌──────────────────────────────────────────────┐
                    │ Layer 0  Samsung 4.14.186（stock-v12 基准）   │
                    └──────────────────────────────────────────────┘
```

---

## 5. 逐层函数级说明

### 5.1 Layer 1 — KernelSU（SukiSU builtin）

**来源（A）**：`KernelSU/` vs 上游 `builtin` 分支 `f9aa2fec`（2026-11-12 21:07 +0800）：

- `uapi/`：**5 个文件全部一致**
- `kernel/`：**4 个文件内容不同 + 1 个新增文件**，其余文件逐字节一致（`diff -rq` 输出仅 5 行）

**5 处本地改动（A，逐条）**：

| 文件 | 改动 | 原因 |
|---|---|---|
| `kernel/extension/kpm/kpm.c` | 整文件包进 `#if defined(CONFIG_KPM) && LINUX_VERSION_CODE >= 5.10` | 4.14 无 KernelPatch 运行时，避免编译 |
| `kernel/kernel_includes.h` | `<linux/static_call.h>` 仅在 ≥5.10 引入 | 4.14 无此头 |
| `kernel/kernel_compat.h` | 新增 `ksu_kvcalloc()`（<4.18）与 `strncpy_from_user_nofault` 别名（<5.8） | 4.14 缺这两个 API |
| `kernel/ksu_main.c` | `selinux_hide` 改为全版本编译，<5.10 走 `selinux_hide_compat.c` | 让管理器"隐藏 SELinux 修改"开关在 4.14 有真实后端 |
| `kernel/feature/selinux_hide_compat.c` | 新增：只注册 feature handler，真正的隐藏交给 KPM | 4.14 无 `struct selinux_policy` |

**supercall 接线（A，宿主侧 7 个）**：

```
do_execveat_common ──► ksu_handle_execveat / _sucompat / _post_execveat_sucompat
faccessat          ──► ksu_handle_faccessat
vfs_statx          ──► ksu_handle_stat
vfs_statx_fd       ──► ksu_handle_vfs_fstat
sys_read           ──► ksu_handle_sys_read
setresuid          ──► ksu_handle_setresuid
sys_reboot         ──► ksu_handle_sys_reboot
input_handle_event ──► ksu_handle_input_handle_event（static key 控制）
```

**版本号机制（A，重点）**：`KernelSU/kernel/Makefile`

```make
KSU_GITHUB_VER := curl releases/latest | tag_name        # 联网取最新 release
GITHUB_COMMITS := curl commits?sha=main&per_page=1 | page=last  # 联网取 main 提交数
LOCAL_COUNT    := GITHUB_COMMITS ?: git rev-list --count main
KSU_VERSION    := 40000 + LOCAL_COUNT - 2815
```

- V12 构建时 main = 3994 → **41179**（与设备管理器 versionCode 一致，A）
- 2026-11-15 实测 main = 3998 → 重编得 **41183**（A）
- 离线时 `LOCAL_COUNT` 回退到**本内核仓库**的 21 个提交 → **37206**（A）
- `KSU_VERSION_FULL` 的 sha 也取自本仓库 HEAD（`KernelSU/.git` 不存在，git 向上查找）：V12 镜像内为 `v4.3.0-r2-90f5352c@main`，而 `90f5352c` 是 **v11 提交**（A）

**风险**：版本号与代码脱钩；离线构建悄悄变成 37206；`build-info.txt` 里 `41179` 是 `ci/package.sh` 硬编码（A）。

### 5.2 Layer 2 — SUSFS 2.4.0

**来源（A）**：`include/linux/susfs.h` → `SUSFS_VERSION "v2.4.0"`；`fs/susfs.c` 2217 行；`patches/susfs_patch_to_4.14.patch`（144 KB）记录移植基线。

**函数级分布（A）**：

| 子功能 | 入口（susfs.c） | 宿主注入点 |
|---|---|---|
| SUS_PATH | `susfs_add_sus_path()`、`susfs_is_inode_sus_path()`、`susfs_run_sus_path_loop()`、fake qstr `"..5.u.S"` | `namei.c` 8 处查找路径 + `readdir.c` 全部 filldir |
| SUS_MOUNT | `susfs_add_sus_mount()`、`susfs_set_hide_sus_mnts_for_non_su_procs()`、`susfs_is_current_proc_umounted()` | `namespace.c` 6 处 + `proc_namespace.c` 的 `susfs_show_*` |
| SUS_KSTAT | `susfs_add_sus_kstat()`、`susfs_sus_kstat_spoof_generic_fillattr()` | `stat.c`、`statfs.c`、`proc/fd.c`、`notify/fdinfo.c`、`task_mmu.c` |
| SUS_MAP | `SUSFS_IS_INODE_SUS_MAP()` 宏 | `task_mmu.c`（maps/smaps/pagemap）、`mm/memory.c`、`proc/base.c` |
| OPEN_REDIRECT | `susfs_add_open_redirect()`、`susfs_open_redirect_spoof_do_sys_openat()` | `namei.c`（do_tmpfile/do_o_path/path_openat/readlink） |
| SPOOF_UNAME | `susfs_spoof_uname()` + static key | `kernel/sys.c:newuname`、`proc/version.c`、`utsname_sysctl.c`（后两处为本地补强） |
| SPOOF_CMDLINE | `susfs_spoof_cmdline_or_bootconfig()` | `proc/cmdline.c` |
| HIDE_SYMBOLS | — | `kallsyms.c:s_show()` 前缀/子串过滤 |
| PRIORITY_UID_FILTER | `susfs_is_priority_uid_filtered()` | `kernel/sys.c`、`block/ioprio.c` |
| AVC 日志伪装 | `susfs_ksu_sid`、`susfs_priv_app_sid` | `avc.c:avc_dump_query()` |

**调用链（A，以隐藏挂载为例）**：
`open("/proc/self/mountinfo")` → `mountinfo_open()` → 若 `susfs_is_hide_sus_mnts_for_non_su_procs_enabled` 且当前非 KSU 域 → `susfs_show_mountinfo()` → 先过 `a226b_hide_mount_line()` → 再按 `mnt_id >= DEFAULT_KSU_MNT_ID` 丢弃 → 输出。

**依赖**：`CONFIG_KSU_SUSFS` 必须与 `CONFIG_KSU` 同时打开（susfs.c 直接 extern KSU 符号）；`SUSFS_VERSION` 被 `ci/verify-features.sh` 断言（A）。

### 5.3 Layer 3 — NoMount 2.3.0

**来源（A）**：与 `maxsteeel/nomount` HEAD `7914458`（"bump v2.2.0 → v2.3.0"）`kernel/src` **逐字节一致**；`NOMOUNT_BASE_VERSION "23"`。

**结构（A）**：`nomount.c` 2234 行，唯一入口 `fs_initcall(nomount_init)`；无导出符号，靠 fs 层重定向 + inode 代理工作。
`ci/verify-features.sh` 断言的符号：`nomount_init`、`nm_hijacked_lookup`、`nm_d_revalidate`、`nm_dir_lookup`、`nm_clear_state`（A）。

**与 SUS_MOUNT 的边界（A/B）**：
- A：SUSFS 的 SUS_MOUNT 在 `fs/namespace.c` 里伪造 `mnt_id` 与 mount 表，属于**视图层**；
- A：NoMount 在 VFS 层重定向 dentry/inode，不创建真实挂载，属于**解析层**；
- B：两者同时命中同一路径时的优先级（NoMount 的 `nm_hijacked_lookup` vs `susfs_is_inode_sus_path`）未实测，需构造用例。

### 5.4 Layer 4 — PathMask / procguard

**性质（A）**：两个 **LKM**，不在内核镜像里；设备 `lsmod` 可见 `pathmask`、`procguard`；`pathmask/src` 只有 6 个源文件被 git 跟踪，`.o/.ko/.cmd` 是未跟踪残留（`git ls-files` 计数为 0）。

**机制（A）**：`kallsyms_name_to_addr()` 解析 VFS 符号 + kretprobe：
- `getattr_entry()`（`pathmask.ko`）—— 隐藏路径的 stat 视图；
- `getdents_entry()` —— 目录项隐藏；
- `procguard.c` —— proc 视图保护；
- `verify-features.sh` 用 `nm pathmask.ko | grep getattr_entry/getdents_entry` 校验（A）。

**风险（A/B）**：LKM 与内核 vermagic 强绑定，换内核必须重编（`pathmask/build.sh`）；B：具体 deny/allow 规则来自 `ksu-module/*.conf`，实际生效集合未逐条核对。

### 5.5 Layer 5 — 反检测（本地策略）

这是**唯一"非上游"的一层**，全部为 A226B 专属逻辑（A）：

| # | 文件:函数 | 行为 | 备注 |
|---|---|---|---|
| 1 | `fs/proc_namespace.c:a226b_hide_mount_line()` | 隐藏 `metadata`/`cache`/`sec_efs`/`efs`/`omr`/`loop5` 六个 devname | `uid<10000 \|\| uid==10296 \|\| uid==10272 \|\| !dev` 直接放行；其余 UID 隐藏 |
| 2 | `fs/proc/root.c:proc_root_init()` | `/proc/fs` 降为 `0700` | 防春秋读 `/proc/fs/ext4`、`jbd2` 判 loop |
| 3 | `kernel/configs.c:ikconfig_init()` | `/proc/config.gz` `0640 root:system` | 0600 会让 Vintf 校验失败并弹"内部错误" |
| 4 | `security/selinux/ss/status.c` | `sequence=4`、`policyload=1` 固定 | 对齐"干净设备"指纹 |
| 5 | `kernel/sys.c` + `block/ioprio.c:ksu_hide_app_uid()` | app 调 `getpriority/ioprio` 枚举其它 app UID 返回 `-ESRCH` | 只对 10000–19999 / 90000–99999 目标生效 |
| 6 | `kernel/module.c:ksu_hide_lkm_name()` | `/proc/modules` 对 app UID 隐藏 `pathmask`/`procguard` | 规则写死两个名字，改名即失效 |
| 7 | `fs/proc/version.c` + `kernel/utsname_sysctl.c` | `/proc/version`、`osrelease`、`version` 与 `uname(2)` 一致伪装 | 防"两个视图互相矛盾" |
| 8 | `init/main.c:setup_command_line()` | 追加 `androidboot.vbmeta.digest=d259db70…` | 硬编码，换 boot 镜像即失效 |
| 9 | `drivers/block/zram/zram_drv.c:is_bdev_avail()` | 增加 `!zram->bdev` | V8 随机重启根因 |
| 10 | `security/selinux/hooks.c:check_nnp_nosuid()` | `init` → `su` 域切换放行 | 配合 su 启动 |

**边界（A）**：所有过滤都以 `uid` 判定，`uid<10000` 一律放行（fail-open）；未触及 `system_server`/`zygote`/`app_zygote`。

### 5.6 Layer 6 — KPM / selinux_hook + 查询过滤

**性质（A）**：**编进内核**，不是 `.kpm` 加载；`CONFIG_SECURITY_SELINUX_HOOK_KPM=y`、`CONFIG_SECURITY_SELINUX_QUERY_FILTER=y` 均在 V12 defconfig 中。

**工作机制（A）**：`kpm_hooks.c` 的注释写明——"Compiled into the kernel we cannot patch text, so `hook_wrap()` only records the callbacks and the wrapped kernel functions call `kp_hook_before()/kp_hook_after()` directly."
即：原函数改名 `__kp_*`，同名函数做包装，直调 hook 表；不是 inline hook，不需要 KernelPatch。

**12 个接线点（A，与 `ci/verify-features.sh` 一致）**：

```
security_setprocattr        selinux_setprocattr        selinux_complete_init
sel_read_handle_status      sel_mmap_handle_status     sel_write_context
sel_write_access            context_struct_compute_av  string_to_context_struct
security_read_policy        selinux_status_update_policyload
simple_read_from_buffer
```

**clean-policy 引擎（A/B）**：`snapshot_clean_policy()` 在启动时把干净 policydb 读进独立副本；`/sys/fs/selinux/policy` 返回 clean blob；`context_struct_compute_av()` 用 clean policydb 重算 AV。
A：代码路径存在且已编译；B：`late_initcall(selinux_hook_kpm_builtin_init)` 在当前设备上是否成功加载了 clean policy（需 dmesg `[selinux_hook]` 日志验证）。

**第二层过滤（A）**：`selinux_query_filter.c` 在 `sel_write_access()` / `sel_write_context()` / `selinux_setprocattr()` 三个入口拦 `access`/`context`/`procattr` 查询，返回 `-EINVAL`（等价"干净策略里没有这个类型"）。V6 起 `/access` 应答由 `0 0 0 0 1 0` 改为 `0 ffffffff 0 ffffffff 1 0`（A，代码 + verify 断言）。

**版本标注冲突（A）**：`selinux_hook.c` 中 `KPM_VERSION(SELINUX_VERSION)`，`SELINUX_VERSION "1.1.7"`；而 `docs/FEATURES.md` 写 `1.1.6.1`。**以源码 1.1.7 为准**，文档需要更正。

---

## 6. 交叉依赖与回调顺序

```
boot
 └─ selinux_init ...................... SELinux 策略加载
     └─ selinux_complete_init ......... KPM 包装点（clean policy 快照时机）
         └─ late_initcall(selinux_hook_kpm_builtin_init)
             └─ hook_wrap() × N ....... 注册回调到 kp_hooks[48]
 └─ fs_initcall(nomount_init) ......... 装入路径重定向
 └─ kernelsu_init() ................... KSU 初始化 → 注册 feature handler
     └─ ksu_selinux_hide_init() ....... 4.14 走 compat 版
 └─ 用户态 ksud / susfs4ksu / nomount / pathmask 依次下发规则
```

依赖约束（A）：
1. `CONFIG_KSU_SUSFS*` 依赖 `CONFIG_KSU`（`fs/susfs.c` extern KSU 符号）；
2. `CONFIG_SECURITY_SELINUX_HOOK_KPM` 依赖 `KALLSYMS_ALL`（Kconfig 明写，A）：`select`/`depends on SECURITY_SELINUX && KALLSYMS_ALL`；
3. PathMask LKM 依赖内核 vermagic，禁止跨内核复用；
4. `susfs4ksu` 模块需要 `susfs` 内核支持，`nomount` 模块需要 `CONFIG_NOMOUNT`（A，模块与内核同名能力对应）。

---

## 7. 已知问题 / 未解决项（A）

| # | 问题 | 证据 | 影响 |
|---|---|---|---|
| 1 | `dist/` 与 `v9-out` 是 shield 期构建 | 镜像内含 `a226b_shield: proc_create failed`、`KSU_VERSION_FULL=86c40b44`；V12 包内为 `90f5352c` 且无 shield 串 | **刷了就是把已回滚的 shield 装回去** |
| 2 | 设备无 boot 备份 | `/data/local/tmp`、`/cache` 已清空 | 刷坏无退路 |
| 3 | 版本号随上游漂移 | 今天重编 = 41183 ≠ 管理器 41179 | 管理器版本提示/门控 |
| 4 | 离线构建版本号坍缩 | 回退到本仓库 21 提交 → 37206 | 管理器判"内核过旧" |
| 5 | `KSU_VERSION_FULL` sha 指向本仓库 | `KernelSU/.git` 缺失，git 向上查找 | 版本串不能代表 SukiSU 源码 |
| 6 | `build-info.txt` 硬编码 41179 | `ci/package.sh:40` | 重编后元数据说谎 |
| 7 | KPM 版本标注不一致 | 源码 1.1.7 vs FEATURES.md 1.1.6.1 | 文档不可全信 |
| 8 | `.o/.ko` 残留在 `pathmask/src` | 未跟踪文件（`git ls-files` 为 0） | 误把旧 ko 打进包 |
| 9 | `ccache` 不在 PATH | `command -v ccache` 为空 | `USE_CCACHE=1` 静默失效 |
| 10 | DirtySepolicy 未真正消除 | Duck 仍报 1 项危险（A：设备实测） | 已知未解决，不要再全局屏蔽 selinuxfs |
| 11 | `fs/stat.c` 重复 extern 块 | 同一段 `extern susfs_is_inode_sus_kstat` 声明两次 | 无害，代码异味 |
| 12 | 无上游锚点的层 | `pathmask/`、`selinux_hook_kpm/`、反检测层 | 只能靠本文件 + tag 追溯 |

---

## 8. 构建与刷机流程（A，脚本已读）

### 8.1 构建

```bash
cd /home/kazaf/a226b/gh-repo
OUT=/home/kazaf/a226b/v9-out bash ci/build.sh      # 产出 Image / Image.gz
OUT=/home/kazaf/a226b/v9-out bash ci/package.sh    # 产出 AK3 + modules + dist/Image.gz
OUT=/home/kazaf/a226b/v9-out bash pathmask/build.sh # 产出 pathmask.ko / procguard.ko
bash ci/verify-features.sh                          # 逐项 gate
```

要点（A）：
- `ci/build.sh` 用 `KBUILD_BUILD_TIMESTAMP`（来自 `ci/repro-date.txt`）+ `LOCALVERSION=` 保证可复现；
- 从零构建时会删除 `arch/arm64/include/generated` 与 `include/generated`，但**复用 `OUT` 就会保留旧 `.o`**；
- `ci/verify-features.sh` 的 gate：14 个 CONFIG、KSU/SUSFS/NoMount/KPM 符号、12 个 hook 目标、query filter 符号、镜像内不得出现 `kpimg`/`KernelPatch`、`/access` 旧指纹、PathMask vermagic。

### 8.2 刷机（未执行）

```bash
# 1) 先备份（当前设备没有备份）
adb shell su -c 'dd if=/dev/block/by-name/boot of=/data/local/tmp/boot-v12-backup.img bs=4096'
adb shell su -c 'sha256sum /data/local/tmp/boot-v12-backup.img'
adb pull /data/local/tmp/boot-v12-backup.img
Get-FileHash .\boot-v12-backup.img -Algorithm SHA256   # 与本机比对

# 2) 推荐 TWRP 刷 AK3；若 dd：
#    dd if=Image of=/dev/block/by-name/boot bs=4096 && sync
# 3) 刷后读回校验 sha256
```

AK3 关键属性（A）：`do.devicecheck=0`、`BLOCK=/dev/block/by-name/boot`、`do.modules=1`、`do.systemless=1`。

### 8.3 回滚

- 有备份：`dd if=boot-v12-backup.img of=/dev/block/by-name/boot bs=4096 && sync`；
- 无备份：重新刷同版本官方 boot（Odin/AP），再按 V12 zip 重刷；
- 源码层回滚：`git checkout v12`（无删除文件，checkout 即完全还原）。

---

## 9. 未确认项（B 清单）

1. **KPM 运行时是否真的装上了 clean policy**：需要 `dmesg | grep selinux_hook`、`cat /sys/fs/selinux/policy | head -c 32`。
2. **12 个 hook 点里哪些在 4.14 实际注册成功**：`hook_wrap()` 失败会返回 -1，需运行时日志。
3. **SUSFS 规则实际集合**：设备上 `susfs4ksu` 下发了哪些 sus_path/sus_kstat，未逐条核对。
4. **NoMount 规则集合**：`/data/adb/modules/nomount` 配置未逐条读。
5. **PathMask deny/allow 规则**：`pathmask/ksu-module/*.conf` 未与设备实际文件比对。
6. **`CONFIG_SECURITY_DEFEX` 关闭的副作用**：原厂 defex LSM 被关，是否影响 Samsung 某些安全特性，未评估。
7. **`a226b_hide_mount_line()` 的 UID 硬编码**：10296/10272 是当前安装 UID，卸载重装检测器后会变，无自动发现机制。
8. **`KSU_VERSION_FULL` 在 `KernelSU/.git` 恢复后的行为**：若有人把 `.git` 放回，sha 会变成真 KSU 提交，与历史构建不一致。
9. **上游 builtin 分支在 f9aa2fec 之后是否有影响功能的提交**：本次只比对了该快照。
10. **`security/selinux/hooks.c` 的 nnp_nosuid 放行**是否扩大攻击面：语义上是"init→su 的 exec 放行"，但未做威胁建模。

---

## 10. 不能做的事（沿用并已代码印证）

1. 不全局屏蔽 selinuxfs —— KPM/query_filter 都是**按查询类型**过滤，不是关文件系统；
2. 不动 UID 1000 / system_server / zygote / app_zygote —— 现有过滤全部 `uid<10000` 放行；
3. 不改 PackageManager 数据库；
4. 不改系统应用；
5. 不全局 spoof uname —— SUSFS 一处，`proc/version`/sysctl 是"镜像同一伪装值"，不是第二套伪装；
6. 不重新引入 `a226b_shield` / LSPosed 互补模块（C 级，已回滚，全树 grep 0 命中）；
7. 刷机前必须备份 boot；
8. 所有过滤 fail-open（`a226b_hide_mount_line()` 首行即 `uid<10000 → false`）；
9. 先读代码再改。

---

## 11. 事实等级总表

| 结论 | 级 |
|---|---|
| 47 改 / 0 删 / 新增子树清单 | A |
| 六层分别落在哪些文件与函数 | A |
| KernelSU = builtin@f9aa2fec + 5 处兼容改动 | A |
| NoMount = 上游逐字节一致 | A |
| KPM 为内核内 shim，12 个接线点 | A |
| `task_mmu.c`/`open.c` 有 hook | A |
| `dist/`+`v9-out` 是 shield 残留 | A |
| 版本号公式与漂移 | A |
| KPM clean policy 运行时生效 | B |
| SUSFS/NoMount/PathMask 实际规则集合 | B |
| DEFEX 关闭的副作用 | B |
| 上游 builtin 后续提交影响 | B |

---

## 12. 复现本审计

```bash
# 1) 解基准
unzip -p SM-A226B_13_Opensource.zip Kernel.tar.gz | tar -xz -C /home/kazaf/a226b/stock-v12
# 2) 全量 diff（见 §2.1）
diff -rq --no-dereference ... /home/kazaf/a226b/stock-v12 /home/kazaf/a226b/gh-repo > audit/rq.txt
# 3) 逐文件 patch
bash .audit/04_patch.sh        # 生成 audit/perfile/*.diff 与 audit/v12.patch
# 4) KSU 身份
git clone --depth 1 --branch builtin https://github.com/SukiSU-Ultra/SukiSU-Ultra /tmp/k
diff -rq /tmp/k/kernel /home/kazaf/a226b/gh-repo/KernelSU/kernel
# 5) NoMount 身份
git clone --depth 50 https://github.com/maxsteeel/nomount /tmp/n
diff -rq /tmp/n/kernel/src /home/kazaf/a226b/gh-repo/NoMount/kernel/src   # 期望 0 行
```

---

## 13. 附：本次审计对既有文档的修正

| 旧说法 | 修正 | 级 |
|---|---|---|
| `fs/proc/task_mmu.c` 没有 maps 过滤 | 有：SUSFS SUS_MAP 隐藏被标记 inode 的 vma/smaps/pagemap | A |
| `fs/open.c` 没有 hook | 有：`faccessat` 接 KSU + `filename_lookup()` | A |
| kernel 源码"裸快照、无锚点" | 锚点已找到：builtin@f9aa2fec | A |
| selinux_hook 1.1.6.1 | 源码标注 1.1.7 | A |
| `docs/README-V12.md` | 不存在；仓库 `docs/` 只有 BUILD/FEATURES/CHANGELOG | A |
| 仓库有 `ksu-module/` | 不存在（`ls` 报 No such file） | A |
| `KernelSU` 是 submodule | 不是：直接提交进仓库的普通目录 | A |
