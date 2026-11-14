# 构建与可复现性 / Build & reproducibility

本仓库已**内置** SukiSU（`KernelSU/`）与 NoMount（`NoMount/`）源码，
不需要再 clone 子项目；`drivers/kernelsu` 与 `fs/nomount` 是符号链接。

## 1. 依赖

Ubuntu 22.04 / 24.04 / 26.04（WSL2 亦可）：

```bash
sudo apt-get update
sudo apt-get install -y build-essential bc bison flex libssl-dev libelf-dev \
    lz4 device-tree-compiler python3 zip unzip cpio rsync curl git zlib1g-dev
```

## 2. 工具链

顶层 `Makefile` 里 `CC` 的路径是硬编码的，工具链必须放在仓库根目录的 `toolchain/`：

```bash
bash ci/fetch_toolchain.sh     # AOSP clang-r383902 + GCC 4.9 binutils，约 330 MB
```

需要的两个目录：

```
toolchain/clang/host/linux-x86/clang-r383902/bin/clang
toolchain/gcc/linux-x86/aarch64/aarch64-linux-android-4.9/bin/aarch64-linux-androidkernel-
```

## 3. 编译

```bash
export ARCH=arm64
export PATH="$PWD/toolchain/gcc/linux-x86/aarch64/aarch64-linux-android-4.9/bin:$PWD/toolchain/clang/host/linux-x86/clang-r383902/bin:$PATH"
export CROSS_COMPILE=aarch64-linux-androidkernel-
export CLANG_TRIPLE=aarch64-linux-gnu-

make O=out a22x_defconfig
make O=out -j"$(nproc)" KCFLAGS=-w \
     CONFIG_SECTION_MISMATCH_WARN_ONLY=y LDFLAGS_BUILD_ID=
```

产物：

```
out/arch/arm64/boot/Image
out/arch/arm64/boot/Image.gz
```

也可以直接用脚本：

```bash
bash ci/build.sh          # 固定 KBUILD_BUILD_USER/HOST/TIMESTAMP，可复现
bash ci/package.sh        # AnyKernel3 zip -> dist/
bash pathmask/build.sh    # PathMask LKM + KSU 模块 zip -> dist/
```

`JOBS=n` 控制并行度，`OUT=...` 换输出目录。

## 4. 关键配置

`arch/arm64/configs/a22x_defconfig` 中：

```
CONFIG_KSU=y
CONFIG_KSU_SUSFS=y
CONFIG_KSU_SUSFS_SUS_PATH=y
CONFIG_KSU_SUSFS_SUS_MOUNT=y
CONFIG_KSU_SUSFS_SUS_KSTAT=y
CONFIG_KSU_SUSFS_SPOOF_UNAME=y
CONFIG_KSU_SUSFS_SPOOF_CMDLINE_OR_BOOTCONFIG=y
CONFIG_KSU_SUSFS_OPEN_REDIRECT=y
CONFIG_KSU_SUSFS_SUS_MAP=y
CONFIG_KSU_SUSFS_HIDE_KSU_SUSFS_SYMBOLS=y
CONFIG_KSU_SUSFS_PRIORITY_UID_FILTER=y
CONFIG_NOMOUNT=y
CONFIG_SECURITY_SELINUX_QUERY_FILTER=y
CONFIG_SECURITY_SELINUX_HOOK_KPM=y
CONFIG_KALLSYMS_ALL=y
```

## 5. 可复现构建

`ci/build.sh` 会固定：

- `KBUILD_BUILD_USER=A226B`、`KBUILD_BUILD_HOST=a22x`、`KBUILD_BUILD_VERSION=1`；
- `KBUILD_BUILD_TIMESTAMP` 取 `ci/repro-date.txt`（每次发版递增）；
- `LOCALVERSION=""`，避免 `setlocalversion` 追加 `+`。

同样的源码 + 同样的 `repro-date.txt` 应得到逐字节相同的 `Image`。

## 6. 常见问题

- **`linux/static_call.h: No such file`**：4.14 没有该头，SukiSU 侧已做版本守卫；
  若你更新 KernelSU 后报错，检查 `KernelSU/kernel/kernel_includes.h`。
- **`access_ok()` 参数数量**：4.14 是 2 参数；SukiSU 的 KPM 扩展在 <5.10 已整体编出。
- **`kvcalloc` / `strncpy_from_user_nofault` 未定义**：4.14 没有，使用
  `ksu_kvcalloc` / `ksu_strncpy_from_user_nofault` 兼容层。
- **符号链接失效**：在 Windows 上 clone 需开启 symlink 支持，或直接在 WSL/Linux 里构建。

## 7. ccache（可选）

`ci/build.sh` 支持 `USE_CCACHE=1`，用 ccache 包住 clang：

```bash
export USE_CCACHE=1
export CCACHE_DIR="$PWD/.ccache"   # 默认就是仓库下 .ccache
bash ci/build.sh
```

ccache 按预处理内容寻址，只复用输入完全相同的编译单元；源码改动会正常失效。
GitHub Actions 已默认开启，并用 `actions/cache` 缓存 `.ccache`（单次上限 1.5GB）。

## 8. GitHub Actions

仓库自带 `.github/workflows/build.yml`，与本地同一套 `ci/` 脚本：

- push 到 `main`（源码/配置变化）→ 自动编译、打包、上传 Actions 产物；
- push `v*` tag → 额外创建/更新 GitHub Release，挂 AnyKernel3 与 PathMask 包；
- `workflow_dispatch` → 手动触发一次；
- 工具链用 `actions/cache` 缓存，第二次构建不再重复下载；
- 构建末尾强制跑 `ci/verify-features.sh`（65 项：config、符号、12 个 hook、
  Image 标记、PathMask、AnyKernel3），任何一项缺失即失败，不会上传产物或发 Release；
- 构建日志由 `ci/verify-log.sh` 扫描，出现任何 `warning:` / `error:` /
  `Syntax error` 都会让 CI 失败，保证每条构建日志都是干净的。

产物在 Actions run 页面的 Artifacts 里：`Image.gz`、`A226B-...zip`、
`A226B-...-modules-...zip`、PathMask zip。
