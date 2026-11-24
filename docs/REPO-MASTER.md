# SM-A226B 仓库总览 / Repository master guide

> 中文 | [English](REPO-MASTER.en.md)

> 面向接手维护的人：一页看懂仓库是什么、由什么组成、怎么构建、运行时怎么工作、哪里不能动。
> 细节（47 个改动文件的函数级说明）见 [ARCHITECTURE-V12.md](ARCHITECTURE-V12.md)。
> 当前基线：**V13**（`v13` tag），内核 4.14.186 / a22x / SM-A226B。

---

## 1. 身份

| 项 | 值 |
|---|---|
| 仓库 | `kazaf080726/SM-A226B-SukiSU-Ultra-Kernel` |
| HEAD | `v13`（源码内容与 `v12` = `9877efbd4` 相同） |
| 提交数 | 21（`v1`–`v7` 指向同一个导入提交 `061b7fcef`） |
| 规模 | 78,141 跟踪文件，工作树 1.2 GB，`.git` 约 430 MB |
| 分支 | `main`；`shield-archive-20261115` 为废弃历史（C 级，不要用） |

V8 之后每版一个提交；V1–V6 只能从 `docs/CHANGELOG.md` 追溯，无法 diff。

---

## 2. 来源谱系

| 组成 | 上游 | 本地改动 |
|---|---|---|
| 内核主体 | Samsung `SM-A226B_13_Opensource` 4.14.186 | 47 个文件，0 个删除 |
| KernelSU | SukiSU-Nova `builtin` @ `4bec532d`（原 SukiSU-Ultra） | 4 改 + 1 新增，全部是 4.14 兼容 |
| NoMount | `maxsteeel/nomount` @ `7914458`（v2.3.0） | `kernel/src` 逐字节一致 |
| SUSFS | NonGKI 4.14 移植补丁（`patches/`） | 10 个 config + 4.14.186 适配 |
| selinux_hook | Admire 的 KPM 1.1.7 源码 | 编入内核 + KernelPatch API shim |
| PathMask / 反检测 / CI | 本项目原创 | — |

重新核对上游身份：

```bash
git clone --depth 1 --branch builtin https://github.com/SukiSU-Nova/SukiSU-Nova /tmp/k
diff -rq /tmp/k/kernel KernelSU/kernel                 # 期望仅 5 行差异

git clone --depth 50 https://github.com/maxsteeel/nomount /tmp/n
diff -rq /tmp/n/kernel/src NoMount/kernel/src          # 期望 0 行
```

---

## 3. 目录地图

```
arch/arm64/configs/a22x_defconfig   唯一配置入口（+15 开关；DMESG_RESTRICT=y；DEFEX 关闭）
fs/ kernel/ mm/ security/ ...       47 个宿主文件（清单见 ARCHITECTURE-V12.md §3）
KernelSU/                           内置 SukiSU（drivers/kernelsu -> ../KernelSU/kernel）
NoMount/                            内置 NoMount（fs/nomount -> ../NoMount/kernel/src）
pathmask/                           PathMask/procguard LKM 源码 + KSU 模块配置
security/selinux/selinux_hook_kpm/  12 个 hook 的内核内实现
security/selinux/selinux_query_filter.c  常开的 v4 SELinux 查询过滤
ci/                                 build / package / verify / fetch_toolchain / repro-date
anykernel/                          AnyKernel3 模板（BLOCK=/dev/block/by-name/boot）
patches/                            移植来源与历史补丁（不参与构建）
docs/                               BUILD / FEATURES / CHANGELOG / ARCHITECTURE-V12 / 本文件
```

---

## 4. 六层运行时

| 层 | 入口 | 关键文件 | 开关 |
|---|---|---|---|
| KernelSU | `kernelsu_init()` | `KernelSU/kernel/ksu_main.c` + 7 个 `ksu_handle_*` 宿主 | `CONFIG_KSU` |
| SUSFS 2.4.0 | `susfs_init` + `CMD_SUSFS_*` ioctl | `fs/susfs.c` + 16 个宿主 | `CONFIG_KSU_SUSFS*` |
| NoMount 2.3.0 | `fs_initcall(nomount_init)` | `NoMount/kernel/src/nomount.c` | `CONFIG_NOMOUNT` |
| PathMask | 模块加载 | `pathmask/src/*.c`（kretprobe + kallsyms） | 运行时加载 |
| 反检测 | 各 syscall/proc 入口 | `fs/proc_namespace.c`、`ss/status.c`、`selinux_query_filter.c` 等 | 编译期固定 |
| KPM / selinux_hook | `late_initcall(selinux_hook_kpm_builtin_init)` | `selinux_hook_kpm/` + 12 个包装点 | `CONFIG_SECURITY_SELINUX_HOOK_KPM` |

运行时自检：

```bash
su -c 'ksud debug version'          # Kernel Version: 30007
su -c 'ksud susfs features'         # 10 项
su -c 'ksud feature list'           # su_compat/kernel_umount/adb_root/selinux_hide
su -c 'lsmod | grep -E "pathmask|procguard"'
su -c 'dmesg | grep -i selinux_hook'
```

---

## 5. 构建与门禁

```bash
OUT=/path/to/out bash ci/build.sh       # 可复现构建；写 $OUT/build.log 与 $OUT/ksu-version.txt
OUT=/path/to/out bash ci/package.sh     # AK3 + modules + dist/Image.gz
OUT=/path/to/out bash pathmask/build.sh # PathMask LKM（vermagic 必须匹配）
bash ci/verify.sh && bash ci/verify-features.sh && bash ci/verify-log.sh build.log
```

- `ci/build.sh`：净化 PATH、固定 `KBUILD_BUILD_*`、`LOCALVERSION=""`、清源树 `generated`、`LDFLAGS_BUILD_ID=`；从日志提取 SukiSU 版本号供打包使用。
- `ci/package.sh`：`build-info.txt` 的 `KSU_VERSION` 取自 `$OUT/ksu-version.txt`（不再硬编码），条目 mtime 与 zip 顺序固定。
- 门禁：镜像 ≤28 MiB、PathMask vermagic、65 项 feature 检查、构建日志零 `warning:`/`error:`。
- CI：push `main` / `v*` tag 触发；tag 构建会自动发布 Release。

**注意**：`KSU_VERSION = 30000 + 上游 main 提交数 + 996`（与管理器 versionCode 同一公式；
可用 `KSU_VERSION_OVERRIDE` 覆盖），由 `KernelSU/kernel/Makefile` 在编译时联网计算。
同一份源码在不同日期编译会得到不同版本号；离线构建会回退到本仓库提交数（数值坍缩）。
这是"内核始终对齐最新管理器"的代价，不是 bug。

---

## 6. 配置矩阵

| 能力 | 开关 | 依赖 |
|---|---|---|
| Root | `CONFIG_KSU=y` | — |
| SUSFS | `CONFIG_KSU_SUSFS` + 9 子项 | 需要 `CONFIG_KSU` |
| NoMount | `CONFIG_NOMOUNT=y` | — |
| 查询过滤 | `CONFIG_SECURITY_SELINUX_QUERY_FILTER=y` | `SECURITY_SELINUX` |
| 内置 KPM | `CONFIG_SECURITY_SELINUX_HOOK_KPM=y` | `SECURITY_SELINUX && KALLSYMS_ALL` |

---

## 7. 不变量（改代码前必须遵守）

1. 过滤一律 fail-open，`uid < 10000` 永不放过滤；
2. 不全局屏蔽 selinuxfs，只按查询类型拦截；
3. 不动 `system_server` / `zygote` / `app_zygote` / UID 1000；
4. 不改 PackageManager 数据库、不改系统应用；
5. 不重新引入 `a226b_shield` 与 LSPosed 互补模块；
6. 刷机前必须备份 boot 并读回校验；
7. 换 boot 镜像要同步 `init/main.c` 的 `vbmeta.digest`；
8. 换内核必须重编 PathMask（vermagic 绑定）；
9. 更新 KernelSU 源码必须保留 5 处 4.14 兼容改动；
10. 检测器 UID（10296 春秋 / 10272 Hunter）是硬编码，重装会变。

---

## 8. 已知问题

| 问题 | 说明 |
|---|---|
| `KSU_VERSION_FULL` 的 sha | `KernelSU/.git` 不存在，git 向上取到本仓库 HEAD，不代表 SukiSU 提交 |
| 离线构建版本号坍缩 | 回退到本仓库提交数（21），得到 37206 这类数值 |
| `dist/` 历史残留 | shield 期构建曾放在这里，刷了等于回滚 shield；已移至 `dist-shield-20261115` |
| `preinit` 关闭 | 未生成 `usr/include`，KSU preinit 阶段不编入（V12 起即如此） |
| `qf_enabled()` 恒真 | 查询过滤器实际常开，与注释"仅 feature 开启时"不符 |
| `security/selinux/ss/status.c` | `sequence=4`/`policyload=1` 为硬编码干净值 |

---

## 9. 排障速查

| 现象 | 先看 |
|---|---|
| 编译报 `static_call.h` / `kvcalloc` / `strncpy_from_user_nofault` | KernelSU 的 4.14 兼容层是否被覆盖 |
| 模块加载失败 | PathMask 是否针对当前内核重编（vermagic） |
| 开机卡 logo | `dmesg` 中 `selinux_hook` / `nomount` 初始化顺序 |
| 检测器报挂载异常 | 该 UID 是否在 10296/10272 豁免表 |
| 管理器提示版本不匹配 | `ksud debug version` 与 APK versionCode 是否一致 |

---

## 10. 文档索引

| 文件 | 内容 |
|---|---|
| `docs/ARCHITECTURE-V12.md` | 全量 diff、47 文件函数级说明、A/B 事实等级 |
| `docs/BUILD.md` | 依赖、工具链、可复现构建、CI |
| `docs/FEATURES.md` | 功能清单与验证方法 |
| `docs/CHANGELOG.md` | V1–V13 变更记录 |
| `patches/README.md` | 移植来源与补齐项 |
