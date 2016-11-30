/*
 *
 * Definitions for mount interface. This describes the in the kernel build 
 * linkedlist with mounted filesystems.
 *
 * Author:  Marco van Wieringen <mvw@planets.elm.net>
 *
 */
#ifndef _LINUX_MOUNT_H
#define _LINUX_MOUNT_H

#include <linux/types.h>
#include <linux/list.h>
#include <linux/nodemask.h>
#include <linux/spinlock.h>
#include <asm/atomic.h>

struct super_block;
struct vfsmount;
struct dentry;
struct mnt_namespace;

/**
 * 在已经安装文件系统中禁止setuid和setgid标志。
 */
#define MNT_NOSUID	0x01
/**
 * 在已经安装文件系统中禁止访问设备文件
 */
#define MNT_NODEV	0x02
/**
 * 在已经安装文件系统中不允许程序执行。
 */
#define MNT_NOEXEC	0x04
/**
 * 不记录访问时间和修改时间
 */
#define MNT_NOATIME	0x08
#define MNT_NODIRATIME	0x10
#define MNT_RELATIME	0x20
#define MNT_READONLY	0x40	/* does the user want this to be r/o? */
#define MNT_STRICTATIME 0x80

/**
 * 用于NFS和AFS
 */
#define MNT_SHRINKABLE	0x100
#define MNT_WRITE_HOLD	0x200

/* 共享和不可绑定装载 */
#define MNT_SHARED	0x1000	/* if the vfsmount is a shared mount */
#define MNT_UNBINDABLE	0x2000	/* if the vfsmount is a unbindable mount */
#define MNT_PNODE_MASK	0x3000	/* propagation flag mask */

/**
 * 文件系统安装点
 */
struct vfsmount {
	/* 用于将装载点加入到散列表中 */
	struct list_head mnt_hash;
	/**
	 * 指向父文件系统，这个文件系统安装在其上。
	 */
	struct vfsmount *mnt_parent;	/* fs we are mounted on */
	/**
	 * 安装点目录节点, 上一个文件系统。
	 */
	struct dentry *mnt_mountpoint;	/* dentry of mountpoint */
	/**
	 * 指向这个文件系统根目录的dentry。
	 */
	struct dentry *mnt_root;	/* root of the mounted tree */
	/**
	 * 该文件系统的超级块对象。
	 */
	struct super_block *mnt_sb;	/* pointer to superblock */
	/**
	 * 包含所有文件系统描述符链表的头 子文件系统链表
	 */
	struct list_head mnt_mounts;	/* list of children, anchored here */
	/**
	 * 已安装文件系统链表头。通过此字段将其加入父文件系统的mnt_mounts链表中。
	 */
	struct list_head mnt_child;	/* and going through their mnt_child */
	/**
	 * mount标志
	 */
	int mnt_flags;
	/* 4 bytes hole on 64bits arches */
	/**
	 * 设备文件名。
	 */	
	const char *mnt_devname;	/* Name of device e.g. /dev/dsk/hda1 */
	/**
	 * 已安装文件系统描述符的namespace链表指针?
	 * 通过此字段将其加入到namespace的list链表中。
	 */
	struct list_head mnt_list;
	/* 自动过期的装载链表元素 */
	struct list_head mnt_expire;	/* link in fs-specific expiry list */
	/* 共享装载链表元素 */
	struct list_head mnt_share;	/* circular list of shared mounts */
	/* 从属装载、主装载使用的链表和指针 */
	struct list_head mnt_slave_list;/* list of slave mounts */
	/* 从安装链表的入口 */
	struct list_head mnt_slave;	/* slave list entry */
    /* 从安装链表的主人 */
    struct vfsmount *mnt_master;	/* slave is on master->mnt_slave_list */
	/* 所属的命名空间 */
	struct mnt_namespace *mnt_ns;	/* containing namespace */
	/* 安装标识符 */
	int mnt_id;			/* mount identifier */
	/* 组标识符 */
	int mnt_group_id;		/* peer group identifier */
	/*
	 * We put mnt_count & mnt_expiry_mark at the end of struct vfsmount
	 * to let these frequently modified fields in a separate cache line
	 * (so that reads of mnt_flags wont ping-pong on SMP machines)
	 */
	/* 计数器，使用mntput和mntget维护 */
	atomic_t mnt_count;
	/* 是否处理装载过期，如果到期，则值为真 */
	int mnt_expiry_mark;		/* true if marked for expiry */
	/* "钉住"进程计数*/
	int mnt_pinned;
	/* "镜像"引用计数 */
	int mnt_ghosts;
#ifdef CONFIG_SMP
	int *mnt_writers;
#else
	/* 写者引用计数 */
	int mnt_writers;
#endif
};

static inline int *get_mnt_writers_ptr(struct vfsmount *mnt)
{
#ifdef CONFIG_SMP
	return mnt->mnt_writers;
#else
	return &mnt->mnt_writers;
#endif
}

static inline struct vfsmount *mntget(struct vfsmount *mnt)
{
	if (mnt)
		atomic_inc(&mnt->mnt_count);
	return mnt;
}

struct file; /* forward dec */

extern int mnt_want_write(struct vfsmount *mnt);
extern int mnt_want_write_file(struct file *file);
extern int mnt_clone_write(struct vfsmount *mnt);
extern void mnt_drop_write(struct vfsmount *mnt);
extern void mntput_no_expire(struct vfsmount *mnt);
extern void mnt_pin(struct vfsmount *mnt);
extern void mnt_unpin(struct vfsmount *mnt);
extern int __mnt_is_readonly(struct vfsmount *mnt);

static inline void mntput(struct vfsmount *mnt)
{
	if (mnt) {
		mnt->mnt_expiry_mark = 0;
		mntput_no_expire(mnt);
	}
}

extern struct vfsmount *do_kern_mount(const char *fstype, int flags,
				      const char *name, void *data);

struct file_system_type;
extern struct vfsmount *vfs_kern_mount(struct file_system_type *type,
				      int flags, const char *name,
				      void *data);

struct nameidata;

struct path;
extern int do_add_mount(struct vfsmount *newmnt, struct path *path,
			int mnt_flags, struct list_head *fslist);

extern void mark_mounts_for_expiry(struct list_head *mounts);

/**
 * 保护已经安装文件系统的链表。
 */
extern spinlock_t vfsmount_lock;
extern dev_t name_to_dev_t(char *name);

#endif /* _LINUX_MOUNT_H */
