# NOTICE / third-party code and licenses

> [中文](NOTICE.md) | **English**

This is a **Linux 4.14.186 kernel integration repository for the Samsung Galaxy A22 5G
(SM-A226B)**, free and public, buildable by anyone. Most files come from Samsung's
official open-source kernel release; the rest are build scripts, configuration,
porting code, the in-house 4.14 SELinux hiding engine and documentation added by
this project.

## License overview

| Part | License | Source |
|---|---|---|
| Samsung 4.14.186 kernel source | GPL-2.0 | Samsung Open Source Release Center |
| This project's scripts, README, patches, 4.14 SELinux hiding engine | GPL-2.0 | this repository |
| SukiSU-Nova (kernel part, formerly SukiSU-Ultra) | GPL-2.0 | https://github.com/SukiSU-Ultra/SukiSU-Ultra |
| SUSFS v2.4.0 | GPL-2.0 | https://gitlab.com/simonpunk/susfs4ksu |
| NoMount v2.3.1 (kernel part) | GPL-2.0 | https://github.com/maxsteeel/nomount |
| NoMount metamodule/WebUI | GPL-3.0 | same, not shipped here |
| PathMask v2.8.3 (4.14 port) | GPL-2.0 | https://github.com/Andrea-lyz/LKM-PathMask |
| AnyKernel3 | see upstream | https://github.com/osm0sis/AnyKernel3 |

`LICENSE` contains the GPL-2.0 text for this project's own parts and the kernel
source; upstream components remain under the LICENSE of their own repositories.

## Notes

- Since V28, **KPM / selinux_hook / KPatch-Next / kpimg have all been removed**.
  SELinux hiding is now this project's own 4.14 in-kernel engine
  (`security/selinux/ksu_selinux_hide.h` + `ss/services.c` + `selinuxfs.c` +
  `hooks.c`), sharing the same logic as the LKM (main branch) `selinux_hide.c`;
- PathMask explicitly states that it must not be used to bypass game anti-cheat;
  follow its upstream disclaimer.

## Free distribution

- Official releases of this project are permanently free: no paid versions, paid
  services or license keys.
- Forking, modifying, building and redistributing are allowed, but you must:
  - comply with the GPL license of each component;
  - keep the original authors, source links and this NOTICE;
  - not claim to be the official paid/exclusive build, or remove copyright notices.

## Disclaimer

- Flashing, unlocking the bootloader and modifying the boot partition are risky and
  may cause data loss, a non-booting device or permanent KNOX tripping.
- This project is provided "as is", without any express or implied warranty; you
  use it at your own risk.
- Samsung, KNOX, SukiSU, SUSFS, NoMount, PathMask and other names/trademarks belong
  to their respective owners.
