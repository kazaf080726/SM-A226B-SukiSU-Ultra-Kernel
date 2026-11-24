/* SPDX-License-Identifier: GPL-2.0 */
/*
 * 4.14 builtin SELinux hide support (LKM-style self-answered queries).
 *
 * The 5.10+ tree answers /sys/fs/selinux/{access,context} from a saved clean
 * policydb (my_write_access/my_write_context in feature/selinux_hide.c).
 * 4.14 lacks struct selinux_policy, so the same semantics are implemented
 * here: snapshot the pristine policydb before KernelSU patches it, then
 * evaluate user queries against that snapshot.
 */
#ifndef _KSU_SELINUX_HIDE_H
#define _KSU_SELINUX_HIDE_H

#include <linux/types.h>

#ifdef CONFIG_KSU
struct av_decision;
extern bool ksu_selinux_hide_enabled;

int ksu_selinux_hide_capture_clean_policy(void);
bool ksu_selinux_hide_policy_ready(void);
bool ksu_selinux_hide_active(void);
int ksu_selinux_hide_context_to_sid(const char *scontext, u32 scontext_len, u32 *sid);
int ksu_selinux_hide_sid_to_context(u32 sid, char *buf, u32 size);
int ksu_selinux_hide_policy_blob(void **data, size_t *len);
void ksu_selinux_hide_compute_av(u32 ssid, u32 tsid, u16 tclass,
                                 struct av_decision *avd);
#else
static inline int ksu_selinux_hide_capture_clean_policy(void) { return -ENOSYS; }
static inline bool ksu_selinux_hide_policy_ready(void) { return false; }
static inline bool ksu_selinux_hide_active(void) { return false; }
static inline int ksu_selinux_hide_context_to_sid(const char *s, u32 l, u32 *sid)
{ return -ENOSYS; }
static inline int ksu_selinux_hide_sid_to_context(u32 sid, char *buf, u32 size)
{ return -ENOSYS; }
static inline int ksu_selinux_hide_policy_blob(void **d, size_t *l) { return -ENOSYS; }
static inline void ksu_selinux_hide_compute_av(u32 a, u32 b, u16 c, void *d) {}
#endif

#endif /* _KSU_SELINUX_HIDE_H */
