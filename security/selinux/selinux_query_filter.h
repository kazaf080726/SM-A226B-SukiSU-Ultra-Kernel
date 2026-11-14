/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Native SELinux query filter (no KernelPatch / no KPM).
 *
 * Ported from selinux_hook 1.1.6.1 (GPL-2.0, Admire) legacy 4.14 path.
 */
#ifndef _SELINUX_QUERY_FILTER_H
#define _SELINUX_QUERY_FILTER_H

#include <linux/types.h>

#ifdef CONFIG_SECURITY_SELINUX_QUERY_FILTER
int selinux_query_filter_access(char *buf, size_t size);
int selinux_query_filter_context(const char *buf, size_t size);
bool selinux_query_filter_setprocattr(const char *name, const void *value,
				      size_t size);
#else
static inline int selinux_query_filter_access(char *buf, size_t size)
{
	return 0;
}
static inline int selinux_query_filter_context(const char *buf, size_t size)
{
	return 0;
}
static inline bool selinux_query_filter_setprocattr(const char *name,
						    const void *value,
						    size_t size)
{
	return false;
}
#endif

#endif /* _SELINUX_QUERY_FILTER_H */
