#ifndef _LINUX_NAMEI_H
#define _LINUX_NAMEI_H

#include <linux/dcache.h>
#include <linux/linkage.h>
#include <linux/path.h>

struct vfsmount;

struct open_intent {
	int	flags;
	int	create_mode;
	struct file *file;
};

enum { MAX_NESTED_LINKS = 8 };

/**
 * 路径查找的结果
 */
struct nameidata {
	/**
	 * 查找到的PATH对象。
	 */
	struct path	path;

	/**
	 * 路径名的最后一个分量。当指定LOOKUP_PARENT时使用。
	 */
	struct qstr	last;
	struct path	root;
	/**
	 * 查找标志。
	 */
	unsigned int	flags;
	/**
	 * 路径名最后一个分量的类型。如LAST_NORM
	 */
	int		last_type;
	/**
	 * 符号链接查找的嵌套深度。
	 */
	unsigned	depth;
	/**
	 * 嵌套关联路径名数组。
	 */
	char *saved_names[MAX_NESTED_LINKS + 1];

	/* Intent data */
	/**
	 * 指定如何访问文件。
	 */
	union {
		struct open_intent open;
	} intent;
};

/*
 * Type of the last component on LOOKUP_PARENT
 */
/**
 * LAST_NORM:	最后一个分量是普通文件名
 * LAST_ROOT:	最后一个分量是"/"
 * LAST_DOT:	最后一个分量是"."
 * LAST_DOTDOT:	最后一个分量是".."
 * LAST_BIND:	最后一个分量是链接到特殊文件系统的符号链接
 */
enum {LAST_NORM, LAST_ROOT, LAST_DOT, LAST_DOTDOT, LAST_BIND};

/*
 * The bitmask for a lookup event:
 *  - follow links at the end
 *  - require a directory
 *  - ending slashes ok even for nonexistent files
 *  - internal "there are more path components" flag
 *  - locked when lookup done with dcache_lock held
 *  - dentry cache is untrusted; force a real lookup
 */
/**
 * 如果最后一个分量是符号链接，则解释它。
 */
#define LOOKUP_FOLLOW		 1
/**
 * 最后一个分量必须是目录。
 */
#define LOOKUP_DIRECTORY	 2
/**
 * 在路径名中还有文件名要检查。
 */
#define LOOKUP_CONTINUE		 4
/**
 * 查找最后一个分量所在的目录
 */
#define LOOKUP_PARENT		16
#define LOOKUP_REVAL		64
/*
 * Intent data
 */
/**
 * 试图打开一个文件
 */
#define LOOKUP_OPEN		0x0100
/**
 * 试图创建一个文件
 */
#define LOOKUP_CREATE		0x0200
#define LOOKUP_EXCL		0x0400
#define LOOKUP_RENAME_TARGET	0x0800

extern int user_path_at(int, const char __user *, unsigned, struct path *);

#define user_path(name, path) user_path_at(AT_FDCWD, name, LOOKUP_FOLLOW, path)
#define user_lpath(name, path) user_path_at(AT_FDCWD, name, 0, path)
#define user_path_dir(name, path) \
	user_path_at(AT_FDCWD, name, LOOKUP_FOLLOW | LOOKUP_DIRECTORY, path)

extern int kern_path(const char *, unsigned, struct path *);

extern int path_lookup(const char *, unsigned, struct nameidata *);
extern int vfs_path_lookup(struct dentry *, struct vfsmount *,
			   const char *, unsigned int, struct nameidata *);

extern struct file *lookup_instantiate_filp(struct nameidata *nd, struct dentry *dentry,
		int (*open)(struct inode *, struct file *));
extern struct file *nameidata_to_filp(struct nameidata *nd, int flags);
extern void release_open_intent(struct nameidata *);

extern struct dentry *lookup_one_len(const char *, struct dentry *, int);
extern struct dentry *lookup_one_noperm(const char *, struct dentry *);

extern int follow_down(struct path *);
extern int follow_up(struct path *);

extern struct dentry *lock_rename(struct dentry *, struct dentry *);
extern void unlock_rename(struct dentry *, struct dentry *);

static inline void nd_set_link(struct nameidata *nd, char *path)
{
	nd->saved_names[nd->depth] = path;
}

static inline char *nd_get_link(struct nameidata *nd)
{
	return nd->saved_names[nd->depth];
}

static inline void nd_terminate_link(void *name, size_t len, size_t maxlen)
{
	((char *) name)[min(len, maxlen)] = '\0';
}

#endif /* _LINUX_NAMEI_H */
