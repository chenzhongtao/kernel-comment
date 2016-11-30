#ifndef _LINUX_FS_STRUCT_H
#define _LINUX_FS_STRUCT_H

#include <linux/path.h>

/**
 * 用于表示进程与文件系统之间的结构关系,比如当前的工作目录,进程的根目录等等.
 默认情况下，一个进程使用一个fs_struct  (inux 内核的设计与实现13.14)
 */
struct fs_struct {
	/* 用户数目 */
	int users; 
	/* 保护该结构体的锁 */
	rwlock_t lock;
	/* 掩码 */
	int umask;
	/* 当前正在执行的文件 */
	int in_exec;
    /**
	 * root	  当前进程的根目录
	 * pwd	  当前进程的工作目录
	 */
	struct path root, pwd;
};

extern struct kmem_cache *fs_cachep;

extern void exit_fs(struct task_struct *);
extern void set_fs_root(struct fs_struct *, struct path *);
extern void set_fs_pwd(struct fs_struct *, struct path *);
extern struct fs_struct *copy_fs_struct(struct fs_struct *);
extern void free_fs_struct(struct fs_struct *);
extern void daemonize_fs_struct(void);
extern int unshare_fs_struct(void);

#endif /* _LINUX_FS_STRUCT_H */
