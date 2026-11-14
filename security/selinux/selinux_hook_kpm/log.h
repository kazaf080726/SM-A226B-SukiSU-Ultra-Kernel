/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _KPM_SHIM_LOG_H
#define _KPM_SHIM_LOG_H
#include <linux/printk.h>
#define logki(fmt, ...) pr_info("[selinux_hook] " fmt, ##__VA_ARGS__)
#define logke(fmt, ...) pr_err("[selinux_hook] " fmt, ##__VA_ARGS__)
#define logkd(fmt, ...) pr_debug("[selinux_hook] " fmt, ##__VA_ARGS__)
#endif
