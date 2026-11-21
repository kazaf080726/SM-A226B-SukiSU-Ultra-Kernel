// SPDX-License-Identifier: GPL-2.0
/*
 * AVC log spoofing feature handler for the 4.14 builtin tree.
 *
 * Upstream SukiSU-Nova implements this as KSU_FEATURE_AVC_SPOOF (id 10003)
 * and backs it with a kprobe on slow_avc_audit().  This tree already carries
 * an equivalent rewrite inside security/selinux/avc.c (avc_dump_query()),
 * driven by the SUSFS static key susfs_is_avc_log_spoofing_enabled, so the
 * handler only has to expose and toggle that switch.
 *
 * Without this handler the manager asks for feature 10003, gets "not
 * supported" back, and the "AVC Spoof" switch stays greyed out.
 */
#include <linux/types.h>
#include <linux/printk.h>
#include <linux/jump_label.h>

#include "feature/avc_spoof.h"
#include "policy/feature.h"
#include "uapi/feature.h"

#ifdef CONFIG_KSU_SUSFS
extern struct static_key_false susfs_is_avc_log_spoofing_enabled;

static bool ksu_avc_spoof_enabled __read_mostly = true;

static int avc_spoof_feature_get(u64 *value)
{
    *value = ksu_avc_spoof_enabled ? 1 : 0;
    return 0;
}

static int avc_spoof_feature_set(u64 value)
{
    bool enable = value != 0;

    ksu_avc_spoof_enabled = enable;
    if (enable)
        static_branch_enable(&susfs_is_avc_log_spoofing_enabled);
    else
        static_branch_disable(&susfs_is_avc_log_spoofing_enabled);
    pr_info("avc_spoof: set to %d\n", enable);
    return 0;
}

static const struct ksu_feature_handler avc_spoof_handler = {
    .feature_id = KSU_FEATURE_AVC_SPOOF,
    .name = "avc_spoof",
    .get_handler = avc_spoof_feature_get,
    .set_handler = avc_spoof_feature_set,
};

void __init ksu_avc_spoof_init(void)
{
    if (ksu_register_feature_handler(&avc_spoof_handler)) {
        pr_err("avc_spoof: failed to register feature handler\n");
        return;
    }
    /* Default on: the reset device has no modules to enable it at boot. */
    if (ksu_avc_spoof_enabled)
        static_branch_enable(&susfs_is_avc_log_spoofing_enabled);
    pr_info("avc_spoof: feature registered, default=%d\n", ksu_avc_spoof_enabled);
}

void __exit ksu_avc_spoof_exit(void)
{
    ksu_unregister_feature_handler(KSU_FEATURE_AVC_SPOOF);
}
#else
void __init ksu_avc_spoof_init(void) {}
void __exit ksu_avc_spoof_exit(void) {}
#endif
