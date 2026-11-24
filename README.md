# SM-A226B-SukiSU-Ultra-Kernel

[![Build kernel](https://github.com/kazaf080726/SM-A226B-SukiSU-Ultra-Kernel/actions/workflows/build.yml/badge.svg)](https://github.com/kazaf080726/SM-A226B-SukiSU-Ultra-Kernel/actions/workflows/build.yml)

> Samsung Galaxy A22 5G（SM-A226B / a22x / MediaTek MT6833）Linux 4.14.186 内核源码与卡刷包
>
> **内核级 Root：SukiSU-Nova**（原 SukiSU-Ultra；builtin，v3.0.0 / KSU_VERSION 35061，随上游 main 漂移）
>
> 🌐 中文（默认） | [English](README.en.md)

本仓库是 **SM-A226B 的完整内核源码**（基于三星官方 `SM-A226B_13_Opensource`），
集成 SukiSU-Ultra、SUSFS、NoMount、PathMask，并把 selinux_hook 的 KPM 源码
**直接编入内核**，不再依赖 KPatch-Next / kpimg。

所有源码与构建产物永久免费公开，可自行编译、修改、再分发（遵循各组件 GPL 许可）。

---

## 一、当前版本（V28）

| 组件 | 版本 / 说明 |
|---|---|
| 内核 | Linux 4.14.186，Samsung `SM-A226B_13_Opensource` |
| Root | SukiSU-Nova `builtin`（原 SukiSU-Ultra），v3.0.0，KSU_VERSION **35061**（构建期覆盖，对齐管理器） |
| SUSFS | **v2.4.0**（Non-GKI 4.14 移植） |
| NoMount | **v2.3.0**（内核驱动内置） |
| PathMask | 4.14 移植（`pathmask.ko` + `procguard.ko`，KSU 模块） |
| SELinux 隐藏 | selinux_hook **1.1.7 全量源码编入**（4.7k 行目录，12 个静态 hook 接线） |
| 反检测 | clean-policy 引擎、status 伪造、`/access` 真实形状应答、v4 原生过滤层 |
| KernelPatch / kpimg | **无**（0 痕迹，不需要 KPatch-Next 模块） |

设备只有 `boot` 分区，没有 `init_boot`；AnyKernel3 刷写路径
`/dev/block/by-name/boot`，`do.devicecheck=0`。

---

## 二、版本历史（Releases）

| 版本 | 主要内容 | 结果 |
|---|---|---|
| **V1** | SukiSU + SUSFS 2.4.0 + NoMount 2.2.0 + PathMask 初始集成 | 可开机 |
| **V2** | 修 SukiSU 管理器“隐藏 SELinux 修改”开关在 4.14 不可用；提供 KPM（kpimg）与标准两版 | 可开机；NativeCheck 报 `FolkPatch/APatch` + `SELinux Policy` |
| **V3** | 彻底移除 KPatch-Next / kpimg | `FolkPatch/APatch` 消失，`SELinux Policy` 仍在 |
| **V4** | status 伪造（sequence=4 / policyload=1）+ 常开原生 SELinux 过滤层 | 为 V5 的 clean-policy 引擎铺路 |
| **V5** | selinux_hook 1.1.6.1 **全量源码编入**，12 个 hook 接线，clean-policy 快照/重定向 | `SELinux Policy` 消失；`FolkPatch/APatch` 间歇回归 |
| **V6** | 修 `/sys/fs/selinux/access` 固定应答指纹（KPM + 原生过滤两处） | **两项误报全部消失，全功能生效** |
| **V7** | NoMount 2.2.0 → **2.3.0**（inode 规则、预打开缓存、OOB/UAF 修复、安全加固、启动提速） | 稳定基线 |
| **V8** | zram `is_bdev_avail()` NULL bdev 修复；PathMask 4.14 写钩子 `SyS_*` 适配 | 修复随机重启 |
| **V9** | 对 app UID 隐藏原厂 metadata/cache/efs/omr 与 loop5 挂载 | 小骨 MountInfo/Loop 通过 |
| **V10** | 春秋（UID 10296）保留完整挂载视图，修“挂载间隙” | NativeCheck 通过 |
| **V11** | `init/main.c` 注入 `androidboot.vbmeta.digest` | Bootloader/TEE 危险项消失 |
| **V12** | Hunter（UID 10272）排除在挂载隐藏之外 | Hunter 红屏消除 |
| **V13** | 仅重编：KSU 版本随上游 main 前进到 **41183**，源码与 V12 相同；新增全仓库审计与总览文档 | 上一版 |
| **V14** | SukiSU 更名 **SukiSU-Nova**：内置源码升级到 `builtin@4bec532d`，包名改 `com.sukisu.nova`，完整版本串 `v3.0.0`；5 处 4.14 兼容改动全部保留 | 上一版 |
| **V15** | SUSFS 4.14 补丁同步上游（SUS_PATH `i_state` 等修复）；内核版本公式改为与管理器一致，输出 **35040** | 上一版 |
| **V16** | NoMount 升级 **2.3.1**（KEYS select、export_operations 代理、启动优化）；版本对齐管理器 **35049** | 上一版 |
| **V17** | AVC 日志伪装改为内核默认启用（不依赖 KSU 模块）；版本对齐 35061 | 上一版 |
| **V18** | 补上 KSU feature avc_spoof（ID 10003），管理器可识别并开关 AVC 伪装 | 上一版 |
| **V19** | 修春秋“挂载间隙”：重置后 UID 变为 10265/10267，豁免表同步更新 | 上一版 |
| **V20** | NoMount 同步上游 6849f00（虚拟 backing 路径解析）；管理器 r1（版本号仍 35061） | 上一版 |
| **V21** | 对齐 Telegram 渠道管理器 35070（源码未变，仅版本对齐） | 上一版 |
| **V22** | 对齐 Telegram 渠道管理器 35073（源码未变，仅版本对齐） | 上一版 |
| **V23** | 对齐管理器 35081（翻译提交，源码未变，仅版本对齐） | 上一版 |
| **V25** | 内核级动态挂载豁免：解析 `packages.list` 取春秋/Hunter 当前 UID，根治重置后“挂载间隙” | 上一版 |
| **V26** | 修复 selinux_hook clean-policy 重定向在“应用UID+root域”组合下的 kernel panic | 上一版 |
| **V27** | SELinux 隐藏对齐原厂：状态页回退 0/0 + App 视角假状态页 + 去掉 clean-policy 重定向 | 上一版 |
| **V28** | 移除 KPM，内建 LKM 同款 SELinux 隐藏（clean policydb 快照 + 自答 access/context + policy blob + setprocattr 语义） | 当前推荐版本 |

每个 Release 都附对应卡刷包与更新说明。

> V7 的 AnyKernel3 包已在 SM-A226B 实机验证，可正常开机。

---

## 三、功能清单

### 3.1 SukiSU-Nova（`CONFIG_KSU`，原 SukiSU-Ultra）
- `builtin` 分支（SukiSU-Nova），管理器 v3.0.0（versionCode 35000），APK 使用
  `SukiSU_Nova_v3.0.0_35000-arm64-v8a-release.apk`；包名 `com.sukisu.nova`（旧 Ultra 包不再被识别）；
- 源码位于 `KernelSU/`，`drivers/kernelsu -> ../KernelSU/kernel`；
- 4.14 兼容层：`static_call.h` 版本守卫、`ksu_kvcalloc`、
  `strncpy_from_user_nofault` 别名、KPM 扩展在 <5.10 编出等。

### 3.2 SUSFS v2.4.0（`CONFIG_KSU_SUSFS`）
- `fs/susfs.c` + `include/linux/susfs.h` + `include/linux/susfs_def.h`；
- 18 个内核文件合并（NonGKI 4.14 移植 + BakaSU 4.14.186 修复）；
- 已开启：SUS_PATH、SUS_MOUNT、SUS_KSTAT、SPOOF_UNAME、SPOOF_CMDLINE、
  OPEN_REDIRECT、SUS_MAP、HIDE_KSU_SUSFS_SYMBOLS、PRIORITY_UID_FILTER、ENABLE_LOG。

### 3.3 NoMount v2.3.0（`CONFIG_NOMOUNT`）
- 源码位于 `NoMount/`，`fs/nomount -> ../NoMount/kernel/src`，内置驱动；
- 2.3.0：inode 基准规则、预打开 backing file 缓存、O(1) revalidate、
  readdir 并发修复、OOB/UAF 修复、xattr LSM 检查、跨 sb 代理 inode、启动提速。

### 3.4 PathMask 4.14
- `pathmask/`：`pathmask.ko` + `procguard.ko`，KSU 模块 WebUI 配置隐藏路径/UID；
- 与内核同一次构建，vermagic 匹配。

### 3.5 SELinux 隐藏（selinux_hook 1.1.6.1 内置）
- `security/selinux/selinux_hook_kpm/`：原 KPM 源码 + `hook_wrap` 注册表垫片；
- `include/linux/selinux_hook_kpm.h`：`hook_fargsN_t` 参数块；
- 12 个真实 hook 点：`sel_write_access`、`sel_write_context`、
  `security_read_policy`、`simple_read_from_buffer`、`sel_read_handle_status`、
  `sel_mmap_handle_status`、`selinux_status_update_policyload`、
  `security_setprocattr`、`selinux_setprocattr`、`context_struct_compute_av`、
  `string_to_context_struct`、`selinux_complete_init`；
- 4.14 不存在的 `selinux_status_update_seqlock` / `selinux_policy_commit`
  按源码 `if (addr)` 分支跳过（N/A，非删功能）。

### 3.6 反检测细节
- **clean-policy 引擎**：启动时快照干净策略到独立 `policydb`，
  `/sys/fs/selinux/policy` 返回 clean blob，AV 决策与 context 解析走 clean 数据；
- **status 伪造**：`sequence=4`、`policyload=1`；
- **`/access` 应答**：真内核形状 `0 ffffffff 0 ffffffff 1 0`，
  不再使用旧 KPM 的 `0 0 0 0 1 0` 指纹；
- **双层过滤**：KPM + v4 的 `security/selinux/selinux_query_filter.c`。

---

## 四、仓库结构

```
.
├── arch/arm64/configs/a22x_defconfig   # 默认配置
├── KernelSU/                           # SukiSU-Nova 源码（builtin）
├── NoMount/                            # NoMount v2.3.0（内核 + 模块）
├── pathmask/                           # PathMask 4.14 LKM 与 KSU 模块
├── security/selinux/selinux_hook_kpm/  # selinux_hook 内置移植
├── security/selinux/selinux_query_filter.c
├── security/selinux/ss/status.c        # status 伪造
├── anykernel/                          # AnyKernel3 模板
├── ci/                                 # 构建/打包脚本
├── docs/                               # 构建、功能、变更、架构审计、仓库总览
└── out/                                # 构建输出（不入库）
```

`drivers/kernelsu`、`fs/nomount` 是符号链接，指向上面两个子项目。

---

## 五、构建（WSL / Ubuntu）

依赖：

```bash
sudo apt-get update
sudo apt-get install -y build-essential bc bison flex libssl-dev libelf-dev \
    lz4 device-tree-compiler python3 zip unzip cpio rsync curl git zlib1g-dev
```

工具链（AOSP clang-r383902 + GCC 4.9 binutils）：

```bash
bash ci/fetch_toolchain.sh          # 下载到 ./toolchain（约 330 MB）
```

编译：

```bash
export ARCH=arm64
export PATH="$PWD/toolchain/gcc/linux-x86/aarch64/aarch64-linux-android-4.9/bin:$PWD/toolchain/clang/host/linux-x86/clang-r383902/bin:$PATH"
export CROSS_COMPILE=aarch64-linux-androidkernel-
export CLANG_TRIPLE=aarch64-linux-gnu-

make O=out a22x_defconfig
make O=out -j"$(nproc)" KCFLAGS=-w \
     CONFIG_SECTION_MISMATCH_WARN_ONLY=y LDFLAGS_BUILD_ID=
```

产物：`out/arch/arm64/boot/Image`、`Image.gz`。

或直接：

```bash
bash ci/build.sh      # 可复现构建
bash ci/package.sh    # 生成 dist/A226B-...zip（AnyKernel3）
bash pathmask/build.sh
```

可选：用 ccache 加速重复构建（只复用输入完全相同的编译单元）：

```bash
export USE_CCACHE=1
bash ci/build.sh
```

---

## 六、刷写

1. **先备份 boot**：TWRP → Backup → Boot；
2. TWRP → Install → 刷 Release 里的 `A226B-...zip`；
3. 重启，安装 **SukiSU-Nova v3.0.0** 管理器 APK（versionCode 35000，包名 `com.sukisu.nova`）；先卸载旧的 `com.sukisu.ultra`；
4. 需要路径隐藏时，再刷 `A226B-PathMask-4.14.zip` 并在 KSU 管理器里配置。

> 机型无 `init_boot`；不要刷到 `boot` 以外的分区。

---

## 七、验证

```bash
# 编译期
grep -E 'CONFIG_(KSU|NOMOUNT|SECURITY_SELINUX_HOOK_KPM)=' out/.config
nm out/vmlinux | grep -E 'hook_wrap|snapshot_clean_policy|selinux_hook_kpm_builtin_init'
strings out/arch/arm64/boot/Image | grep -icE 'kpimg|KernelPatch'   # 0

# 全功能收尾检查（CI 同一脚本，65 项）
bash ci/verify-features.sh

# CI 构建日志必须零 warning / error
bash ci/verify-log.sh build.log

# 设备端
dmesg | grep -i selinux_hook
dmesg | grep -i nomount
```

---

## 八、许可与致谢

- Samsung 4.14.186 内核源码：GPL-2.0；
- SukiSU-Ultra：GPL-2.0；SUSFS：GPL-2.0；NoMount：GPL-2.0；PathMask：GPL-2.0；
  selinux_hook：GPL-2.0；AnyKernel3：见上游。
- 详细第三方声明见 [NOTICE.md](NOTICE.md)。

本项目免费公开，不卖包、不设付费墙；刷机有风险，请自行承担。
