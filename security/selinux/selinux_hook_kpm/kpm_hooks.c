// SPDX-License-Identifier: GPL-2.0
/*
 * KernelPatch hook API shim for the built-in selinux_hook port.
 *
 * The original KPM installs inline hooks through kpimg.  Compiled into the
 * kernel we cannot patch text, so hook_wrap() only records the callbacks and
 * the wrapped kernel functions call kp_hook_before()/kp_hook_after() directly.
 */
#include "kpm_core.h"

struct kp_hook_slot kp_hooks[KP_MAX_HOOKS];

struct kp_hook_slot *kp_hook_find(void *addr)
{
	int i;

	for (i = 0; i < KP_MAX_HOOKS; i++) {
		if (kp_hooks[i].used && kp_hooks[i].addr == addr)
			return &kp_hooks[i];
	}
	return NULL;
}

hook_err_t hook_wrap(void *func, int argno, void *before, void *after,
		     void *udata)
{
	int i;
	int free_slot = -1;

	if (!func)
		return -1;

	for (i = 0; i < KP_MAX_HOOKS; i++) {
		if (kp_hooks[i].used && kp_hooks[i].addr == func) {
			free_slot = i;
			break;
		}
		if (!kp_hooks[i].used && free_slot < 0)
			free_slot = i;
	}

	if (free_slot < 0)
		return -1;

	kp_hooks[free_slot].addr = func;
	kp_hooks[free_slot].argc = argno;
	kp_hooks[free_slot].before = before;
	kp_hooks[free_slot].after = after;
	kp_hooks[free_slot].udata = udata;
	kp_hooks[free_slot].used = 1;

	pr_info("[selinux_hook] hook_wrap %ps argc=%d before=%ps after=%ps\n",
		func, argno, before, after);
	return 0;
}

void hook_unwrap_remove(void *func)
{
	struct kp_hook_slot *s = kp_hook_find(func);

	if (s) {
		s->used = 0;
		s->addr = NULL;
		s->before = NULL;
		s->after = NULL;
	}
}

void kp_hook_before(void *addr, void *fargs)
{
	struct kp_hook_slot *s = kp_hook_find(addr);

	if (s && s->before)
		((void (*)(void *, void *))s->before)(fargs, s->udata);
}

void kp_hook_after(void *addr, void *fargs)
{
	struct kp_hook_slot *s = kp_hook_find(addr);

	if (s && s->after)
		((void (*)(void *, void *))s->after)(fargs, s->udata);
}
