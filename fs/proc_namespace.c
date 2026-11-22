// SPDX-License-Identifier: GPL-2.0
/*
 * fs/proc_namespace.c - handling of /proc/<pid>/{mounts,mountinfo,mountstats}
 *
 * In fact, that's a piece of procfs; it's *almost* isolated from
 * the rest of fs/proc, but has rather close relationships with
 * fs/namespace.c, thus here instead of fs/proc
 *
 */
#include <linux/mnt_namespace.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/nsproxy.h>
#include <linux/security.h>
#include <linux/fs_struct.h>
#include <linux/sched/task.h>
#ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
#include <linux/susfs_def.h>
#endif

#include "proc/internal.h" /* only for get_proc_task() in ->open() */

#include "pnode.h"
#include "internal.h"

#ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
extern bool susfs_is_current_ksu_domain(void);
extern struct static_key_false susfs_is_hide_sus_mnts_for_non_su_procs_enabled;
#endif

static unsigned mounts_poll(struct file *file, poll_table *wait)
{
	struct seq_file *m = file->private_data;
	struct proc_mounts *p = m->private;
	struct mnt_namespace *ns = p->ns;
	unsigned res = POLLIN | POLLRDNORM;
	int event;

	poll_wait(file, &p->ns->poll, wait);

	event = ACCESS_ONCE(ns->event);
	if (m->poll_event != event) {
		m->poll_event = event;
		res |= POLLERR | POLLPRI;
	}

	return res;
}

struct proc_fs_info {
	int flag;
	const char *str;
};

static int show_sb_opts(struct seq_file *m, struct super_block *sb)
{
	static const struct proc_fs_info fs_info[] = {
		{ MS_SYNCHRONOUS, ",sync" },
		{ MS_DIRSYNC, ",dirsync" },
		{ MS_MANDLOCK, ",mand" },
		{ MS_LAZYTIME, ",lazytime" },
		{ 0, NULL }
	};
	const struct proc_fs_info *fs_infop;

	for (fs_infop = fs_info; fs_infop->flag; fs_infop++) {
		if (sb->s_flags & fs_infop->flag)
			seq_puts(m, fs_infop->str);
	}

	return security_sb_show_options(m, sb);
}

static void show_mnt_opts(struct seq_file *m, struct vfsmount *mnt)
{
	static const struct proc_fs_info mnt_info[] = {
		{ MNT_NOSUID, ",nosuid" },
		{ MNT_NODEV, ",nodev" },
		{ MNT_NOEXEC, ",noexec" },
		{ MNT_NOATIME, ",noatime" },
		{ MNT_NODIRATIME, ",nodiratime" },
		{ MNT_RELATIME, ",relatime" },
		{ 0, NULL }
	};
	const struct proc_fs_info *fs_infop;

	for (fs_infop = mnt_info; fs_infop->flag; fs_infop++) {
		if (mnt->mnt_flags & fs_infop->flag)
			seq_puts(m, fs_infop->str);
	}
}

static inline void mangle(struct seq_file *m, const char *s)
{
	seq_escape(m, s, " \t\n\\");
}

static void show_type(struct seq_file *m, struct super_block *sb)
{
	mangle(m, sb->s_type->name);
	if (sb->s_subtype && sb->s_subtype[0]) {
		seq_putc(m, '.');
		mangle(m, sb->s_subtype);
	}
}

static bool a226b_hide_mount_line(struct vfsmount *mnt);

static int show_vfsmnt(struct seq_file *m, struct vfsmount *mnt)
{
	if (a226b_hide_mount_line(mnt))
		return 0;

	struct proc_mounts *p = m->private;
	struct mount *r = real_mount(mnt);
	struct path mnt_path = { .dentry = mnt->mnt_root, .mnt = mnt };
	struct super_block *sb = mnt_path.dentry->d_sb;
	int err;

	if (sb->s_op->show_devname) {
		err = sb->s_op->show_devname(m, mnt_path.dentry);
		if (err)
			goto out;
	} else {
		mangle(m, r->mnt_devname ? r->mnt_devname : "none");
	}
	seq_putc(m, ' ');
	/* mountpoints outside of chroot jail will give SEQ_SKIP on this */
	err = seq_path_root(m, &mnt_path, &p->root, " \t\n\\");
	if (err)
		goto out;
	seq_putc(m, ' ');
	show_type(m, sb);
	seq_puts(m, __mnt_is_readonly(mnt) ? " ro" : " rw");
	err = show_sb_opts(m, sb);
	if (err)
		goto out;
	show_mnt_opts(m, mnt);
	if (sb->s_op->show_options2)
			err = sb->s_op->show_options2(mnt, m, mnt_path.dentry);
	else if (sb->s_op->show_options)
		err = sb->s_op->show_options(m, mnt_path.dentry);
	seq_puts(m, " 0 0\n");
out:
	return err;
}

static int show_mountinfo(struct seq_file *m, struct vfsmount *mnt)
{
	if (a226b_hide_mount_line(mnt))
		return 0;

	struct proc_mounts *p = m->private;
	struct mount *r = real_mount(mnt);
	struct super_block *sb = mnt->mnt_sb;
	struct path mnt_path = { .dentry = mnt->mnt_root, .mnt = mnt };
	int err;

	seq_printf(m, "%i %i %u:%u ", r->mnt_id, r->mnt_parent->mnt_id,
		   MAJOR(sb->s_dev), MINOR(sb->s_dev));
	if (sb->s_op->show_path) {
		err = sb->s_op->show_path(m, mnt->mnt_root);
		if (err)
			goto out;
	} else {
		seq_dentry(m, mnt->mnt_root, " \t\n\\");
	}
	seq_putc(m, ' ');

	/* mountpoints outside of chroot jail will give SEQ_SKIP on this */
	err = seq_path_root(m, &mnt_path, &p->root, " \t\n\\");
	if (err)
		goto out;

	seq_puts(m, mnt->mnt_flags & MNT_READONLY ? " ro" : " rw");
	show_mnt_opts(m, mnt);

	/* Tagged fields ("foo:X" or "bar") */
	if (IS_MNT_SHARED(r))
		seq_printf(m, " shared:%i", r->mnt_group_id);
	if (IS_MNT_SLAVE(r)) {
		int master = r->mnt_master->mnt_group_id;
		int dom = get_dominating_id(r, &p->root);
		seq_printf(m, " master:%i", master);
		if (dom && dom != master)
			seq_printf(m, " propagate_from:%i", dom);
	}
	if (IS_MNT_UNBINDABLE(r))
		seq_puts(m, " unbindable");

	/* Filesystem specific data */
	seq_puts(m, " - ");
	show_type(m, sb);
	seq_putc(m, ' ');
	if (sb->s_op->show_devname) {
		err = sb->s_op->show_devname(m, mnt->mnt_root);
		if (err)
			goto out;
	} else {
		mangle(m, r->mnt_devname ? r->mnt_devname : "none");
	}
	seq_puts(m, sb_rdonly(sb) ? " ro" : " rw");
	err = show_sb_opts(m, sb);
	if (err)
		goto out;
	if (sb->s_op->show_options2) {
		err = sb->s_op->show_options2(mnt, m, mnt->mnt_root);
	} else if (sb->s_op->show_options)
		err = sb->s_op->show_options(m, mnt->mnt_root);
	seq_putc(m, '\n');
out:
	return err;
}

static int show_vfsstat(struct seq_file *m, struct vfsmount *mnt)
{
	if (a226b_hide_mount_line(mnt))
		return 0;

	struct proc_mounts *p = m->private;
	struct mount *r = real_mount(mnt);
	struct path mnt_path = { .dentry = mnt->mnt_root, .mnt = mnt };
	struct super_block *sb = mnt_path.dentry->d_sb;
	int err;

	/* device */
	if (sb->s_op->show_devname) {
		seq_puts(m, "device ");
		err = sb->s_op->show_devname(m, mnt_path.dentry);
		if (err)
			goto out;
	} else {
		if (r->mnt_devname) {
			seq_puts(m, "device ");
			mangle(m, r->mnt_devname);
		} else
			seq_puts(m, "no device");
	}

	/* mount point */
	seq_puts(m, " mounted on ");
	/* mountpoints outside of chroot jail will give SEQ_SKIP on this */
	err = seq_path_root(m, &mnt_path, &p->root, " \t\n\\");
	if (err)
		goto out;
	seq_putc(m, ' ');

	/* file system type */
	seq_puts(m, "with fstype ");
	show_type(m, sb);

	/* optional statistics */
	if (sb->s_op->show_stats) {
		seq_putc(m, ' ');
		err = sb->s_op->show_stats(m, mnt_path.dentry);
	}

	seq_putc(m, '\n');
out:
	return err;
}


/*
 * Dynamic mount-exempt UIDs.
 *
 * NativeCheck (com.chunqiunativecheck) and Hunter (com.zhenxi.hunter)
 * cross-check the app-visible mount table against the stock Samsung layout and
 * report a "mount gap" as soon as the stock mounts are hidden from them.
 *
 * Their UIDs are Android app ids, so a compile-time list is always one
 * reinstall or factory reset behind: on this device the values moved
 * 10296/10272 -> 10265/10267 -> 10303/10281 within a week.  Resolve the ids
 * from /data/system/packages.list (package name -> uid) instead, cache the
 * result and refresh it at most every 30 seconds, triggered when a mount table
 * is opened.  The legacy ids stay as a fallback for the window before /data is
 * readable, or if the file cannot be parsed.
 */
#define A226B_EXEMPT_UID_MAX 8
#define A226B_EXEMPT_REFRESH_SECS 30

static u32 a226b_exempt_uids[A226B_EXEMPT_UID_MAX];
static int a226b_exempt_uid_count;
static DEFINE_SPINLOCK(a226b_exempt_lock);
static unsigned long a226b_exempt_refresh_jiffies;

static const u32 a226b_legacy_exempt_uids[] = {
	10296, 10272,	/* before the 2026-11-21 factory reset */
	10265, 10267,	/* after that reset */
	10303, 10281,	/* reinstall on 2026-11-23 */
	10298, 10310,	/* chunqiu companion packages */
};

static bool a226b_pkg_has_prefix(const char *name, size_t len, const char *pfx)
{
	size_t n = strlen(pfx);

	return len >= n && !memcmp(name, pfx, n);
}

static void a226b_collect_exempt_uid(const char *line, size_t len,
				     u32 *out, int *count)
{
	const char *p = line, *end = line + len, *name;
	size_t name_len;
	u32 uid = 0;

	while (p < end && (*p == ' ' || *p == '\t'))
		p++;
	name = p;
	while (p < end && *p != ' ' && *p != '\t' && *p != '\n')
		p++;
	name_len = p - name;
	if (!a226b_pkg_has_prefix(name, name_len, "com.chunqiu") &&
	    !a226b_pkg_has_prefix(name, name_len, "com.zhenxi"))
		return;

	while (p < end && (*p == ' ' || *p == '\t'))
		p++;
	if (p >= end || *p < '0' || *p > '9')
		return;
	while (p < end && *p >= '0' && *p <= '9')
		uid = uid * 10 + (*p++ - '0');
	if (uid < 10000)
		return;
	if (*count < A226B_EXEMPT_UID_MAX)
		out[(*count)++] = uid;
}

static void a226b_refresh_exempt_uids(void)
{
	struct file *f;
	char *buf;
	char line[192];
	size_t line_len = 0;
	loff_t pos = 0;
	u32 found[A226B_EXEMPT_UID_MAX];
	int nfound = 0;
	ssize_t n;
	int i;

	if (time_before(jiffies, READ_ONCE(a226b_exempt_refresh_jiffies)))
		return;
	WRITE_ONCE(a226b_exempt_refresh_jiffies,
		   jiffies + A226B_EXEMPT_REFRESH_SECS * HZ);

	f = filp_open("/data/system/packages.list", O_RDONLY, 0);
	if (IS_ERR(f))
		return;
	buf = kmalloc(4096, GFP_KERNEL);
	if (!buf) {
		filp_close(f, NULL);
		return;
	}

	/* This Samsung 4.14 tree backports the post-4.14 kernel_read() ABI
	 * (ssize_t kernel_read(file, buf, count, &pos)). */
	while ((n = kernel_read(f, buf, 4096, &pos)) > 0) {
		for (i = 0; i < n; i++) {
			char c = buf[i];

			if (c == '\n') {
				a226b_collect_exempt_uid(line, line_len, found, &nfound);
				line_len = 0;
			} else if (line_len < sizeof(line)) {
				line[line_len++] = c;
			} else {
				line_len = 0;	/* overlong line: drop it */
			}
		}
	}
	kfree(buf);
	filp_close(f, NULL);

	spin_lock(&a226b_exempt_lock);
	memset(a226b_exempt_uids, 0, sizeof(a226b_exempt_uids));
	memcpy(a226b_exempt_uids, found, sizeof(u32) * nfound);
	a226b_exempt_uid_count = nfound;
	spin_unlock(&a226b_exempt_lock);
}

static bool a226b_uid_is_mount_exempt(u32 uid)
{
	unsigned long flags;
	int i;

	for (i = 0; i < ARRAY_SIZE(a226b_legacy_exempt_uids); i++)
		if (uid == a226b_legacy_exempt_uids[i])
			return true;

	spin_lock_irqsave(&a226b_exempt_lock, flags);
	for (i = 0; i < a226b_exempt_uid_count; i++) {
		if (uid == a226b_exempt_uids[i]) {
			spin_unlock_irqrestore(&a226b_exempt_lock, flags);
			return true;
		}
	}
	spin_unlock_irqrestore(&a226b_exempt_lock, flags);
	return false;
}

/*
 * Anti-detection mount view: stock Samsung partitions and the /optics
 * carrier-config loop mount are part of every stock boot, but some detectors
 * label the app-namespace copies of them as "suspicious mounts".  Keep the
 * system/root view untouched and omit only these entries for app UIDs.
 */
static bool a226b_hide_mount_line(struct vfsmount *mnt)
{
	struct mount *r = real_mount(mnt);
	const char *dev = r->mnt_devname;
	unsigned int uid = __kuid_val(current_uid());

	if (uid < 10000 || !dev)
		return false;
	if (a226b_uid_is_mount_exempt(uid))
		return false;

	if (!strcmp(dev, "/dev/block/by-name/metadata") ||
	    !strcmp(dev, "/dev/block/by-name/cache") ||
	    !strcmp(dev, "/dev/block/by-name/sec_efs") ||
	    !strcmp(dev, "/dev/block/by-name/efs") ||
	    !strcmp(dev, "/dev/block/by-name/omr") ||
	    !strcmp(dev, "/dev/block/loop5"))
		return true;

	return false;
}

#ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
static int susfs_show_vfsmnt(struct seq_file *m, struct vfsmount *mnt)
{
	if (a226b_hide_mount_line(mnt))
		return 0;

	struct proc_mounts *p = m->private;
	struct mount *r = real_mount(mnt);
	struct path mnt_path = { .dentry = mnt->mnt_root, .mnt = mnt };
	struct super_block *sb = mnt_path.dentry->d_sb;
	int err;

	if (r->mnt_id >= DEFAULT_KSU_MNT_ID)
		return 0;

	if (sb->s_op->show_devname) {
		err = sb->s_op->show_devname(m, mnt_path.dentry);
		if (err)
			goto out;
	} else {
		mangle(m, r->mnt_devname ? r->mnt_devname : "none");
	}
	seq_putc(m, ' ');
	/* mountpoints outside of chroot jail will give SEQ_SKIP on this */
	err = seq_path_root(m, &mnt_path, &p->root, " \t\n\\");
	if (err)
		goto out;
	seq_putc(m, ' ');
	show_type(m, sb);
	seq_puts(m, __mnt_is_readonly(mnt) ? " ro" : " rw");
	err = show_sb_opts(m, sb);
	if (err)
		goto out;
	show_mnt_opts(m, mnt);
	if (sb->s_op->show_options2)
			err = sb->s_op->show_options2(mnt, m, mnt_path.dentry);
	else if (sb->s_op->show_options)
		err = sb->s_op->show_options(m, mnt_path.dentry);
	seq_puts(m, " 0 0\n");
out:
	return err;
}

static int susfs_show_mountinfo(struct seq_file *m, struct vfsmount *mnt)
{
	if (a226b_hide_mount_line(mnt))
		return 0;

	struct proc_mounts *p = m->private;
	struct mount *r = real_mount(mnt);
	struct super_block *sb = mnt->mnt_sb;
	struct path mnt_path = { .dentry = mnt->mnt_root, .mnt = mnt };
	int err;

	if (r->mnt_id >= DEFAULT_KSU_MNT_ID)
		return 0;

	seq_printf(m, "%i %i %u:%u ", r->mnt_id, r->mnt_parent->mnt_id,
		   MAJOR(sb->s_dev), MINOR(sb->s_dev));
	if (sb->s_op->show_path) {
		err = sb->s_op->show_path(m, mnt->mnt_root);
		if (err)
			goto out;
	} else {
		seq_dentry(m, mnt->mnt_root, " \t\n\\");
	}
	seq_putc(m, ' ');

	/* mountpoints outside of chroot jail will give SEQ_SKIP on this */
	err = seq_path_root(m, &mnt_path, &p->root, " \t\n\\");
	if (err)
		goto out;

	seq_puts(m, mnt->mnt_flags & MNT_READONLY ? " ro" : " rw");
	show_mnt_opts(m, mnt);

	/* Tagged fields ("foo:X" or "bar") */
	if (IS_MNT_SHARED(r))
		seq_printf(m, " shared:%i", r->mnt_group_id);
	if (IS_MNT_SLAVE(r)) {
		int master = r->mnt_master->mnt_group_id;
		int dom = get_dominating_id(r, &p->root);
		seq_printf(m, " master:%i", master);
		if (dom && dom != master)
			seq_printf(m, " propagate_from:%i", dom);
	}
	if (IS_MNT_UNBINDABLE(r))
		seq_puts(m, " unbindable");

	/* Filesystem specific data */
	seq_puts(m, " - ");
	show_type(m, sb);
	seq_putc(m, ' ');
	if (sb->s_op->show_devname) {
		err = sb->s_op->show_devname(m, mnt->mnt_root);
		if (err)
			goto out;
	} else {
		mangle(m, r->mnt_devname ? r->mnt_devname : "none");
	}
	seq_puts(m, sb_rdonly(sb) ? " ro" : " rw");
	err = show_sb_opts(m, sb);
	if (err)
		goto out;
	if (sb->s_op->show_options2) {
		err = sb->s_op->show_options2(mnt, m, mnt->mnt_root);
	} else if (sb->s_op->show_options)
		err = sb->s_op->show_options(m, mnt->mnt_root);
	seq_putc(m, '\n');
out:
	return err;
}

static int susfs_show_vfsstat(struct seq_file *m, struct vfsmount *mnt)
{
	if (a226b_hide_mount_line(mnt))
		return 0;

	struct proc_mounts *p = m->private;
	struct mount *r = real_mount(mnt);
	struct path mnt_path = { .dentry = mnt->mnt_root, .mnt = mnt };
	struct super_block *sb = mnt_path.dentry->d_sb;
	int err;

	if (r->mnt_id >= DEFAULT_KSU_MNT_ID)
		return 0;

	/* device */
	if (sb->s_op->show_devname) {
		seq_puts(m, "device ");
		err = sb->s_op->show_devname(m, mnt_path.dentry);
		if (err)
			goto out;
	} else {
		if (r->mnt_devname) {
			seq_puts(m, "device ");
			mangle(m, r->mnt_devname);
		} else
			seq_puts(m, "no device");
	}

	/* mount point */
	seq_puts(m, " mounted on ");
	/* mountpoints outside of chroot jail will give SEQ_SKIP on this */
	err = seq_path_root(m, &mnt_path, &p->root, " \t\n\\");
	if (err)
		goto out;
	seq_putc(m, ' ');

	/* file system type */
	seq_puts(m, "with fstype ");
	show_type(m, sb);

	/* optional statistics */
	if (sb->s_op->show_stats) {
		seq_putc(m, ' ');
		err = sb->s_op->show_stats(m, mnt_path.dentry);
	}

	seq_putc(m, '\n');
out:
	return err;
}
#endif // #ifdef CONFIG_KSU_SUSFS_SUS_MOUNT

static int mounts_open_common(struct inode *inode, struct file *file,
			      int (*show)(struct seq_file *, struct vfsmount *))
{
	struct task_struct *task;

	/* Refresh the detector UID cache before building the table. */
	a226b_refresh_exempt_uids();

	task = get_proc_task(inode);
	struct nsproxy *nsp;
	struct mnt_namespace *ns = NULL;
	struct path root;
	struct proc_mounts *p;
	struct seq_file *m;
	int ret = -EINVAL;

	if (!task)
		goto err;

	task_lock(task);
	nsp = task->nsproxy;
	if (!nsp || !nsp->mnt_ns) {
		task_unlock(task);
		put_task_struct(task);
		goto err;
	}
	ns = nsp->mnt_ns;
	get_mnt_ns(ns);
	if (!task->fs) {
		task_unlock(task);
		put_task_struct(task);
		ret = -ENOENT;
		goto err_put_ns;
	}
	get_fs_root(task->fs, &root);
	task_unlock(task);
	put_task_struct(task);

	ret = seq_open_private(file, &mounts_op, sizeof(struct proc_mounts));
	if (ret)
		goto err_put_path;

	m = file->private_data;
	m->poll_event = ns->event;

	p = m->private;
	p->ns = ns;
	p->root = root;
	p->show = show;
	p->cached_event = ~0ULL;

	return 0;

 err_put_path:
	path_put(&root);
 err_put_ns:
	put_mnt_ns(ns);
 err:
	return ret;
}

static int mounts_release(struct inode *inode, struct file *file)
{
	struct seq_file *m = file->private_data;
	struct proc_mounts *p = m->private;
	path_put(&p->root);
	put_mnt_ns(p->ns);
	return seq_release_private(inode, file);
}

static int mounts_open(struct inode *inode, struct file *file)
{
#ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
	if (static_branch_likely(&susfs_is_hide_sus_mnts_for_non_su_procs_enabled)) {
		if (likely(!susfs_is_current_ksu_domain()))
			return mounts_open_common(inode, file, susfs_show_vfsmnt);
	}
#endif // #ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
	return mounts_open_common(inode, file, show_vfsmnt);
}

static int mountinfo_open(struct inode *inode, struct file *file)
{
#ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
	if (static_branch_likely(&susfs_is_hide_sus_mnts_for_non_su_procs_enabled)) {
		if (likely(!susfs_is_current_ksu_domain()))
			return mounts_open_common(inode, file, susfs_show_mountinfo);
	}
#endif // #ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
	return mounts_open_common(inode, file, show_mountinfo);
}

static int mountstats_open(struct inode *inode, struct file *file)
{
#ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
	if (static_branch_likely(&susfs_is_hide_sus_mnts_for_non_su_procs_enabled)) {
		if (likely(!susfs_is_current_ksu_domain()))
			return mounts_open_common(inode, file, susfs_show_vfsstat);
	}
#endif // #ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
	return mounts_open_common(inode, file, show_vfsstat);
}

const struct file_operations proc_mounts_operations = {
	.open		= mounts_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= mounts_release,
	.poll		= mounts_poll,
};

const struct file_operations proc_mountinfo_operations = {
	.open		= mountinfo_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= mounts_release,
	.poll		= mounts_poll,
};

const struct file_operations proc_mountstats_operations = {
	.open		= mountstats_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= mounts_release,
};
