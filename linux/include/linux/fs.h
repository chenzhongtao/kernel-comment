#ifndef _LINUX_FS_H
#define _LINUX_FS_H

/*
 * This file has definitions for some important file table
 * structures etc.
 */

#include <linux/limits.h>
#include <linux/ioctl.h>

/*
 * It's silly to have NR_OPEN bigger than NR_FILE, but you can change
 * the file limit at runtime and only root can increase the per-process
 * nr_file rlimit, so it's safe to set up a ridiculously high absolute
 * upper limit on files-per-process.
 *
 * Some programs (notably those using select()) may have to be 
 * recompiled to take full advantage of the new limits..  
 */

/* Fixed constants first: */
#undef NR_OPEN
#define INR_OPEN 1024		/* Initial setting for nfile rlimits */

#define BLOCK_SIZE_BITS 10
#define BLOCK_SIZE (1<<BLOCK_SIZE_BITS)

#define SEEK_SET	0	/* seek relative to beginning of file */
#define SEEK_CUR	1	/* seek relative to current file position */
#define SEEK_END	2	/* seek relative to end of file */
#define SEEK_MAX	SEEK_END

/* And dynamically-tunable limits and defaults: */
struct files_stat_struct {
	int nr_files;		/* read only */
	int nr_free_files;	/* read only */
	int max_files;		/* tunable */
};

struct inodes_stat_t {
	int nr_inodes;
	int nr_unused;
	int dummy[5];		/* padding for sysctl ABI compatibility */
};


#define NR_FILE  8192	/* this can well be larger on a larger system */

#define MAY_EXEC 1
#define MAY_WRITE 2
#define MAY_READ 4
#define MAY_APPEND 8
#define MAY_ACCESS 16
#define MAY_OPEN 32

/*
 * flags in file.f_mode.  Note that FMODE_READ and FMODE_WRITE must correspond
 * to O_WRONLY and O_RDWR via the strange trick in __dentry_open()
 */

/* file is open for reading */
#define FMODE_READ		((__force fmode_t)1)
/* file is open for writing */
#define FMODE_WRITE		((__force fmode_t)2)
/* file is seekable */
#define FMODE_LSEEK		((__force fmode_t)4)
/* file can be accessed using pread */
#define FMODE_PREAD		((__force fmode_t)8)
/* file can be accessed using pwrite */
#define FMODE_PWRITE		((__force fmode_t)16)
/* File is opened for execution with sys_execve / sys_uselib */
#define FMODE_EXEC		((__force fmode_t)32)
/* File is opened with O_NDELAY (only set for block devices) */
#define FMODE_NDELAY		((__force fmode_t)64)
/* File is opened with O_EXCL (only set for block devices) */
#define FMODE_EXCL		((__force fmode_t)128)
/* File is opened using open(.., 3, ..) and is writeable only for ioctls
   (specialy hack for floppy.c) */
#define FMODE_WRITE_IOCTL	((__force fmode_t)256)

/*
 * Don't update ctime and mtime.
 *
 * Currently a special hack for the XFS open_by_handle ioctl, but we'll
 * hopefully graduate it to a proper O_CMTIME flag supported by open(2) soon.
 */
#define FMODE_NOCMTIME		((__force fmode_t)2048)

/*
 * The below are the various read and write types that we support. Some of
 * them include behavioral modifiers that send information down to the
 * block layer and IO scheduler. Terminology:
 *
 *	The block layer uses device plugging to defer IO a little bit, in
 *	the hope that we will see more IO very shortly. This increases
 *	coalescing of adjacent IO and thus reduces the number of IOs we
 *	have to send to the device. It also allows for better queuing,
 *	if the IO isn't mergeable. If the caller is going to be waiting
 *	for the IO, then he must ensure that the device is unplugged so
 *	that the IO is dispatched to the driver.
 *
 *	All IO is handled async in Linux. This is fine for background
 *	writes, but for reads or writes that someone waits for completion
 *	on, we want to notify the block layer and IO scheduler so that they
 *	know about it. That allows them to make better scheduling
 *	decisions. So when the below references 'sync' and 'async', it
 *	is referencing this priority hint.
 *
 * With that in mind, the available types are:
 *
 * READ			A normal read operation. Device will be plugged.
 * READ_SYNC		A synchronous read. Device is not plugged, caller can
 *			immediately wait on this read without caring about
 *			unplugging.
 * READA		Used for read-ahead operations. Lower priority, and the
 *			 block layer could (in theory) choose to ignore this
 *			request if it runs into resource problems.
 * WRITE		A normal async write. Device will be plugged.
 * SWRITE		Like WRITE, but a special case for ll_rw_block() that
 *			tells it to lock the buffer first. Normally a buffer
 *			must be locked before doing IO.
 * WRITE_SYNC_PLUG	Synchronous write. Identical to WRITE, but passes down
 *			the hint that someone will be waiting on this IO
 *			shortly. The device must still be unplugged explicitly,
 *			WRITE_SYNC_PLUG does not do this as we could be
 *			submitting more writes before we actually wait on any
 *			of them.
 * WRITE_SYNC		Like WRITE_SYNC_PLUG, but also unplugs the device
 *			immediately after submission. The write equivalent
 *			of READ_SYNC.
 * WRITE_ODIRECT	Special case write for O_DIRECT only.
 * SWRITE_SYNC
 * SWRITE_SYNC_PLUG	Like WRITE_SYNC/WRITE_SYNC_PLUG, but locks the buffer.
 *			See SWRITE.
 * WRITE_BARRIER	Like WRITE, but tells the block layer that all
 *			previously submitted writes must be safely on storage
 *			before this one is started. Also guarantees that when
 *			this write is complete, it itself is also safely on
 *			storage. Prevents reordering of writes on both sides
 *			of this IO.
 *
 */
#define RW_MASK		1
#define RWA_MASK	2
#define READ 0
#define WRITE 1
#define READA 2		/* read-ahead  - don't block if no resources */
#define SWRITE 3	/* for ll_rw_block() - wait for buffer lock */
#define READ_SYNC	(READ | (1 << BIO_RW_SYNCIO) | (1 << BIO_RW_UNPLUG))
#define READ_META	(READ | (1 << BIO_RW_META))
#define WRITE_SYNC_PLUG	(WRITE | (1 << BIO_RW_SYNCIO) | (1 << BIO_RW_NOIDLE))
#define WRITE_SYNC	(WRITE_SYNC_PLUG | (1 << BIO_RW_UNPLUG))
#define WRITE_ODIRECT	(WRITE | (1 << BIO_RW_SYNCIO) | (1 << BIO_RW_UNPLUG))
#define SWRITE_SYNC_PLUG	\
			(SWRITE | (1 << BIO_RW_SYNCIO) | (1 << BIO_RW_NOIDLE))
#define SWRITE_SYNC	(SWRITE_SYNC_PLUG | (1 << BIO_RW_UNPLUG))
#define WRITE_BARRIER	(WRITE | (1 << BIO_RW_BARRIER))

/*
 * These aren't really reads or writes, they pass down information about
 * parts of device that are now unused by the file system.
 */
#define DISCARD_NOBARRIER (WRITE | (1 << BIO_RW_DISCARD))
#define DISCARD_BARRIER (DISCARD_NOBARRIER | (1 << BIO_RW_BARRIER))

#define SEL_IN		1
#define SEL_OUT		2
#define SEL_EX		4

/* public flags for file_system_type */
/**
 * 这种类型的文件系统必须位于物理磁盘上。
 */
#define FS_REQUIRES_DEV 1
/**
 * 使用二进制安装数据?
 */
#define FS_BINARY_MOUNTDATA 2
#define FS_HAS_SUBTYPE 4
/**
 * 需要检查"."和".."，用于NFS
 */
#define FS_REVAL_DOT	16384	/* Check the paths ".", ".." for staleness */
/**
 * 重命令就是移动，用于NFS
 */
#define FS_RENAME_DOES_D_MOVE	32768	/* FS will handle d_move()
					 * during rename() internally.
					 */

/*
 * These are the fs-independent mount-flags: up to 32 flags are supported
 */
/**
 * 只读mount
 */
#define MS_RDONLY	 1	/* Mount read-only */
/**
 * 禁止setuid和setgid标志。
 */
#define MS_NOSUID	 2	/* Ignore suid and sgid bits */
/**
 * 禁止访问设备文件。
 */
#define MS_NODEV	 4	/* Disallow access to device special files */
/**
 * 不允许文件执行。
 */
#define MS_NOEXEC	 8	/* Disallow program execution */
/**
 * 文件和目录上的写操作是即时的。在mount时指定了sync参数。
 */
#define MS_SYNCHRONOUS	16	/* Writes are synced at once */
/**
 * 重新安装文件系统。
 */
#define MS_REMOUNT	32	/* Alter flags of a mounted FS */
/**
 * 允许强制加锁。
 */
#define MS_MANDLOCK	64	/* Allow mandatory locks on an FS */
/**
 * 目录上的写操作是即时的。
 */
#define MS_DIRSYNC	128	/* Directory modifications are synchronous */
/**
 * 不更新文件访问时间。
 */
#define MS_NOATIME	1024	/* Do not update access times. */
/**
 * 不更新目录访问时间。
 */
#define MS_NODIRATIME	2048	/* Do not update directory access times */
/**
 * 创建一个"绑定安装"。这样一个文件或目录在系统的另外一个点上可以被看见。参见mount命令的__bind选项。
 */
#define MS_BIND		4096
/**
 * 自动把一个已安装文件系统移动到另外一个安装点。参见mount命令的__move选项。
 */
#define MS_MOVE		8192
/**
 * 为目录子树递归地创建"绑定安装"
 */
#define MS_REC		16384
/**
 * 出错时产生详细的内核消息。
 */
#define MS_VERBOSE	32768	/* War is peace. Verbosity is silence.
				   MS_VERBOSE is deprecated. */
#define MS_SILENT	32768
#define MS_POSIXACL	(1<<16)	/* VFS does not apply the umask */
#define MS_UNBINDABLE	(1<<17)	/* change to unbindable */
#define MS_PRIVATE	(1<<18)	/* change to private */
#define MS_SLAVE	(1<<19)	/* change to slave */
#define MS_SHARED	(1<<20)	/* change to shared */
#define MS_RELATIME	(1<<21)	/* Update atime relative to mtime/ctime. */
#define MS_KERNMOUNT	(1<<22) /* this is a kern_mount call */
#define MS_I_VERSION	(1<<23) /* Update inode I_version field */
#define MS_STRICTATIME	(1<<24) /* Always perform atime updates */
#define MS_ACTIVE	(1<<30)
#define MS_NOUSER	(1<<31)

/*
 * Superblock flags that can be altered by MS_REMOUNT
 */
#define MS_RMT_MASK	(MS_RDONLY|MS_SYNCHRONOUS|MS_MANDLOCK|MS_I_VERSION)

/*
 * Old magic mount flag and mask
 */
#define MS_MGC_VAL 0xC0ED0000
#define MS_MGC_MSK 0xffff0000

/* Inode flags - they have nothing to superblock flags now */

#define S_SYNC		1	/* Writes are synced at once */
#define S_NOATIME	2	/* Do not update access times */
#define S_APPEND	4	/* Append-only file */
#define S_IMMUTABLE	8	/* Immutable file */
#define S_DEAD		16	/* removed, but still open directory */
#define S_NOQUOTA	32	/* Inode is not counted to quota */
#define S_DIRSYNC	64	/* Directory modifications are synchronous */
#define S_NOCMTIME	128	/* Do not update file c/mtime */
#define S_SWAPFILE	256	/* Do not truncate: swapon got its bmaps */
#define S_PRIVATE	512	/* Inode is fs-internal */

/*
 * Note that nosuid etc flags are inode-specific: setting some file-system
 * flags just means all the inodes inherit those flags by default. It might be
 * possible to override it selectively if you really wanted to with some
 * ioctl() that is not currently implemented.
 *
 * Exception: MS_RDONLY is always applied to the entire file system.
 *
 * Unfortunately, it is possible to change a filesystems flags with it mounted
 * with files in use.  This means that all of the inodes will not have their
 * i_flags updated.  Hence, i_flags no longer inherit the superblock mount
 * flags, so these have to be checked separately. -- rmk@arm.uk.linux.org
 */
#define __IS_FLG(inode,flg) ((inode)->i_sb->s_flags & (flg))

#define IS_RDONLY(inode) ((inode)->i_sb->s_flags & MS_RDONLY)
#define IS_SYNC(inode)		(__IS_FLG(inode, MS_SYNCHRONOUS) || \
					((inode)->i_flags & S_SYNC))
#define IS_DIRSYNC(inode)	(__IS_FLG(inode, MS_SYNCHRONOUS|MS_DIRSYNC) || \
					((inode)->i_flags & (S_SYNC|S_DIRSYNC)))
#define IS_MANDLOCK(inode)	__IS_FLG(inode, MS_MANDLOCK)
#define IS_NOATIME(inode)   __IS_FLG(inode, MS_RDONLY|MS_NOATIME)
#define IS_I_VERSION(inode)   __IS_FLG(inode, MS_I_VERSION)

#define IS_NOQUOTA(inode)	((inode)->i_flags & S_NOQUOTA)
#define IS_APPEND(inode)	((inode)->i_flags & S_APPEND)
#define IS_IMMUTABLE(inode)	((inode)->i_flags & S_IMMUTABLE)
#define IS_POSIXACL(inode)	__IS_FLG(inode, MS_POSIXACL)

#define IS_DEADDIR(inode)	((inode)->i_flags & S_DEAD)
#define IS_NOCMTIME(inode)	((inode)->i_flags & S_NOCMTIME)
#define IS_SWAPFILE(inode)	((inode)->i_flags & S_SWAPFILE)
#define IS_PRIVATE(inode)	((inode)->i_flags & S_PRIVATE)

/* the read-only stuff doesn't really belong here, but any other place is
   probably as bad and I don't want to create yet another include file. */

#define BLKROSET   _IO(0x12,93)	/* set device read-only (0 = read-write) */
#define BLKROGET   _IO(0x12,94)	/* get read-only status (0 = read_write) */
#define BLKRRPART  _IO(0x12,95)	/* re-read partition table */
#define BLKGETSIZE _IO(0x12,96)	/* return device size /512 (long *arg) */
#define BLKFLSBUF  _IO(0x12,97)	/* flush buffer cache */
#define BLKRASET   _IO(0x12,98)	/* set read ahead for block device */
#define BLKRAGET   _IO(0x12,99)	/* get current read ahead setting */
#define BLKFRASET  _IO(0x12,100)/* set filesystem (mm/filemap.c) read-ahead */
#define BLKFRAGET  _IO(0x12,101)/* get filesystem (mm/filemap.c) read-ahead */
#define BLKSECTSET _IO(0x12,102)/* set max sectors per request (ll_rw_blk.c) */
#define BLKSECTGET _IO(0x12,103)/* get max sectors per request (ll_rw_blk.c) */
#define BLKSSZGET  _IO(0x12,104)/* get block device sector size */
#if 0
#define BLKPG      _IO(0x12,105)/* See blkpg.h */

/* Some people are morons.  Do not use sizeof! */

#define BLKELVGET  _IOR(0x12,106,size_t)/* elevator get */
#define BLKELVSET  _IOW(0x12,107,size_t)/* elevator set */
/* This was here just to show that the number is taken -
   probably all these _IO(0x12,*) ioctls should be moved to blkpg.h. */
#endif
/* A jump here: 108-111 have been used for various private purposes. */
#define BLKBSZGET  _IOR(0x12,112,size_t)
#define BLKBSZSET  _IOW(0x12,113,size_t)
#define BLKGETSIZE64 _IOR(0x12,114,size_t)	/* return device size in bytes (u64 *arg) */
#define BLKTRACESETUP _IOWR(0x12,115,struct blk_user_trace_setup)
#define BLKTRACESTART _IO(0x12,116)
#define BLKTRACESTOP _IO(0x12,117)
#define BLKTRACETEARDOWN _IO(0x12,118)
#define BLKDISCARD _IO(0x12,119)
#define BLKIOMIN _IO(0x12,120)
#define BLKIOOPT _IO(0x12,121)
#define BLKALIGNOFF _IO(0x12,122)
#define BLKPBSZGET _IO(0x12,123)

#define BMAP_IOCTL 1		/* obsolete - kept for compatibility */
#define FIBMAP	   _IO(0x00,1)	/* bmap access */
#define FIGETBSZ   _IO(0x00,2)	/* get the block size used for bmap */
#define FIFREEZE	_IOWR('X', 119, int)	/* Freeze */
#define FITHAW		_IOWR('X', 120, int)	/* Thaw */

#define	FS_IOC_GETFLAGS			_IOR('f', 1, long)
#define	FS_IOC_SETFLAGS			_IOW('f', 2, long)
#define	FS_IOC_GETVERSION		_IOR('v', 1, long)
#define	FS_IOC_SETVERSION		_IOW('v', 2, long)
#define FS_IOC_FIEMAP			_IOWR('f', 11, struct fiemap)
#define FS_IOC32_GETFLAGS		_IOR('f', 1, int)
#define FS_IOC32_SETFLAGS		_IOW('f', 2, int)
#define FS_IOC32_GETVERSION		_IOR('v', 1, int)
#define FS_IOC32_SETVERSION		_IOW('v', 2, int)

/*
 * Inode flags (FS_IOC_GETFLAGS / FS_IOC_SETFLAGS)
 */
#define	FS_SECRM_FL			0x00000001 /* Secure deletion */
#define	FS_UNRM_FL			0x00000002 /* Undelete */
#define	FS_COMPR_FL			0x00000004 /* Compress file */
#define FS_SYNC_FL			0x00000008 /* Synchronous updates */
#define FS_IMMUTABLE_FL			0x00000010 /* Immutable file */
#define FS_APPEND_FL			0x00000020 /* writes to file may only append */
#define FS_NODUMP_FL			0x00000040 /* do not dump file */
#define FS_NOATIME_FL			0x00000080 /* do not update atime */
/* Reserved for compression usage... */
#define FS_DIRTY_FL			0x00000100
#define FS_COMPRBLK_FL			0x00000200 /* One or more compressed clusters */
#define FS_NOCOMP_FL			0x00000400 /* Don't compress */
#define FS_ECOMPR_FL			0x00000800 /* Compression error */
/* End compression flags --- maybe not all used */
#define FS_BTREE_FL			0x00001000 /* btree format dir */
#define FS_INDEX_FL			0x00001000 /* hash-indexed directory */
#define FS_IMAGIC_FL			0x00002000 /* AFS directory */
#define FS_JOURNAL_DATA_FL		0x00004000 /* Reserved for ext3 */
#define FS_NOTAIL_FL			0x00008000 /* file tail should not be merged */
#define FS_DIRSYNC_FL			0x00010000 /* dirsync behaviour (directories only) */
#define FS_TOPDIR_FL			0x00020000 /* Top of directory hierarchies*/
#define FS_EXTENT_FL			0x00080000 /* Extents */
#define FS_DIRECTIO_FL			0x00100000 /* Use direct i/o */
#define FS_RESERVED_FL			0x80000000 /* reserved for ext2 lib */

#define FS_FL_USER_VISIBLE		0x0003DFFF /* User visible flags */
#define FS_FL_USER_MODIFIABLE		0x000380FF /* User modifiable flags */


#define SYNC_FILE_RANGE_WAIT_BEFORE	1
#define SYNC_FILE_RANGE_WRITE		2
#define SYNC_FILE_RANGE_WAIT_AFTER	4

#ifdef __KERNEL__

#include <linux/linkage.h>
#include <linux/wait.h>
#include <linux/types.h>
#include <linux/kdev_t.h>
#include <linux/dcache.h>
#include <linux/path.h>
#include <linux/stat.h>
#include <linux/cache.h>
#include <linux/kobject.h>
#include <linux/list.h>
#include <linux/radix-tree.h>
#include <linux/prio_tree.h>
#include <linux/init.h>
#include <linux/pid.h>
#include <linux/mutex.h>
#include <linux/capability.h>
#include <linux/semaphore.h>
#include <linux/fiemap.h>

#include <asm/atomic.h>
#include <asm/byteorder.h>

struct export_operations;
struct hd_geometry;
struct iovec;
struct nameidata;
struct kiocb;
struct pipe_inode_info;
struct poll_table_struct;
struct kstatfs;
struct vm_area_struct;
struct vfsmount;
struct cred;

extern void __init inode_init(void);
extern void __init inode_init_early(void);
extern void __init files_init(unsigned long);

extern struct files_stat_struct files_stat;
extern int get_max_files(void);
extern int sysctl_nr_open;
extern struct inodes_stat_t inodes_stat;
extern int leases_enable, lease_break_time;
#ifdef CONFIG_DNOTIFY
extern int dir_notify_enable;
#endif

struct buffer_head;
typedef int (get_block_t)(struct inode *inode, sector_t iblock,
			struct buffer_head *bh_result, int create);
typedef void (dio_iodone_t)(struct kiocb *iocb, loff_t offset,
			ssize_t bytes, void *private);

/*
 * Attribute flags.  These should be or-ed together to figure out what
 * has been changed!
 */
#define ATTR_MODE	(1 << 0)
#define ATTR_UID	(1 << 1)
#define ATTR_GID	(1 << 2)
#define ATTR_SIZE	(1 << 3)
#define ATTR_ATIME	(1 << 4)
#define ATTR_MTIME	(1 << 5)
#define ATTR_CTIME	(1 << 6)
#define ATTR_ATIME_SET	(1 << 7)
#define ATTR_MTIME_SET	(1 << 8)
#define ATTR_FORCE	(1 << 9) /* Not a change, but a change it */
#define ATTR_ATTR_FLAG	(1 << 10)
#define ATTR_KILL_SUID	(1 << 11)
#define ATTR_KILL_SGID	(1 << 12)
#define ATTR_FILE	(1 << 13)
#define ATTR_KILL_PRIV	(1 << 14)
#define ATTR_OPEN	(1 << 15) /* Truncating from open(O_TRUNC) */
#define ATTR_TIMES_SET	(1 << 16)

/*
 * This is the Inode Attributes structure, used for notify_change().  It
 * uses the above definitions as flags, to know which values have changed.
 * Also, in this manner, a Filesystem can look at only the values it cares
 * about.  Basically, these are the attributes that the VFS layer can
 * request to change from the FS layer.
 *
 * Derek Atkins <warlord@MIT.EDU> 94-10-20
 */
/**
 * NFS客户端修改服务器端文件属性时用到的结构。
 */
struct iattr {
	unsigned int	ia_valid;
	/**
	 * 文件保护位
	 */
	umode_t		ia_mode;
	/**
	 * 文件拥有者的id
	 */
	uid_t		ia_uid;
	/**
	 * 文件所属的组
	 */
	gid_t		ia_gid;
	/**
	 * 文件大小
	 */
	loff_t		ia_size;
	/**
	 * 文件的最后访问时间
	 */
	struct timespec	ia_atime;
	/**
	 * 文件的最后修改时间
	 */
	struct timespec	ia_mtime;
	struct timespec	ia_ctime;

	/*
	 * Not an attribute, but an auxilary info for filesystems wanting to
	 * implement an ftruncate() like method.  NOTE: filesystem should
	 * check for (ia_valid & ATTR_FILE), and not for (ia_file != NULL).
	 */
	struct file	*ia_file;
};

/*
 * Includes for diskquotas.
 */
#include <linux/quota.h>

/** 
 * enum positive_aop_returns - aop return codes with specific semantics
 *
 * @AOP_WRITEPAGE_ACTIVATE: Informs the caller that page writeback has
 * 			    completed, that the page is still locked, and
 * 			    should be considered active.  The VM uses this hint
 * 			    to return the page to the active list -- it won't
 * 			    be a candidate for writeback again in the near
 * 			    future.  Other callers must be careful to unlock
 * 			    the page if they get this return.  Returned by
 * 			    writepage(); 
 *
 * @AOP_TRUNCATED_PAGE: The AOP method that was handed a locked page has
 *  			unlocked it and the page might have been truncated.
 *  			The caller should back up to acquiring a new page and
 *  			trying again.  The aop will be taking reasonable
 *  			precautions not to livelock.  If the caller held a page
 *  			reference, it should drop it before retrying.  Returned
 *  			by readpage().
 *
 * address_space_operation functions return these large constants to indicate
 * special semantics to the caller.  These are much larger than the bytes in a
 * page to allow for functions that return the number of bytes operated on in a
 * given page.
 */

enum positive_aop_returns {
	AOP_WRITEPAGE_ACTIVATE	= 0x80000,
	AOP_TRUNCATED_PAGE	= 0x80001,
};

#define AOP_FLAG_UNINTERRUPTIBLE	0x0001 /* will not do a short write */
#define AOP_FLAG_CONT_EXPAND		0x0002 /* called from cont_expand */
#define AOP_FLAG_NOFS			0x0004 /* used by filesystem to direct
						* helper code (eg buffer layer)
						* to clear GFP_FS from alloc */

/*
 * oh the beauties of C type declarations.
 */
struct page;
struct address_space;
struct writeback_control;

struct iov_iter {
	const struct iovec *iov;
	unsigned long nr_segs;
	size_t iov_offset;
	size_t count;
};

size_t iov_iter_copy_from_user_atomic(struct page *page,
		struct iov_iter *i, unsigned long offset, size_t bytes);
size_t iov_iter_copy_from_user(struct page *page,
		struct iov_iter *i, unsigned long offset, size_t bytes);
void iov_iter_advance(struct iov_iter *i, size_t bytes);
int iov_iter_fault_in_readable(struct iov_iter *i, size_t bytes);
size_t iov_iter_single_seg_count(struct iov_iter *i);

static inline void iov_iter_init(struct iov_iter *i,
			const struct iovec *iov, unsigned long nr_segs,
			size_t count, size_t written)
{
	i->iov = iov;
	i->nr_segs = nr_segs;
	i->iov_offset = 0;
	i->count = count + written;

	iov_iter_advance(i, written);
}

static inline size_t iov_iter_count(struct iov_iter *i)
{
	return i->count;
}

/*
 * "descriptor" for what we're up to with a read.
 * This allows us to use the same read code yet
 * have multiple different users of the data that
 * we read from a file.
 *
 * The simplest case just copies the data to user
 * mode.
 */
typedef struct {
	/**
	 * 已经拷贝到用户态缓冲区的字节数
	 */	
	size_t written;
	size_t count;
	/**
	 * 在用户态缓冲区中的当前位置
	 */
	union {
		char __user *buf;
		void *data;
	} arg;
	/**
	 * 读操作的错误码。0表示无错误。
	 */
	int error;
} read_descriptor_t;

typedef int (*read_actor_t)(read_descriptor_t *, struct page *,
		unsigned long, unsigned long);
/**
 * 与某个文件映射相关的地址空间回调函数
 */
struct address_space_operations {
	/* 回写一个页面到文件中，标准实现是block_write_full_page */
	int (*writepage)(struct page *page, struct writeback_control *wbc);
	/* 从文件中读取一页到物理内存中，标准函数是mpage_readpage */
	int (*readpage)(struct file *, struct page *);
	/* 将页面回写到设备，它拨出设备，以强制回写到设备，标准函数是block_sync_page */
	void (*sync_page)(struct page *);

	/* Write back some dirty pages from this mapping. */
	/**
	 * 把指定数量的所有者脏页写回磁盘
	 */
	int (*writepages)(struct address_space *, struct writeback_control *);

	/* Set a page dirty.  Return true if this dirtied it */
	/**
	 * 表示一页的内容已经改变，即与块设备上的原始内容不再匹配
	 * 一般不设置，自动调用__set_page_dirty_buffers，将页在缓冲区层次上标记为脏，同时在基树中置为脏
	 */
	int (*set_page_dirty)(struct page *page);

	/* 从文件中读取一组页面到物理内存中 */
	int (*readpages)(struct file *filp, struct address_space *mapping,
			struct list_head *pages, unsigned nr_pages);
	/**
	 * 为写操作做准备（由磁盘文件系统使用）
	 */
	int (*write_begin)(struct file *, struct address_space *mapping,
				loff_t pos, unsigned len, unsigned flags,
				struct page **pagep, void **fsdata);
	/**
	 * 完成写操作（由磁盘文件系统使用）
	 */
	int (*write_end)(struct file *, struct address_space *mapping,
				loff_t pos, unsigned len, unsigned copied,
				struct page *page, void *fsdata);

	/* Unfortunately this kludge is needed for FIBMAP. Don't use it */
	/* 将逻辑块映射到物理块号 */
	sector_t (*bmap)(struct address_space *, sector_t);
	/* 当要移除页时，并且有PG_private标志表示有缓冲区与页关联，那么会调用invalidatepage使之无效 */
	void (*invalidatepage) (struct page *, unsigned long);
	/* 用于日志文件系统，释放页 */
	int (*releasepage) (struct page *, gfp_t);
	/* 绕过块层，直接与块设备通信进行读写 */
	ssize_t (*direct_IO)(int, struct kiocb *, const struct iovec *iov,
			loff_t offset, unsigned long nr_segs);
    /* 用于就地执行机制，当文件系统是RAM或者ROM文件系统时，不必使用缓存 */
	int (*get_xip_mem)(struct address_space *, pgoff_t, int,
						void **, unsigned long *);
	/* migrate the contents of a page to the specified target */
	/* 用于内存迁移过程 */
	int (*migratepage) (struct address_space *,
			struct page *, struct page *);
	/* 在释放页之前，回写脏页的最后机会 */
	int (*launder_page) (struct page *);
	int (*is_partially_uptodate) (struct page *, read_descriptor_t *,
					unsigned long);
	int (*error_remove_page)(struct address_space *, struct page *);
};

/*
 * pagecache_write_begin/pagecache_write_end must be used by general code
 * to write into the pagecache.
 */
int pagecache_write_begin(struct file *, struct address_space *mapping,
				loff_t pos, unsigned len, unsigned flags,
				struct page **pagep, void **fsdata);

int pagecache_write_end(struct file *, struct address_space *mapping,
				loff_t pos, unsigned len, unsigned copied,
				struct page *page, void *fsdata);

struct backing_dev_info;
/**
 * 页高速缓存的核心数据结构
 * 它是一个嵌入在页所有者的索引结点对象中的数据结构
 * (页被换出可能会引起缺页异常，这些被换出的页拥有在不在任何索引结点中的公共address_space对象中)
 * 高速缓存中中的许多页可能属于同一个所有者，从而可能被链接到同一个address_space对象中。
 * 该对象还在所有者的页和对这些页的操作之间建立起链接关系。
 */
struct address_space {
	/**
	 * 如果存在，就指向拥有该对象的索引结点的指针。
	 */
	struct inode		*host;		/* owner: inode, block_device */
	/**
	 * 拥有者的页的基树
	 */
	struct radix_tree_root	page_tree;	/* radix tree of all pages */
	/**
	 * 保护基数的自旋锁
	 */
	spinlock_t		tree_lock;	/* and lock protecting it */
	/**
	 * 地址空间中共享内存映射的个数。
	 */
	unsigned int		i_mmap_writable;/* count VM_SHARED mappings */
	/**
	 * radix优先搜索树的根，用于映射页(如共享程序文件、共享C库)的反向映射。
	 */
	struct prio_tree_root	i_mmap;		/* tree of private and shared mappings */
	/**
	 * 地址空间中非线性内存区的链表
	 */
	struct list_head	i_mmap_nonlinear;/*list VM_NONLINEAR mappings */
	/**
	 * 保护radix优先搜索树的自旋锁。
	 */
	spinlock_t		i_mmap_lock;	/* protect tree, count, list */
	/**
	 * 截断文件时使用的顺序计数器。
	 */
	unsigned int		truncate_count;	/* Cover race condition with truncate */
	/**
	 * 所有者的页总数。
	 */
	unsigned long		nrpages;	/* number of total pages */
	/**
	 * 最后一次回写操作所作用的页的索引
	 */
	pgoff_t			writeback_index;/* writeback starts here */
	/**
	 * 对所有者页进行操作的方法
	 */
	const struct address_space_operations *a_ops;	/* methods */
	/**
	 * 错误位和内存分配器的标志
	 */
	unsigned long		flags;		/* error bits/gfp mask */
	/**
	 * 指向拥有所有者数据的块设备的backing_dev_info指针
	 */
	struct backing_dev_info *backing_dev_info; /* device readahead, etc */
	/**
	 * 通常是管理private_list链表时使用的自旋锁
	 * 以下几个字段可以由各文件系统自行使用。
	 */
	spinlock_t		private_lock;	/* for use by the address_space */
	/**
	 * 通常是与索引结点相关的间接块的脏缓冲区的链表
	 */
	struct list_head	private_list;	/* ditto */
	/**
	 * 通常是指向间接块所在块设备的address_space对象的指针
	 */
	struct address_space	*assoc_mapping;	/* ditto */
} __attribute__((aligned(sizeof(long))));
	/*
	 * On most architectures that alignment is already the case; but
	 * must be enforced here for CRIS, to let the least signficant bit
	 * of struct page's "mapping" pointer be used for PAGE_MAPPING_ANON.
	 */
/**
 * 一个块设备驱动程序可以处理几个块设备.
 * 例如：一个IDE驱动程序可以处理几个IDE磁盘。其中的每个都是一个单独的块设备。
 * 并且，每个磁盘都可以被分区。每个分区又可以被看成是一个逻辑设备。
 * 每个块设备都都是由block_device定义的。
 */
struct block_device {
	/**
	 * 块设备的主设备号和次设备号
	 */
	dev_t			bd_dev;  /* not a kdev_t - it's a search key */
	/**
	 * 指向bdev文件系统中块设备对应的文件索引结点的指针。
	 */
	struct inode *		bd_inode;	/* will die */
	struct super_block *	bd_super;
	/**
	 * 计数器，统计设备已经被打开了多少次
	 */
	int			bd_openers;
	/* 同步对块设备的打开、关闭操作 */
	struct mutex		bd_mutex;	/* open/close mutex */
	struct list_head	bd_inodes;
	/**
	 * 块设备描述符的当前所有者
	 */
	void *			bd_holder;
	/**
	 * 计数器，统计对bd_holder字段多次设置的次数。
	 */
	int			bd_holders;
#ifdef CONFIG_SYSFS
	struct list_head	bd_holder_list;
#endif
	/**
	 * 如果设备是一个分区。则指向整个磁盘的块设备描述符。
	 * 否则，指向该块设备描述符
	 */
	struct block_device *	bd_contains;
	/**
	 * 块大小
	 */
	unsigned		bd_block_size;
	/**
	 * 指向分区描述符的指针（如果块设备不是分区，则为NULL）
	 */
	struct hd_struct *	bd_part;
	/* number of times partitions within this device have been opened. */
	/**
	 * 计数器，统计包含在块设备中的分区已经被打开了多少次
	 */
	unsigned		bd_part_count;
	/**
	 * 当需要读块设备的分区表时设置的标志
	 */
	int			bd_invalidated;
	/**
	 * 指向块设备中基本磁盘的gendisk结构的指针
	 */
	struct gendisk *	bd_disk;
	/**
	 * 用于块设备描述符链表的指针
	 */
	struct list_head	bd_list;
	/**
	 * 指向块设备的专门描述符（通常为NULL）
	 */
	struct backing_dev_info *bd_inode_backing_dev_info;
	/*
	 * Private data.  You must have bd_claim'ed the block_device
	 * to use this.  NOTE:  bd_claim allows an owner to claim
	 * the same device multiple times, the owner must take special
	 * care to not mess up bd_private for that case.
	 */
	/**
	 * 块设备持有者的私有数据指针
	 */
	unsigned long		bd_private;

	/* The counter of freeze processes */
	int			bd_fsfreeze_count;
	/* Mutex for freeze */
	struct mutex		bd_fsfreeze_mutex;
};

/*
 * Radix-tree tags, for tagging dirty and writeback pages within the pagecache
 * radix trees
 */
#define PAGECACHE_TAG_DIRTY	0
#define PAGECACHE_TAG_WRITEBACK	1

int mapping_tagged(struct address_space *mapping, int tag);

/*
 * Might pages of this file be mapped into userspace?
 */
static inline int mapping_mapped(struct address_space *mapping)
{
	return	!prio_tree_empty(&mapping->i_mmap) ||
		!list_empty(&mapping->i_mmap_nonlinear);
}

/*
 * Might pages of this file have been modified in userspace?
 * Note that i_mmap_writable counts all VM_SHARED vmas: do_mmap_pgoff
 * marks vma as VM_SHARED if it is shared, and the file was opened for
 * writing i.e. vma may be mprotected writable even if now readonly.
 */
static inline int mapping_writably_mapped(struct address_space *mapping)
{
	return mapping->i_mmap_writable != 0;
}

/*
 * Use sequence counter to get consistent i_size on 32-bit processors.
 */
#if BITS_PER_LONG==32 && defined(CONFIG_SMP)
#include <linux/seqlock.h>
#define __NEED_I_SIZE_ORDERED
#define i_size_ordered_init(inode) seqcount_init(&inode->i_size_seqcount)
#else
#define i_size_ordered_init(inode) do { } while (0)
#endif

struct posix_acl;
#define ACL_NOT_CACHED ((void *)(-1))

/**
 * 内核用该结构在内部表示一个文件。它与file不同。file表示的的文件描述符。
 * 对单个文件，可能会有许多个表示打开的文件描述符的filep结构，但是它们都指向单个inode结构。
 */
struct inode {
	/**
	 * 通过此字段将inode添加到inode_hashtable哈希表中。 这样可以通过inode编号和超级块来快速定位inode。
	 * 可用于NFS。
	 */
	struct hlist_node	i_hash;
	/**
	 * 通过此字段将队列链入不同状态的链表中。
	 * 根据inode的使用状态存在于以下三个链表中的某个链表中:
     *  1. 未用的: inode_unused 链表
     *  2. 正在使用的: inode_in_use 链表
     *  3. 脏的: super block中的s_dirty 链表
	 */
	struct list_head	i_list;		/* backing dev IO list */
	/**
	 * 通过此字段将其链入到超级块的inode链表中。
	 * inode所在文件系统的super block的 s_inodes 链表中
	 */
	struct list_head	i_sb_list;
	/**
	 * 引用索引节点的目录项对象链表头。
	 */
	struct list_head	i_dentry;
	/**
	 * 索引节点编号。
	 */
	unsigned long		i_ino;
	/**
	 * 引用计数器。
	 */
	atomic_t		i_count;
	/**
	 * 硬链接数目。
	 */
	unsigned int		i_nlink;
	/**
	 * 所有者ID
	 */
	uid_t			i_uid;
	/**
	 * 所有者组标识符。
	 */
	gid_t			i_gid;
	/**
	 * 对表示设备文件的inode结构，该字段包含了真正的设备编号。
	 */
	dev_t			i_rdev;
	u64			i_version;
	/**
	 * 文件的字节数。
	 */
	loff_t			i_size;
#ifdef __NEED_I_SIZE_ORDERED
	seqcount_t		i_size_seqcount;
#endif
	/**
	 * 上次访问文件的时间。
	 */
	struct timespec		i_atime;
	/**
	 * 上次与文件的时间。
	 */
	struct timespec		i_mtime;
	/**
	 * 上次修改索引节点的时间。
	 */
	struct timespec		i_ctime;
	/* 文件长度，以设备块为单位 */
	blkcnt_t		i_blocks;
	/**
	 * 块的位数。
	 */
	unsigned int		i_blkbits;
	/**
	 * 文件最后一个块的字节数。
	 */
	unsigned short          i_bytes;
	/* 文件类型，面向块还是面向字符。文件访问权限。 */
	umode_t			i_mode;
	/**
	 * 保护索引节点某些字段的自旋锁。
	 */
	spinlock_t		i_lock;	/* i_blocks, i_bytes, maybe i_size */
	struct mutex		i_mutex;
	/**
	 * 在直接IO文件操作中避免出现竞争条件的读写信号量。
	 */
	struct rw_semaphore	i_alloc_sem;
	/**
	 * 索引节点的操作。
	 */
	const struct inode_operations	*i_op;
	/**
	 * 缺省文件操作。
	 */
	const struct file_operations	*i_fop;	/* former ->i_op->default_file_ops */
	/**
	 * inode所在的超级块。
	 */
	struct super_block	*i_sb;
	/**
	 * 文件锁链表,通过此字段将文件上的所有锁链接成一个单链表。
	 */
	struct file_lock	*i_flock;
	/**
	 * 指向address_space对象的指针。
	 */
	struct address_space	*i_mapping;
	/**
	 * 文件的address_space对象。
	 */
	struct address_space	i_data;
#ifdef CONFIG_QUOTA
	/**
	 * 索引节点的磁盘限额。
	 */
	struct dquot		*i_dquot[MAXQUOTAS];
#endif
	/**
	 * 用于具体的字符或块设备的索引节点链表指针。
	 */
	struct list_head	i_devices;
	/* 指向文件所在的设备结构 */
	union {
		/**
    	 * 如果内核是一个管道则非0
    	 */
		struct pipe_inode_info	*i_pipe;
        /**
         * 指向块设备驱动程序的指针。
         */
		struct block_device	*i_bdev;
        /**
         * 表示字符设备的内部数据结构。当inode指向一个字符设备文件时，该字段包含了指向struct cdev结构的指针。
         */
		struct cdev		*i_cdev;
	};

	/**
	 * 索引节点版本号。由某些文件系统使用。
	 */
	__u32			i_generation;

#ifdef CONFIG_FSNOTIFY
	__u32			i_fsnotify_mask; /* all events this inode cares about */
	struct hlist_head	i_fsnotify_mark_entries; /* fsnotify mark entries */
#endif

#ifdef CONFIG_INOTIFY
	struct list_head	inotify_watches; /* watches on this inode */
	struct mutex		inotify_mutex;	/* protects the watches list */
#endif

	/**
	 * 索引节点状态标志。
	 */
	unsigned long		i_state;
	/**
	 * 索引节点弄脏的时间，以jiffies为单位。
	 */
	unsigned long		dirtied_when;	/* jiffies of first dirtying */

	/**
	 * 文件系统的安装标志。
	 */
	unsigned int		i_flags;

	/**
	 * 用于写进程的引用计数。
	 */
	atomic_t		i_writecount;
#ifdef CONFIG_SECURITY
	/**
	 * 索引节点安全结构。
	 */
	void			*i_security;
#endif
#ifdef CONFIG_FS_POSIX_ACL
	struct posix_acl	*i_acl;
	struct posix_acl	*i_default_acl;
#endif
	/* 文件系统私有数据结构，如ext2_inode_info */
	void			*i_private; /* fs or device private pointer */
};

/*
 * inode->i_mutex nesting subclasses for the lock validator:
 *
 * 0: the object of the current VFS operation
 * 1: parent
 * 2: child/target
 * 3: quota file
 *
 * The locking order between these classes is
 * parent -> child -> normal -> xattr -> quota
 */
enum inode_i_mutex_lock_class
{
	I_MUTEX_NORMAL,
	I_MUTEX_PARENT,
	I_MUTEX_CHILD,
	I_MUTEX_XATTR,
	I_MUTEX_QUOTA
};

/*
 * NOTE: in a 32bit arch with a preemptable kernel and
 * an UP compile the i_size_read/write must be atomic
 * with respect to the local cpu (unlike with preempt disabled),
 * but they don't need to be atomic with respect to other cpus like in
 * true SMP (so they need either to either locally disable irq around
 * the read or for example on x86 they can be still implemented as a
 * cmpxchg8b without the need of the lock prefix). For SMP compiles
 * and 64bit archs it makes no difference if preempt is enabled or not.
 */
static inline loff_t i_size_read(const struct inode *inode)
{
#if BITS_PER_LONG==32 && defined(CONFIG_SMP)
	loff_t i_size;
	unsigned int seq;

	do {
		seq = read_seqcount_begin(&inode->i_size_seqcount);
		i_size = inode->i_size;
	} while (read_seqcount_retry(&inode->i_size_seqcount, seq));
	return i_size;
#elif BITS_PER_LONG==32 && defined(CONFIG_PREEMPT)
	loff_t i_size;

	preempt_disable();
	i_size = inode->i_size;
	preempt_enable();
	return i_size;
#else
	return inode->i_size;
#endif
}

/*
 * NOTE: unlike i_size_read(), i_size_write() does need locking around it
 * (normally i_mutex), otherwise on 32bit/SMP an update of i_size_seqcount
 * can be lost, resulting in subsequent i_size_read() calls spinning forever.
 */
static inline void i_size_write(struct inode *inode, loff_t i_size)
{
#if BITS_PER_LONG==32 && defined(CONFIG_SMP)
	write_seqcount_begin(&inode->i_size_seqcount);
	inode->i_size = i_size;
	write_seqcount_end(&inode->i_size_seqcount);
#elif BITS_PER_LONG==32 && defined(CONFIG_PREEMPT)
	preempt_disable();
	inode->i_size = i_size;
	preempt_enable();
#else
	inode->i_size = i_size;
#endif
}

static inline unsigned iminor(const struct inode *inode)
{
	return MINOR(inode->i_rdev);
}

static inline unsigned imajor(const struct inode *inode)
{
	return MAJOR(inode->i_rdev);
}

extern struct block_device *I_BDEV(struct inode *inode);

struct fown_struct {
	rwlock_t lock;          /* protects pid, uid, euid fields */
	struct pid *pid;	/* pid or -pgrp where SIGIO should be sent */
	enum pid_type pid_type;	/* Kind of process group SIGIO should be sent to */
	uid_t uid, euid;	/* uid/euid of process setting the owner */
	int signum;		/* posix.1b rt signal to be delivered on IO */
};

/*
 * Track a single file's readahead state
 */
/**
 * 预读算法使用的主要数据结构.每个文件对象在它的f_ra字段中存放该描述符。
 */
struct file_ra_state {
	/**
	 * 当前窗内第一页的索引。
	 */
	pgoff_t start;			/* where readahead started */
	/**
	 * 当前窗内的页数。禁止预读时为－1，0表示当前窗为空
	 */
	unsigned int size;		/* # of readahead pages */
	unsigned int async_size;	/* do asynchronous readahead when
					   there are only # of pages ahead */

	unsigned int ra_pages;		/* Maximum readahead window */
	unsigned int mmap_miss;		/* Cache miss stat for mmap accesses */
	loff_t prev_pos;		/* Cache last read() position */
};

/*
 * Check if @index falls in the readahead windows.
 */
static inline int ra_has_index(struct file_ra_state *ra, pgoff_t index)
{
	return (index >= ra->start &&
		index <  ra->start + ra->size);
}

#define FILE_MNT_WRITE_TAKEN	1
#define FILE_MNT_WRITE_RELEASED	2

/**
 * 代表一个打开的文件。由内核在open时创建。当文件的所有实例都被关闭后，才释放该结构。
 */
struct file {
	/*
	 * fu_list becomes invalid after file_free is called and queued via
	 * fu_rcuhead for RCU freeing
	 */
	union {
		/**
    	 * 用于通用文件对象链表的指针。
    	 */
		struct list_head	fu_list;
		struct rcu_head 	fu_rcuhead;
	} f_u;
	/* 文件名和inode之间的关联，文件系统信息 */
	struct path		f_path;
	/**
	 * 文件对应的目录项结构。除了用filp->f_dentry->d_inode的方式来访问索引节点结构之外，设备驱动程序的开发者们一般无需关心dentry结构。
	 */
#define f_dentry	f_path.dentry
	/**
	 * 含有该文件的已经安装的文件系统。
	 */
#define f_vfsmnt	f_path.mnt
	/**
	 * 与文件相关的操作。内核在执行open操作时，对这个指针赋值，以后需要处理这些操作时就读取这个指针。
	 * 不能为了方便而保存起来。也就是说，可以在任何需要的时候修改文件的关联操作。即"方法重载"。
	 */
	const struct file_operations	*f_op;
	spinlock_t		f_lock;  /* f_ep_links, f_flags, no IRQ */
	/**
	 * 文件对象的引用计数。
	 * 指引用文件对象的进程数。内核也可能增加此计数。
	 */
	atomic_long_t		f_count;
	/**
	 * 文件标志。如O_RONLY、O_NONBLOCK和O_SYNC。为了检查用户请求是否非阻塞式的操作，驱动程序需要检查O_NONBLOCK标志，其他标志较少用到。
	 * 检查读写权限应该查看f_mode而不是f_flags。
	 */
	unsigned int 		f_flags;
	/**
	 * 文件模式。FMODE_READ和FMODE_WRITE分别表示读写权限。
	 */
	fmode_t			f_mode;
	/**
	 * 当前的读写位置。它是一个64位数。如果驱动程序需要知道文件中的当前位置，可以读取这个值但是不要去修改它。
	 * read/write会使用它们接收到的最后那个指针参数来更新这一位置。
	 */
	loff_t			f_pos;
	/**
	 * 通过信号进行邋IO进程通知的数据。
	 */
	struct fown_struct	f_owner;
	const struct cred	*f_cred;
	/**
	 * 文件的预读状态。
	 */
	struct file_ra_state	f_ra;

	/* 用于检查file实例是否仍然与inode兼容。用于确保缓存一致性。 */
	u64			f_version;
#ifdef CONFIG_SECURITY
	/**
	 * 文件对象的安全结构指针。
	 */
	void			*f_security;
#endif
	/* needed for tty driver, and maybe others */
	/**
	 * open系统调用在调用驱动程序的open方法前将这个指针置为NULL。驱动程序可以将这个字段用于任何目的或者忽略这个字段。
	 * 驱动程序可以用这个字段指向已分配的数据，但是一定要在内核销毁file结构前在release方法中释放内存。
	 * 它是跨系统调用时保存状态的非常有用的资源。
	 */
	void			*private_data;

#ifdef CONFIG_EPOLL
	/* Used by fs/eventpoll.c to link all the hooks to this file */
	/**
	 * 文件的事件轮询等待者链表头。
	 */
	struct list_head	f_ep_links;
#endif /* #ifdef CONFIG_EPOLL */
	/**
	 * 指向文件地址空间的对象。
	 */
	struct address_space	*f_mapping;
#ifdef CONFIG_DEBUG_WRITECOUNT
	unsigned long f_mnt_write_state;
#endif
};
extern spinlock_t files_lock;
#define file_list_lock() spin_lock(&files_lock);
#define file_list_unlock() spin_unlock(&files_lock);

#define get_file(x)	atomic_long_inc(&(x)->f_count)
#define file_count(x)	atomic_long_read(&(x)->f_count)

#ifdef CONFIG_DEBUG_WRITECOUNT
static inline void file_take_write(struct file *f)
{
	WARN_ON(f->f_mnt_write_state != 0);
	f->f_mnt_write_state = FILE_MNT_WRITE_TAKEN;
}
static inline void file_release_write(struct file *f)
{
	f->f_mnt_write_state |= FILE_MNT_WRITE_RELEASED;
}
static inline void file_reset_write(struct file *f)
{
	f->f_mnt_write_state = 0;
}
static inline void file_check_state(struct file *f)
{
	/*
	 * At this point, either both or neither of these bits
	 * should be set.
	 */
	WARN_ON(f->f_mnt_write_state == FILE_MNT_WRITE_TAKEN);
	WARN_ON(f->f_mnt_write_state == FILE_MNT_WRITE_RELEASED);
}
static inline int file_check_writeable(struct file *f)
{
	if (f->f_mnt_write_state == FILE_MNT_WRITE_TAKEN)
		return 0;
	printk(KERN_WARNING "writeable file with no "
			    "mnt_want_write()\n");
	WARN_ON(1);
	return -EINVAL;
}
#else /* !CONFIG_DEBUG_WRITECOUNT */
static inline void file_take_write(struct file *filp) {}
static inline void file_release_write(struct file *filp) {}
static inline void file_reset_write(struct file *filp) {}
static inline void file_check_state(struct file *filp) {}
static inline int file_check_writeable(struct file *filp)
{
	return 0;
}
#endif /* CONFIG_DEBUG_WRITECOUNT */

#define	MAX_NON_LFS	((1UL<<31) - 1)

/* Page cache limit. The filesystems should put that into their s_maxbytes 
   limits, otherwise bad things can happen in VM. */ 
#if BITS_PER_LONG==32
#define MAX_LFS_FILESIZE	(((u64)PAGE_CACHE_SIZE << (BITS_PER_LONG-1))-1) 
#elif BITS_PER_LONG==64
#define MAX_LFS_FILESIZE 	0x7fffffffffffffffUL
#endif

#define FL_POSIX	1
#define FL_FLOCK	2
#define FL_ACCESS	8	/* not trying to lock, just looking */
#define FL_EXISTS	16	/* when unlocking, test for existence */
#define FL_LEASE	32	/* lease held on this file */
#define FL_CLOSE	64	/* unlock on close */
#define FL_SLEEP	128	/* A blocking lock */

/*
 * Special return value from posix_lock_file() and vfs_lock_file() for
 * asynchronous locking.
 */
#define FILE_LOCK_DEFERRED 1

/*
 * The POSIX file lock owner is determined by
 * the "struct files_struct" in the thread group
 * (or NULL for no owner - BSD locks).
 *
 * Lockd stuffs a "host" pointer into this.
 */
typedef struct files_struct *fl_owner_t;

struct file_lock_operations {
	void (*fl_copy_lock)(struct file_lock *, struct file_lock *);
	void (*fl_release_private)(struct file_lock *);
};

struct lock_manager_operations {
	int (*fl_compare_owner)(struct file_lock *, struct file_lock *);
	void (*fl_notify)(struct file_lock *);	/* unblock callback */
	int (*fl_grant)(struct file_lock *, struct file_lock *, int);
	void (*fl_copy_lock)(struct file_lock *, struct file_lock *);
	void (*fl_release_private)(struct file_lock *);
	void (*fl_break)(struct file_lock *);
	int (*fl_mylease)(struct file_lock *, struct file_lock *);
	int (*fl_change)(struct file_lock **, int);
};

struct lock_manager {
	struct list_head list;
};

void locks_start_grace(struct lock_manager *);
void locks_end_grace(struct lock_manager *);
int locks_in_grace(void);

/* that will die - we need it for nfs_lock_info */
#include <linux/nfs_fs_i.h>

/**
 * 文件锁
 */
struct file_lock {
	/**
	 * 文件中的下一个锁。
	 */
	struct file_lock *fl_next;	/* singly linked list for this inode  */
	/**
	 * 将锁加到活动或者阻塞链表。
	 */
	struct list_head fl_link;	/* doubly linked list of all locks */
	/**
	 * 等待者链表。
	 */
	struct list_head fl_block;	/* circular list of blocked processes */
	/**
	 * 文件所有者的files_struct
	 */
	fl_owner_t fl_owner;
	unsigned char fl_flags;
	unsigned char fl_type;
	/**
	 * 拥有者的PID
	 */
	unsigned int fl_pid;
	struct pid *fl_nspid;
	unsigned int fl_pid;
	/**
	 * 阻塞进程的等待队列，所有等待解锁的进程在此队列中。
	 */
	wait_queue_head_t fl_wait;
	/**
	 * 指向文件对象的指针。
	 */
	struct file *fl_file;
	/**
	 * 被锁区域的起始位置
	 */
	loff_t fl_start;
	/**
	 * 被锁区域的结束位置。
	 */
	loff_t fl_end;

	/**
	 * 用于租借锁中断通知。
	 */
	struct fasync_struct *	fl_fasync; /* for lease break notifications */
	/**
	 * 租借结束前的剩余时间
	 */
	unsigned long fl_break_time;	/* for nonblocking lease breaks */

	/**
	 * 文件锁操作指针
	 */
	const struct file_lock_operations *fl_ops;	/* Callbacks for filesystems */
	/**
	 * 锁管理操作指针。
	 */
	const struct lock_manager_operations *fl_lmops;	/* Callbacks for lockmanagers */
	/**
	 * 具体文件系统的信息。NFS使用。
	 */
	union {
		struct nfs_lock_info	nfs_fl;
		struct nfs4_lock_info	nfs4_fl;
		struct {
			struct list_head link;	/* link in AFS vnode's pending_locks list */
			int state;		/* state of grant or error if -ve */
		} afs;
	} fl_u;
};

/* The following constant reflects the upper bound of the file/locking space */
#ifndef OFFSET_MAX
#define INT_LIMIT(x)	(~((x)1 << (sizeof(x)*8 - 1)))
#define OFFSET_MAX	INT_LIMIT(loff_t)
#define OFFT_OFFSET_MAX	INT_LIMIT(off_t)
#endif

#include <linux/fcntl.h>

extern void send_sigio(struct fown_struct *fown, int fd, int band);

/* fs/sync.c */
extern int do_sync_mapping_range(struct address_space *mapping, loff_t offset,
			loff_t endbyte, unsigned int flags);

#ifdef CONFIG_FILE_LOCKING
extern int fcntl_getlk(struct file *, struct flock __user *);
extern int fcntl_setlk(unsigned int, struct file *, unsigned int,
			struct flock __user *);

#if BITS_PER_LONG == 32
extern int fcntl_getlk64(struct file *, struct flock64 __user *);
extern int fcntl_setlk64(unsigned int, struct file *, unsigned int,
			struct flock64 __user *);
#endif

extern int fcntl_setlease(unsigned int fd, struct file *filp, long arg);
extern int fcntl_getlease(struct file *filp);

/* fs/locks.c */
extern void locks_init_lock(struct file_lock *);
extern void locks_copy_lock(struct file_lock *, struct file_lock *);
extern void __locks_copy_lock(struct file_lock *, const struct file_lock *);
extern void locks_remove_posix(struct file *, fl_owner_t);
extern void locks_remove_flock(struct file *);
extern void locks_release_private(struct file_lock *);
extern void posix_test_lock(struct file *, struct file_lock *);
extern int posix_lock_file(struct file *, struct file_lock *, struct file_lock *);
extern int posix_lock_file_wait(struct file *, struct file_lock *);
extern int posix_unblock_lock(struct file *, struct file_lock *);
extern int vfs_test_lock(struct file *, struct file_lock *);
extern int vfs_lock_file(struct file *, unsigned int, struct file_lock *, struct file_lock *);
extern int vfs_cancel_lock(struct file *filp, struct file_lock *fl);
extern int flock_lock_file_wait(struct file *filp, struct file_lock *fl);
extern int __break_lease(struct inode *inode, unsigned int flags);
extern void lease_get_mtime(struct inode *, struct timespec *time);
extern int generic_setlease(struct file *, long, struct file_lock **);
extern int vfs_setlease(struct file *, long, struct file_lock **);
extern int lease_modify(struct file_lock **, int);
extern int lock_may_read(struct inode *, loff_t start, unsigned long count);
extern int lock_may_write(struct inode *, loff_t start, unsigned long count);
#else /* !CONFIG_FILE_LOCKING */
static inline int fcntl_getlk(struct file *file, struct flock __user *user)
{
	return -EINVAL;
}

static inline int fcntl_setlk(unsigned int fd, struct file *file,
			      unsigned int cmd, struct flock __user *user)
{
	return -EACCES;
}

#if BITS_PER_LONG == 32
static inline int fcntl_getlk64(struct file *file, struct flock64 __user *user)
{
	return -EINVAL;
}

static inline int fcntl_setlk64(unsigned int fd, struct file *file,
				unsigned int cmd, struct flock64 __user *user)
{
	return -EACCES;
}
#endif
static inline int fcntl_setlease(unsigned int fd, struct file *filp, long arg)
{
	return 0;
}

static inline int fcntl_getlease(struct file *filp)
{
	return 0;
}

static inline void locks_init_lock(struct file_lock *fl)
{
	return;
}

static inline void __locks_copy_lock(struct file_lock *new, struct file_lock *fl)
{
	return;
}

static inline void locks_copy_lock(struct file_lock *new, struct file_lock *fl)
{
	return;
}

static inline void locks_remove_posix(struct file *filp, fl_owner_t owner)
{
	return;
}

static inline void locks_remove_flock(struct file *filp)
{
	return;
}

static inline void posix_test_lock(struct file *filp, struct file_lock *fl)
{
	return;
}

static inline int posix_lock_file(struct file *filp, struct file_lock *fl,
				  struct file_lock *conflock)
{
	return -ENOLCK;
}

static inline int posix_lock_file_wait(struct file *filp, struct file_lock *fl)
{
	return -ENOLCK;
}

static inline int posix_unblock_lock(struct file *filp,
				     struct file_lock *waiter)
{
	return -ENOENT;
}

static inline int vfs_test_lock(struct file *filp, struct file_lock *fl)
{
	return 0;
}

static inline int vfs_lock_file(struct file *filp, unsigned int cmd,
				struct file_lock *fl, struct file_lock *conf)
{
	return -ENOLCK;
}

static inline int vfs_cancel_lock(struct file *filp, struct file_lock *fl)
{
	return 0;
}

static inline int flock_lock_file_wait(struct file *filp,
				       struct file_lock *request)
{
	return -ENOLCK;
}

static inline int __break_lease(struct inode *inode, unsigned int mode)
{
	return 0;
}

static inline void lease_get_mtime(struct inode *inode, struct timespec *time)
{
	return;
}

static inline int generic_setlease(struct file *filp, long arg,
				    struct file_lock **flp)
{
	return -EINVAL;
}

static inline int vfs_setlease(struct file *filp, long arg,
			       struct file_lock **lease)
{
	return -EINVAL;
}

static inline int lease_modify(struct file_lock **before, int arg)
{
	return -EINVAL;
}

static inline int lock_may_read(struct inode *inode, loff_t start,
				unsigned long len)
{
	return 1;
}

static inline int lock_may_write(struct inode *inode, loff_t start,
				 unsigned long len)
{
	return 1;
}

#endif /* !CONFIG_FILE_LOCKING */


struct fasync_struct {
	int	magic;
	int	fa_fd;
	struct	fasync_struct	*fa_next; /* singly linked list */
	struct	file 		*fa_file;
};

#define FASYNC_MAGIC 0x4601

/* SMP safe fasync helpers: */
extern int fasync_helper(int, struct file *, int, struct fasync_struct **);
/* can be called from interrupts */
extern void kill_fasync(struct fasync_struct **, int, int);
/* only for net: no internal synchronization */
extern void __kill_fasync(struct fasync_struct *, int, int);

extern int __f_setown(struct file *filp, struct pid *, enum pid_type, int force);
extern int f_setown(struct file *filp, unsigned long arg, int force);
extern void f_delown(struct file *filp);
extern pid_t f_getown(struct file *filp);
extern int send_sigurg(struct fown_struct *fown);

/*
 *	Umount options
 */

#define MNT_FORCE	0x00000001	/* Attempt to forcibily umount */
#define MNT_DETACH	0x00000002	/* Just detach from the tree */
#define MNT_EXPIRE	0x00000004	/* Mark for expiry */

/**
 * 所有的超级块对象链表。
 */
extern struct list_head super_blocks;
/**
 * 保护超级块对象链表的自旋锁。
 */
extern spinlock_t sb_lock;

#define sb_entry(list)  list_entry((list), struct super_block, s_list)
#define S_BIAS (1<<30)
/**
 * 超级块对象
 */
struct super_block {
	/**
	 * 指向超级块链表的指针 系统将所有文件系统的超级块组成链表  所有的super_block都存在于 super-blocks 链表中
	 */
	struct list_head	s_list;		/* Keep this first */
	/**
	 * 设备标识符
	 */
	dev_t			s_dev;		/* search index; _not_ kdev_t */
	/**
	 * 以字节为单位的块大小
	 */
	unsigned long		s_blocksize;
	/**
	 * 以位为单位的块大小
	 */
	unsigned char		s_blocksize_bits;
	/**
	 * 脏标志
	 */
	unsigned char		s_dirt;
	/**
	 * 文件的最大长度
	 */
	loff_t			s_maxbytes;	/* Max file size */
	/**
	 * 文件系统类型。
	 */
	struct file_system_type	*s_type;
	/**
	 * 超级块方法
	 */
	const struct super_operations	*s_op;
	/**
	 * 磁盘限额处理方法
	 */
	const struct dquot_operations	*dq_op;
	/**
	 * NFS使用的输出操作
	 */
	const struct quotactl_ops	*s_qcop;
	const struct export_operations *s_export_op;
	/**
	 * 安装标志
	 */
	unsigned long		s_flags;
	/**
	 * 文件系统的魔数
	 */
	unsigned long		s_magic;
	/**
	 * 文件系统根目录的目录项
	 */
	struct dentry		*s_root;
	/**
	 * 卸载时使用的信号量
	 */
	struct rw_semaphore	s_umount;
	struct mutex		s_lock;
	/**
	 * 引用计数
	 */
	int			s_count;
	/**
	 * 对超级块的已安装文件系统进行同步的的标志
	 */
	int			s_need_sync;
	/**
	 * 次引用计数。
	 */
	atomic_t		s_active;
#ifdef CONFIG_SECURITY
	/**
	 * 超级块安全数据结构
	 */
	void                    *s_security;
#endif
	/**
	 * 超级块扩展属性结构的指针
	 */
	struct xattr_handler	**s_xattr;

	/**
	 * 所有索引节点链表
	 */
	struct list_head	s_inodes;	/* all inodes */
	/**
	 * 匿名目录项链表，用于NFS
	 */
	struct hlist_head	s_anon;		/* anonymous dentries for (nfs) exporting */
	/**
	 * 文件对象链表
	 */
	struct list_head	s_files;
	/* s_dentry_lru and s_nr_dentry_unused are protected by dcache_lock */
	struct list_head	s_dentry_lru;	/* unused dentry lru */
	int			s_nr_dentry_unused;	/* # of dentry on lru */

	/**
	 * 指向块设备驱动程序描述符的指针
	 */
	struct block_device	*s_bdev;
	struct backing_dev_info *s_bdi;
	struct mtd_info		*s_mtd;
	/**
	 * 相同文件类型的超级块对象链表。
	 * 对于特定的文件系统, 该文件系统的所有的super block 都存在于file_sytem_type中的fs_supers链表
	 * 中.而所有的文件系统,都存在于file_systems链表中.这是通过调用register_filesystem接口来注册文件系统的.
	 */
	struct list_head	s_instances;
	/**
	 * 磁盘限额的描述符
	 */
	struct quota_info	s_dquot;	/* Diskquota specific options */

	/**
	 * 冻结文件系统时使用的标志，用于强制设置一致性状态。
	 */
	int			s_frozen;
	/**
	 * 等待解冻的队列。
	 */
	wait_queue_head_t	s_wait_unfrozen;

	/**
	 * 包含超级块的块设备名称
	 */
	char s_id[32];				/* Informational name */

	/**
	 * 指向特定文件系统的超级块信息的指针。各文件系统自定义。
	 * 对ext2来说，是指向一个ext2_sb_info类型的结构。
	 */
	void 			*s_fs_info;	/* Filesystem private info */
	fmode_t			s_mode;

	/*
	 * The next field is for VFS *only*. No filesystems have any business
	 * even looking at it. You had been warned.
	 */
	struct mutex s_vfs_rename_mutex;	/* Kludge */

	/* Granularity of c/m/atime in ns.
	   Cannot be worse than a second */
	/**
	 * c/m/atime的时间戳粒度。
	 */
	u32		   s_time_gran;

	/*
	 * Filesystem subtype.  If non-empty the filesystem type field
	 * in /proc/mounts will be "type.subtype"
	 */
	char *s_subtype;

	/*
	 * Saved mount options for lazy filesystems using
	 * generic_show_options()
	 */
	char *s_options;
};

extern struct timespec current_fs_time(struct super_block *sb);

/*
 * Snapshotting support.
 */
enum {
	SB_UNFROZEN = 0,
	SB_FREEZE_WRITE	= 1,
	SB_FREEZE_TRANS = 2,
};

#define vfs_check_frozen(sb, level) \
	wait_event((sb)->s_wait_unfrozen, ((sb)->s_frozen < (level)))

#define get_fs_excl() atomic_inc(&current->fs_excl)
#define put_fs_excl() atomic_dec(&current->fs_excl)
#define has_fs_excl() atomic_read(&current->fs_excl)

#define is_owner_or_cap(inode)	\
	((current_fsuid() == (inode)->i_uid) || capable(CAP_FOWNER))

/* not quite ready to be deprecated, but... */
extern void lock_super(struct super_block *);
extern void unlock_super(struct super_block *);

/*
 * VFS helper functions..
 */
extern int vfs_create(struct inode *, struct dentry *, int, struct nameidata *);
extern int vfs_mkdir(struct inode *, struct dentry *, int);
extern int vfs_mknod(struct inode *, struct dentry *, int, dev_t);
extern int vfs_symlink(struct inode *, struct dentry *, const char *);
extern int vfs_link(struct dentry *, struct inode *, struct dentry *);
extern int vfs_rmdir(struct inode *, struct dentry *);
extern int vfs_unlink(struct inode *, struct dentry *);
extern int vfs_rename(struct inode *, struct dentry *, struct inode *, struct dentry *);

/*
 * VFS dentry helper functions.
 */
extern void dentry_unhash(struct dentry *dentry);

/*
 * VFS file helper functions.
 */
extern int file_permission(struct file *, int);

/*
 * VFS FS_IOC_FIEMAP helper definitions.
 */
struct fiemap_extent_info {
	unsigned int fi_flags;		/* Flags as passed from user */
	unsigned int fi_extents_mapped;	/* Number of mapped extents */
	unsigned int fi_extents_max;	/* Size of fiemap_extent array */
	struct fiemap_extent *fi_extents_start; /* Start of fiemap_extent
						 * array */
};
int fiemap_fill_next_extent(struct fiemap_extent_info *info, u64 logical,
			    u64 phys, u64 len, u32 flags);
int fiemap_check_flags(struct fiemap_extent_info *fieinfo, u32 fs_flags);

/*
 * File types
 *
 * NOTE! These match bits 12..15 of stat.st_mode
 * (ie "(i_mode >> 12) & 15").
 */
#define DT_UNKNOWN	0
#define DT_FIFO		1
#define DT_CHR		2
#define DT_DIR		4
#define DT_BLK		6
#define DT_REG		8
#define DT_LNK		10
#define DT_SOCK		12
#define DT_WHT		14

/*
 * This is the "filldir" function type, used by readdir() to let
 * the kernel specify what kind of dirent layout it wants to have.
 * This allows the kernel to read directories into kernel space or
 * to have different dirent layouts depending on the binary type.
 */
typedef int (*filldir_t)(void *, const char *, int, loff_t, u64, unsigned);
/**
 * gendisk的fops字段指向一个表block_device_operations，该表为块设备主要操作存放了几个定制的方法
 */
struct block_device_operations;

/* These macros are for out of kernel modules to test that
 * the kernel supports the unlocked_ioctl and compat_ioctl
 * fields in struct file_operations. */
#define HAVE_COMPAT_IOCTL 1
#define HAVE_UNLOCKED_IOCTL 1

/*
 * NOTE:
 * read, write, poll, fsync, readv, writev, unlocked_ioctl and compat_ioctl
 * can be called without the big kernel lock held in all filesystems.
 */
/**
 * 文件操作选项
 */
struct file_operations {
	/**
	 * 拥有该结构的模块的指针。避免模块正在被使用时，误卸载模块。
	 * 几乎在所有情况下，该成员都会被初始化为THIS_MODULE。
	 */
	struct module *owner;
	/**
	 * 方法llseek用来修改文件的当前读写位置。并将新位置作为返回值返回。
	 * 参数loff_t是一个长偏移量，即使在32位平台上也至少占用64位的数据宽度。
	 * 出错时返回一个负的返回值。如果这个函数指针是NULL，对seek的调用将会以某种不可预期的方式修改file结构中的位置计数。
	 */
	loff_t (*llseek) (struct file *, loff_t, int);
	/**
	 * 用来从设备中读取数据。该函数指针被赋为NULL时，将导致read系统调用出错并返回-EINVAL。函数返回非负值表示成功读取的字节数。
	 */
	ssize_t (*read) (struct file *, char __user *, size_t, loff_t *);
	ssize_t (*write) (struct file *, const char __user *, size_t, loff_t *);
	/**
	 * 初始化一个异步的读取操作。即在函数返回之前可能不会完成的读取操作。如果该方法为NULL，所有的操作都通过read同步完成。
	 */
	ssize_t (*aio_read) (struct kiocb *, const struct iovec *, unsigned long, loff_t);
	/**
	 * 初始化设备上的异步写入操作。
	 */
	ssize_t (*aio_write) (struct kiocb *, const struct iovec *, unsigned long, loff_t);
	/**
	 * 对于设备文件来说，这个字段应该为NULL。它仅用于读取目录，只对文件系统有用。
	 * filldir_t用于提取目录项的各个字段。
	 */
	int (*readdir) (struct file *, void *, filldir_t);
	/**
	 * POLL方法是poll、epoll和select这三个系统调用的后端实现。这三个系统调用可用来查询某个或多个文件描述符上的读取或写入是否会被阻塞。
	 * poll方法应该返回一个位掩码，用来指出非阻塞的读取或写入是否可能。并且也会向内核提供将调用进程置于休眠状态直到IO变为可能时的信息。
	 * 如果驱动程序将POLL方法定义为NULL，则设备会被认为既可读也可写，并且不会阻塞。
	 */
	unsigned int (*poll) (struct file *, struct poll_table_struct *);
	/**
	 * 系统调用ioctl提供了一种执行设备特殊命令的方法(如格式化软盘的某个磁道，这既不是读也不是写操作)。
	 * 另外，内核还能识别一部分ioctl命令，而不必调用fops表中的ioctl。如果设备不提供ioctl入口点，则对于任何内核未预先定义的请求，ioctl系统调用将返回错误(-ENOTYY)
	 */
	int (*ioctl) (struct inode *, struct file *, unsigned int, unsigned long);
	/**
	 * 与ioctl类似，但是不获取大内核锁。
	 */
	long (*unlocked_ioctl) (struct file *, unsigned int, unsigned long);
	/**
	 * 64位内核使用该方法实现32位系统调用。
	 */
	long (*compat_ioctl) (struct file *, unsigned int, unsigned long);
	/**
	 * 请求将设备内存映射到进程地址空间。如果设备没有实现这个方法，那么mmap系统调用将返回-ENODEV。
	 */
	int (*mmap) (struct file *, struct vm_area_struct *);
	/**
	 * 尽管这始终是对设备文件执行的第一个操作，然而却并不要求驱动程序一定要声明一个相应的方法。
	 * 如果这个入口为NULL，设备的打开操作永远成功，但系统不会通知驱动程序。
	 */
	int (*open) (struct inode *, struct file *);

	/**
	 * 对flush操作的调用发生在进程关闭设备文件描述符副本的时候，它应该执行(并等待)设备上尚未完结的操作。
	 * 请不要将它同用户程序使用的fsync操作相混淆。目前，flush仅仅用于少数几个驱动程序。比如，SCSI磁带驱动程序用它来确保设备被关闭之前所有的数据都被写入磁带中。
	 * 如果flush被置为NULL，内核将简单地忽略用户应用程序的请求。
	 */	int (*flush) (struct file *, fl_owner_t id);

	/**
	 * 当file结构被释放时，将调用这个操作。与open相似，也可以将release设置为NULL。
	 */
	int (*release) (struct inode *, struct file *);
	/**
	 * 该方法是fsync系统调用的后端实现。用户调用它来刷新待处理的数据。如果驱动程序没有实现这一方法，fsync系统调用将返回-EINVAL。
	 */
	int (*fsync) (struct file *, struct dentry *, int datasync);
	/**
	 * 这是fsync的异步版本。
	 */
	int (*aio_fsync) (struct kiocb *, int datasync);
	/**
	 * 这个操作用来通知设备其FASYNC标志发生了变化。异步通知是比较高级的话题，如果设备不支持异步通知，该字段可以是NULL。
	 */
	int (*fasync) (int, struct file *, int);
	/**
	 * LOCK方法用于实现文件锁定，锁定是常规文件不可缺少的特性。但是设备驱动程序几乎从来不会实现这个方法。
	 */
	int (*lock) (struct file *, int, struct file_lock *);
	/**
	 * sendpage是sendfile系统调用的另一半。它由内核调用以将数据发送到对应的文件。每次一个数据页。
	 * 设备驱动程序通常也不需要实现sendfile。
	 */
	ssize_t (*sendpage) (struct file *, struct page *, int, size_t, loff_t *, int);
	/**
	 * 在进程的地址空间中找到一个合适的位置，以便将底层设备中的内存段映射到该位置。
	 * 该任务通常由内存管理代码完成，但该方法的存在可允许驱动程序强制满足特定设备需要的任何对齐要求。大部分驱动程序可设置该方法为NULL。
	 */
	unsigned long (*get_unmapped_area)(struct file *, unsigned long, unsigned long, unsigned long, unsigned long);
	/**
	 * 该方法允许模块检查传递给fcntl调用的标志。当前只适用于NFS
	 */
	int (*check_flags)(int);
	/**
	 * 用于定制flock系统调用的行为。当进程试图对文件加锁时，回调此函数。
	 */
	int (*flock) (struct file *, int, struct file_lock *);
	ssize_t (*splice_write)(struct pipe_inode_info *, struct file *, loff_t *, size_t, unsigned int);
	ssize_t (*splice_read)(struct file *, loff_t *, struct pipe_inode_info *, size_t, unsigned int);
	int (*setlease)(struct file *, long, struct file_lock **);
};

/**
 * 索引节点操作
 */
struct inode_operations {
	/**
	 * 在某一目录下，为与目录项对象相关的普通文件创建一个新的磁盘索引节点。
	 */
	int (*create) (struct inode *,struct dentry *,int, struct nameidata *);
	/**
	 * 为包含在一个目录项对象中的文件名对应的索引节点查找目录?
	 */
	struct dentry * (*lookup) (struct inode *,struct dentry *, struct nameidata *);
	/**
	 * 创建硬连接。
	 */
	int (*link) (struct dentry *,struct inode *,struct dentry *);
	/**
	 * 删除文件的硬连接。
	 */
	int (*unlink) (struct inode *,struct dentry *);
	/**
	 * 创建软链接。
	 */
	int (*symlink) (struct inode *,struct dentry *,const char *);
	/**
	 * 创建目录。
	 */
	int (*mkdir) (struct inode *,struct dentry *,int);
	/**
	 * 移除目录。
	 */
	int (*rmdir) (struct inode *,struct dentry *);
	/**
	 * 为特定设备文件创建一个索引节点。
	 */
	int (*mknod) (struct inode *,struct dentry *,int,dev_t);
	/**
	 * 重命名文件。
	 */
	int (*rename) (struct inode *, struct dentry *,
			struct inode *, struct dentry *);
	/**
	 * 读取符号链接对应的文件路径名。
	 */
	int (*readlink) (struct dentry *, char __user *,int);
	/**
	 * 解析索引节点对象指定的符号链表。
	 */
	void * (*follow_link) (struct dentry *, struct nameidata *);
	/**
	 * 释放follow_link分配的临时数据结构。
	 */
	void (*put_link) (struct dentry *, struct nameidata *, void *);
	/**
	 * 根据i_size字段，修改索引节点相关的文件长度。
	 */
	void (*truncate) (struct inode *);
	/**
	 * 修改文件属性。
	 */
	int (*permission) (struct inode *, int);
	int (*check_acl)(struct inode *, int);
	int (*setattr) (struct dentry *, struct iattr *);
	/**
	 * 读取文件属性。
	 */
	int (*getattr) (struct vfsmount *mnt, struct dentry *, struct kstat *);
	/**
	 * 设置扩展属性。这些属性放在索引节点外的磁盘块中。
	 */
	int (*setxattr) (struct dentry *, const char *,const void *,size_t,int);
	/**
	 * 获取扩展属性。
	 */
	ssize_t (*getxattr) (struct dentry *, const char *, void *, size_t);
	/**
	 * 获取扩展属性名称的整个链表。
	 */
	ssize_t (*listxattr) (struct dentry *, char *, size_t);
	/**
	 * 删除索引节点的扩展属性。
	 */
	int (*removexattr) (struct dentry *, const char *);
	void (*truncate_range)(struct inode *, loff_t, loff_t);
	long (*fallocate)(struct inode *inode, int mode, loff_t offset,
			  loff_t len);
	int (*fiemap)(struct inode *, struct fiemap_extent_info *, u64 start,
		      u64 len);
};

struct seq_file;

ssize_t rw_copy_check_uvector(int type, const struct iovec __user * uvector,
				unsigned long nr_segs, unsigned long fast_segs,
				struct iovec *fast_pointer,
				struct iovec **ret_pointer);

extern ssize_t vfs_read(struct file *, char __user *, size_t, loff_t *);
extern ssize_t vfs_write(struct file *, const char __user *, size_t, loff_t *);
extern ssize_t vfs_readv(struct file *, const struct iovec __user *,
		unsigned long, loff_t *);
extern ssize_t vfs_writev(struct file *, const struct iovec __user *,
		unsigned long, loff_t *);


/**
 * 超级块操作方法
 */
struct super_operations {
	/**
	 * 为索引节点对象分配空间，包括具体文件系统的数据所需要的空间。
	 */
   	struct inode *(*alloc_inode)(struct super_block *sb);
	/**
	 * 释放索引节点对象。
	 */
	void (*destroy_inode)(struct inode *);

  	/**
  	 * 当索引节点标记为脏时调用。日志文件系统用来更新磁盘上的文件系统日志。
  	 */
   	void (*dirty_inode) (struct inode *);
	/**
	 * 更新索引节点对象的内容。flag参数表示IO操作是否应当同步。
	 */
	int (*write_inode) (struct inode *, int);
	/**
	 * 当最后一个用户释放索引节点时调用。通常调用generic_drop_inode。
	 */
	void (*drop_inode) (struct inode *);
	/**
	 * 删除内存中的索引节点和磁盘上的文件数据和元数据。
	 */
	void (*delete_inode) (struct inode *);
	/**
	 * 由于文件系统被卸载而释放对超级块的引用。
	 */
	void (*put_super) (struct super_block *);
	/**
	 * 更新文件系统超级块。
	 */
	void (*write_super) (struct super_block *);
	/**
	 * 清除文件系统以更新磁盘上文件系统数据结构
	 */
	int (*sync_fs)(struct super_block *sb, int wait);
	int (*freeze_fs) (struct super_block *);
	int (*unfreeze_fs) (struct super_block *);
	/**
	 * 返回文件系统的统计信息。
	 */
	int (*statfs) (struct dentry *, struct kstatfs *);

	/**
	 * 用新的选项重新安装文件系统。
	 */
	 int (*remount_fs) (struct super_block *, int *, char *);
	/**
	 * 撤销磁盘索引节点时调用。
	 */
	void (*clear_inode) (struct inode *);
	/**
	 * 开始卸载操作。只在NFS中使用。
	 */
	void (*umount_begin) (struct super_block *);

	/**
	 * 显示特定文件系统的选项。
	 */
	int (*show_options)(struct seq_file *, struct vfsmount *);
	int (*show_stats)(struct seq_file *, struct vfsmount *);
#ifdef CONFIG_QUOTA

	/**
	 * 读取限额设置。
	 */
	ssize_t (*quota_read)(struct super_block *, int, char *, size_t, loff_t);
	/**
	 * 修改限额配置。
	 */
	ssize_t (*quota_write)(struct super_block *, int, const char *, size_t, loff_t);
#endif
	int (*bdev_try_to_free_page)(struct super_block*, struct page*, gfp_t);
};

/*
 * Inode state bits.  Protected by inode_lock.
 *
 * Three bits determine the dirty state of the inode, I_DIRTY_SYNC,
 * I_DIRTY_DATASYNC and I_DIRTY_PAGES.
 *
 * Four bits define the lifetime of an inode.  Initially, inodes are I_NEW,
 * until that flag is cleared.  I_WILL_FREE, I_FREEING and I_CLEAR are set at
 * various stages of removing an inode.
 *
 * Two bits are used for locking and completion notification, I_LOCK and I_SYNC.
 *
 * I_DIRTY_SYNC		Inode is dirty, but doesn't have to be written on
 *			fdatasync().  i_atime is the usual cause.
 * I_DIRTY_DATASYNC	Data-related inode changes pending. We keep track of
 *			these changes separately from I_DIRTY_SYNC so that we
 *			don't have to write inode on fdatasync() when only
 *			mtime has changed in it.
 * I_DIRTY_PAGES	Inode has dirty pages.  Inode itself may be clean.
 * I_NEW		get_new_inode() sets i_state to I_LOCK|I_NEW.  Both
 *			are cleared by unlock_new_inode(), called from iget().
 * I_WILL_FREE		Must be set when calling write_inode_now() if i_count
 *			is zero.  I_FREEING must be set when I_WILL_FREE is
 *			cleared.
 * I_FREEING		Set when inode is about to be freed but still has dirty
 *			pages or buffers attached or the inode itself is still
 *			dirty.
 * I_CLEAR		Set by clear_inode().  In this state the inode is clean
 *			and can be destroyed.
 *
 *			Inodes that are I_WILL_FREE, I_FREEING or I_CLEAR are
 *			prohibited for many purposes.  iget() must wait for
 *			the inode to be completely released, then create it
 *			anew.  Other functions will just ignore such inodes,
 *			if appropriate.  I_LOCK is used for waiting.
 *
 * I_LOCK		Serves as both a mutex and completion notification.
 *			New inodes set I_LOCK.  If two processes both create
 *			the same inode, one of them will release its inode and
 *			wait for I_LOCK to be released before returning.
 *			Inodes in I_WILL_FREE, I_FREEING or I_CLEAR state can
 *			also cause waiting on I_LOCK, without I_LOCK actually
 *			being set.  find_inode() uses this to prevent returning
 *			nearly-dead inodes.
 * I_SYNC		Similar to I_LOCK, but limited in scope to writeback
 *			of inode dirty data.  Having a separate lock for this
 *			purpose reduces latency and prevents some filesystem-
 *			specific deadlocks.
 *
 * Q: What is the difference between I_WILL_FREE and I_FREEING?
 * Q: igrab() only checks on (I_FREEING|I_WILL_FREE).  Should it also check on
 *    I_CLEAR?  If not, why?
 */
#define I_DIRTY_SYNC		1
#define I_DIRTY_DATASYNC	2
#define I_DIRTY_PAGES		4
#define I_NEW			8
#define I_WILL_FREE		16
/**
 * 索引节点正在被释放。
 */
#define I_FREEING		32
/**
 * 索引节点对象的内容不再有意义。
 */
#define I_CLEAR			64
#define __I_LOCK		7
#define I_LOCK			(1 << __I_LOCK)
#define __I_SYNC		8
#define I_SYNC			(1 << __I_SYNC)

/**
 * 判断索引节点是否为脏。
 */
#define I_DIRTY (I_DIRTY_SYNC | I_DIRTY_DATASYNC | I_DIRTY_PAGES)

extern void __mark_inode_dirty(struct inode *, int);
static inline void mark_inode_dirty(struct inode *inode)
{
	__mark_inode_dirty(inode, I_DIRTY);
}

static inline void mark_inode_dirty_sync(struct inode *inode)
{
	__mark_inode_dirty(inode, I_DIRTY_SYNC);
}

/**
 * inc_nlink - directly increment an inode's link count
 * @inode: inode
 *
 * This is a low-level filesystem helper to replace any
 * direct filesystem manipulation of i_nlink.  Currently,
 * it is only here for parity with dec_nlink().
 */
static inline void inc_nlink(struct inode *inode)
{
	inode->i_nlink++;
}

static inline void inode_inc_link_count(struct inode *inode)
{
	inc_nlink(inode);
	mark_inode_dirty(inode);
}

/**
 * drop_nlink - directly drop an inode's link count
 * @inode: inode
 *
 * This is a low-level filesystem helper to replace any
 * direct filesystem manipulation of i_nlink.  In cases
 * where we are attempting to track writes to the
 * filesystem, a decrement to zero means an imminent
 * write when the file is truncated and actually unlinked
 * on the filesystem.
 */
static inline void drop_nlink(struct inode *inode)
{
	inode->i_nlink--;
}

/**
 * clear_nlink - directly zero an inode's link count
 * @inode: inode
 *
 * This is a low-level filesystem helper to replace any
 * direct filesystem manipulation of i_nlink.  See
 * drop_nlink() for why we care about i_nlink hitting zero.
 */
static inline void clear_nlink(struct inode *inode)
{
	inode->i_nlink = 0;
}

static inline void inode_dec_link_count(struct inode *inode)
{
	drop_nlink(inode);
	mark_inode_dirty(inode);
}

/**
 * inode_inc_iversion - increments i_version
 * @inode: inode that need to be updated
 *
 * Every time the inode is modified, the i_version field will be incremented.
 * The filesystem has to be mounted with i_version flag
 */

static inline void inode_inc_iversion(struct inode *inode)
{
       spin_lock(&inode->i_lock);
       inode->i_version++;
       spin_unlock(&inode->i_lock);
}

extern void touch_atime(struct vfsmount *mnt, struct dentry *dentry);
static inline void file_accessed(struct file *file)
{
	if (!(file->f_flags & O_NOATIME))
		touch_atime(file->f_path.mnt, file->f_path.dentry);
}

int sync_inode(struct inode *inode, struct writeback_control *wbc);
/**
 * 对内核支持的每一种文件系统，存在一个这样的结构对其进行描述。
 */
struct file_system_type {
	/**
	 * 文件系统类型的名称
	 */
	const char *name;
	/**
	 * 此文件系统类型的属性
	 */
	int fs_flags;
	/**
	 * 函数指针，当安装此类型的文件系统时，就由VFS调用此例程从设备上将此文件系统的superblock读入内存中
     * vfs_kern_mount 函数调用
	 */
	int (*get_sb) (struct file_system_type *, int,
		       const char *, void *, struct vfsmount *);
	/**
	 * 删除超级块的方法。
	 */
	void (*kill_sb) (struct super_block *);
	/**
	 * 指向实现文件系统的模块的指针。
	 */
	struct module *owner;
	/**
	 * 下一个文件系统指针。
	 */
	struct file_system_type * next;
	/**
	 * 具有相同文件系统类型的超级块对象链表的头。
	 */
	struct list_head fs_supers;

	struct lock_class_key s_lock_key;
	struct lock_class_key s_umount_key;

	struct lock_class_key i_lock_key;
	struct lock_class_key i_mutex_key;
	struct lock_class_key i_mutex_dir_key;
	struct lock_class_key i_alloc_sem_key;
};

extern int get_sb_ns(struct file_system_type *fs_type, int flags, void *data,
	int (*fill_super)(struct super_block *, void *, int),
	struct vfsmount *mnt);
extern int get_sb_bdev(struct file_system_type *fs_type,
	int flags, const char *dev_name, void *data,
	int (*fill_super)(struct super_block *, void *, int),
	struct vfsmount *mnt);
extern int get_sb_single(struct file_system_type *fs_type,
	int flags, void *data,
	int (*fill_super)(struct super_block *, void *, int),
	struct vfsmount *mnt);
extern int get_sb_nodev(struct file_system_type *fs_type,
	int flags, void *data,
	int (*fill_super)(struct super_block *, void *, int),
	struct vfsmount *mnt);
void generic_shutdown_super(struct super_block *sb);
void kill_block_super(struct super_block *sb);
void kill_anon_super(struct super_block *sb);
void kill_litter_super(struct super_block *sb);
void deactivate_super(struct super_block *sb);
void deactivate_locked_super(struct super_block *sb);
int set_anon_super(struct super_block *s, void *data);
struct super_block *sget(struct file_system_type *type,
			int (*test)(struct super_block *,void *),
			int (*set)(struct super_block *,void *),
			void *data);
extern int get_sb_pseudo(struct file_system_type *, char *,
	const struct super_operations *ops, unsigned long,
	struct vfsmount *mnt);
extern void simple_set_mnt(struct vfsmount *mnt, struct super_block *sb);
int __put_super_and_need_restart(struct super_block *sb);
void put_super(struct super_block *sb);

/* Alas, no aliases. Too much hassle with bringing module.h everywhere */
#define fops_get(fops) \
	(((fops) && try_module_get((fops)->owner) ? (fops) : NULL))
#define fops_put(fops) \
	do { if (fops) module_put((fops)->owner); } while(0)

extern int register_filesystem(struct file_system_type *);
extern int unregister_filesystem(struct file_system_type *);
extern struct vfsmount *kern_mount_data(struct file_system_type *, void *data);
#define kern_mount(type) kern_mount_data(type, NULL)
extern int may_umount_tree(struct vfsmount *);
extern int may_umount(struct vfsmount *);
extern long do_mount(char *, char *, char *, unsigned long, void *);
extern struct vfsmount *collect_mounts(struct path *);
extern void drop_collected_mounts(struct vfsmount *);

extern int vfs_statfs(struct dentry *, struct kstatfs *);

extern int current_umask(void);

/* /sys/fs */
extern struct kobject *fs_kobj;

extern int rw_verify_area(int, struct file *, loff_t *, size_t);

#define FLOCK_VERIFY_READ  1
#define FLOCK_VERIFY_WRITE 2

#ifdef CONFIG_FILE_LOCKING
extern int locks_mandatory_locked(struct inode *);
extern int locks_mandatory_area(int, struct inode *, struct file *, loff_t, size_t);

/*
 * Candidates for mandatory locking have the setgid bit set
 * but no group execute bit -  an otherwise meaningless combination.
 */

static inline int __mandatory_lock(struct inode *ino)
{
	return (ino->i_mode & (S_ISGID | S_IXGRP)) == S_ISGID;
}

/*
 * ... and these candidates should be on MS_MANDLOCK mounted fs,
 * otherwise these will be advisory locks
 */

static inline int mandatory_lock(struct inode *ino)
{
	return IS_MANDLOCK(ino) && __mandatory_lock(ino);
}

static inline int locks_verify_locked(struct inode *inode)
{
	if (mandatory_lock(inode))
		return locks_mandatory_locked(inode);
	return 0;
}

static inline int locks_verify_truncate(struct inode *inode,
				    struct file *filp,
				    loff_t size)
{
	if (inode->i_flock && mandatory_lock(inode))
		return locks_mandatory_area(
			FLOCK_VERIFY_WRITE, inode, filp,
			size < inode->i_size ? size : inode->i_size,
			(size < inode->i_size ? inode->i_size - size
			 : size - inode->i_size)
		);
	return 0;
}

static inline int break_lease(struct inode *inode, unsigned int mode)
{
	if (inode->i_flock)
		return __break_lease(inode, mode);
	return 0;
}
#else /* !CONFIG_FILE_LOCKING */
static inline int locks_mandatory_locked(struct inode *inode)
{
	return 0;
}

static inline int locks_mandatory_area(int rw, struct inode *inode,
				       struct file *filp, loff_t offset,
				       size_t count)
{
	return 0;
}

static inline int __mandatory_lock(struct inode *inode)
{
	return 0;
}

static inline int mandatory_lock(struct inode *inode)
{
	return 0;
}

static inline int locks_verify_locked(struct inode *inode)
{
	return 0;
}

static inline int locks_verify_truncate(struct inode *inode, struct file *filp,
					size_t size)
{
	return 0;
}

static inline int break_lease(struct inode *inode, unsigned int mode)
{
	return 0;
}

#endif /* CONFIG_FILE_LOCKING */

/* fs/open.c */

extern int do_truncate(struct dentry *, loff_t start, unsigned int time_attrs,
		       struct file *filp);
extern int do_fallocate(struct file *file, int mode, loff_t offset,
			loff_t len);
extern long do_sys_open(int dfd, const char __user *filename, int flags,
			int mode);
extern struct file *filp_open(const char *, int, int);
extern struct file * dentry_open(struct dentry *, struct vfsmount *, int,
				 const struct cred *);
extern int filp_close(struct file *, fl_owner_t id);
extern char * getname(const char __user *);

/* fs/ioctl.c */

extern int ioctl_preallocate(struct file *filp, void __user *argp);

/* fs/dcache.c */
extern void __init vfs_caches_init_early(void);
extern void __init vfs_caches_init(unsigned long);

extern struct kmem_cache *names_cachep;

#define __getname_gfp(gfp)	kmem_cache_alloc(names_cachep, (gfp))
#define __getname()		__getname_gfp(GFP_KERNEL)
#define __putname(name)		kmem_cache_free(names_cachep, (void *)(name))
#ifndef CONFIG_AUDITSYSCALL
#define putname(name)   __putname(name)
#else
extern void putname(const char *name);
#endif

#ifdef CONFIG_BLOCK
extern int register_blkdev(unsigned int, const char *);
extern void unregister_blkdev(unsigned int, const char *);
extern struct block_device *bdget(dev_t);
extern struct block_device *bdgrab(struct block_device *bdev);
extern void bd_set_size(struct block_device *, loff_t size);
extern void bd_forget(struct inode *inode);
extern void bdput(struct block_device *);
extern struct block_device *open_by_devnum(dev_t, fmode_t);
extern void invalidate_bdev(struct block_device *);
extern int sync_blockdev(struct block_device *bdev);
extern struct super_block *freeze_bdev(struct block_device *);
extern void emergency_thaw_all(void);
extern int thaw_bdev(struct block_device *bdev, struct super_block *sb);
extern int fsync_bdev(struct block_device *);
#else
static inline void bd_forget(struct inode *inode) {}
static inline int sync_blockdev(struct block_device *bdev) { return 0; }
static inline void invalidate_bdev(struct block_device *bdev) {}

static inline struct super_block *freeze_bdev(struct block_device *sb)
{
	return NULL;
}

static inline int thaw_bdev(struct block_device *bdev, struct super_block *sb)
{
	return 0;
}
#endif
extern int sync_filesystem(struct super_block *);
extern const struct file_operations def_blk_fops;
extern const struct file_operations def_chr_fops;
extern const struct file_operations bad_sock_fops;
extern const struct file_operations def_fifo_fops;
#ifdef CONFIG_BLOCK
extern int ioctl_by_bdev(struct block_device *, unsigned, unsigned long);
extern int blkdev_ioctl(struct block_device *, fmode_t, unsigned, unsigned long);
extern long compat_blkdev_ioctl(struct file *, unsigned, unsigned long);
extern int blkdev_get(struct block_device *, fmode_t);
extern int blkdev_put(struct block_device *, fmode_t);
extern int bd_claim(struct block_device *, void *);
extern void bd_release(struct block_device *);
#ifdef CONFIG_SYSFS
extern int bd_claim_by_disk(struct block_device *, void *, struct gendisk *);
extern void bd_release_from_disk(struct block_device *, struct gendisk *);
#else
#define bd_claim_by_disk(bdev, holder, disk)	bd_claim(bdev, holder)
#define bd_release_from_disk(bdev, disk)	bd_release(bdev)
#endif
#endif

/* fs/char_dev.c */
#define CHRDEV_MAJOR_HASH_SIZE	255
extern int alloc_chrdev_region(dev_t *, unsigned, unsigned, const char *);
extern int register_chrdev_region(dev_t, unsigned, const char *);
extern int __register_chrdev(unsigned int major, unsigned int baseminor,
			     unsigned int count, const char *name,
			     const struct file_operations *fops);
extern void __unregister_chrdev(unsigned int major, unsigned int baseminor,
				unsigned int count, const char *name);
extern void unregister_chrdev_region(dev_t, unsigned);
extern void chrdev_show(struct seq_file *,off_t);

static inline int register_chrdev(unsigned int major, const char *name,
				  const struct file_operations *fops)
{
	return __register_chrdev(major, 0, 256, name, fops);
}

static inline void unregister_chrdev(unsigned int major, const char *name)
{
	__unregister_chrdev(major, 0, 256, name);
}

/* fs/block_dev.c */
#define BDEVNAME_SIZE	32	/* Largest string for a blockdev identifier */
#define BDEVT_SIZE	10	/* Largest string for MAJ:MIN for blkdev */

#ifdef CONFIG_BLOCK
#define BLKDEV_MAJOR_HASH_SIZE	255
extern const char *__bdevname(dev_t, char *buffer);
extern const char *bdevname(struct block_device *bdev, char *buffer);
extern struct block_device *lookup_bdev(const char *);
extern struct block_device *open_bdev_exclusive(const char *, fmode_t, void *);
extern void close_bdev_exclusive(struct block_device *, fmode_t);
extern void blkdev_show(struct seq_file *,off_t);

#else
#define BLKDEV_MAJOR_HASH_SIZE	0
#endif

extern void init_special_inode(struct inode *, umode_t, dev_t);

/* Invalid inode operations -- fs/bad_inode.c */
extern void make_bad_inode(struct inode *);
extern int is_bad_inode(struct inode *);

extern const struct file_operations read_pipefifo_fops;
extern const struct file_operations write_pipefifo_fops;
extern const struct file_operations rdwr_pipefifo_fops;

extern int fs_may_remount_ro(struct super_block *);

#ifdef CONFIG_BLOCK
/*
 * return READ, READA, or WRITE
 */
#define bio_rw(bio)		((bio)->bi_rw & (RW_MASK | RWA_MASK))

/*
 * return data direction, READ or WRITE
 */
/**
 * 确定一个BIO请求是一个读请求还是写请求。
 */
#define bio_data_dir(bio)	((bio)->bi_rw & 1)

extern void check_disk_size_change(struct gendisk *disk,
				   struct block_device *bdev);
extern int revalidate_disk(struct gendisk *);
extern int check_disk_change(struct block_device *);
extern int __invalidate_device(struct block_device *);
extern int invalidate_partition(struct gendisk *, int);
#endif
extern int invalidate_inodes(struct super_block *);
unsigned long invalidate_mapping_pages(struct address_space *mapping,
					pgoff_t start, pgoff_t end);

static inline unsigned long __deprecated
invalidate_inode_pages(struct address_space *mapping)
{
	return invalidate_mapping_pages(mapping, 0, ~0UL);
}

static inline void invalidate_remote_inode(struct inode *inode)
{
	if (S_ISREG(inode->i_mode) || S_ISDIR(inode->i_mode) ||
	    S_ISLNK(inode->i_mode))
		invalidate_mapping_pages(inode->i_mapping, 0, -1);
}
extern int invalidate_inode_pages2(struct address_space *mapping);
extern int invalidate_inode_pages2_range(struct address_space *mapping,
					 pgoff_t start, pgoff_t end);
extern int write_inode_now(struct inode *, int);
extern int filemap_fdatawrite(struct address_space *);
extern int filemap_flush(struct address_space *);
extern int filemap_fdatawait(struct address_space *);
extern int filemap_fdatawait_range(struct address_space *, loff_t lstart,
				   loff_t lend);
extern int filemap_write_and_wait(struct address_space *mapping);
extern int filemap_write_and_wait_range(struct address_space *mapping,
				        loff_t lstart, loff_t lend);
extern int wait_on_page_writeback_range(struct address_space *mapping,
				pgoff_t start, pgoff_t end);
extern int __filemap_fdatawrite_range(struct address_space *mapping,
				loff_t start, loff_t end, int sync_mode);
extern int filemap_fdatawrite_range(struct address_space *mapping,
				loff_t start, loff_t end);

extern int vfs_fsync_range(struct file *file, struct dentry *dentry,
			   loff_t start, loff_t end, int datasync);
extern int vfs_fsync(struct file *file, struct dentry *dentry, int datasync);
extern int generic_write_sync(struct file *file, loff_t pos, loff_t count);
extern void sync_supers(void);
extern void emergency_sync(void);
extern void emergency_remount(void);
#ifdef CONFIG_BLOCK
extern sector_t bmap(struct inode *, sector_t);
#endif
extern int notify_change(struct dentry *, struct iattr *);
extern int inode_permission(struct inode *, int);
extern int generic_permission(struct inode *, int,
		int (*check_acl)(struct inode *, int));

static inline bool execute_ok(struct inode *inode)
{
	return (inode->i_mode & S_IXUGO) || S_ISDIR(inode->i_mode);
}

extern int get_write_access(struct inode *);
extern int deny_write_access(struct file *);
static inline void put_write_access(struct inode * inode)
{
	atomic_dec(&inode->i_writecount);
}
static inline void allow_write_access(struct file *file)
{
	if (file)
		atomic_inc(&file->f_path.dentry->d_inode->i_writecount);
}
extern int do_pipe_flags(int *, int);
extern struct file *create_read_pipe(struct file *f, int flags);
extern struct file *create_write_pipe(int flags);
extern void free_write_pipe(struct file *);

extern struct file *do_filp_open(int dfd, const char *pathname,
		int open_flag, int mode, int acc_mode);
extern int may_open(struct path *, int, int);

extern int kernel_read(struct file *, loff_t, char *, unsigned long);
extern struct file * open_exec(const char *);
 
/* fs/dcache.c -- generic fs support functions */
extern int is_subdir(struct dentry *, struct dentry *);
extern ino_t find_inode_number(struct dentry *, struct qstr *);

#include <linux/err.h>

/* needed for stackable file system support */
extern loff_t default_llseek(struct file *file, loff_t offset, int origin);

extern loff_t vfs_llseek(struct file *file, loff_t offset, int origin);

extern int inode_init_always(struct super_block *, struct inode *);
extern void inode_init_once(struct inode *);
extern void inode_add_to_lists(struct super_block *, struct inode *);
extern void iput(struct inode *);
extern struct inode * igrab(struct inode *);
extern ino_t iunique(struct super_block *, ino_t);
extern int inode_needs_sync(struct inode *inode);
extern void generic_delete_inode(struct inode *inode);
extern void generic_drop_inode(struct inode *inode);
extern int generic_detach_inode(struct inode *inode);

extern struct inode *ilookup5_nowait(struct super_block *sb,
		unsigned long hashval, int (*test)(struct inode *, void *),
		void *data);
extern struct inode *ilookup5(struct super_block *sb, unsigned long hashval,
		int (*test)(struct inode *, void *), void *data);
extern struct inode *ilookup(struct super_block *sb, unsigned long ino);

extern struct inode * iget5_locked(struct super_block *, unsigned long, int (*test)(struct inode *, void *), int (*set)(struct inode *, void *), void *);
extern struct inode * iget_locked(struct super_block *, unsigned long);
extern int insert_inode_locked4(struct inode *, unsigned long, int (*test)(struct inode *, void *), void *);
extern int insert_inode_locked(struct inode *);
extern void unlock_new_inode(struct inode *);

extern void __iget(struct inode * inode);
extern void iget_failed(struct inode *);
extern void clear_inode(struct inode *);
extern void destroy_inode(struct inode *);
extern void __destroy_inode(struct inode *);
extern struct inode *new_inode(struct super_block *);
extern int should_remove_suid(struct dentry *);
extern int file_remove_suid(struct file *);

extern void __insert_inode_hash(struct inode *, unsigned long hashval);
extern void remove_inode_hash(struct inode *);
static inline void insert_inode_hash(struct inode *inode) {
	__insert_inode_hash(inode, inode->i_ino);
}

extern struct file * get_empty_filp(void);
extern void file_move(struct file *f, struct list_head *list);
extern void file_kill(struct file *f);
#ifdef CONFIG_BLOCK
struct bio;
extern void submit_bio(int, struct bio *);
extern int bdev_read_only(struct block_device *);
#endif
extern int set_blocksize(struct block_device *, int);
extern int sb_set_blocksize(struct super_block *, int);
extern int sb_min_blocksize(struct super_block *, int);

extern int generic_file_mmap(struct file *, struct vm_area_struct *);
extern int generic_file_readonly_mmap(struct file *, struct vm_area_struct *);
extern int file_read_actor(read_descriptor_t * desc, struct page *page, unsigned long offset, unsigned long size);
int generic_write_checks(struct file *file, loff_t *pos, size_t *count, int isblk);
extern ssize_t generic_file_aio_read(struct kiocb *, const struct iovec *, unsigned long, loff_t);
extern ssize_t __generic_file_aio_write(struct kiocb *, const struct iovec *, unsigned long,
		loff_t *);
extern ssize_t generic_file_aio_write(struct kiocb *, const struct iovec *, unsigned long, loff_t);
extern ssize_t generic_file_direct_write(struct kiocb *, const struct iovec *,
		unsigned long *, loff_t, loff_t *, size_t, size_t);
extern ssize_t generic_file_buffered_write(struct kiocb *, const struct iovec *,
		unsigned long, loff_t, loff_t *, size_t, ssize_t);
extern ssize_t do_sync_read(struct file *filp, char __user *buf, size_t len, loff_t *ppos);
extern ssize_t do_sync_write(struct file *filp, const char __user *buf, size_t len, loff_t *ppos);
extern int generic_segment_checks(const struct iovec *iov,
		unsigned long *nr_segs, size_t *count, int access_flags);

/* fs/block_dev.c */
extern ssize_t blkdev_aio_write(struct kiocb *iocb, const struct iovec *iov,
				unsigned long nr_segs, loff_t pos);

/* fs/splice.c */
extern ssize_t generic_file_splice_read(struct file *, loff_t *,
		struct pipe_inode_info *, size_t, unsigned int);
extern ssize_t default_file_splice_read(struct file *, loff_t *,
		struct pipe_inode_info *, size_t, unsigned int);
extern ssize_t generic_file_splice_write(struct pipe_inode_info *,
		struct file *, loff_t *, size_t, unsigned int);
extern ssize_t generic_splice_sendpage(struct pipe_inode_info *pipe,
		struct file *out, loff_t *, size_t len, unsigned int flags);
extern long do_splice_direct(struct file *in, loff_t *ppos, struct file *out,
		size_t len, unsigned int flags);

extern void
file_ra_state_init(struct file_ra_state *ra, struct address_space *mapping);
extern loff_t no_llseek(struct file *file, loff_t offset, int origin);
extern loff_t generic_file_llseek(struct file *file, loff_t offset, int origin);
extern loff_t generic_file_llseek_unlocked(struct file *file, loff_t offset,
			int origin);
extern int generic_file_open(struct inode * inode, struct file * filp);
extern int nonseekable_open(struct inode * inode, struct file * filp);

#ifdef CONFIG_FS_XIP
extern ssize_t xip_file_read(struct file *filp, char __user *buf, size_t len,
			     loff_t *ppos);
extern int xip_file_mmap(struct file * file, struct vm_area_struct * vma);
extern ssize_t xip_file_write(struct file *filp, const char __user *buf,
			      size_t len, loff_t *ppos);
extern int xip_truncate_page(struct address_space *mapping, loff_t from);
#else
static inline int xip_truncate_page(struct address_space *mapping, loff_t from)
{
	return 0;
}
#endif

#ifdef CONFIG_BLOCK
ssize_t __blockdev_direct_IO(int rw, struct kiocb *iocb, struct inode *inode,
	struct block_device *bdev, const struct iovec *iov, loff_t offset,
	unsigned long nr_segs, get_block_t get_block, dio_iodone_t end_io,
	int lock_type);

enum {
	DIO_LOCKING = 1, /* need locking between buffered and direct access */
	DIO_NO_LOCKING,  /* bdev; no locking at all between buffered/direct */
	DIO_OWN_LOCKING, /* filesystem locks buffered and direct internally */
};

static inline ssize_t blockdev_direct_IO(int rw, struct kiocb *iocb,
	struct inode *inode, struct block_device *bdev, const struct iovec *iov,
	loff_t offset, unsigned long nr_segs, get_block_t get_block,
	dio_iodone_t end_io)
{
	return __blockdev_direct_IO(rw, iocb, inode, bdev, iov, offset,
				nr_segs, get_block, end_io, DIO_LOCKING);
}

static inline ssize_t blockdev_direct_IO_no_locking(int rw, struct kiocb *iocb,
	struct inode *inode, struct block_device *bdev, const struct iovec *iov,
	loff_t offset, unsigned long nr_segs, get_block_t get_block,
	dio_iodone_t end_io)
{
	return __blockdev_direct_IO(rw, iocb, inode, bdev, iov, offset,
				nr_segs, get_block, end_io, DIO_NO_LOCKING);
}

static inline ssize_t blockdev_direct_IO_own_locking(int rw, struct kiocb *iocb,
	struct inode *inode, struct block_device *bdev, const struct iovec *iov,
	loff_t offset, unsigned long nr_segs, get_block_t get_block,
	dio_iodone_t end_io)
{
	return __blockdev_direct_IO(rw, iocb, inode, bdev, iov, offset,
				nr_segs, get_block, end_io, DIO_OWN_LOCKING);
}
#endif

extern const struct file_operations generic_ro_fops;

#define special_file(m) (S_ISCHR(m)||S_ISBLK(m)||S_ISFIFO(m)||S_ISSOCK(m))

extern int vfs_readlink(struct dentry *, char __user *, int, const char *);
extern int vfs_follow_link(struct nameidata *, const char *);
extern int page_readlink(struct dentry *, char __user *, int);
extern void *page_follow_link_light(struct dentry *, struct nameidata *);
extern void page_put_link(struct dentry *, struct nameidata *, void *);
extern int __page_symlink(struct inode *inode, const char *symname, int len,
		int nofs);
extern int page_symlink(struct inode *inode, const char *symname, int len);
extern const struct inode_operations page_symlink_inode_operations;
extern int generic_readlink(struct dentry *, char __user *, int);
extern void generic_fillattr(struct inode *, struct kstat *);
extern int vfs_getattr(struct vfsmount *, struct dentry *, struct kstat *);
void inode_add_bytes(struct inode *inode, loff_t bytes);
void inode_sub_bytes(struct inode *inode, loff_t bytes);
loff_t inode_get_bytes(struct inode *inode);
void inode_set_bytes(struct inode *inode, loff_t bytes);

extern int vfs_readdir(struct file *, filldir_t, void *);

extern int vfs_stat(char __user *, struct kstat *);
extern int vfs_lstat(char __user *, struct kstat *);
extern int vfs_fstat(unsigned int, struct kstat *);
extern int vfs_fstatat(int , char __user *, struct kstat *, int);

extern int do_vfs_ioctl(struct file *filp, unsigned int fd, unsigned int cmd,
		    unsigned long arg);
extern int __generic_block_fiemap(struct inode *inode,
				  struct fiemap_extent_info *fieinfo, u64 start,
				  u64 len, get_block_t *get_block);
extern int generic_block_fiemap(struct inode *inode,
				struct fiemap_extent_info *fieinfo, u64 start,
				u64 len, get_block_t *get_block);

extern void get_filesystem(struct file_system_type *fs);
extern void put_filesystem(struct file_system_type *fs);
extern struct file_system_type *get_fs_type(const char *name);
extern struct super_block *get_super(struct block_device *);
extern struct super_block *get_active_super(struct block_device *bdev);
extern struct super_block *user_get_super(dev_t);
extern void drop_super(struct super_block *sb);

extern int dcache_dir_open(struct inode *, struct file *);
extern int dcache_dir_close(struct inode *, struct file *);
extern loff_t dcache_dir_lseek(struct file *, loff_t, int);
extern int dcache_readdir(struct file *, void *, filldir_t);
extern int simple_getattr(struct vfsmount *, struct dentry *, struct kstat *);
extern int simple_statfs(struct dentry *, struct kstatfs *);
extern int simple_link(struct dentry *, struct inode *, struct dentry *);
extern int simple_unlink(struct inode *, struct dentry *);
extern int simple_rmdir(struct inode *, struct dentry *);
extern int simple_rename(struct inode *, struct dentry *, struct inode *, struct dentry *);
extern int simple_sync_file(struct file *, struct dentry *, int);
extern int simple_empty(struct dentry *);
extern int simple_readpage(struct file *file, struct page *page);
extern int simple_prepare_write(struct file *file, struct page *page,
			unsigned offset, unsigned to);
extern int simple_write_begin(struct file *file, struct address_space *mapping,
			loff_t pos, unsigned len, unsigned flags,
			struct page **pagep, void **fsdata);
extern int simple_write_end(struct file *file, struct address_space *mapping,
			loff_t pos, unsigned len, unsigned copied,
			struct page *page, void *fsdata);

extern struct dentry *simple_lookup(struct inode *, struct dentry *, struct nameidata *);
extern ssize_t generic_read_dir(struct file *, char __user *, size_t, loff_t *);
extern const struct file_operations simple_dir_operations;
extern const struct inode_operations simple_dir_inode_operations;
struct tree_descr { char *name; const struct file_operations *ops; int mode; };
struct dentry *d_alloc_name(struct dentry *, const char *);
extern int simple_fill_super(struct super_block *, int, struct tree_descr *);
extern int simple_pin_fs(struct file_system_type *, struct vfsmount **mount, int *count);
extern void simple_release_fs(struct vfsmount **mount, int *count);

extern ssize_t simple_read_from_buffer(void __user *to, size_t count,
			loff_t *ppos, const void *from, size_t available);

extern int simple_fsync(struct file *, struct dentry *, int);

#ifdef CONFIG_MIGRATION
extern int buffer_migrate_page(struct address_space *,
				struct page *, struct page *);
#else
#define buffer_migrate_page NULL
#endif

extern int inode_change_ok(const struct inode *, struct iattr *);
extern int inode_newsize_ok(const struct inode *, loff_t offset);
extern int __must_check inode_setattr(struct inode *, struct iattr *);

extern void file_update_time(struct file *file);

extern int generic_show_options(struct seq_file *m, struct vfsmount *mnt);
extern void save_mount_options(struct super_block *sb, char *options);
extern void replace_mount_options(struct super_block *sb, char *options);

static inline ino_t parent_ino(struct dentry *dentry)
{
	ino_t res;

	spin_lock(&dentry->d_lock);
	res = dentry->d_parent->d_inode->i_ino;
	spin_unlock(&dentry->d_lock);
	return res;
}

/* Transaction based IO helpers */

/*
 * An argresp is stored in an allocated page and holds the
 * size of the argument or response, along with its content
 */
struct simple_transaction_argresp {
	ssize_t size;
	char data[0];
};

#define SIMPLE_TRANSACTION_LIMIT (PAGE_SIZE - sizeof(struct simple_transaction_argresp))

char *simple_transaction_get(struct file *file, const char __user *buf,
				size_t size);
ssize_t simple_transaction_read(struct file *file, char __user *buf,
				size_t size, loff_t *pos);
int simple_transaction_release(struct inode *inode, struct file *file);

void simple_transaction_set(struct file *file, size_t n);

/*
 * simple attribute files
 *
 * These attributes behave similar to those in sysfs:
 *
 * Writing to an attribute immediately sets a value, an open file can be
 * written to multiple times.
 *
 * Reading from an attribute creates a buffer from the value that might get
 * read with multiple read calls. When the attribute has been read
 * completely, no further read calls are possible until the file is opened
 * again.
 *
 * All attributes contain a text representation of a numeric value
 * that are accessed with the get() and set() functions.
 */
#define DEFINE_SIMPLE_ATTRIBUTE(__fops, __get, __set, __fmt)		\
static int __fops ## _open(struct inode *inode, struct file *file)	\
{									\
	__simple_attr_check_format(__fmt, 0ull);			\
	return simple_attr_open(inode, file, __get, __set, __fmt);	\
}									\
static const struct file_operations __fops = {				\
	.owner	 = THIS_MODULE,						\
	.open	 = __fops ## _open,					\
	.release = simple_attr_release,					\
	.read	 = simple_attr_read,					\
	.write	 = simple_attr_write,					\
};

static inline void __attribute__((format(printf, 1, 2)))
__simple_attr_check_format(const char *fmt, ...)
{
	/* don't do anything, just let the compiler check the arguments; */
}

int simple_attr_open(struct inode *inode, struct file *file,
		     int (*get)(void *, u64 *), int (*set)(void *, u64),
		     const char *fmt);
int simple_attr_release(struct inode *inode, struct file *file);
ssize_t simple_attr_read(struct file *file, char __user *buf,
			 size_t len, loff_t *ppos);
ssize_t simple_attr_write(struct file *file, const char __user *buf,
			  size_t len, loff_t *ppos);

struct ctl_table;
int proc_nr_files(struct ctl_table *table, int write,
		  void __user *buffer, size_t *lenp, loff_t *ppos);

int __init get_filesystem_list(char *buf);

#endif /* __KERNEL__ */
#endif /* _LINUX_FS_H */
