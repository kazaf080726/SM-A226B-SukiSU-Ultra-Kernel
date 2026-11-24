# V28 发布说明 — 重大更新（Major Update）

> 中文 | [English](RELEASE-v28.en.md)

**日期：** 2026-11-24  
**内核：** Linux 4.14.186（Samsung SM-A226B / A22 5G）  
**Root：** SukiSU-Nova builtin · KSU_VERSION 35081  
**标签：** `v28`

---

## ⬆ 为什么是重大更新

V28 把本项目最核心的 SELinux 隐藏层从 **KPM 热补丁垫片**整体替换为
**与 LKM（main 分支）同源的内核内实现**，架构级变更，不是小修补：

- 删除 KPM shim（10 文件约 4.7k 行）+ 旧的原生查询过滤层；
- 还原 12 个被内联补丁改写的内核函数；
- 新增 4.14 内建的 clean policydb 引擎（策略快照、自答查询、策略 blob、setprocattr 语义）；
- 隐藏行为与一加 Ace 5（5.10+ LKM）的 `selinux_hide.c` 对齐。

## 主要变更

### 移除
- `security/selinux/selinux_hook_kpm/`
- `include/linux/selinux_hook_kpm.h`
- `security/selinux/selinux_query_filter.{c,h}`
- `CONFIG_SECURITY_SELINUX_HOOK_KPM` / `CONFIG_SECURITY_SELINUX_QUERY_FILTER`

### 新增（4.14 builtin，LKM 语义）
- `ksu_selinux_hide_capture_clean_policy()`：在 KSU 改写 live policy 之前抓取原厂
  policydb，得到独立的 `policydb + sidtab + policy blob`；
- clean 版 `context_to_sid / sid_to_context / compute_av(seqno=1)`；
- `selinuxfs`: `SEL_ACCESS/SEL_CONTEXT` 写节点由 clean 引擎应答；App 读
  `/sys/fs/selinux/policy`（含 mmap）返回 clean blob；
- `hooks.c`: App 写 `/proc/self/attr/current` 的隐藏上下文返回 `-EINVAL`；
- `kernel/reboot.c` + `supercall/dispatch.c`: App 侧 reboot supercall 与 stock 一致；
- `status.c`: 原厂 0/0 行为 + App 视角 fake status page `1 0 1 0 1`。

## 验证结果（真机 SM-A226B）

| 项目 | 结果 |
|---|---|
| Maple Ultra 2.8.2 | **完美！ALL CLEAR**（283/283、284/284 完美，0 危险 / 0 可疑） |
| Maple SELinux 侧信道项 | 修复后所有轮次**不再出现** |
| Hunter 6.72 | **未发现风险**（108 项，高风险 0 / 不可用 0 / 超时 0） |
| SukiSU Nova 管理器 | 工作中（12 超级用户 / 12 模块，Built-in） |
| 状态页 | root 真实页；App `1 0 1 0 1` |
| 稳定性 | 正常开机，无 panic；boot 备份已保留 |

> 说明：A22 上 Maple 的 APatch/KernelSU **计时类**检测项受负载影响会偶发
> （其自身证据亦声明"受性能波动影响可能不稳定出现"），与 SELinux 隐藏无关；
> 可在设备空闲时复测。

## 产物

- `A226B-SukiSU-SUSFS240-NoMount-v28-20261114.zip`（AnyKernel3）
- `A226B-SukiSU-SUSFS240-NoMount-v28-modules-20261114.zip`（模块）
- `A226B-PathMask-4.14-20261114.zip`
- `Image.gz` / `SHA256SUMS.txt`

SHA256 见同目录 `SHA256SUMS.txt`。

## 升级建议

1. 先备份当前 boot（TWRP 或 `dd if=/dev/block/by-name/boot`）；
2. 刷入 `A226B-...-v28-*.zip`（AK3，`do.devicecheck=0`）；
3. 首次开机会抓取 clean policy，属正常一次开销；
4. 管理器打开确认 "工作中 / Built-in / 35081"。
