// SPDX-License-Identifier: GPL-2.0
/*
 * SELinux hide feature handler for kernels older than 5.10.
 *
 * The full implementation in feature/selinux_hide.c depends on the per-policy
 * (struct selinux_policy) layout that only exists since 5.10, so it cannot be
 * built for 4.14.  On these kernels the actual hiding is performed by the
 * "selinux_hook" KernelPatch module (selinux_hook_1.1.6.1.kpm), which keeps a
 * clean copy of the policydb and serves it to /sys/fs/selinux readers.
 *
 * This file only exports the KSU feature handler so that the manager's
 * "Hide SELinux modification" switch has a real kernel backend:
 *   - KSU_FEATURE_SELINUX_HIDE get/set work and the value is persisted by
 *     ksud (feature save/load), exactly like on 5.10+;
 *   - the matching KSU userspace module reads this value at boot and
 *     loads/unloads the KPM accordingly.
 */
#include <linux/version.h>
#include <linux/types.h>
#include <linux/printk.h>

#include "feature/selinux_hide.h"
#include "policy/feature.h"
#include "uapi/feature.h"

bool ksu_selinux_hide_enabled __read_mostly = false;
bool ksu_selinux_hide_running __read_mostly = false;

static int selinux_hide_feature_get(u64 *value)
{
    *value = ksu_selinux_hide_enabled ? 1 : 0;
    return 0;
}

static int selinux_hide_feature_set(u64 value)
{
    bool enable = value != 0;

    ksu_selinux_hide_enabled = enable;
    /* The KPM backend is loaded/unloaded by userspace at next boot. */
    ksu_selinux_hide_running = enable;
    pr_info("selinux_hide(4.14): set to %d (selinux_hook KPM backend)\n", enable);
    return 0;
}

static const struct ksu_feature_handler selinux_hide_handler = {
    .feature_id = KSU_FEATURE_SELINUX_HIDE,
    .name = "selinux_hide",
    .get_handler = selinux_hide_feature_get,
    .set_handler = selinux_hide_feature_set,
};

void __init ksu_selinux_hide_init(void)
{
    if (ksu_register_feature_handler(&selinux_hide_handler))
        pr_err("selinux_hide(4.14): failed to register feature handler\n");
}

void ksu_selinux_hide_exit(void)
{
    ksu_unregister_feature_handler(KSU_FEATURE_SELINUX_HIDE);
}

/* Not needed on 4.14: the KPM owns the clean policy. */
void ksu_selinux_hide_drop_backup_if_unused(void) {}
void ksu_selinux_hide_handle_second_stage(void) {}
void ksu_selinux_hide_handle_post_fs_data(void) {}
