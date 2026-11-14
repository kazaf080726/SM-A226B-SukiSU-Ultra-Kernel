# NOTICE / third-party code and licenses

This is a **Linux 4.14.186 kernel integration repository for the Samsung Galaxy
A22 5G (SM-A226B)**. Most files come from Samsung's official open-source kernel
release; the rest are build scripts, configuration, porting code and docs added
by this project.

| Part | License | Source |
|---|---|---|
| Samsung 4.14.186 kernel source | GPL-2.0 | Samsung Open Source Release Center |
| This project's scripts, README, patches | GPL-2.0 | this repository |
| SukiSU-Ultra (kernel part) | GPL-2.0 | https://github.com/ShirkNeko/SukiSU-Ultra |
| SUSFS v2.4.0 | GPL-2.0 | https://gitlab.com/simonpunk/susfs4ksu |
| NoMount v2.3.0 (kernel part) | GPL-2.0 | https://github.com/maxsteeel/nomount |
| NoMount metamodule/WebUI | GPL-3.0 | same, not shipped here |
| PathMask | GPL-2.0 | https://github.com/Andrea-lyz/LKM-PathMask |
| selinux_hook 1.1.6.1 | GPL-2.0 | https://github.com/Admirepowered/selinux_hook |
| AnyKernel3 | see upstream | https://github.com/osm0sis/AnyKernel3 |

This kernel does **not** contain KPatch-Next or kpimg; selinux_hook is compiled
in as source. Flashing is at your own risk; Samsung/KNOX/SukiSU/SUSFS/NoMount/
PathMask names and trademarks belong to their respective owners.
