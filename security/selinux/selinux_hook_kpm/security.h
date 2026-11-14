/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Minimal stand-in for the KernelPatch "security.h" the KPM was built against.
 * selinux_hook.c carries its own copies of the SELinux on-disk structures, so
 * the kernel's real headers must NOT be pulled in here.
 */
#ifndef _KPM_SHIM_SECURITY_H
#define _KPM_SHIM_SECURITY_H

#include <linux/types.h>

struct selinux_state;
struct policydb;
struct sidtab;

struct av_decision {
	u32 allowed;
	u32 auditallow;
	u32 auditdeny;
	u32 seqno;
	u32 flags;
};

struct extended_perms_data {
	u32 p[8];
};

struct extended_perms {
	u16 len;
	struct extended_perms_data drivers;
};

#define security_xperm_set(perms, x) ((perms)[(x) >> 5] |= 1 << ((x) & 0x1f))

#endif /* _KPM_SHIM_SECURITY_H */
