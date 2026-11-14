> 🌐 中文（默认） | [English](NOTICE.en.md)

# NOTICE / 第三方代码与许可

本项目是 **Samsung Galaxy A22 5G (SM-A226B) 的 Linux 4.14.186 内核集成仓库**，
免费公开、可自行编译。仓库中大部分文件来自三星官方开源内核，
其余为本项目新增的构建脚本、配置、移植代码与文档。

## 许可概览

| 部分 | 许可 | 来源 |
|---|---|---|
| Samsung 4.14.186 内核源码 | GPL-2.0 | Samsung Open Source Release Center |
| 本项目构建脚本、README、patches | GPL-2.0 | 本仓库 |
| SukiSU-Ultra（内核部分） | GPL-2.0 | https://github.com/ShirkNeko/SukiSU-Ultra |
| SUSFS v2.4.0 | GPL-2.0 | https://gitlab.com/simonpunk/susfs4ksu |
| NoMount v2.3.0（内核部分） | GPL-2.0 | https://github.com/maxsteeel/nomount |
| NoMount Metamodule/WebUI | GPL-3.0 | 同上（未包含在本仓库） |
| PathMask | GPL-2.0 | https://github.com/Andrea-lyz/LKM-PathMask |
| selinux_hook 1.1.6.1 | GPL-2.0 | https://github.com/Admirepowered/selinux_hook |
| AnyKernel3 | 见上游 | https://github.com/osm0sis/AnyKernel3 |

`LICENSE` 为本仓库自有部分与内核源码的 GPL-2.0 文本；上游组件仍以各自仓库的
LICENSE 为准。

## 说明

- 本内核**不包含** KPatch-Next / kpimg；selinux_hook 以源码形式编入，
  不再需要 kpatch 或 KPM 加载器。
- PathMask 明确声明不用于规避游戏反作弊；请遵守其上游免责声明。

## 免费与分发

- 本项目官方发布永久免费，不提供付费版本、付费服务或授权码。
- 允许 fork、修改、自行编译和再分发，但必须：
  - 遵守对应组件的 GPL 许可；
  - 保留原作者、来源链接和本 NOTICE；
  - 不得声称是官方付费独家版本，或删除版权声明。

## 免责声明

- 刷机、解锁 BootLoader、修改 boot 分区均有风险，可能导致数据丢失、
  无法开机或 KNOX 永久熔断。
- 本项目按“现状”提供，不提供任何明示或暗示担保；使用风险由使用者自行承担。
- Samsung、KNOX、SukiSU、SUSFS、NoMount、PathMask 等名称和商标归各自所有者。
