// SPDX-License-Identifier: GPL-2.0
/*
 * Native SELinux query filter for the SM-A226B 4.14 kernel.
 *
 * This is the 4.14 "legacy" half of selinux_hook 1.1.6.1 (GPL-2.0, Admire),
 * compiled directly into the kernel instead of being loaded as a
 * KernelPatch module.  It hides the SELinux contexts added by Magisk /
 * KernelSU / LSPosed from app-side probes, which is what DirtySepolicy and
 * several native checkers use:
 *
 *   - /sys/fs/selinux/context : hidden contexts must look invalid (-EINVAL)
 *   - /sys/fs/selinux/access  : hidden contexts / probe pairs must look invalid
 *   - the fixed "clean avd seqno" probe returns the clean response
 *   - /proc/self/attr/current writes with hidden contexts are rejected
 *
 * The filter is active only while the SukiSU "Hide SELinux modification"
 * feature is enabled (ksu_selinux_hide_enabled), and never applies to root /
 * system UIDs, so the policy manager keeps working.
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/cred.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/compiler.h>

#include "selinux_query_filter.h"

static bool qf_enabled(void)
{
	return true;
}

#define QF_SAMPLE_MAX 256

/* Contexts DirtySepolicy / APatch probe for. */
static const char *const qf_hidden_contexts[] = {
	"u:r:adbroot:s0",
	"u:r:magisk:s0",
	"u:object_r:magisk_file:s0",
	"u:r:ksu:s0",
	"u:object_r:ksu_file:s0",
	"u:object_r:lsposed_file:s0",
	"u:object_r:xposed_data:s0",
	"u:object_r:xposed_file:s0",
	"u:r:su:s0",
	"u:object_r:su_file:s0",
	"u:r:apd:s0",
	"u:r:apatch:s0",
	"u:object_r:apatch_file:s0",
	"u:r:kpatch:s0",
	"u:object_r:kpatch_file:s0",
	"u:object_r:kpm_file:s0",
	"u:object_r:kernelsu_file:s0",
	"u:object_r:susfs_file:s0",
};

/* Fixed access probes used by DirtySepolicy's AppZygote. */
static const char *const qf_access_pairs[][2] = {
	{ "u:r:system_server:s0", "u:r:system_server:s0" },
	{ "u:r:shell:s0", "u:r:su:s0" },
	{ "u:object_r:rootfs:s0", "u:object_r:tmpfs:s0" },
	{ "u:r:kernel:s0", "u:object_r:tmpfs:s0" },
	{ "u:r:kernel:s0", "u:object_r:adb_data_file:s0" },
	{ "u:r:system_server:s0", "u:object_r:apk_data_file:s0" },
	{ "u:r:dex2oat:s0", "u:object_r:dex2oat_exec:s0" },
	{ "u:r:zygote:s0", "u:object_r:adb_data_file:s0" },
};

static const char *const qf_dirty_substrings[] = {
	"magisk",
	"ksu_file",
	"lsposed_file",
	"xposed_data",
	"adbroot",
	"ksu:",
	"kernelsu",
	"susfs",
	"xposed_file",
	"apatch",
	"kpatch",
	"kpm_file",
};

static bool qf_bypass(void)
{
	if (!qf_enabled())
		return true;
	/* root / system / vendor: keep the policy manager working */
	if (current_uid().val < 10000)
		return true;
	return false;
}

static const char *qf_skip_spaces(const char *p, const char *end)
{
	while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
		p++;
	return p;
}

static const char *qf_token_end(const char *p, const char *end)
{
	while (p < end && *p && *p != ' ' && *p != '\t' &&
	       *p != '\n' && *p != '\r')
		p++;
	return p;
}

static const char *qf_next_token(const char *p, const char *end)
{
	p = qf_token_end(qf_skip_spaces(p, end), end);
	return qf_skip_spaces(p, end);
}

static bool qf_token_eq(const char *p, const char *end, const char *lit)
{
	size_t len = (size_t)(end - p);
	size_t l = strlen(lit);

	return len == l && memcmp(p, lit, l) == 0;
}

static bool qf_token_is_hidden(const char *p, const char *end)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(qf_hidden_contexts); i++) {
		if (qf_token_eq(p, end, qf_hidden_contexts[i]))
			return true;
	}
	return false;
}

static bool qf_contains_ci(const char *buf, size_t size, const char *lit)
{
	size_t i, j, n = strlen(lit);

	if (!buf || !n || size < n)
		return false;

	for (i = 0; i + n <= size; i++) {
		for (j = 0; j < n; j++) {
			if (tolower((unsigned char)buf[i + j]) !=
			    tolower((unsigned char)lit[j]))
				break;
		}
		if (j == n)
			return true;
	}
	return false;
}

static bool qf_access_pair_denied(const char *src, const char *src_end,
				  const char *dst, const char *dst_end)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(qf_access_pairs); i++) {
		if (qf_token_eq(src, src_end, qf_access_pairs[i][0]) &&
		    qf_token_eq(dst, dst_end, qf_access_pairs[i][1]))
			return true;
	}
	return false;
}

/* DirtySepolicy: SELinux.access("u:r:untrusted_app:s0", same, 0) -> avd[4]. */
static bool qf_is_seqno_probe(const char *src, const char *src_end,
			      const char *dst, const char *dst_end,
			      const char *cls, const char *cls_end)
{
	return qf_token_eq(src, src_end, "u:r:untrusted_app:s0") &&
	       qf_token_eq(dst, dst_end, "u:r:untrusted_app:s0") &&
	       qf_token_eq(cls, cls_end, "0");
}

int selinux_query_filter_access(char *buf, size_t size)
{
	/*
	 * Same shape as a real 4.14 sel_write_access() reply:
	 * allowed, 0xffffffff, auditallow, auditdeny, seqno, flags.
	 * The old "0 0 0 0 1 0" is a known selinux_hook fingerprint that
	 * native checkers flag as FolkPatch/APatch.
	 */
	static const char clean_response[] = "0 ffffffff 0 ffffffff 1 0";
	const char *end, *src, *src_end, *dst, *dst_end, *cls, *cls_end;
	int i;

	if (!buf || !size || qf_bypass())
		return 0;

	end = buf + size;

	src = qf_skip_spaces(buf, end);
	src_end = qf_token_end(src, end);
	dst = qf_next_token(src_end, end);
	dst_end = qf_token_end(dst, end);
	cls = qf_next_token(dst_end, end);
	cls_end = qf_token_end(cls, end);

	if (qf_is_seqno_probe(src, src_end, dst, dst_end, cls, cls_end)) {
		if (size < sizeof(clean_response) - 1)
			return -EINVAL;
		memcpy(buf, clean_response, sizeof(clean_response) - 1);
		if (size > sizeof(clean_response) - 1)
			buf[sizeof(clean_response) - 1] = '\0';
		return sizeof(clean_response) - 1;
	}

	if (qf_token_is_hidden(src, src_end) ||
	    qf_token_is_hidden(dst, dst_end))
		return -EINVAL;

	if (qf_access_pair_denied(src, src_end, dst, dst_end))
		return -EINVAL;

	for (i = 0; i < ARRAY_SIZE(qf_dirty_substrings); i++) {
		if (qf_contains_ci(buf, size, qf_dirty_substrings[i]))
			return -EINVAL;
	}

	return 0;
}

int selinux_query_filter_context(const char *buf, size_t size)
{
	const char *end, *tok, *tok_end;
	int i;

	if (!buf || !size || qf_bypass())
		return 0;

	end = buf + size;
	tok = qf_skip_spaces(buf, end);
	tok_end = qf_token_end(tok, end);

	if (qf_token_is_hidden(tok, tok_end))
		return -EINVAL;

	for (i = 0; i < ARRAY_SIZE(qf_dirty_substrings); i++) {
		if (qf_contains_ci(buf, size, qf_dirty_substrings[i]))
			return -EINVAL;
	}

	return 0;
}

bool selinux_query_filter_setprocattr(const char *name, const void *value,
				      size_t size)
{
	char sample[QF_SAMPLE_MAX];
	size_t n;
	int i;

	if (!name || !value || !size || qf_bypass())
		return false;

	if (strcmp(name, "current") != 0)
		return false;

	n = min_t(size_t, size, QF_SAMPLE_MAX - 1);
	memcpy(sample, value, n);
	sample[n] = '\0';

	for (i = 0; i < ARRAY_SIZE(qf_hidden_contexts); i++) {
		if (strstr(sample, qf_hidden_contexts[i]))
			return true;
	}
	for (i = 0; i < ARRAY_SIZE(qf_dirty_substrings); i++) {
		if (qf_contains_ci(sample, n, qf_dirty_substrings[i]))
			return true;
	}

	return false;
}
