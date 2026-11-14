/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Public glue between the wrapped kernel functions and the built-in
 * selinux_hook port.  The argument blocks mirror the KernelPatch ABI the
 * original KPM was written against.
 */
#ifndef _LINUX_SELINUX_HOOK_KPM_H
#define _LINUX_SELINUX_HOOK_KPM_H

#include <linux/types.h>

typedef struct {
	union {
		struct {
			uint64_t data0;
			uint64_t data1;
			uint64_t data2;
			uint64_t data3;
			uint64_t data4;
			uint64_t data5;
			uint64_t data6;
			uint64_t data7;
		};
		uint64_t data[8];
	};
} hook_local_t;

typedef struct {
	void *chain;
	int skip_origin;
	hook_local_t local;
	uint64_t ret;
	union {
		struct {
			uint64_t arg0;
		};
		uint64_t args[1];
	};
} hook_fargs0_t;

typedef struct {
	void *chain;
	int skip_origin;
	hook_local_t local;
	uint64_t ret;
	union {
		struct {
			uint64_t arg0;
			uint64_t arg1;
			uint64_t arg2;
			uint64_t arg3;
		};
		uint64_t args[4];
	};
} hook_fargs4_t;

typedef hook_fargs4_t hook_fargs1_t;
typedef hook_fargs4_t hook_fargs2_t;
typedef hook_fargs4_t hook_fargs3_t;

typedef struct {
	void *chain;
	int skip_origin;
	hook_local_t local;
	uint64_t ret;
	union {
		struct {
			uint64_t arg0;
			uint64_t arg1;
			uint64_t arg2;
			uint64_t arg3;
			uint64_t arg4;
			uint64_t arg5;
			uint64_t arg6;
			uint64_t arg7;
		};
		uint64_t args[8];
	};
} hook_fargs8_t;

typedef hook_fargs8_t hook_fargs5_t;
typedef hook_fargs8_t hook_fargs6_t;
typedef hook_fargs8_t hook_fargs7_t;

typedef int32_t hook_err_t;

struct kp_hook_slot {
	void *addr;
	int argc;
	void *before;
	void *after;
	void *udata;
	int used;
};

void kp_hook_before(void *addr, void *fargs);
void kp_hook_after(void *addr, void *fargs);

#ifndef SELINUX_KERNEL_STATUS_VERSION
#define SELINUX_KERNEL_STATUS_VERSION 1
#endif

#endif /* _LINUX_SELINUX_HOOK_KPM_H */
