/*
 *  linux/fs/namei.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 */

/*
 * Some corrections by tytso.
 */

/* [Feb 1997 T. Schoebel-Theuer] Complete rewrite of the pathname
 * lookup logic.
 */
/* [Feb-Apr 2000, AV] Rewrite to the new namespace architecture.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/namei.h>
#include <linux/quotaops.h>
#include <linux/pagemap.h>
#include <linux/fsnotify.h>
#include <linux/personality.h>
#include <linux/security.h>
#include <linux/ima.h>
#include <linux/syscalls.h>
#include <linux/mount.h>
#include <linux/audit.h>
#include <linux/capability.h>
#include <linux/file.h>
#include <linux/fcntl.h>
#include <linux/device_cgroup.h>
#include <linux/fs_struct.h>
#include <asm/uaccess.h>

#define ACC_MODE(x) ("\000\004\002\006"[(x)&O_ACCMODE])

/* [Feb-1997 T. Schoebel-Theuer]
 * Fundamental changes in the pathname lookup mechanisms (namei)
 * were necessary because of omirr.  The reason is that omirr needs
 * to know the _real_ pathname, not the user-supplied one, in case
 * of symlinks (and also when transname replacements occur).
 *
 * The new code replaces the old recursive symlink resolution with
 * an iterative one (in case of non-nested symlink chains).  It does
 * this with calls to <fs>_follow_link().
 * As a side effect, dir_namei(), _namei() and follow_link() are now 
 * replaced with a single function lookup_dentry() that can handle all 
 * the special cases of the former code.
 *
 * With the new dcache, the pathname is stored at each inode, at least as
 * long as the refcount of the inode is positive.  As a side effect, the
 * size of the dcache depends on the inode cache and thus is dynamic.
 *
 * [29-Apr-1998 C. Scott Ananian] Updated above description of symlink
 * resolution to correspond with current state of the code.
 *
 * Note that the symlink resolution is not *completely* iterative.
 * There is still a significant amount of tail- and mid- recursion in
 * the algorithm.  Also, note that <fs>_readlink() is not used in
 * lookup_dentry(): lookup_dentry() on the result of <fs>_readlink()
 * may return different results than <fs>_follow_link().  Many virtual
 * filesystems (including /proc) exhibit this behavior.
 */

/* [24-Feb-97 T. Schoebel-Theuer] Side effects caused by new implementation:
 * New symlink semantics: when open() is called with flags O_CREAT | O_EXCL
 * and the name already exists in form of a symlink, try to create the new
 * name indicated by the symlink. The old code always complained that the
 * name already exists, due to not following the symlink even if its target
 * is nonexistent.  The new semantics affects also mknod() and link() when
 * the name is a symlink pointing to a non-existant name.
 *
 * I don't know which semantics is the right one, since I have no access
 * to standards. But I found by trial that HP-UX 9.0 has the full "new"
 * semantics implemented, while SunOS 4.1.1 and Solaris (SunOS 5.4) have the
 * "old" one. Personally, I think the new semantics is much more logical.
 * Note that "ln old new" where "new" is a symlink pointing to a non-existing
 * file does succeed in both HP-UX and SunOs, but not in Solaris
 * and in the old Linux semantics.
 */

/* [16-Dec-97 Kevin Buhr] For security reasons, we change some symlink
 * semantics.  See the comments in "open_namei" and "do_link" below.
 *
 * [10-Sep-98 Alan Modra] Another symlink change.
 */

/* [Feb-Apr 2000 AV] Complete rewrite. Rules for symlinks:
 *	inside the path - always follow.
 *	in the last component in creation/removal/renaming - never follow.
 *	if LOOKUP_FOLLOW passed - follow.
 *	if the pathname has trailing slashes - follow.
 *	otherwise - don't follow.
 * (applied in that order).
 *
 * [Jun 2000 AV] Inconsistent behaviour of open() in case if flags==O_CREAT
 * restored for 2.4. This is the last surviving part of old 4.2BSD bug.
 * During the 2.4 we need to fix the userland stuff depending on it -
 * hopefully we will be able to get rid of that wart in 2.5. So far only
 * XEmacs seems to be relying on it...
 */
/*
 * [Sep 2001 AV] Single-semaphore locking scheme (kudos to David Holland)
 * implemented.  Let's see if raised priority of ->s_vfs_rename_mutex gives
 * any extra contention...
 */

static int __link_path_walk(const char *name, struct nameidata *nd);

/* In order to reduce some races, while at the same time doing additional
 * checking and hopefully speeding things up, we copy filenames to the
 * kernel data space before using them..
 *
 * POSIX.1 2.4: an empty pathname is invalid (ENOENT).
 * PATH_MAX includes the nul terminator --RR.
 */
static int do_getname(const char __user *filename, char *page)
{
	int retval;
	unsigned long len = PATH_MAX;

	if (!segment_eq(get_fs(), KERNEL_DS)) {
		if ((unsigned long) filename >= TASK_SIZE)
			return -EFAULT;
		if (TASK_SIZE - (unsigned long) filename < PATH_MAX)
			len = TASK_SIZE - (unsigned long) filename;
	}

	retval = strncpy_from_user(page, filename, len);
	if (retval > 0) {
		if (retval < len)
			return 0;
		return -ENAMETOOLONG;
	} else if (!retval)
		retval = -ENOENT;
	return retval;
}

/**
 * 通过__getname()分配一个大小为PATH_MAX(4096B)的SLAB变量，最终调用函数
 * strncpy_from_user()将用户空间的名字信息(此处为挂载目录名)
 */
char * getname(const char __user * filename)
{
	char *tmp, *result;

	result = ERR_PTR(-ENOMEM);
	tmp = __getname();
	if (tmp)  {
		int retval = do_getname(filename, tmp);

		result = tmp;
		if (retval < 0) {
			__putname(tmp);
			result = ERR_PTR(retval);
		}
	}
	audit_getname(result);
	return result;
}

#ifdef CONFIG_AUDITSYSCALL
void putname(const char *name)
{
	if (unlikely(!audit_dummy_context()))
		audit_putname(name);
	else
		__putname(name);
}
EXPORT_SYMBOL(putname);
#endif

/*
 * This does basic POSIX ACL permission checking
 */
static int acl_permission_check(struct inode *inode, int mask,
		int (*check_acl)(struct inode *inode, int mask))
{
	umode_t			mode = inode->i_mode;

	mask &= MAY_READ | MAY_WRITE | MAY_EXEC;

	if (current_fsuid() == inode->i_uid)
		mode >>= 6;
	else {
		if (IS_POSIXACL(inode) && (mode & S_IRWXG) && check_acl) {
			int error = check_acl(inode, mask);
			if (error != -EAGAIN)
				return error;
		}

		if (in_group_p(inode->i_gid))
			mode >>= 3;
	}

	/*
	 * If the DACs are ok we don't need any capability check.
	 */
	if ((mask & ~mode) == 0)
		return 0;
	return -EACCES;
}

/**
 * generic_permission  -  check for access rights on a Posix-like filesystem
 * @inode:	inode to check access rights for
 * @mask:	right to check for (%MAY_READ, %MAY_WRITE, %MAY_EXEC)
 * @check_acl:	optional callback to check for Posix ACLs
 *
 * Used to check for read/write/execute permissions on a file.
 * We use "fsuid" for this, letting us set arbitrary permissions
 * for filesystem access without changing the "normal" uids which
 * are used for other things..
 */
int generic_permission(struct inode *inode, int mask,
		int (*check_acl)(struct inode *inode, int mask))
{
	int ret;

	/*
	 * Do the basic POSIX ACL permission checks.
	 */
	ret = acl_permission_check(inode, mask, check_acl);
	if (ret != -EACCES)
		return ret;

	/*
	 * Read/write DACs are always overridable.
	 * Executable DACs are overridable if at least one exec bit is set.
	 */
	if (!(mask & MAY_EXEC) || execute_ok(inode))
		if (capable(CAP_DAC_OVERRIDE))
			return 0;

	/*
	 * Searching includes executable on directories, else just read.
	 */
	if (mask == MAY_READ || (S_ISDIR(inode->i_mode) && !(mask & MAY_WRITE)))
		if (capable(CAP_DAC_READ_SEARCH))
			return 0;

	return -EACCES;
}

/**
 * inode_permission  -  check for access rights to a given inode
 * @inode:	inode to check permission on
 * @mask:	right to check for (%MAY_READ, %MAY_WRITE, %MAY_EXEC)
 *
 * Used to check for read/write/execute permissions on an inode.
 * We use "fsuid" for this, letting us set arbitrary permissions
 * for filesystem access without changing the "normal" uids which
 * are used for other things.
 */
int inode_permission(struct inode *inode, int mask)
{
	int retval;

	if (mask & MAY_WRITE) {
		umode_t mode = inode->i_mode;

		/*
		 * Nobody gets write access to a read-only fs.
		 */
		if (IS_RDONLY(inode) &&
		    (S_ISREG(mode) || S_ISDIR(mode) || S_ISLNK(mode)))
			return -EROFS;

		/*
		 * Nobody gets write access to an immutable file.
		 */
		if (IS_IMMUTABLE(inode))
			return -EACCES;
	}

	if (inode->i_op->permission)
		retval = inode->i_op->permission(inode, mask);
	else
		retval = generic_permission(inode, mask, inode->i_op->check_acl);

	if (retval)
		return retval;

	retval = devcgroup_inode_permission(inode, mask);
	if (retval)
		return retval;

	return security_inode_permission(inode,
			mask & (MAY_READ|MAY_WRITE|MAY_EXEC|MAY_APPEND));
}

/**
 * file_permission  -  check for additional access rights to a given file
 * @file:	file to check access rights for
 * @mask:	right to check for (%MAY_READ, %MAY_WRITE, %MAY_EXEC)
 *
 * Used to check for read/write/execute permissions on an already opened
 * file.
 *
 * Note:
 *	Do not use this function in new code.  All access checks should
 *	be done using inode_permission().
 */
int file_permission(struct file *file, int mask)
{
	return inode_permission(file->f_path.dentry->d_inode, mask);
}

/*
 * get_write_access() gets write permission for a file.
 * put_write_access() releases this write permission.
 * This is used for regular files.
 * We cannot support write (and maybe mmap read-write shared) accesses and
 * MAP_DENYWRITE mmappings simultaneously. The i_writecount field of an inode
 * can have the following values:
 * 0: no writers, no VM_DENYWRITE mappings
 * < 0: (-i_writecount) vm_area_structs with VM_DENYWRITE set exist
 * > 0: (i_writecount) users are writing to the file.
 *
 * Normally we operate on that counter with atomic_{inc,dec} and it's safe
 * except for the cases where we don't hold i_writecount yet. Then we need to
 * use {get,deny}_write_access() - these functions check the sign and refuse
 * to do the change if sign is wrong. Exclusion between them is provided by
 * the inode->i_lock spinlock.
 */

int get_write_access(struct inode * inode)
{
	spin_lock(&inode->i_lock);
	if (atomic_read(&inode->i_writecount) < 0) {
		spin_unlock(&inode->i_lock);
		return -ETXTBSY;
	}
	atomic_inc(&inode->i_writecount);
	spin_unlock(&inode->i_lock);

	return 0;
}

int deny_write_access(struct file * file)
{
	struct inode *inode = file->f_path.dentry->d_inode;

	spin_lock(&inode->i_lock);
	/**
	 * deny_write_access通过检查索引结点的i_writecount字段，
	 * 确定可执行文件没有被写入。
	 */
	if (atomic_read(&inode->i_writecount) > 0) {
		spin_unlock(&inode->i_lock);
		return -ETXTBSY;
	}
	/**
	 * 并把-1存放在这个字段以禁止进一步的写访问
	 */
	atomic_dec(&inode->i_writecount);
	spin_unlock(&inode->i_lock);

	return 0;
}

/**
 * path_get - get a reference to a path
 * @path: path to get the reference to
 *
 * Given a path increment the reference count to the dentry and the vfsmount.
 */
void path_get(struct path *path)
{
	mntget(path->mnt);
	dget(path->dentry);
}
EXPORT_SYMBOL(path_get);

/**
 * path_put - put a reference to a path
 * @path: path to put the reference to
 *
 * Given a path decrement the reference count to the dentry and the vfsmount.
 */
void path_put(struct path *path)
{
	dput(path->dentry);
	mntput(path->mnt);
}
EXPORT_SYMBOL(path_put);

/**
 * release_open_intent - free up open intent resources
 * @nd: pointer to nameidata
 */
void release_open_intent(struct nameidata *nd)
{
	if (nd->intent.open.file->f_path.dentry == NULL)
		put_filp(nd->intent.open.file);
	else
		fput(nd->intent.open.file);
}

static inline struct dentry *
do_revalidate(struct dentry *dentry, struct nameidata *nd)
{
	int status = dentry->d_op->d_revalidate(dentry, nd);
	if (unlikely(status <= 0)) {
		/*
		 * The dentry failed validation.
		 * If d_revalidate returned 0 attempt to invalidate
		 * the dentry otherwise d_revalidate is asking us
		 * to return a fail status.
		 */
		if (!status) {
			if (!d_invalidate(dentry)) {
				dput(dentry);
				dentry = NULL;
			}
		} else {
			dput(dentry);
			dentry = ERR_PTR(status);
		}
	}
	return dentry;
}

/*
 * Internal lookup() using the new generic dcache.
 * SMP-safe
 */
static struct dentry * cached_lookup(struct dentry * parent, struct qstr * name, struct nameidata *nd)
{
	struct dentry * dentry = __d_lookup(parent, name);

	/* lockess __d_lookup may fail due to concurrent d_move() 
	 * in some unrelated directory, so try with d_lookup
	 */
	if (!dentry)
		dentry = d_lookup(parent, name);

	if (dentry && dentry->d_op && dentry->d_op->d_revalidate)
		dentry = do_revalidate(dentry, nd);

	return dentry;
}

/*
 * Short-cut version of permission(), for calling by
 * path_walk(), when dcache lock is held.  Combines parts
 * of permission() and generic_permission(), and tests ONLY for
 * MAY_EXEC permission.
 *
 * If appropriate, check DAC only.  If not appropriate, or
 * short-cut DAC fails, then call permission() to do more
 * complete permission check.
 */
/**
 * 检查存放在索引结点i_node字段的访问模式和运行进程的特权。
 */
static int exec_permission_lite(struct inode *inode)
{
	int ret;

	if (inode->i_op->permission) {
		ret = inode->i_op->permission(inode, MAY_EXEC);
		if (!ret)
			goto ok;
		return ret;
	}
	ret = acl_permission_check(inode, MAY_EXEC, inode->i_op->check_acl);
	if (!ret)
		goto ok;

	if (capable(CAP_DAC_OVERRIDE) || capable(CAP_DAC_READ_SEARCH))
		goto ok;

	return ret;
ok:
	return security_inode_permission(inode, MAY_EXEC);
}

/*
 * This is called when everything else fails, and we actually have
 * to go to the low-level filesystem to find out what we should do..
 *
 * We get the directory semaphore, and after getting that we also
 * make sure that nobody added the entry to the dcache in the meantime..
 * SMP-safe
 */
/**
 * 从磁盘中读取目录项，并搜索特定名称的目录项
 */
static struct dentry * real_lookup(struct dentry * parent, struct qstr * name, struct nameidata *nd)
{
	struct dentry * result;
	struct inode *dir = parent->d_inode;

	mutex_lock(&dir->i_mutex);
	/*
	 * First re-do the cached lookup just in case it was created
	 * while we waited for the directory semaphore..
	 *
	 * FIXME! This could use version numbering or similar to
	 * avoid unnecessary cache lookups.
	 *
	 * The "dcache_lock" is purely to protect the RCU list walker
	 * from concurrent renames at this point (we mustn't get false
	 * negatives from the RCU list walk here, unlike the optimistic
	 * fast walk).
	 *
	 * so doing d_lookup() (with seqlock), instead of lockfree __d_lookup
	 */
	/* 在获取锁的过程中，目录项可能已经被读到缓存中，再次从目录项缓存中搜索 */
	result = d_lookup(parent, name);
	if (!result) {/* 目录项缓存中仍然不存在 */
		struct dentry *dentry;

		/* Don't create child dentry for a dead directory. */
		result = ERR_PTR(-ENOENT);
		if (IS_DEADDIR(dir))
			goto out_unlock;

		/* 分配一个dentry并进行相应的初始化 */
		dentry = d_alloc(parent, name);
		result = ERR_PTR(-ENOMEM);
		if (dentry) {
			/* 从磁盘中读取目录项缓存 */
			/* 调用特定于文件系统的lookup函数从磁盘中读取数据并将dentry添入散列表 */
			result = dir->i_op->lookup(dir, dentry, nd);
			if (result)
				dput(dentry);
			else
				result = dentry;
		}
out_unlock:
		mutex_unlock(&dir->i_mutex);
		return result;
	}

	/*
	 * Uhhuh! Nasty case: the cache was re-populated while
	 * we waited on the semaphore. Need to revalidate.
	 */
	mutex_unlock(&dir->i_mutex);
	/* 文件系统要求对目录进行校验 */
	if (result->d_op && result->d_op->d_revalidate) {
	/* 如果校验不成功，说明目录项已经失效，返回失败 */
		result = do_revalidate(result, nd);
		if (!result)
			result = ERR_PTR(-ENOENT);
	}
	return result;
}

/*
 * Wrapper to retry pathname resolution whenever the underlying
 * file system returns an ESTALE.
 *
 * Retry the whole path once, forcing real lookup requests
 * instead of relying on the dcache.
 */
static __always_inline int link_path_walk(const char *name, struct nameidata *nd)
{
	/* 先把路径保存起来 */
	struct path save = nd->path;
	int result;

	/* make sure the stuff we saved doesn't go away */
	path_get(&save);

	result = __link_path_walk(name, nd);
	if (result == -ESTALE) {
		/* nd->path had been dropped */
		nd->path = save;
		path_get(&nd->path);
		nd->flags |= LOOKUP_REVAL;
		result = __link_path_walk(name, nd);
	}

	path_put(&save);

	return result;
}

/**
 * 设置nd的root为当前进程的根目录
 */
static __always_inline void set_root(struct nameidata *nd)
{
	if (!nd->root.mnt) {
		struct fs_struct *fs = current->fs;
		read_lock(&fs->lock);
		nd->root = fs->root;
		path_get(&nd->root);
		read_unlock(&fs->lock);
	}
}

static __always_inline int __vfs_follow_link(struct nameidata *nd, const char *link)
{
	int res = 0;
	char *name;
	if (IS_ERR(link))
		goto fail;

	/**
	 * 检查符号链接是否以'/'开头
	 */
	if (*link == '/') {
		set_root(nd);
		path_put(&nd->path);
		nd->path = nd->root;
		path_get(&nd->root);
	}
	/**
	 * link_path_walk解析符号链的路径名,并返回res
	 */
	res = link_path_walk(link, nd);
	if (nd->depth || res || nd->last_type!=LAST_NORM)
		return res;
	/*
	 * If it is an iterative symlinks resolution in open_namei() we
	 * have to copy the last component. And all that crap because of
	 * bloody create() on broken symlinks. Furrfu...
	 */
	name = __getname();
	if (unlikely(!name)) {
		path_put(&nd->path);
		return -ENOMEM;
	}
	strcpy(name, nd->last.name);
	nd->last.name = name;
	return 0;
fail:
	path_put(&nd->path);
	return PTR_ERR(link);
}

static void path_put_conditional(struct path *path, struct nameidata *nd)
{
	dput(path->dentry);
	if (path->mnt != nd->path.mnt)
		mntput(path->mnt);
}

static inline void path_to_nameidata(struct path *path, struct nameidata *nd)
{
	dput(nd->path.dentry);
	if (nd->path.mnt != path->mnt)
		mntput(nd->path.mnt);
	nd->path.mnt = path->mnt;
	nd->path.dentry = path->dentry;
}

static __always_inline int __do_follow_link(struct path *path, struct nameidata *nd)
{
	int error;
	void *cookie;
	struct dentry *dentry = path->dentry;

	/**
	 * 更新索引结点的访问时间。
	 */
	touch_atime(path->mnt, dentry);
	nd_set_link(nd, NULL);

	if (path->mnt != nd->path.mnt) {
		path_to_nameidata(path, nd);
		dget(dentry);
	}
	mntget(path->mnt);
	/**
	 * 调用文件系统相关的函数实现的follow_link方法。
	 * 它读取存放在符号链接索引结点中的路径名，并把这个路径名保存在nd->save_names数组的项中
	 */
	cookie = dentry->d_inode->i_op->follow_link(dentry, nd);
	error = PTR_ERR(cookie);
	if (!IS_ERR(cookie)) {
		char *s = nd_get_link(nd);
		error = 0;
		if (s)
		 	/**
		 	 * 如果符号链接的第一个字符是"/"，则在内存中不用保存前一个路径的信息。并将结果指向当前进程的根目录。
		 	 * 否则调用link_path_walk解析路径名。
		 	 */
			error = __vfs_follow_link(nd, s);
		/**
		 * 如果有put_link方法，就执行它，释放由follow_link方法分配的临时数据结构。
		 */
		if (dentry->d_inode->i_op->put_link)
			dentry->d_inode->i_op->put_link(dentry, nd, cookie);
	}
	path_put(path);

	return error;
}

/*
 * This limits recursive symlink follows to 8, while
 * limiting consecutive symlinks to 40.
 *
 * Without that kind of total limit, nasty chains of consecutive
 * symlinks can cause almost arbitrarily long lookups. 
 */
/**
 * 被link_path_walk调用。link_path_walk用于查找符号链接
 * dentry是符号链接的地址。nd是nameidata结构的址址。
 */
static inline int do_follow_link(struct path *path, struct nameidata *nd)
{
	int err = -ELOOP;
	/**
	 * 由于符号链接可能嵌套，如果不判断一下，可能在此形成递归调用。死循环。
	 */
    /*如果处理的链接数超过下面的限制则会放弃搜索(因为有可能陷入死循环)*/  
    /*link_count表示连续的符号链接数，其值不能超过8，total_link_count表示总共的 
     链接数，其值不能超过40*/  
	if (current->link_count >= MAX_NESTED_LINKS)
		goto loop;
	if (current->total_link_count >= 40)
		goto loop;
	BUG_ON(nd->depth >= MAX_NESTED_LINKS);
	/**
	 * 判断一下抢占
	 */
	cond_resched();
	err = security_inode_follow_link(path->dentry, nd);
	if (err)
		goto loop;
	/**
	 * 计数，请参见前面的注释。
	 */
	/*每处理一次符号链接，link_count,total_link_count和depth都要加1*/
	current->link_count++;
	current->total_link_count++;
	nd->depth++;
	/**
	 * __do_follow_link是一个递归调用，
	 */
	err = __do_follow_link(path, nd);
    /*每执行完一次符号链接，link_count和depth都要减1*/
	current->link_count--;
	/**
	 * 本次查找的递归次数。
	 */
	nd->depth--;
	return err;
loop:
	path_put_conditional(path, nd);
	path_put(&nd->path);
	return err;
}

int follow_up(struct path *path)
{
	struct vfsmount *parent;
	struct dentry *mountpoint;
	spin_lock(&vfsmount_lock);
	parent = path->mnt->mnt_parent;
	if (parent == path->mnt) {
		spin_unlock(&vfsmount_lock);
		return 0;
	}
	mntget(parent);
	mountpoint = dget(path->mnt->mnt_mountpoint);
	spin_unlock(&vfsmount_lock);
	dput(path->dentry);
	path->dentry = mountpoint;
	mntput(path->mnt);
	path->mnt = parent;
	return 1;
}

/* no need for dcache_lock, as serialization is taken care in
 * namespace.c
 */
/* 调用__follow_mount跟踪到最后一次挂载的目录点, 如果跟换path返回1 */
static int __follow_mount(struct path *path)
{
	int res = 0;
	/* 直到当前路径不再是一个挂载点后再退出 */
	while (d_mountpoint(path->dentry)) {
	    /* 在哈希表中查找当前路径的文件系统装载实例 */
		struct vfsmount *mounted = lookup_mnt(path);
		/* 未加载文件系统，退出 */
		if (!mounted)
			break;
		/* 将文件系统的根目录作为当前路径，继续查找 */
		dput(path->dentry);
		if (res)
			mntput(path->mnt);
		path->mnt = mounted;
		path->dentry = dget(mounted->mnt_root);
		res = 1;
	}
	return res;
}

/**
 * 调用follow_mount跟踪到最后一次挂载的目录点
 */
static void follow_mount(struct path *path)
{ 
	/* 通过逐级的循环来找到最近的那个被挂载的文件系统 */
	while (d_mountpoint(path->dentry)) { 
		/* 找到path下挂载的子文件系统 */
		struct vfsmount *mounted = lookup_mnt(path);
		/* 如果mounted为0，也就表示之前处理的子文件系统是最后一个子文件系统了 */
		if (!mounted)
			break;
		dput(path->dentry);
		mntput(path->mnt);
		/* 保存vfsmount  */
		path->mnt = mounted;
		/* 保存dentry，并更新引用计数 */
		path->dentry = dget(mounted->mnt_root);
	}
}

/* no need for dcache_lock, as serialization is taken care in
 * namespace.c
 */
/**
 * 如果path已经被挂载，更新path
 */
int follow_down(struct path *path)
{
	struct vfsmount *mounted;

	mounted = lookup_mnt(path);
	if (mounted) {
		dput(path->dentry);
		mntput(path->mnt);
		path->mnt = mounted;
		path->dentry = dget(mounted->mnt_root);
		return 1;
	}
	return 0;
}

/**
 * 把nd设置为前一级目录,   回退父目录
 */
static __always_inline void follow_dotdot(struct nameidata *nd)
{
	set_root(nd);

	while(1) {
		struct vfsmount *parent;
		struct dentry *old = nd->path.dentry;

        /*如果当前所处的目录即为根目录则break*/
        if (nd->path.dentry == nd->root.dentry &&
		    nd->path.mnt == nd->root.mnt) {
			break;
		}
		spin_lock(&dcache_lock);
		/* 如果当前所处的目录不为当前路径所属文件系统的根目录，也就是说可以直接向上退一级 */
		if (nd->path.dentry != nd->path.mnt->mnt_root) {
			nd->path.dentry = dget(nd->path.dentry->d_parent);
			spin_unlock(&dcache_lock);
			dput(old);
			break;
		}
		spin_unlock(&dcache_lock);
		/* 运行到这里，说明到达所在文件系统的根目录 */
		spin_lock(&vfsmount_lock);
		/* 取父文件系统 */
		parent = nd->path.mnt->mnt_parent;
		if (parent == nd->path.mnt) {
			spin_unlock(&vfsmount_lock);
			break;
		}
		mntget(parent);
		/* 目录项变为当前文件系统的挂载点 ,切换完文件系统，继续下一轮查找*/
		nd->path.dentry = dget(nd->path.mnt->mnt_mountpoint);
		spin_unlock(&vfsmount_lock);
		dput(old);
		mntput(nd->path.mnt);
		nd->path.mnt = parent;
	}
	/* 最后找到的目录可能是一个挂载点，调用follow_mount跟踪到最后一次挂载的目录点 */
	follow_mount(&nd->path);
}

/*
 *  It's more convoluted than I'd like it to be, but... it's still fairly
 *  small and for now I'd prefer to have fast path as straight as possible.
 *  It _is_ time-critical.
 */
/**
 * 得到与给定的父目录和文件名相关的目录项对象.
 */
static int do_lookup(struct nameidata *nd, struct qstr *name,
		     struct path *path)
{
	struct vfsmount *mnt = nd->path.mnt;
    /**
     * __d_lookup在目录项高速缓存中搜索分量的目录项对象.
     */
	struct dentry *dentry = __d_lookup(nd->path.dentry, name);

	/* 在目录项缓存中没有找到，则需要从磁盘上读取目录项后查找 */
	if (!dentry)
		goto need_lookup;
	/* 在目录项缓存中搜索到目录项，但是文件系统要求对缓存中的目录项进行校验 */
	/* 对于网络文件系统，还要通过文件系统中定义的d_revalidate()函数来判断该dentry是否有效以保证一致性 */
	if (dentry->d_op && dentry->d_op->d_revalidate)
		goto need_revalidate;
done:
	path->mnt = mnt;
	path->dentry = dentry;
    /*这里由于path往下走了一层，因此要调用__follow_mount()判断dentry对应的目录下是否挂载了其
    他的文件系统，以保证对应的mnt是正确的*/
	__follow_mount(path);
	return 0;

need_lookup:
	/**
	 * 没有在目录项高速缓存中找到目录项对象,就调用real_lookup
	 * real_lookup执行索引节点的lookup方法从磁盘读取目录,创建一个新的目录项对象并把它插入到目录项高速缓存中.
	 * 然后创建一个新的索引节点,并将它插入到索引节点高速缓存中.
	 */
	dentry = real_lookup(nd->path.dentry, name, nd);
	if (IS_ERR(dentry))
		goto fail;
	goto done;

need_revalidate:
	dentry = do_revalidate(dentry, nd);
	if (!dentry)
		goto need_lookup;
	if (IS_ERR(dentry))
		goto fail;
	goto done;

fail:
	return PTR_ERR(dentry);
}

/*
 * Name resolution.
 * This is the basic name resolution function, turning a pathname into
 * the final dentry. We expect 'base' to be positive and a directory.
 *
 * Returns 0 and nd will have valid dentry and mnt on success.
 * Returns error and drops reference to input namei data on failure.
 */
/**
 * 真正的路径查找实现。
 */
static int __link_path_walk(const char *name, struct nameidata *nd)
{
	struct path next;
	struct inode *inode;
	int err;
	/* 查找标志 */
	unsigned int lookup_flags = nd->flags;
	
	/* 多个/仅代表一个/ */
	while (*name=='/')
		name++;
	/* 查找根目录或者空目录，结果已经保存在nd中了，直接返回 */
	if (!*name)
		goto return_reval;

    /* 每次在inode所在的目录下进行查找 */
	inode = nd->path.dentry->d_inode;
    /**
     * 符号链接查找。
     */
	if (nd->depth)
		lookup_flags = LOOKUP_FOLLOW | (nd->flags & LOOKUP_CONTINUE);

	/* At this point we know we have a real path component. */
	/**
	 * 循环处理每一个路径分量。
	 */
	for(;;) {
		unsigned long hash;
		struct qstr this;
		unsigned int c;

		nd->flags |= LOOKUP_CONTINUE;
		/**
		 * 只有目录有可执行权限，才能被检索。
		 */
		err = exec_permission_lite(inode);
		/* 权限错误 */
 		if (err)
			break;

		/* 当前进行解析的文件名分量 */
		this.name = name;
		c = *(const unsigned char *)name;

		/**
		 * 计算散列值
		 */
		
		/* hash值初始化为0  */
		hash = init_name_hash();
        /*计算当前文件名分量的hash值*/ 
		do {
			name++;
			hash = partial_name_hash(c, hash);
			c = *(const unsigned char *)name;
		} while (c && (c != '/'));
		/* 保存文件名分量的长度 */
		this.len = name - (const char *) this.name;
		/* 保存当前文件名分量的hash值 */
		this.hash = end_name_hash(hash);

		/* remove trailing slashes? */
		/**
		 * 最后一个分量，并且没有以"/"结束，是一个文件。
		 */
		if (!c)
			goto last_component;
		/**
		 * 如果分量以"/"结束，退出。
		 */
		while (*++name == '/');
		if (!*name)
			goto last_with_slashes;

		/*
		 * "." and ".." are special - ".." especially so because it has
		 * to be able to know about the current root directory and
		 * parent relationships.
		 */
		if (this.name[0] == '.') switch (this.len) {/* 分量是一个"." */
		    /*文件名长度不为1和2，说明是以点开头的文件名  */
			default:
				break;
			/* 文件名长度为2并且下一个字符也为'.'则表明要退回到上级目录  */
			case 2:	
				if (this.name[1] != '.')
					break;
				follow_dotdot(nd);
				/* dentry就是上一层目录，转回上级目录并继续。 */
				/* 更新inode */
				inode = nd->path.dentry->d_inode;
				/* fallthrough */
			    /* 注意这里没有break，因此将通过后面的continue直接回到循环开头 */

            /* 当前目录，继续处理下一个分量。 */
			case 1:
				continue;
		}
		/*
		 * See if the low-level filesystem might want
		 * to use its own hash..
		 */
		/**
		 * 如果文件系统允许目录缓存，并且有自定义的散列函数，则计算它的散列值。
		 */
		if (nd->path.dentry->d_op && nd->path.dentry->d_op->d_hash) {
			err = nd->path.dentry->d_op->d_hash(nd->path.dentry,
							    &this);
			if (err < 0)
				break;
		}
		/* This does the actual lookups.. */
		/**
		 * 在当前目录中搜索下一个分量。
		 */
		/* do_lookup执行实际的查找，注意nd中的path对应的是父目录，而this是对应的当前解析的路径分量 */
		err = do_lookup(nd, &this, &next);
		if (err)
			break;

		err = -ENOENT;
		inode = next.dentry->d_inode;
		if (!inode)
			goto out_dput;

		/* 如果刚刚解析的路径为符号链接，则通过do_follow_link进行处理 */
		if (inode->i_op->follow_link) {
			err = do_follow_link(&next, nd);
			if (err)
				goto return_err;
			err = -ENOENT;
			inode = nd->path.dentry->d_inode;
			if (!inode)
				break;
		} else
			/* 不为符号链接，则路径分量解析完毕，根据next更新nd中的path准备解析下一个分量 */
			path_to_nameidata(&next, nd);
		err = -ENOTDIR; 
		/**
		 * 刚解析的分量是否指向一个目录。如果没有，则返回错误。因为中间目录分量必须是一个目录。
		 */
		/* 如果刚刚解析的路径分量对应的inode没有定义lookup函数， 则无法以此为父目录进行解析了*/
		if (!inode->i_op->lookup)
			break;
		/* 继续查找 */
		continue;
		/* here ends the main loop */

last_with_slashes:
		/**
		 * 文件名最后一个分量是"/"，设置LOOKUP_FOLLOW和LOOKUP_DIRECTORY标志。后面的函数会强制将最后一个分量作为目录。
		 */
		lookup_flags |= LOOKUP_FOLLOW | LOOKUP_DIRECTORY;
last_component:
		/* Clear LOOKUP_CONTINUE iff it was previously unset */
		/**
		 * 最后一个分量。清除LOOKUP_CONTINUE
		 */
		nd->flags &= lookup_flags | ~LOOKUP_CONTINUE;
		/* 查找父路径名 如果最终要查找的是最后一个路径分量的父目录 */
		if (lookup_flags & LOOKUP_PARENT)
			goto lookup_parent;
		if (this.name[0] == '.') switch (this.len) {
			default:
				break;
			case 2:
			    /* 不是".."，应当是正常的文件名 */
				if (this.name[1] != '.')
					break;
				/* ".."，尝试回到父目录,如果当前是文件系统根目录并且根目录没有绑到其他目录，则返回根目录，否则向上移动 */
				follow_dotdot(nd);
				inode = nd->path.dentry->d_inode;
				/* fallthrough */
			/* ".."，尝试回到父目录,如果当前是文件系统根目录并且根目录没有绑到其他目录，则返回根目录，否则向上移动 */
			case 1:
				goto return_reval;
		}
		/**
		 * 最后一个分量是文件或目录名，先试图有文件系统的哈希方法重新计算哈希值。
		 */
		if (nd->path.dentry->d_op && nd->path.dentry->d_op->d_hash) {
			err = nd->path.dentry->d_op->d_hash(nd->path.dentry,
							    &this);
			if (err < 0)
				break;
		}
		/**
		 * 在目录高速缓存或者磁盘上搜索文件。
		 */
		err = do_lookup(nd, &this, &next);
		if (err)
			break;
		inode = next.dentry->d_inode;
		if ((lookup_flags & LOOKUP_FOLLOW)
		    && inode && inode->i_op->follow_link) {
			err = do_follow_link(&next, nd);
			if (err)
				goto return_err;
			inode = nd->path.dentry->d_inode;
		} else
			path_to_nameidata(&next, nd);
        /**
         * 检查最后一个分量对应的d_inode是否为空，如果是，则说明对应的目录不存在，返回错误。
         */
		err = -ENOENT;
		if (!inode)
			break;
		/**
		 * LOOKUP_DIRECTORY标志要求最后一个分量必须是目录
		 */
		if (lookup_flags & LOOKUP_DIRECTORY) {
			err = -ENOTDIR; 
			if (!inode->i_op->lookup)
				break;
		}
		goto return_base;
/**
 * 如果目标是最后一个分量的父目录，则不用进行查找了，  
 * 因为nd的path总是指向当前要分析的路径的父目录的  
 * 下面只需要对nd的last_tpye字段进行处理，表示最后一个分量的类型  
 */
lookup_parent:
		/**
		 * 将last设置为最后一个分量
		 */
		nd->last = this;
		/**
		 * 如果最后一个分量不是"."或者".."，那么最后一个分量默认就是LAST_NORM
		 */
		nd->last_type = LAST_NORM;
		if (this.name[0] != '.')
			goto return_base;
		/**
		 * 对"."和".."进行特殊处理
		 */
		if (this.len == 1)
			nd->last_type = LAST_DOT;
		else if (this.len == 2 && this.name[1] == '.')
			nd->last_type = LAST_DOTDOT;
		else
		    /* 由于没有对最后一个分量进行解释，因此返回的dentry就是父目录 */
			goto return_base;
return_reval:
		/*
		 * We bypassed the ordinary revalidation routines.
		 * We may need to check the cached dentry for staleness.
		 */
		if (nd->path.dentry && nd->path.dentry->d_sb &&
		    (nd->path.dentry->d_sb->s_type->fs_flags & FS_REVAL_DOT)) {
			err = -ESTALE;
			/* Note: we do not d_invalidate() */
			if (!nd->path.dentry->d_op->d_revalidate(
					nd->path.dentry, nd))
				break;
		}
return_base:
		return 0;
out_dput:
		path_put_conditional(&next, nd);
		break;
	}
	path_put(&nd->path);
return_err:
	return err;
}

static int path_walk(const char *name, struct nameidata *nd)
{
	/* 连接层数统计 */
	current->total_link_count = 0;
	return link_path_walk(name, nd);
}

/**
 * path_init进行一些搜索前的初始化工作，主要是确定起始搜索的起点并保存在nd中  
 * 根据路径给nd赋初值
 */
static int path_init(int dfd, const char *name, unsigned int flags, struct nameidata *nd)
{
	int retval = 0;
	int fput_needed;
	struct file *file;

	nd->last_type = LAST_ROOT; /* if there are only slashes... */
	nd->flags = flags;
	nd->depth = 0;
	nd->root.mnt = NULL;

    /* 从根目录开始查找 如果文件路径是以绝对路径的形式给出*/ 
	if (*name=='/') {
		/* 设置nd的根目录  */
		set_root(nd);
		nd->path = nd->root;
		path_get(&nd->root);
    /*dfd为无效*/
	} else if (dfd == AT_FDCWD) {
    /**
     * 从当前目录开始查找。
     */
		struct fs_struct *fs = current->fs;
		read_lock(&fs->lock);
		/* 获取当前目录的path */
		nd->path = fs->pwd;
		path_get(&fs->pwd);
		read_unlock(&fs->lock);
	} else {
	    /**
         * 从dfd对应的目录开始查找。
         */
		struct dentry *dentry;

        /*根据dfd，从当前进程的fs_struct结构的fdtable中取出文件描述符指针*/ 
		file = fget_light(dfd, &fput_needed);
		retval = -EBADF;
		if (!file)
			goto out_fail;

		dentry = file->f_path.dentry;

		retval = -ENOTDIR;
		/* 判读dfd对应的目录项是否为目录 */
		if (!S_ISDIR(dentry->d_inode->i_mode))
			goto fput_fail;

		/* 是否有权限 */
		retval = file_permission(file, MAY_EXEC);
		if (retval)
			goto fput_fail;

		/* nd的path设置为dfd对应的目录 */
		nd->path = file->f_path;
		path_get(&file->f_path);

		fput_light(file, fput_needed);
	}
	return 0;

fput_fail:
	fput_light(file, fput_needed);
out_fail:
	return retval;
}

/* Returns 0 and nd will be valid on success; Retuns error, otherwise. */
/**
 * dfd : 目录的fd,没有为负值
 * name:路径名
 * flags:查找操作的标识
 * struct nameidata:用于保存当前的相关查找结果
 */
static int do_path_lookup(int dfd, const char *name,
				unsigned int flags, struct nameidata *nd)
{
     /*path_init进行一些搜索前的初始化工作，主要是确定起始搜索的起点并保存在nd中*/  
	int retval = path_init(dfd, name, flags, nd);
    //初始化没问题的话就开始解析
	if (!retval)
		retval = path_walk(name, nd);
	if (unlikely(!retval && !audit_dummy_context() && nd->path.dentry &&
				nd->path.dentry->d_inode))
		audit_inode(name, nd->path.dentry);
	if (nd->root.mnt) {
		path_put(&nd->root);
		nd->root.mnt = NULL;
	}
	return retval;
}

/**
 * name:路径名
 * flags:查找操作的标识
 * struct nameidata:用于保存当前的相关查找结果
 */
int path_lookup(const char *name, unsigned int flags,
			struct nameidata *nd)
{
	return do_path_lookup(AT_FDCWD, name, flags, nd);
}

int kern_path(const char *name, unsigned int flags, struct path *path)
{
	struct nameidata nd;
	int res = do_path_lookup(AT_FDCWD, name, flags, &nd);
	if (!res)
		*path = nd.path;
	return res;
}

/**
 * vfs_path_lookup - lookup a file path relative to a dentry-vfsmount pair
 * @dentry:  pointer to dentry of the base directory
 * @mnt: pointer to vfs mount of the base directory
 * @name: pointer to file name
 * @flags: lookup flags
 * @nd: pointer to nameidata
 */
int vfs_path_lookup(struct dentry *dentry, struct vfsmount *mnt,
		    const char *name, unsigned int flags,
		    struct nameidata *nd)
{
	int retval;

	/* same as do_path_lookup */
	nd->last_type = LAST_ROOT;
	nd->flags = flags;
	nd->depth = 0;

	nd->path.dentry = dentry;
	nd->path.mnt = mnt;
	path_get(&nd->path);
	nd->root = nd->path;
	path_get(&nd->root);

	retval = path_walk(name, nd);
	if (unlikely(!retval && !audit_dummy_context() && nd->path.dentry &&
				nd->path.dentry->d_inode))
		audit_inode(name, nd->path.dentry);

	path_put(&nd->root);
	nd->root.mnt = NULL;

	return retval;
}

/**
 * path_lookup_open - lookup a file path with open intent
 * @dfd: the directory to use as base, or AT_FDCWD
 * @name: pointer to file name
 * @lookup_flags: lookup intent flags
 * @nd: pointer to nameidata
 * @open_flags: open intent flags
 */
static int path_lookup_open(int dfd, const char *name,
		unsigned int lookup_flags, struct nameidata *nd, int open_flags)
{
	struct file *filp = get_empty_filp();
	int err;

	if (filp == NULL)
		return -ENFILE;
	nd->intent.open.file = filp;
	nd->intent.open.flags = open_flags;
	nd->intent.open.create_mode = 0;
	err = do_path_lookup(dfd, name, lookup_flags|LOOKUP_OPEN, nd);
	if (IS_ERR(nd->intent.open.file)) {
		if (err == 0) {
			err = PTR_ERR(nd->intent.open.file);
			path_put(&nd->path);
		}
	} else if (err != 0)
		release_open_intent(nd);
	return err;
}

static struct dentry *__lookup_hash(struct qstr *name,
		struct dentry *base, struct nameidata *nd)
{
	struct dentry *dentry;
	struct inode *inode;
	int err;

	inode = base->d_inode;

	/*
	 * See if the low-level filesystem might want
	 * to use its own hash..
	 */
	if (base->d_op && base->d_op->d_hash) {
		err = base->d_op->d_hash(base, name);
		dentry = ERR_PTR(err);
		if (err < 0)
			goto out;
	}

	dentry = cached_lookup(base, name, nd);
	if (!dentry) {
		struct dentry *new;

		/* Don't create child dentry for a dead directory. */
		dentry = ERR_PTR(-ENOENT);
		if (IS_DEADDIR(inode))
			goto out;

		new = d_alloc(base, name);
		dentry = ERR_PTR(-ENOMEM);
		if (!new)
			goto out;
		dentry = inode->i_op->lookup(inode, new, nd);
		if (!dentry)
			dentry = new;
		else
			dput(new);
	}
out:
	return dentry;
}

/*
 * Restricted form of lookup. Doesn't follow links, single-component only,
 * needs parent already locked. Doesn't follow mounts.
 * SMP-safe.
 */
static struct dentry *lookup_hash(struct nameidata *nd)
{
	int err;

	err = inode_permission(nd->path.dentry->d_inode, MAY_EXEC);
	if (err)
		return ERR_PTR(err);
	return __lookup_hash(&nd->last, nd->path.dentry, nd);
}

static int __lookup_one_len(const char *name, struct qstr *this,
		struct dentry *base, int len)
{
	unsigned long hash;
	unsigned int c;

	this->name = name;
	this->len = len;
	if (!len)
		return -EACCES;

	hash = init_name_hash();
	while (len--) {
		c = *(const unsigned char *)name++;
		if (c == '/' || c == '\0')
			return -EACCES;
		hash = partial_name_hash(c, hash);
	}
	this->hash = end_name_hash(hash);
	return 0;
}

/**
 * lookup_one_len - filesystem helper to lookup single pathname component
 * @name:	pathname component to lookup
 * @base:	base directory to lookup from
 * @len:	maximum length @len should be interpreted to
 *
 * Note that this routine is purely a helper for filesystem usage and should
 * not be called by generic code.  Also note that by using this function the
 * nameidata argument is passed to the filesystem methods and a filesystem
 * using this helper needs to be prepared for that.
 */
struct dentry *lookup_one_len(const char *name, struct dentry *base, int len)
{
	int err;
	struct qstr this;

	WARN_ON_ONCE(!mutex_is_locked(&base->d_inode->i_mutex));

	err = __lookup_one_len(name, &this, base, len);
	if (err)
		return ERR_PTR(err);

	err = inode_permission(base->d_inode, MAY_EXEC);
	if (err)
		return ERR_PTR(err);
	return __lookup_hash(&this, base, NULL);
}

/**
 * lookup_one_noperm - bad hack for sysfs
 * @name:	pathname component to lookup
 * @base:	base directory to lookup from
 *
 * This is a variant of lookup_one_len that doesn't perform any permission
 * checks.   It's a horrible hack to work around the braindead sysfs
 * architecture and should not be used anywhere else.
 *
 * DON'T USE THIS FUNCTION EVER, thanks.
 */
struct dentry *lookup_one_noperm(const char *name, struct dentry *base)
{
	int err;
	struct qstr this;

	err = __lookup_one_len(name, &this, base, strlen(name));
	if (err)
		return ERR_PTR(err);
	return __lookup_hash(&this, base, NULL);
}

int user_path_at(int dfd, const char __user *name, unsigned flags,
		 struct path *path)
{
	struct nameidata nd;
	char *tmp = getname(name);
	int err = PTR_ERR(tmp);
	if (!IS_ERR(tmp)) {

		BUG_ON(flags & LOOKUP_PARENT);

		err = do_path_lookup(dfd, tmp, flags, &nd);
		putname(tmp);
		if (!err)
			*path = nd.path;
	}
	return err;
}

static int user_path_parent(int dfd, const char __user *path,
			struct nameidata *nd, char **name)
{
	char *s = getname(path);
	int error;

	if (IS_ERR(s))
		return PTR_ERR(s);

	error = do_path_lookup(dfd, s, LOOKUP_PARENT, nd);
	if (error)
		putname(s);
	else
		*name = s;

	return error;
}

/*
 * It's inline, so penalty for filesystems that don't use sticky bit is
 * minimal.
 */
static inline int check_sticky(struct inode *dir, struct inode *inode)
{
	uid_t fsuid = current_fsuid();

	if (!(dir->i_mode & S_ISVTX))
		return 0;
	if (inode->i_uid == fsuid)
		return 0;
	if (dir->i_uid == fsuid)
		return 0;
	return !capable(CAP_FOWNER);
}

/*
 *	Check whether we can remove a link victim from directory dir, check
 *  whether the type of victim is right.
 *  1. We can't do it if dir is read-only (done in permission())
 *  2. We should have write and exec permissions on dir
 *  3. We can't remove anything from append-only dir
 *  4. We can't do anything with immutable dir (done in permission())
 *  5. If the sticky bit on dir is set we should either
 *	a. be owner of dir, or
 *	b. be owner of victim, or
 *	c. have CAP_FOWNER capability
 *  6. If the victim is append-only or immutable we can't do antyhing with
 *     links pointing to it.
 *  7. If we were asked to remove a directory and victim isn't one - ENOTDIR.
 *  8. If we were asked to remove a non-directory and victim isn't one - EISDIR.
 *  9. We can't remove a root or mountpoint.
 * 10. We don't allow removal of NFS sillyrenamed files; it's handled by
 *     nfs_async_unlink().
 */
static int may_delete(struct inode *dir,struct dentry *victim,int isdir)
{
	int error;

	if (!victim->d_inode)
		return -ENOENT;

	BUG_ON(victim->d_parent->d_inode != dir);
	audit_inode_child(victim->d_name.name, victim, dir);

	error = inode_permission(dir, MAY_WRITE | MAY_EXEC);
	if (error)
		return error;
	if (IS_APPEND(dir))
		return -EPERM;
	if (check_sticky(dir, victim->d_inode)||IS_APPEND(victim->d_inode)||
	    IS_IMMUTABLE(victim->d_inode) || IS_SWAPFILE(victim->d_inode))
		return -EPERM;
	if (isdir) {
		if (!S_ISDIR(victim->d_inode->i_mode))
			return -ENOTDIR;
		if (IS_ROOT(victim))
			return -EBUSY;
	} else if (S_ISDIR(victim->d_inode->i_mode))
		return -EISDIR;
	if (IS_DEADDIR(dir))
		return -ENOENT;
	if (victim->d_flags & DCACHE_NFSFS_RENAMED)
		return -EBUSY;
	return 0;
}

/*	Check whether we can create an object with dentry child in directory
 *  dir.
 *  1. We can't do it if child already exists (open has special treatment for
 *     this case, but since we are inlined it's OK)
 *  2. We can't do it if dir is read-only (done in permission())
 *  3. We should have write and exec permissions on dir
 *  4. We can't do it if dir is immutable (done in permission())
 */
static inline int may_create(struct inode *dir, struct dentry *child)
{
	if (child->d_inode)
		return -EEXIST;
	if (IS_DEADDIR(dir))
		return -ENOENT;
	return inode_permission(dir, MAY_WRITE | MAY_EXEC);
}

/* 
 * O_DIRECTORY translates into forcing a directory lookup.
 */
static inline int lookup_flags(unsigned int f)
{
	unsigned long retval = LOOKUP_FOLLOW;

	if (f & O_NOFOLLOW)
		retval &= ~LOOKUP_FOLLOW;
	
	if (f & O_DIRECTORY)
		retval |= LOOKUP_DIRECTORY;

	return retval;
}

/*
 * p1 and p2 should be directories on the same fs.
 */
struct dentry *lock_rename(struct dentry *p1, struct dentry *p2)
{
	struct dentry *p;

	if (p1 == p2) {
		mutex_lock_nested(&p1->d_inode->i_mutex, I_MUTEX_PARENT);
		return NULL;
	}

	mutex_lock(&p1->d_inode->i_sb->s_vfs_rename_mutex);

	p = d_ancestor(p2, p1);
	if (p) {
		mutex_lock_nested(&p2->d_inode->i_mutex, I_MUTEX_PARENT);
		mutex_lock_nested(&p1->d_inode->i_mutex, I_MUTEX_CHILD);
		return p;
	}

	p = d_ancestor(p1, p2);
	if (p) {
		mutex_lock_nested(&p1->d_inode->i_mutex, I_MUTEX_PARENT);
		mutex_lock_nested(&p2->d_inode->i_mutex, I_MUTEX_CHILD);
		return p;
	}

	mutex_lock_nested(&p1->d_inode->i_mutex, I_MUTEX_PARENT);
	mutex_lock_nested(&p2->d_inode->i_mutex, I_MUTEX_CHILD);
	return NULL;
}

void unlock_rename(struct dentry *p1, struct dentry *p2)
{
	mutex_unlock(&p1->d_inode->i_mutex);
	if (p1 != p2) {
		mutex_unlock(&p2->d_inode->i_mutex);
		mutex_unlock(&p1->d_inode->i_sb->s_vfs_rename_mutex);
	}
}

int vfs_create(struct inode *dir, struct dentry *dentry, int mode,
		struct nameidata *nd)
{
	int error = may_create(dir, dentry);

	if (error)
		return error;

	if (!dir->i_op->create)
		return -EACCES;	/* shouldn't it be ENOSYS? */
	mode &= S_IALLUGO;
	mode |= S_IFREG;
	error = security_inode_create(dir, dentry, mode);
	if (error)
		return error;
	vfs_dq_init(dir);
	error = dir->i_op->create(dir, dentry, mode, nd);
	if (!error)
		fsnotify_create(dir, dentry);
	return error;
}

int may_open(struct path *path, int acc_mode, int flag)
{
	struct dentry *dentry = path->dentry;
	struct inode *inode = dentry->d_inode;
	int error;

	if (!inode)
		return -ENOENT;

	switch (inode->i_mode & S_IFMT) {
	case S_IFLNK:
		return -ELOOP;
	case S_IFDIR:
		if (acc_mode & MAY_WRITE)
			return -EISDIR;
		break;
	case S_IFBLK:
	case S_IFCHR:
		if (path->mnt->mnt_flags & MNT_NODEV)
			return -EACCES;
		/*FALLTHRU*/
	case S_IFIFO:
	case S_IFSOCK:
		flag &= ~O_TRUNC;
		break;
	}

	error = inode_permission(inode, acc_mode);
	if (error)
		return error;

	error = ima_path_check(path, acc_mode ?
			       acc_mode & (MAY_READ | MAY_WRITE | MAY_EXEC) :
			       ACC_MODE(flag) & (MAY_READ | MAY_WRITE),
			       IMA_COUNT_UPDATE);

	if (error)
		return error;
	/*
	 * An append-only file must be opened in append mode for writing.
	 */
	if (IS_APPEND(inode)) {
		error = -EPERM;
		if  ((flag & FMODE_WRITE) && !(flag & O_APPEND))
			goto err_out;
		if (flag & O_TRUNC)
			goto err_out;
	}

	/* O_NOATIME can only be set by the owner or superuser */
	if (flag & O_NOATIME)
		if (!is_owner_or_cap(inode)) {
			error = -EPERM;
			goto err_out;
		}

	/*
	 * Ensure there are no outstanding leases on the file.
	 */
	error = break_lease(inode, flag);
	if (error)
		goto err_out;

	if (flag & O_TRUNC) {
		error = get_write_access(inode);
		if (error)
			goto err_out;

		/*
		 * Refuse to truncate files with mandatory locks held on them.
		 */
		error = locks_verify_locked(inode);
		if (!error)
			error = security_path_truncate(path, 0,
					       ATTR_MTIME|ATTR_CTIME|ATTR_OPEN);
		if (!error) {
			vfs_dq_init(inode);

			error = do_truncate(dentry, 0,
					    ATTR_MTIME|ATTR_CTIME|ATTR_OPEN,
					    NULL);
		}
		put_write_access(inode);
		if (error)
			goto err_out;
	} else
		if (flag & FMODE_WRITE)
			vfs_dq_init(inode);

	return 0;
err_out:
	ima_counts_put(path, acc_mode ?
		       acc_mode & (MAY_READ | MAY_WRITE | MAY_EXEC) :
		       ACC_MODE(flag) & (MAY_READ | MAY_WRITE));
	return error;
}

/*
 * Be careful about ever adding any more callers of this
 * function.  Its flags must be in the namei format, not
 * what get passed to sys_open().
 */
static int __open_namei_create(struct nameidata *nd, struct path *path,
				int flag, int mode)
{
	int error;
	struct dentry *dir = nd->path.dentry;

	if (!IS_POSIXACL(dir->d_inode))
		mode &= ~current_umask();
	error = security_path_mknod(&nd->path, path->dentry, mode, 0);
	if (error)
		goto out_unlock;
	error = vfs_create(dir->d_inode, path->dentry, mode, nd);
out_unlock:
	mutex_unlock(&dir->d_inode->i_mutex);
	dput(nd->path.dentry);
	nd->path.dentry = path->dentry;
	if (error)
		return error;
	/* Don't check for write permission, don't truncate */
	return may_open(&nd->path, 0, flag & ~O_TRUNC);
}

/*
 * Note that while the flag value (low two bits) for sys_open means:
 *	00 - read-only
 *	01 - write-only
 *	10 - read-write
 *	11 - special
 * it is changed into
 *	00 - no permissions needed
 *	01 - read-permission
 *	10 - write-permission
 *	11 - read-write
 * for the internal routines (ie open_namei()/follow_link() etc)
 * This is more logical, and also allows the 00 "no perm needed"
 * to be used for symlinks (where the permissions are checked
 * later).
 *
*/
static inline int open_to_namei_flags(int flag)
{
	if ((flag+1) & O_ACCMODE)
		flag++;
	return flag;
}

static int open_will_write_to_fs(int flag, struct inode *inode)
{
	/*
	 * We'll never write to the fs underlying
	 * a device file.
	 */
	if (special_file(inode->i_mode))
		return 0;
	return (flag & O_TRUNC);
}

/*
 * Note that the low bits of the passed in "open_flag"
 * are not the same as in the local variable "flag". See
 * open_to_namei_flags() for more details.
 */
/**
 * 查找文件的inode，并打开它
 */
struct file *do_filp_open(int dfd, const char *pathname,
		int open_flag, int mode, int acc_mode)
{
	struct file *filp;
	struct nameidata nd;
	int error;
	struct path path;
	struct dentry *dir;
	int count = 0;
	int will_write;
	int flag = open_to_namei_flags(open_flag);

	if (!acc_mode)
		acc_mode = MAY_OPEN | ACC_MODE(flag);

	/* O_TRUNC implies we need access checks for write permissions */
    /*根据 O_TRUNC标志设置写权限 */  
	if (flag & O_TRUNC)
		acc_mode |= MAY_WRITE;

	/* Allow the LSM permission hook to distinguish append 
	   access from general write access. */
	if (flag & O_APPEND)
		acc_mode |= MAY_APPEND;

	/*
	 * The simplest case - just a plain lookup.
	 */
	 /*如果不是创建文件*/ 
	if (!(flag & O_CREAT)) {
		/* 当内核要访问一个文件的时候，第一步要做的是找到这个文件， 
        而查找文件的过程在vfs里面是由path_lookup或者path_lookup_open函数来完成的。 
        这两个函数将用户传进来的字符串表示的文件路径转换成一个dentry结构， 
        并建立好相应的inode和file结构，将指向file的描述符返回用户。用户随后 
        通过文件描述符，来访问这些数据结构 */
		error = path_lookup_open(dfd, pathname, lookup_flags(flag),
					 &nd, flag);
		if (error)
			return ERR_PTR(error);
        /*跳过下面的创建部分*/ 
		goto ok;
	}

	/*
	 * Create - we need to know the parent.
	 */
    /*到此则是要创建文件*/  
    /* path-init为查找作准备工作，path_walk真正上路查找， 
    这两个函数联合起来根据一段路径名找到对应的dentry */ 
	error = path_init(dfd, pathname, LOOKUP_PARENT, &nd);
	if (error)
		return ERR_PTR(error);
	error = path_walk(pathname, &nd);
	if (error) {
		if (nd.root.mnt)
			path_put(&nd.root);
		return ERR_PTR(error);
	}
	if (unlikely(!audit_dummy_context()))
         /*保存inode节点信息*/  
		audit_inode(pathname, nd.path.dentry);

	/*
	 * We have the parent and last component. First of all, check
	 * that we are not asked to creat(2) an obvious directory - that
	 * will not do.
	 */
	error = -EISDIR;
     /*父节点信息*/ 
	if (nd.last_type != LAST_NORM || nd.last.name[nd.last.len])
		goto exit_parent;

	error = -ENFILE;
     /*获取文件指针*/ 
	filp = get_empty_filp();
	if (filp == NULL)
		goto exit_parent;
    /*填充nameidata 结构*/
	nd.intent.open.file = filp;
	nd.intent.open.flags = flag;
	nd.intent.open.create_mode = mode;
	dir = nd.path.dentry;
	nd.flags &= ~LOOKUP_PARENT;
	nd.flags |= LOOKUP_CREATE | LOOKUP_OPEN;
	if (flag & O_EXCL)
		nd.flags |= LOOKUP_EXCL;
	mutex_lock(&dir->d_inode->i_mutex);
    /*从哈希表中查找目的文件对应的dentry,上面路径搜索的是父节点 
    也就是目的文件的上一层目录，为了得到目的文件的 
    path结构，我们用nd中的last结构和上一层目录的dentry结构 
    可以找到*/  
	path.dentry = lookup_hash(&nd);
	path.mnt = nd.path.mnt;
    /*到此目标节点的path结构已经找到*/
do_last:
	error = PTR_ERR(path.dentry);
	if (IS_ERR(path.dentry)) {
		mutex_unlock(&dir->d_inode->i_mutex);
		goto exit;
	}

	if (IS_ERR(nd.intent.open.file)) {
		error = PTR_ERR(nd.intent.open.file);
		goto exit_mutex_unlock;
	}

	/* Negative dentry, just create the file */
    /*如果此dentry结构没有对应的inode节点，说明是无效的，应该创建文件节点 */ 
	if (!path.dentry->d_inode) {
		/*
		 * This write is needed to ensure that a
		 * ro->rw transition does not occur between
		 * the time when the file is created and when
		 * a permanent write count is taken through
		 * the 'struct file' in nameidata_to_filp().
		 */
		 /*write权限是必需的*/
		error = mnt_want_write(nd.path.mnt);
		if (error)
			goto exit_mutex_unlock;
        /*按照namei格式的flag open*,主要是创建inode*/  
		error = __open_namei_create(&nd, &path, flag, mode);
		if (error) {
			mnt_drop_write(nd.path.mnt);
			goto exit;
		}
        /*根据nameidata 得到相应的file结构*/
		filp = nameidata_to_filp(&nd, open_flag);
		if (IS_ERR(filp))
			ima_counts_put(&nd.path,
				       acc_mode & (MAY_READ | MAY_WRITE |
						   MAY_EXEC));
        /*放弃写权限*/
		mnt_drop_write(nd.path.mnt);
		if (nd.root.mnt)
			path_put(&nd.root);
		return filp;
	}

	/*
	 * It already exists.
	 */
	 /*要打开的文件已经存在*/  
	mutex_unlock(&dir->d_inode->i_mutex);
     /*保存inode节点*/
	audit_inode(pathname, path.dentry);

	error = -EEXIST;
	/* 文件已经存在，如果指定了O_EXCL标志，则需要返回错误。 */
	if (flag & O_EXCL)
		goto exit_dput;

    /*如果path上安装了文件系统，则依次往下找，直到找到 
    的文件系统没有安装别的文件系统，更新path结构为 
    此文件系统的根目录信息*/ 
	if (__follow_mount(&path)) {
		error = -ELOOP;
		if (flag & O_NOFOLLOW)
			goto exit_dput;
	}

	error = -ENOENT;
	if (!path.dentry->d_inode)
		goto exit_dput;
	if (path.dentry->d_inode->i_op->follow_link)
		goto do_link;
    
    /*路径转化为相应的nameidata 结构*/
	path_to_nameidata(&path, &nd);
	error = -EISDIR;
	if (path.dentry->d_inode && S_ISDIR(path.dentry->d_inode->i_mode))
		goto exit;
    /*到这里，nd结构中存放的信息已经是最后的目的文件信息*/
ok:
	/*
	 * Consider:
	 * 1. may_open() truncates a file
	 * 2. a rw->ro mount transition occurs
	 * 3. nameidata_to_filp() fails due to
	 *    the ro mount.
	 * That would be inconsistent, and should
	 * be avoided. Taking this mnt write here
	 * ensures that (2) can not occur.
	 */
	will_write = open_will_write_to_fs(flag, nd.path.dentry->d_inode);
	if (will_write) {
		error = mnt_want_write(nd.path.mnt);
		if (error)
			goto exit;
	}
    /*may_open执行权限检测、文件打开和truncate的操作*/
	error = may_open(&nd.path, acc_mode, flag);
	if (error) {
		if (will_write)
			mnt_drop_write(nd.path.mnt);
		goto exit;
	}
    /*将nameidata转化为file*/  
	filp = nameidata_to_filp(&nd, open_flag);
	if (IS_ERR(filp))
		ima_counts_put(&nd.path,
			       acc_mode & (MAY_READ | MAY_WRITE | MAY_EXEC));
	/*
	 * It is now safe to drop the mnt write
	 * because the filp has had a write taken
	 * on its behalf.
	 */
	if (will_write)
        /*释放写权限*/ 
		mnt_drop_write(nd.path.mnt);
	if (nd.root.mnt)
        /*释放引用计数*/ 
		path_put(&nd.root);
	return filp;

exit_mutex_unlock:
	mutex_unlock(&dir->d_inode->i_mutex);
exit_dput:
	path_put_conditional(&path, &nd);
exit:
	if (!IS_ERR(nd.intent.open.file))
		release_open_intent(&nd);
exit_parent:
	if (nd.root.mnt)
		path_put(&nd.root);
	path_put(&nd.path);
	return ERR_PTR(error);
/*允许遍历连接文件，则手工找到连接文件对应的文件*/ 
do_link:
	error = -ELOOP;
	if (flag & O_NOFOLLOW)
        /*不允许遍历连接文件，返回错误*/ 
		goto exit_dput;
	/*
	 * This is subtle. Instead of calling do_follow_link() we do the
	 * thing by hands. The reason is that this way we have zero link_count
	 * and path_walk() (called from ->follow_link) honoring LOOKUP_PARENT.
	 * After that we have the parent and last component, i.e.
	 * we are in the same situation as after the first path_walk().
	 * Well, almost - if the last component is normal we get its copy
	 * stored in nd->last.name and we will have to putname() it when we
	 * are done. Procfs-like symlinks just set LAST_BIND.
	 */
	/*设置查找LOOKUP_PARENT标志*/
	nd.flags |= LOOKUP_PARENT;
    /*判断操作是否安全*/ 
	error = security_inode_follow_link(path.dentry, &nd);
	if (error)
		goto exit_dput;
    /*处理符号链接,即路径搜索，结果放入nd中*/  
	error = __do_follow_link(&path, &nd);
	if (error) {
		/* Does someone understand code flow here? Or it is only
		 * me so stupid? Anathema to whoever designed this non-sense
		 * with "intent.open".
		 */
		release_open_intent(&nd);
		if (nd.root.mnt)
			path_put(&nd.root);
		return ERR_PTR(error);
	}
	nd.flags &= ~LOOKUP_PARENT;
	if (nd.last_type == LAST_BIND)
		goto ok;
	error = -EISDIR;
    /*检查最后一段文件或目录名的属性情况*/
	if (nd.last_type != LAST_NORM)
		goto exit;
	if (nd.last.name[nd.last.len]) {
		__putname(nd.last.name);
		goto exit;
	}
	error = -ELOOP;
     /*出现回环标志: 循环超过32次*/  
	if (count++==32) {
		__putname(nd.last.name);
		goto exit;
	}
	dir = nd.path.dentry;
	mutex_lock(&dir->d_inode->i_mutex);
    /*更新路径的挂接点和dentry*/  
	path.dentry = lookup_hash(&nd);
	path.mnt = nd.path.mnt;
	__putname(nd.last.name);
	goto do_last;
}

/**
 * filp_open - open file and return file pointer
 *
 * @filename:	path to open
 * @flags:	open flags as per the open(2) second argument
 * @mode:	mode for the new file if O_CREAT is set, else ignored
 *
 * This is the helper to open a file from kernelspace if you really
 * have to.  But in generally you should not do this, so please move
 * along, nothing to see here..
 */
struct file *filp_open(const char *filename, int flags, int mode)
{
	return do_filp_open(AT_FDCWD, filename, flags, mode, 0);
}
EXPORT_SYMBOL(filp_open);

/**
 * lookup_create - lookup a dentry, creating it if it doesn't exist
 * @nd: nameidata info
 * @is_dir: directory flag
 *
 * Simple function to lookup and return a dentry and create it
 * if it doesn't exist.  Is SMP-safe.
 *
 * Returns with nd->path.dentry->d_inode->i_mutex locked.
 */
struct dentry *lookup_create(struct nameidata *nd, int is_dir)
{
	struct dentry *dentry = ERR_PTR(-EEXIST);

	mutex_lock_nested(&nd->path.dentry->d_inode->i_mutex, I_MUTEX_PARENT);
	/*
	 * Yucky last component or no last component at all?
	 * (foo/., foo/.., /////)
	 */
	if (nd->last_type != LAST_NORM)
		goto fail;
	nd->flags &= ~LOOKUP_PARENT;
	nd->flags |= LOOKUP_CREATE | LOOKUP_EXCL;
	nd->intent.open.flags = O_EXCL;

	/*
	 * Do the final lookup.
	 */
	dentry = lookup_hash(nd);
	if (IS_ERR(dentry))
		goto fail;

	if (dentry->d_inode)
		goto eexist;
	/*
	 * Special case - lookup gave negative, but... we had foo/bar/
	 * From the vfs_mknod() POV we just have a negative dentry -
	 * all is fine. Let's be bastards - you had / on the end, you've
	 * been asking for (non-existent) directory. -ENOENT for you.
	 */
	if (unlikely(!is_dir && nd->last.name[nd->last.len])) {
		dput(dentry);
		dentry = ERR_PTR(-ENOENT);
	}
	return dentry;
eexist:
	dput(dentry);
	dentry = ERR_PTR(-EEXIST);
fail:
	return dentry;
}
EXPORT_SYMBOL_GPL(lookup_create);

int vfs_mknod(struct inode *dir, struct dentry *dentry, int mode, dev_t dev)
{
	int error = may_create(dir, dentry);

	if (error)
		return error;

	if ((S_ISCHR(mode) || S_ISBLK(mode)) && !capable(CAP_MKNOD))
		return -EPERM;

	if (!dir->i_op->mknod)
		return -EPERM;

	error = devcgroup_inode_mknod(mode, dev);
	if (error)
		return error;

	error = security_inode_mknod(dir, dentry, mode, dev);
	if (error)
		return error;

	vfs_dq_init(dir);
	error = dir->i_op->mknod(dir, dentry, mode, dev);
	if (!error)
		fsnotify_create(dir, dentry);
	return error;
}

static int may_mknod(mode_t mode)
{
	switch (mode & S_IFMT) {
	case S_IFREG:
	case S_IFCHR:
	case S_IFBLK:
	case S_IFIFO:
	case S_IFSOCK:
	case 0: /* zero mode translates to S_IFREG */
		return 0;
	case S_IFDIR:
		return -EPERM;
	default:
		return -EINVAL;
	}
}

SYSCALL_DEFINE4(mknodat, int, dfd, const char __user *, filename, int, mode,
		unsigned, dev)
{
	int error;
	char *tmp;
	struct dentry *dentry;
	struct nameidata nd;

	if (S_ISDIR(mode))
		return -EPERM;

	error = user_path_parent(dfd, filename, &nd, &tmp);
	if (error)
		return error;

	dentry = lookup_create(&nd, 0);
	if (IS_ERR(dentry)) {
		error = PTR_ERR(dentry);
		goto out_unlock;
	}
	if (!IS_POSIXACL(nd.path.dentry->d_inode))
		mode &= ~current_umask();
	error = may_mknod(mode);
	if (error)
		goto out_dput;
	error = mnt_want_write(nd.path.mnt);
	if (error)
		goto out_dput;
	error = security_path_mknod(&nd.path, dentry, mode, dev);
	if (error)
		goto out_drop_write;
	switch (mode & S_IFMT) {
		case 0: case S_IFREG:
			error = vfs_create(nd.path.dentry->d_inode,dentry,mode,&nd);
			break;
		case S_IFCHR: case S_IFBLK:
			error = vfs_mknod(nd.path.dentry->d_inode,dentry,mode,
					new_decode_dev(dev));
			break;
		case S_IFIFO: case S_IFSOCK:
			error = vfs_mknod(nd.path.dentry->d_inode,dentry,mode,0);
			break;
	}
out_drop_write:
	mnt_drop_write(nd.path.mnt);
out_dput:
	dput(dentry);
out_unlock:
	mutex_unlock(&nd.path.dentry->d_inode->i_mutex);
	path_put(&nd.path);
	putname(tmp);

	return error;
}

SYSCALL_DEFINE3(mknod, const char __user *, filename, int, mode, unsigned, dev)
{
	return sys_mknodat(AT_FDCWD, filename, mode, dev);
}

int vfs_mkdir(struct inode *dir, struct dentry *dentry, int mode)
{
	int error = may_create(dir, dentry);

	if (error)
		return error;

	if (!dir->i_op->mkdir)
		return -EPERM;

	mode &= (S_IRWXUGO|S_ISVTX);
	error = security_inode_mkdir(dir, dentry, mode);
	if (error)
		return error;

	vfs_dq_init(dir);
	error = dir->i_op->mkdir(dir, dentry, mode);
	if (!error)
		fsnotify_mkdir(dir, dentry);
	return error;
}

SYSCALL_DEFINE3(mkdirat, int, dfd, const char __user *, pathname, int, mode)
{
	int error = 0;
	char * tmp;
	struct dentry *dentry;
	struct nameidata nd;

	error = user_path_parent(dfd, pathname, &nd, &tmp);
	if (error)
		goto out_err;

	dentry = lookup_create(&nd, 1);
	error = PTR_ERR(dentry);
	if (IS_ERR(dentry))
		goto out_unlock;

	if (!IS_POSIXACL(nd.path.dentry->d_inode))
		mode &= ~current_umask();
	error = mnt_want_write(nd.path.mnt);
	if (error)
		goto out_dput;
	error = security_path_mkdir(&nd.path, dentry, mode);
	if (error)
		goto out_drop_write;
	error = vfs_mkdir(nd.path.dentry->d_inode, dentry, mode);
out_drop_write:
	mnt_drop_write(nd.path.mnt);
out_dput:
	dput(dentry);
out_unlock:
	mutex_unlock(&nd.path.dentry->d_inode->i_mutex);
	path_put(&nd.path);
	putname(tmp);
out_err:
	return error;
}

SYSCALL_DEFINE2(mkdir, const char __user *, pathname, int, mode)
{
	return sys_mkdirat(AT_FDCWD, pathname, mode);
}

/*
 * We try to drop the dentry early: we should have
 * a usage count of 2 if we're the only user of this
 * dentry, and if that is true (possibly after pruning
 * the dcache), then we drop the dentry now.
 *
 * A low-level filesystem can, if it choses, legally
 * do a
 *
 *	if (!d_unhashed(dentry))
 *		return -EBUSY;
 *
 * if it cannot handle the case of removing a directory
 * that is still in use by something else..
 */
void dentry_unhash(struct dentry *dentry)
{
	dget(dentry);
	shrink_dcache_parent(dentry);
	spin_lock(&dcache_lock);
	spin_lock(&dentry->d_lock);
	if (atomic_read(&dentry->d_count) == 2)
		__d_drop(dentry);
	spin_unlock(&dentry->d_lock);
	spin_unlock(&dcache_lock);
}

int vfs_rmdir(struct inode *dir, struct dentry *dentry)
{
	int error = may_delete(dir, dentry, 1);

	if (error)
		return error;

	if (!dir->i_op->rmdir)
		return -EPERM;

	vfs_dq_init(dir);

	mutex_lock(&dentry->d_inode->i_mutex);
	dentry_unhash(dentry);
	if (d_mountpoint(dentry))
		error = -EBUSY;
	else {
		error = security_inode_rmdir(dir, dentry);
		if (!error) {
			error = dir->i_op->rmdir(dir, dentry);
			if (!error)
				dentry->d_inode->i_flags |= S_DEAD;
		}
	}
	mutex_unlock(&dentry->d_inode->i_mutex);
	if (!error) {
		d_delete(dentry);
	}
	dput(dentry);

	return error;
}

static long do_rmdir(int dfd, const char __user *pathname)
{
	int error = 0;
	char * name;
	struct dentry *dentry;
	struct nameidata nd;

	error = user_path_parent(dfd, pathname, &nd, &name);
	if (error)
		return error;

	switch(nd.last_type) {
	case LAST_DOTDOT:
		error = -ENOTEMPTY;
		goto exit1;
	case LAST_DOT:
		error = -EINVAL;
		goto exit1;
	case LAST_ROOT:
		error = -EBUSY;
		goto exit1;
	}

	nd.flags &= ~LOOKUP_PARENT;

	mutex_lock_nested(&nd.path.dentry->d_inode->i_mutex, I_MUTEX_PARENT);
	dentry = lookup_hash(&nd);
	error = PTR_ERR(dentry);
	if (IS_ERR(dentry))
		goto exit2;
	error = mnt_want_write(nd.path.mnt);
	if (error)
		goto exit3;
	error = security_path_rmdir(&nd.path, dentry);
	if (error)
		goto exit4;
	error = vfs_rmdir(nd.path.dentry->d_inode, dentry);
exit4:
	mnt_drop_write(nd.path.mnt);
exit3:
	dput(dentry);
exit2:
	mutex_unlock(&nd.path.dentry->d_inode->i_mutex);
exit1:
	path_put(&nd.path);
	putname(name);
	return error;
}

SYSCALL_DEFINE1(rmdir, const char __user *, pathname)
{
	return do_rmdir(AT_FDCWD, pathname);
}

int vfs_unlink(struct inode *dir, struct dentry *dentry)
{
	int error = may_delete(dir, dentry, 0);

	if (error)
		return error;

	if (!dir->i_op->unlink)
		return -EPERM;

	vfs_dq_init(dir);

	mutex_lock(&dentry->d_inode->i_mutex);
	if (d_mountpoint(dentry))
		error = -EBUSY;
	else {
		error = security_inode_unlink(dir, dentry);
		if (!error)
			error = dir->i_op->unlink(dir, dentry);
	}
	mutex_unlock(&dentry->d_inode->i_mutex);

	/* We don't d_delete() NFS sillyrenamed files--they still exist. */
	if (!error && !(dentry->d_flags & DCACHE_NFSFS_RENAMED)) {
		fsnotify_link_count(dentry->d_inode);
		d_delete(dentry);
	}

	return error;
}

/*
 * Make sure that the actual truncation of the file will occur outside its
 * directory's i_mutex.  Truncate can take a long time if there is a lot of
 * writeout happening, and we don't want to prevent access to the directory
 * while waiting on the I/O.
 */
static long do_unlinkat(int dfd, const char __user *pathname)
{
	int error;
	char *name;
	struct dentry *dentry;
	struct nameidata nd;
	struct inode *inode = NULL;

	error = user_path_parent(dfd, pathname, &nd, &name);
	if (error)
		return error;

	error = -EISDIR;
	if (nd.last_type != LAST_NORM)
		goto exit1;

	nd.flags &= ~LOOKUP_PARENT;

	mutex_lock_nested(&nd.path.dentry->d_inode->i_mutex, I_MUTEX_PARENT);
	dentry = lookup_hash(&nd);
	error = PTR_ERR(dentry);
	if (!IS_ERR(dentry)) {
		/* Why not before? Because we want correct error value */
		if (nd.last.name[nd.last.len])
			goto slashes;
		inode = dentry->d_inode;
		if (inode)
			atomic_inc(&inode->i_count);
		error = mnt_want_write(nd.path.mnt);
		if (error)
			goto exit2;
		error = security_path_unlink(&nd.path, dentry);
		if (error)
			goto exit3;
		error = vfs_unlink(nd.path.dentry->d_inode, dentry);
exit3:
		mnt_drop_write(nd.path.mnt);
	exit2:
		dput(dentry);
	}
	mutex_unlock(&nd.path.dentry->d_inode->i_mutex);
	if (inode)
		iput(inode);	/* truncate the inode here */
exit1:
	path_put(&nd.path);
	putname(name);
	return error;

slashes:
	error = !dentry->d_inode ? -ENOENT :
		S_ISDIR(dentry->d_inode->i_mode) ? -EISDIR : -ENOTDIR;
	goto exit2;
}

SYSCALL_DEFINE3(unlinkat, int, dfd, const char __user *, pathname, int, flag)
{
	if ((flag & ~AT_REMOVEDIR) != 0)
		return -EINVAL;

	if (flag & AT_REMOVEDIR)
		return do_rmdir(dfd, pathname);

	return do_unlinkat(dfd, pathname);
}

SYSCALL_DEFINE1(unlink, const char __user *, pathname)
{
	return do_unlinkat(AT_FDCWD, pathname);
}

int vfs_symlink(struct inode *dir, struct dentry *dentry, const char *oldname)
{
	int error = may_create(dir, dentry);

	if (error)
		return error;

	if (!dir->i_op->symlink)
		return -EPERM;

	error = security_inode_symlink(dir, dentry, oldname);
	if (error)
		return error;

	vfs_dq_init(dir);
	error = dir->i_op->symlink(dir, dentry, oldname);
	if (!error)
		fsnotify_create(dir, dentry);
	return error;
}

SYSCALL_DEFINE3(symlinkat, const char __user *, oldname,
		int, newdfd, const char __user *, newname)
{
	int error;
	char *from;
	char *to;
	struct dentry *dentry;
	struct nameidata nd;

	from = getname(oldname);
	if (IS_ERR(from))
		return PTR_ERR(from);

	error = user_path_parent(newdfd, newname, &nd, &to);
	if (error)
		goto out_putname;

	dentry = lookup_create(&nd, 0);
	error = PTR_ERR(dentry);
	if (IS_ERR(dentry))
		goto out_unlock;

	error = mnt_want_write(nd.path.mnt);
	if (error)
		goto out_dput;
	error = security_path_symlink(&nd.path, dentry, from);
	if (error)
		goto out_drop_write;
	error = vfs_symlink(nd.path.dentry->d_inode, dentry, from);
out_drop_write:
	mnt_drop_write(nd.path.mnt);
out_dput:
	dput(dentry);
out_unlock:
	mutex_unlock(&nd.path.dentry->d_inode->i_mutex);
	path_put(&nd.path);
	putname(to);
out_putname:
	putname(from);
	return error;
}

SYSCALL_DEFINE2(symlink, const char __user *, oldname, const char __user *, newname)
{
	return sys_symlinkat(oldname, AT_FDCWD, newname);
}

int vfs_link(struct dentry *old_dentry, struct inode *dir, struct dentry *new_dentry)
{
	struct inode *inode = old_dentry->d_inode;
	int error;

	if (!inode)
		return -ENOENT;

	error = may_create(dir, new_dentry);
	if (error)
		return error;

	if (dir->i_sb != inode->i_sb)
		return -EXDEV;

	/*
	 * A link to an append-only or immutable file cannot be created.
	 */
	if (IS_APPEND(inode) || IS_IMMUTABLE(inode))
		return -EPERM;
	if (!dir->i_op->link)
		return -EPERM;
	if (S_ISDIR(inode->i_mode))
		return -EPERM;

	error = security_inode_link(old_dentry, dir, new_dentry);
	if (error)
		return error;

	mutex_lock(&inode->i_mutex);
	vfs_dq_init(dir);
	error = dir->i_op->link(old_dentry, dir, new_dentry);
	mutex_unlock(&inode->i_mutex);
	if (!error)
		fsnotify_link(dir, inode, new_dentry);
	return error;
}

/*
 * Hardlinks are often used in delicate situations.  We avoid
 * security-related surprises by not following symlinks on the
 * newname.  --KAB
 *
 * We don't follow them on the oldname either to be compatible
 * with linux 2.0, and to avoid hard-linking to directories
 * and other special files.  --ADM
 */
SYSCALL_DEFINE5(linkat, int, olddfd, const char __user *, oldname,
		int, newdfd, const char __user *, newname, int, flags)
{
	struct dentry *new_dentry;
	struct nameidata nd;
	struct path old_path;
	int error;
	char *to;

	if ((flags & ~AT_SYMLINK_FOLLOW) != 0)
		return -EINVAL;

	error = user_path_at(olddfd, oldname,
			     flags & AT_SYMLINK_FOLLOW ? LOOKUP_FOLLOW : 0,
			     &old_path);
	if (error)
		return error;

	error = user_path_parent(newdfd, newname, &nd, &to);
	if (error)
		goto out;
	error = -EXDEV;
	if (old_path.mnt != nd.path.mnt)
		goto out_release;
	new_dentry = lookup_create(&nd, 0);
	error = PTR_ERR(new_dentry);
	if (IS_ERR(new_dentry))
		goto out_unlock;
	error = mnt_want_write(nd.path.mnt);
	if (error)
		goto out_dput;
	error = security_path_link(old_path.dentry, &nd.path, new_dentry);
	if (error)
		goto out_drop_write;
	error = vfs_link(old_path.dentry, nd.path.dentry->d_inode, new_dentry);
out_drop_write:
	mnt_drop_write(nd.path.mnt);
out_dput:
	dput(new_dentry);
out_unlock:
	mutex_unlock(&nd.path.dentry->d_inode->i_mutex);
out_release:
	path_put(&nd.path);
	putname(to);
out:
	path_put(&old_path);

	return error;
}

SYSCALL_DEFINE2(link, const char __user *, oldname, const char __user *, newname)
{
	return sys_linkat(AT_FDCWD, oldname, AT_FDCWD, newname, 0);
}

/*
 * The worst of all namespace operations - renaming directory. "Perverted"
 * doesn't even start to describe it. Somebody in UCB had a heck of a trip...
 * Problems:
 *	a) we can get into loop creation. Check is done in is_subdir().
 *	b) race potential - two innocent renames can create a loop together.
 *	   That's where 4.4 screws up. Current fix: serialization on
 *	   sb->s_vfs_rename_mutex. We might be more accurate, but that's another
 *	   story.
 *	c) we have to lock _three_ objects - parents and victim (if it exists).
 *	   And that - after we got ->i_mutex on parents (until then we don't know
 *	   whether the target exists).  Solution: try to be smart with locking
 *	   order for inodes.  We rely on the fact that tree topology may change
 *	   only under ->s_vfs_rename_mutex _and_ that parent of the object we
 *	   move will be locked.  Thus we can rank directories by the tree
 *	   (ancestors first) and rank all non-directories after them.
 *	   That works since everybody except rename does "lock parent, lookup,
 *	   lock child" and rename is under ->s_vfs_rename_mutex.
 *	   HOWEVER, it relies on the assumption that any object with ->lookup()
 *	   has no more than 1 dentry.  If "hybrid" objects will ever appear,
 *	   we'd better make sure that there's no link(2) for them.
 *	d) some filesystems don't support opened-but-unlinked directories,
 *	   either because of layout or because they are not ready to deal with
 *	   all cases correctly. The latter will be fixed (taking this sort of
 *	   stuff into VFS), but the former is not going away. Solution: the same
 *	   trick as in rmdir().
 *	e) conversion from fhandle to dentry may come in the wrong moment - when
 *	   we are removing the target. Solution: we will have to grab ->i_mutex
 *	   in the fhandle_to_dentry code. [FIXME - current nfsfh.c relies on
 *	   ->i_mutex on parents, which works but leads to some truely excessive
 *	   locking].
 */
static int vfs_rename_dir(struct inode *old_dir, struct dentry *old_dentry,
			  struct inode *new_dir, struct dentry *new_dentry)
{
	int error = 0;
	struct inode *target;

	/*
	 * If we are going to change the parent - check write permissions,
	 * we'll need to flip '..'.
	 */
	if (new_dir != old_dir) {
		error = inode_permission(old_dentry->d_inode, MAY_WRITE);
		if (error)
			return error;
	}

	error = security_inode_rename(old_dir, old_dentry, new_dir, new_dentry);
	if (error)
		return error;

	target = new_dentry->d_inode;
	if (target) {
		mutex_lock(&target->i_mutex);
		dentry_unhash(new_dentry);
	}
	if (d_mountpoint(old_dentry)||d_mountpoint(new_dentry))
		error = -EBUSY;
	else 
		error = old_dir->i_op->rename(old_dir, old_dentry, new_dir, new_dentry);
	if (target) {
		if (!error)
			target->i_flags |= S_DEAD;
		mutex_unlock(&target->i_mutex);
		if (d_unhashed(new_dentry))
			d_rehash(new_dentry);
		dput(new_dentry);
	}
	if (!error)
		if (!(old_dir->i_sb->s_type->fs_flags & FS_RENAME_DOES_D_MOVE))
			d_move(old_dentry,new_dentry);
	return error;
}

static int vfs_rename_other(struct inode *old_dir, struct dentry *old_dentry,
			    struct inode *new_dir, struct dentry *new_dentry)
{
	struct inode *target;
	int error;

	error = security_inode_rename(old_dir, old_dentry, new_dir, new_dentry);
	if (error)
		return error;

	dget(new_dentry);
	target = new_dentry->d_inode;
	if (target)
		mutex_lock(&target->i_mutex);
	if (d_mountpoint(old_dentry)||d_mountpoint(new_dentry))
		error = -EBUSY;
	else
		error = old_dir->i_op->rename(old_dir, old_dentry, new_dir, new_dentry);
	if (!error) {
		if (!(old_dir->i_sb->s_type->fs_flags & FS_RENAME_DOES_D_MOVE))
			d_move(old_dentry, new_dentry);
	}
	if (target)
		mutex_unlock(&target->i_mutex);
	dput(new_dentry);
	return error;
}

int vfs_rename(struct inode *old_dir, struct dentry *old_dentry,
	       struct inode *new_dir, struct dentry *new_dentry)
{
	int error;
	int is_dir = S_ISDIR(old_dentry->d_inode->i_mode);
	const char *old_name;

	if (old_dentry->d_inode == new_dentry->d_inode)
 		return 0;
 
	error = may_delete(old_dir, old_dentry, is_dir);
	if (error)
		return error;

	if (!new_dentry->d_inode)
		error = may_create(new_dir, new_dentry);
	else
		error = may_delete(new_dir, new_dentry, is_dir);
	if (error)
		return error;

	if (!old_dir->i_op->rename)
		return -EPERM;

	vfs_dq_init(old_dir);
	vfs_dq_init(new_dir);

	old_name = fsnotify_oldname_init(old_dentry->d_name.name);

	if (is_dir)
		error = vfs_rename_dir(old_dir,old_dentry,new_dir,new_dentry);
	else
		error = vfs_rename_other(old_dir,old_dentry,new_dir,new_dentry);
	if (!error) {
		const char *new_name = old_dentry->d_name.name;
		fsnotify_move(old_dir, new_dir, old_name, new_name, is_dir,
			      new_dentry->d_inode, old_dentry);
	}
	fsnotify_oldname_free(old_name);

	return error;
}

SYSCALL_DEFINE4(renameat, int, olddfd, const char __user *, oldname,
		int, newdfd, const char __user *, newname)
{
	struct dentry *old_dir, *new_dir;
	struct dentry *old_dentry, *new_dentry;
	struct dentry *trap;
	struct nameidata oldnd, newnd;
	char *from;
	char *to;
	int error;

	error = user_path_parent(olddfd, oldname, &oldnd, &from);
	if (error)
		goto exit;

	error = user_path_parent(newdfd, newname, &newnd, &to);
	if (error)
		goto exit1;

	error = -EXDEV;
	if (oldnd.path.mnt != newnd.path.mnt)
		goto exit2;

	old_dir = oldnd.path.dentry;
	error = -EBUSY;
	if (oldnd.last_type != LAST_NORM)
		goto exit2;

	new_dir = newnd.path.dentry;
	if (newnd.last_type != LAST_NORM)
		goto exit2;

	oldnd.flags &= ~LOOKUP_PARENT;
	newnd.flags &= ~LOOKUP_PARENT;
	newnd.flags |= LOOKUP_RENAME_TARGET;

	trap = lock_rename(new_dir, old_dir);

	old_dentry = lookup_hash(&oldnd);
	error = PTR_ERR(old_dentry);
	if (IS_ERR(old_dentry))
		goto exit3;
	/* source must exist */
	error = -ENOENT;
	if (!old_dentry->d_inode)
		goto exit4;
	/* unless the source is a directory trailing slashes give -ENOTDIR */
	if (!S_ISDIR(old_dentry->d_inode->i_mode)) {
		error = -ENOTDIR;
		if (oldnd.last.name[oldnd.last.len])
			goto exit4;
		if (newnd.last.name[newnd.last.len])
			goto exit4;
	}
	/* source should not be ancestor of target */
	error = -EINVAL;
	if (old_dentry == trap)
		goto exit4;
	new_dentry = lookup_hash(&newnd);
	error = PTR_ERR(new_dentry);
	if (IS_ERR(new_dentry))
		goto exit4;
	/* target should not be an ancestor of source */
	error = -ENOTEMPTY;
	if (new_dentry == trap)
		goto exit5;

	error = mnt_want_write(oldnd.path.mnt);
	if (error)
		goto exit5;
	error = security_path_rename(&oldnd.path, old_dentry,
				     &newnd.path, new_dentry);
	if (error)
		goto exit6;
	error = vfs_rename(old_dir->d_inode, old_dentry,
				   new_dir->d_inode, new_dentry);
exit6:
	mnt_drop_write(oldnd.path.mnt);
exit5:
	dput(new_dentry);
exit4:
	dput(old_dentry);
exit3:
	unlock_rename(new_dir, old_dir);
exit2:
	path_put(&newnd.path);
	putname(to);
exit1:
	path_put(&oldnd.path);
	putname(from);
exit:
	return error;
}

SYSCALL_DEFINE2(rename, const char __user *, oldname, const char __user *, newname)
{
	return sys_renameat(AT_FDCWD, oldname, AT_FDCWD, newname);
}

int vfs_readlink(struct dentry *dentry, char __user *buffer, int buflen, const char *link)
{
	int len;

	len = PTR_ERR(link);
	if (IS_ERR(link))
		goto out;

	len = strlen(link);
	if (len > (unsigned) buflen)
		len = buflen;
	if (copy_to_user(buffer, link, len))
		len = -EFAULT;
out:
	return len;
}

/*
 * A helper for ->readlink().  This should be used *ONLY* for symlinks that
 * have ->follow_link() touching nd only in nd_set_link().  Using (or not
 * using) it for any given inode is up to filesystem.
 */
int generic_readlink(struct dentry *dentry, char __user *buffer, int buflen)
{
	struct nameidata nd;
	void *cookie;
	int res;

	nd.depth = 0;
	cookie = dentry->d_inode->i_op->follow_link(dentry, &nd);
	if (IS_ERR(cookie))
		return PTR_ERR(cookie);

	res = vfs_readlink(dentry, buffer, buflen, nd_get_link(&nd));
	if (dentry->d_inode->i_op->put_link)
		dentry->d_inode->i_op->put_link(dentry, &nd, cookie);
	return res;
}

int vfs_follow_link(struct nameidata *nd, const char *link)
{
	return __vfs_follow_link(nd, link);
}

/* get the link contents into pagecache */
static char *page_getlink(struct dentry * dentry, struct page **ppage)
{
	char *kaddr;
	struct page *page;
	struct address_space *mapping = dentry->d_inode->i_mapping;
	page = read_mapping_page(mapping, 0, NULL);
	if (IS_ERR(page))
		return (char*)page;
	*ppage = page;
	kaddr = kmap(page);
	nd_terminate_link(kaddr, dentry->d_inode->i_size, PAGE_SIZE - 1);
	return kaddr;
}

int page_readlink(struct dentry *dentry, char __user *buffer, int buflen)
{
	struct page *page = NULL;
	char *s = page_getlink(dentry, &page);
	int res = vfs_readlink(dentry,buffer,buflen,s);
	if (page) {
		kunmap(page);
		page_cache_release(page);
	}
	return res;
}

void *page_follow_link_light(struct dentry *dentry, struct nameidata *nd)
{
	struct page *page = NULL;
	nd_set_link(nd, page_getlink(dentry, &page));
	return page;
}

void page_put_link(struct dentry *dentry, struct nameidata *nd, void *cookie)
{
	struct page *page = cookie;

	if (page) {
		kunmap(page);
		page_cache_release(page);
	}
}

/*
 * The nofs argument instructs pagecache_write_begin to pass AOP_FLAG_NOFS
 */
int __page_symlink(struct inode *inode, const char *symname, int len, int nofs)
{
	struct address_space *mapping = inode->i_mapping;
	struct page *page;
	void *fsdata;
	int err;
	char *kaddr;
	unsigned int flags = AOP_FLAG_UNINTERRUPTIBLE;
	if (nofs)
		flags |= AOP_FLAG_NOFS;

retry:
	err = pagecache_write_begin(NULL, mapping, 0, len-1,
				flags, &page, &fsdata);
	if (err)
		goto fail;

	kaddr = kmap_atomic(page, KM_USER0);
	memcpy(kaddr, symname, len-1);
	kunmap_atomic(kaddr, KM_USER0);

	err = pagecache_write_end(NULL, mapping, 0, len-1, len-1,
							page, fsdata);
	if (err < 0)
		goto fail;
	if (err < len-1)
		goto retry;

	mark_inode_dirty(inode);
	return 0;
fail:
	return err;
}

int page_symlink(struct inode *inode, const char *symname, int len)
{
	return __page_symlink(inode, symname, len,
			!(mapping_gfp_mask(inode->i_mapping) & __GFP_FS));
}

const struct inode_operations page_symlink_inode_operations = {
	.readlink	= generic_readlink,
	.follow_link	= page_follow_link_light,
	.put_link	= page_put_link,
};

EXPORT_SYMBOL(user_path_at);
EXPORT_SYMBOL(follow_down);
EXPORT_SYMBOL(follow_up);
EXPORT_SYMBOL(get_write_access); /* binfmt_aout */
EXPORT_SYMBOL(getname);
EXPORT_SYMBOL(lock_rename);
EXPORT_SYMBOL(lookup_one_len);
EXPORT_SYMBOL(page_follow_link_light);
EXPORT_SYMBOL(page_put_link);
EXPORT_SYMBOL(page_readlink);
EXPORT_SYMBOL(__page_symlink);
EXPORT_SYMBOL(page_symlink);
EXPORT_SYMBOL(page_symlink_inode_operations);
EXPORT_SYMBOL(path_lookup);
EXPORT_SYMBOL(kern_path);
EXPORT_SYMBOL(vfs_path_lookup);
EXPORT_SYMBOL(inode_permission);
EXPORT_SYMBOL(file_permission);
EXPORT_SYMBOL(unlock_rename);
EXPORT_SYMBOL(vfs_create);
EXPORT_SYMBOL(vfs_follow_link);
EXPORT_SYMBOL(vfs_link);
EXPORT_SYMBOL(vfs_mkdir);
EXPORT_SYMBOL(vfs_mknod);
EXPORT_SYMBOL(generic_permission);
EXPORT_SYMBOL(vfs_readlink);
EXPORT_SYMBOL(vfs_rename);
EXPORT_SYMBOL(vfs_rmdir);
EXPORT_SYMBOL(vfs_symlink);
EXPORT_SYMBOL(vfs_unlink);
EXPORT_SYMBOL(dentry_unhash);
EXPORT_SYMBOL(generic_readlink);
