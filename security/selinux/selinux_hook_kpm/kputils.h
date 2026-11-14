/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _KPM_SHIM_KPUTILS_H
#define _KPM_SHIM_KPUTILS_H
#include "kpm_core.h"

static inline int compat_copy_to_user(void __user *to, const void *from, int n)
{
	return copy_to_user(to, from, n);
}

static inline long compat_strncpy_from_user(char *dest, const char __user *src,
					    long count)
{
	return strncpy_from_user(dest, src, count);
}

#define kf_sprintf snprintf
#endif
