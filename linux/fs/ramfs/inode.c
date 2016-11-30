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

/* ���ramfs�Ĳ����ص����� */  
static const struct super_operations ramfs_ops;
/* ���Ŀ¼inode�Ĳ����ص����� */
static const struct inode_operations ramfs_dir_inode_operations;

/* �����ײ���豸 */  
static struct backing_dev_info ramfs_backing_dev_info = {
	.name		= "ramfs",
    /* No readahead ����ramfsֱ�ӷ��ڻ����У����Բ���ҪԤ�� */
	.ra_pages	= 0,	/* No readahead */
	   
    /* �����ײ���豸�߱��Ĺ��ܣ� 
       BDI_CAP_NO_ACCT_AND_WRITEBACK����Ϊ����д��ҳ����ͳ����ҳ�����Զ�ͳ�ƻ�д����ҳ  
       BDI_CAP_MAP_DIRECT ��ʾ���豸֧��mmap������MAP_PRIVATE 
       BDI_CAP_MAP_COPY��ʾ���豸֧��mmap������MAP_PRIVATE 
       BDI_CAP_READ_MAP��ʾ���豸֧��mmap������PROT_READ 
       BDI_CAP_WRITE_MAP��ʾ���豸֧��mmap������PROT_WRITE 
       BDI_CAP_EXEC_MAP��ʾ���豸֧��mmap������PROT_EXEC 
    */
	.capabilities	= BDI_CAP_NO_ACCT_AND_WRITEBACK |
			  BDI_CAP_MAP_DIRECT | BDI_CAP_MAP_COPY |
			  BDI_CAP_READ_MAP | BDI_CAP_WRITE_MAP | BDI_CAP_EXEC_MAP,
};

/* ����һ��inode */  
struct inode *ramfs_get_inode(struct super_block *sb, int mode, dev_t dev)
{
    /* ���ڴ��з���һ��inode�ռ� */
	struct inode * inode = new_inode(sb);

	if (inode) {
        /* ���inode�ṹ */
        
        /* �ļ����� */ 
        inode->i_mode = mode;
        /* ��õ�ǰ���̵�UID */
        inode->i_uid = current_fsuid();
        /* ��õ�ǰ���̵�GID */
		inode->i_gid = current_fsgid();
        /* ע���ڴ�����ص� */
		inode->i_mapping->a_ops = &ramfs_aops;
        /* ����ײ���豸��Ϣ */
		inode->i_mapping->backing_dev_info = &ramfs_backing_dev_info;
        /* Ϊinode�����ڴ��ַ�ռ� */
		mapping_set_gfp_mask(inode->i_mapping, GFP_HIGHUSER);
		mapping_set_unevictable(inode->i_mapping);
		inode->i_atime = inode->i_mtime = inode->i_ctime = CURRENT_TIME;
		switch (mode & S_IFMT) {
		default:
            /* ���������inode������socket��fifo�����豸���ַ��豸*/ 
			init_special_inode(inode, mode, dev);
			break;
		case S_IFREG:
            /* ��ͨ�ļ���ע��ص����� */  
			inode->i_op = &ramfs_file_inode_operations;
			inode->i_fop = &ramfs_file_operations;
			break;
		case S_IFDIR:
            /* Ŀ¼��ע��ص����� */
			inode->i_op = &ramfs_dir_inode_operations;
			inode->i_fop = &simple_dir_operations;

			/* directory inodes start off with i_nlink == 2 (for "." entry) */
            /* �����ļ����ü�����inode->i_nlink��Ŀ¼�����ü���Ϊ2����Ϊ������"."  
               ��inode->i_nlinkΪ0ʱ��˵�����inode���� 
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
/* ��ָ����Ŀ¼�´����ڵ� */ 
static int
ramfs_mknod(struct inode *dir, struct dentry *dentry, int mode, dev_t dev)
{
	struct inode * inode = ramfs_get_inode(dir->i_sb, mode, dev);
	int error = -ENOSPC;

	if (inode) {
        /* ���mode����GID����Ҫ��GID����inode */  
		if (dir->i_mode & S_ISGID) {
			inode->i_gid = dir->i_gid;
			if (S_ISDIR(mode))
				inode->i_mode |= S_ISGID;
		}
        /* ������dentry�ṹ����дinode��Ϣ */
		d_instantiate(dentry, inode);
		dget(dentry);	/* Extra count - pin the dentry in core */
		error = 0;
        /* �޸�Ŀ¼�ķ���ʱ�䡢inode�޸�ʱ�� */
		dir->i_mtime = dir->i_ctime = CURRENT_TIME;
	}
	return error;
}

/* ����Ŀ¼ */ 
static int ramfs_mkdir(struct inode * dir, struct dentry * dentry, int mode)
{
	int retval = ramfs_mknod(dir, dentry, mode | S_IFDIR, 0);
	if (!retval)
        /* ��Ŀ¼inode->i_nlink��һ */  
		inc_nlink(dir);
	return retval;
}

/* �����ļ� */ 
static int ramfs_create(struct inode *dir, struct dentry *dentry, int mode, struct nameidata *nd)
{
	return ramfs_mknod(dir, dentry, mode | S_IFREG, 0);
}

/* ���������� */ 
static int ramfs_symlink(struct inode * dir, struct dentry *dentry, const char * symname)
{
	struct inode *inode;
	int error = -ENOSPC;

     /* ���һ��inode */ 
	inode = ramfs_get_inode(dir->i_sb, S_IFLNK|S_IRWXUGO, 0);
	if (inode) {
		int l = strlen(symname)+1;
        /* ��������д��pagecache ������ҳ��Ϊ��*/
		error = page_symlink(inode, symname, l);
		if (!error) {
			if (dir->i_mode & S_ISGID)
				inode->i_gid = dir->i_gid;
            /* ������dentry�ṹ����дinode��Ϣ */
			d_instantiate(dentry, inode);
            /* dentry->d_count��1 */
			dget(dentry);
            /* �ò���ʱ�� */ 
			dir->i_mtime = dir->i_ctime = CURRENT_TIME;
		} else
			iput(inode);
	}
	return error;
}

/* Ϊinode����ע��ص����� */
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

/* Ϊ���������ע��ص� */  
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

/* ��䳬���� */ 
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

    /* ��䳬����ṹ�� */  
	sb->s_maxbytes		= MAX_LFS_FILESIZE;
	sb->s_blocksize		= PAGE_CACHE_SIZE;
	sb->s_blocksize_bits	= PAGE_CACHE_SHIFT;
	sb->s_magic		= RAMFS_MAGIC;
	sb->s_op		= &ramfs_ops;
	sb->s_time_gran		= 1;

    /* Ϊ�ļ�ϵͳroot����inode */  
    inode = ramfs_get_inode(sb, S_IFDIR | fsi->mount_opts.mode, 0);
	if (!inode) {
		err = -ENOMEM;
		goto fail;
	}

    /* Ϊroot���仺�� */
	root = d_alloc_root(inode);
	sb->s_root = root;
	if (!root) {
		err = -ENOMEM;
		goto fail;
	}

	return 0;
/* �쳣���� */ 
fail:
	kfree(fsi);
	sb->s_fs_info = NULL;
	iput(inode);
	return err;
}

/* װ��ramfs�ĳ����� */ 
int ramfs_get_sb(struct file_system_type *fs_type,
	int flags, const char *dev_name, void *data, struct vfsmount *mnt)
{
    /*���ڴ��з���һ��������ṹ (struct super_block) sb������ʼ���䲿�ֳ�Ա������
    ����Ա s_instances ���뵽 rootfs �ļ�ϵͳ���ͽṹ�е� fs_supers ָ���˫�������С�*/  
	return get_sb_nodev(fs_type, flags, data, ramfs_fill_super, mnt);
}

/* װ��rootfs�ĳ����� */
static int rootfs_get_sb(struct file_system_type *fs_type,
	int flags, const char *dev_name, void *data, struct vfsmount *mnt)
{
	return get_sb_nodev(fs_type, flags|MS_NOUSER, data, ramfs_fill_super,
			    mnt);
}

/* ж�س����� */ 
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

/* ��ʼ��ģ�飬ע���ļ�ϵͳ */
static int __init init_ramfs_fs(void)
{
	return register_filesystem(&ramfs_fs_type);
}

/* �˳�ģ�飬ע���ļ�ϵͳ */
static void __exit exit_ramfs_fs(void)
{
	unregister_filesystem(&ramfs_fs_type);
}

module_init(init_ramfs_fs)
module_exit(exit_ramfs_fs)

/* ��ʼ��rootfs�ļ�ϵͳ�������������Ĺ����е��� */  
int __init init_rootfs(void)
{
	int err;

    /* ��ʼ����Ӧ�Ŀ��豸 */ 
	err = bdi_init(&ramfs_backing_dev_info);
	if (err)
		return err;

    /* ע��rootfs�ļ�ϵͳ */ 
	err = register_filesystem(&rootfs_fs_type);
	if (err)
		bdi_destroy(&ramfs_backing_dev_info);

	return err;
}

MODULE_LICENSE("GPL");
