/*
 * Resizable simple ram filesystem for Linux.
 *
 * Copyright (C) 2000 Linus Torvalds.
 *               2000 Transmeta Corp.
 *
 * Usage limits added by David Gibson, Linuxcare Australia.
 * This file is released under the GPL.
 */

/*
 * NOTE! This filesystem is probably most useful
 * not as a real filesystem, but as an example of
 * how virtual filesystems can be written.
 *
 * It doesn't get much simpler than this. Consider
 * that this file implements the full semantics of
 * a POSIX-compliant read-write filesystem.
 *
 * Note in particular how the filesystem does not
 * need to implement any data structures of its own
 * to keep track of the virtual data: using the VFS
 * caches is sufficient.
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/pagemap.h>
#include <linux/highmem.h>
#include <linux/time.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/backing-dev.h>
#include <linux/ramfs.h>
#include <linux/sched.h>
#include <linux/parser.h>
#include <linux/magic.h>
#include <asm/uaccess.h>
#include "internal.h"

#define RAMFS_DEFAULT_MODE	0755

/* 针对ramfs的操作回调函数 */  
static const struct super_operations ramfs_ops;
/* 针对目录inode的操作回调函数 */
static const struct inode_operations ramfs_dir_inode_operations;

/* 描述底层块设备 */  
static struct backing_dev_info ramfs_backing_dev_info = {
	.name		= "ramfs",
    /* No readahead 由于ramfs直接放在缓存中，所以不需要预读 */
	.ra_pages	= 0,	/* No readahead */
	   
    /* 描述底层块设备具备的功能， 
       BDI_CAP_NO_ACCT_AND_WRITEBACK含义为不回写脏页、不统计脏页、不自动统计回写的脏页  
       BDI_CAP_MAP_DIRECT 表示块设备支持mmap操作的MAP_PRIVATE 
       BDI_CAP_MAP_COPY表示块设备支持mmap操作的MAP_PRIVATE 
       BDI_CAP_READ_MAP表示块设备支持mmap操作的PROT_READ 
       BDI_CAP_WRITE_MAP表示块设备支持mmap操作的PROT_WRITE 
       BDI_CAP_EXEC_MAP表示块设备支持mmap操作的PROT_EXEC 
    */
	.capabilities	= BDI_CAP_NO_ACCT_AND_WRITEBACK |
			  BDI_CAP_MAP_DIRECT | BDI_CAP_MAP_COPY |
			  BDI_CAP_READ_MAP | BDI_CAP_WRITE_MAP | BDI_CAP_EXEC_MAP,
};

/* 创建一个inode */  
struct inode *ramfs_get_inode(struct super_block *sb, int mode, dev_t dev)
{
    /* 从内存中分配一个inode空间 */
	struct inode * inode = new_inode(sb);

	if (inode) {
        /* 填充inode结构 */
        
        /* 文件类型 */ 
        inode->i_mode = mode;
        /* 获得当前进程的UID */
        inode->i_uid = current_fsuid();
        /* 获得当前进程的GID */
		inode->i_gid = current_fsgid();
        /* 注册内存操作回调 */
		inode->i_mapping->a_ops = &ramfs_aops;
        /* 保存底层块设备信息 */
		inode->i_mapping->backing_dev_info = &ramfs_backing_dev_info;
        /* 为inode分配内存地址空间 */
		mapping_set_gfp_mask(inode->i_mapping, GFP_HIGHUSER);
		mapping_set_unevictable(inode->i_mapping);
		inode->i_atime = inode->i_mtime = inode->i_ctime = CURRENT_TIME;
		switch (mode & S_IFMT) {
		default:
            /* 处理特殊的inode，包括socket、fifo、块设备、字符设备*/ 
			init_special_inode(inode, mode, dev);
			break;
		case S_IFREG:
            /* 普通文件，注册回调函数 */  
			inode->i_op = &ramfs_file_inode_operations;
			inode->i_fop = &ramfs_file_operations;
			break;
		case S_IFDIR:
            /* 目录，注册回调函数 */
			inode->i_op = &ramfs_dir_inode_operations;
			inode->i_fop = &simple_dir_operations;

			/* directory inodes start off with i_nlink == 2 (for "." entry) */
            /* 增加文件引用计数即inode->i_nlink，目录的引用计数为2，因为包括了"."  
               当inode->i_nlink为0时，说明这个inode闲置 
            */
			inc_nlink(inode);
			break;
		case S_IFLNK:
			inode->i_op = &page_symlink_inode_operations;
			break;
		}
	}
	return inode;
}

/*
 * File creation. Allocate an inode, and we're done..
 */
/* SMP-safe */
/* 在指定的目录下创建节点 */ 
static int
ramfs_mknod(struct inode *dir, struct dentry *dentry, int mode, dev_t dev)
{
	struct inode * inode = ramfs_get_inode(dir->i_sb, mode, dev);
	int error = -ENOSPC;

	if (inode) {
        /* 如果mode带有GID，需要将GID付给inode */  
		if (dir->i_mode & S_ISGID) {
			inode->i_gid = dir->i_gid;
			if (S_ISDIR(mode))
				inode->i_mode |= S_ISGID;
		}
        /* 用于向dentry结构中填写inode信息 */
		d_instantiate(dentry, inode);
		dget(dentry);	/* Extra count - pin the dentry in core */
		error = 0;
        /* 修改目录的访问时间、inode修改时间 */
		dir->i_mtime = dir->i_ctime = CURRENT_TIME;
	}
	return error;
}

/* 创建目录 */ 
static int ramfs_mkdir(struct inode * dir, struct dentry * dentry, int mode)
{
	int retval = ramfs_mknod(dir, dentry, mode | S_IFDIR, 0);
	if (!retval)
        /* 将目录inode->i_nlink加一 */  
		inc_nlink(dir);
	return retval;
}

/* 创建文件 */ 
static int ramfs_create(struct inode *dir, struct dentry *dentry, int mode, struct nameidata *nd)
{
	return ramfs_mknod(dir, dentry, mode | S_IFREG, 0);
}

/* 建立软连接 */ 
static int ramfs_symlink(struct inode * dir, struct dentry *dentry, const char * symname)
{
	struct inode *inode;
	int error = -ENOSPC;

     /* 获得一个inode */ 
	inode = ramfs_get_inode(dir->i_sb, S_IFLNK|S_IRWXUGO, 0);
	if (inode) {
		int l = strlen(symname)+1;
        /* 将软连接写入pagecache ，并将页置为脏*/
		error = page_symlink(inode, symname, l);
		if (!error) {
			if (dir->i_mode & S_ISGID)
				inode->i_gid = dir->i_gid;
            /* 用于向dentry结构中填写inode信息 */
			d_instantiate(dentry, inode);
            /* dentry->d_count加1 */
			dget(dentry);
            /* 置操作时间 */ 
			dir->i_mtime = dir->i_ctime = CURRENT_TIME;
		} else
			iput(inode);
	}
	return error;
}

/* 为inode操作注册回调函数 */
static const struct inode_operations ramfs_dir_inode_operations = {
	.create		= ramfs_create,
	.lookup		= simple_lookup,
	.link		= simple_link,
	.unlink		= simple_unlink,
	.symlink	= ramfs_symlink,
	.mkdir		= ramfs_mkdir,
	.rmdir		= simple_rmdir,
	.mknod		= ramfs_mknod,
	.rename		= simple_rename,
};

/* 为超级块操作注册回调 */  
static const struct super_operations ramfs_ops = {
	.statfs		= simple_statfs,
	.drop_inode	= generic_delete_inode,
	.show_options	= generic_show_options,
};

struct ramfs_mount_opts {
	umode_t mode;
};

enum {
	Opt_mode,
	Opt_err
};

static const match_table_t tokens = {
	{Opt_mode, "mode=%o"},
	{Opt_err, NULL}
};

struct ramfs_fs_info {
	struct ramfs_mount_opts mount_opts;
};

static int ramfs_parse_options(char *data, struct ramfs_mount_opts *opts)
{
	substring_t args[MAX_OPT_ARGS];
	int option;
	int token;
	char *p;

	opts->mode = RAMFS_DEFAULT_MODE;

	while ((p = strsep(&data, ",")) != NULL) {
		if (!*p)
			continue;

		token = match_token(p, tokens, args);
		switch (token) {
		case Opt_mode:
			if (match_octal(&args[0], &option))
				return -EINVAL;
			opts->mode = option & S_IALLUGO;
			break;
		/*
		 * We might like to report bad mount options here;
		 * but traditionally ramfs has ignored all mount options,
		 * and as it is used as a !CONFIG_SHMEM simple substitute
		 * for tmpfs, better continue to ignore other mount options.
		 */
		}
	}

	return 0;
}

/* 填充超级块 */ 
static int ramfs_fill_super(struct super_block * sb, void * data, int silent)
{
	struct ramfs_fs_info *fsi;
	struct inode *inode = NULL;
	struct dentry *root;
	int err;

	save_mount_options(sb, data);

	fsi = kzalloc(sizeof(struct ramfs_fs_info), GFP_KERNEL);
	sb->s_fs_info = fsi;
	if (!fsi) {
		err = -ENOMEM;
		goto fail;
	}

	err = ramfs_parse_options(data, &fsi->mount_opts);
	if (err)
		goto fail;

    /* 填充超级块结构体 */  
	sb->s_maxbytes		= MAX_LFS_FILESIZE;
	sb->s_blocksize		= PAGE_CACHE_SIZE;
	sb->s_blocksize_bits	= PAGE_CACHE_SHIFT;
	sb->s_magic		= RAMFS_MAGIC;
	sb->s_op		= &ramfs_ops;
	sb->s_time_gran		= 1;

    /* 为文件系统root分配inode */  
    inode = ramfs_get_inode(sb, S_IFDIR | fsi->mount_opts.mode, 0);
	if (!inode) {
		err = -ENOMEM;
		goto fail;
	}

    /* 为root分配缓存 */
	root = d_alloc_root(inode);
	sb->s_root = root;
	if (!root) {
		err = -ENOMEM;
		goto fail;
	}

	return 0;
/* 异常处理 */ 
fail:
	kfree(fsi);
	sb->s_fs_info = NULL;
	iput(inode);
	return err;
}

/* 装载ramfs的超级块 */ 
int ramfs_get_sb(struct file_system_type *fs_type,
	int flags, const char *dev_name, void *data, struct vfsmount *mnt)
{
    /*在内存中分配一个超级块结构 (struct super_block) sb，并初始化其部分成员变量，
    将成员 s_instances 插入到 rootfs 文件系统类型结构中的 fs_supers 指向的双向链表中。*/  
	return get_sb_nodev(fs_type, flags, data, ramfs_fill_super, mnt);
}

/* 装载rootfs的超级块 */
static int rootfs_get_sb(struct file_system_type *fs_type,
	int flags, const char *dev_name, void *data, struct vfsmount *mnt)
{
	return get_sb_nodev(fs_type, flags|MS_NOUSER, data, ramfs_fill_super,
			    mnt);
}

/* 卸载超级块 */ 
static void ramfs_kill_sb(struct super_block *sb)
{
	kfree(sb->s_fs_info);
	kill_litter_super(sb);
}

static struct file_system_type ramfs_fs_type = {
	.name		= "ramfs",
	.get_sb		= ramfs_get_sb,
	.kill_sb	= ramfs_kill_sb,
};
static struct file_system_type rootfs_fs_type = {
	.name		= "rootfs",
	.get_sb		= rootfs_get_sb,
	.kill_sb	= kill_litter_super,
};

/* 初始化模块，注册文件系统 */
static int __init init_ramfs_fs(void)
{
	return register_filesystem(&ramfs_fs_type);
}

/* 退出模块，注销文件系统 */
static void __exit exit_ramfs_fs(void)
{
	unregister_filesystem(&ramfs_fs_type);
}

module_init(init_ramfs_fs)
module_exit(exit_ramfs_fs)

/* 初始化rootfs文件系统，在引导开机的过程中调用 */  
int __init init_rootfs(void)
{
	int err;

    /* 初始化对应的块设备 */ 
	err = bdi_init(&ramfs_backing_dev_info);
	if (err)
		return err;

    /* 注册rootfs文件系统 */ 
	err = register_filesystem(&rootfs_fs_type);
	if (err)
		bdi_destroy(&ramfs_backing_dev_info);

	return err;
}

MODULE_LICENSE("GPL");
