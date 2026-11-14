/* SPDX-License-Identifier: GPL-2.0 */
/* KernelPatch API shim for the built-in selinux_hook port. */
#ifndef _SELINUX_HOOK_KPM_CORE_H
#define _SELINUX_HOOK_KPM_CORE_H

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/cred.h>
#include <linux/uidgid.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/namei.h>
#include <linux/kallsyms.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/sched/task.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/version.h>
#include <linux/selinux_hook_kpm.h>

#define VERSION(a, b, c) (((a) << 16) | ((b) << 8) | (c))

static const u32 kver = VERSION(4, 14, 186);

#define KP_MAX_HOOKS 48

extern struct kp_hook_slot kp_hooks[KP_MAX_HOOKS];
struct kp_hook_slot *kp_hook_find(void *addr);

hook_err_t hook_wrap(void *func, int argno, void *before, void *after,
		     void *udata);
void hook_unwrap_remove(void *func);
static inline void unhook(void *addr) { hook_unwrap_remove(addr); }

/* 4.14 does not have this type; the KPM only stores it on the 5.x path. */
struct selinux_load_state {
	void *policy;
	void *convert_data;
};

static inline uid_t kpm_current_uid(void)
{
	return __kuid_val(current_uid());
}

#endif /* _SELINUX_HOOK_KPM_CORE_H */
