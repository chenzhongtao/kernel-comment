/*
 * descriptor table internals; you almost certainly want file.h instead.
 */

#ifndef __LINUX_FDTABLE_H
#define __LINUX_FDTABLE_H

#include <linux/posix_types.h>
#include <linux/compiler.h>
#include <linux/spinlock.h>
#include <linux/rcupdate.h>
#include <linux/types.h>
#include <linux/init.h>

#include <asm/atomic.h>

/*
 * The default fd array needs to be at least BITS_PER_LONG,
 * as this is the granularity returned by copy_fdset().
 */
#define NR_OPEN_DEFAULT BITS_PER_LONG

/*
 * The embedded_fd_set is a small fd_set,
 * suitable for most tasks (which open <= BITS_PER_LONG files)
 */
struct embedded_fd_set {
	unsigned long fds_bits[1];
};

struct fdtable {
	unsigned int max_fds;
	/**
	 * fdtable结构的fd字段指向文件对象的指针数组。该数组的长度存放在max_fds字段中。通常，fd字段指向
	 * files_struct结构的fd_array字段，该字段包括32个文件对象指针。如果进程打开的文件数目多于32，
	 * 内核就分配一个新的、更大的文件指针数组，并将其地址存放在fd字段中，内核同时也更新max_fds字段的值。
	 */
	struct file ** fd;      /* current fd array */
	fd_set *close_on_exec;
	fd_set *open_fds;
	struct rcu_head rcu;
	struct fdtable *next;
};

/*
 * Open file table structure
 * 用于表示当前进程打开的文件
 * 默认情况下，一个进程使用一个files_struct  (inux 内核的设计与实现13.14)
 */
struct files_struct {
  /*
   * read mostly part
   */
	/* 结构的使用计数 */
	atomic_t count;
	/* 指向其他fd表的指针 */
	struct fdtable *fdt;
	/* 基fd表 */
	struct fdtable fdtab;
  /*
   * written part on a separate cache line in SMP
   */
	/* 单个文件的锁 */
	spinlock_t file_lock ____cacheline_aligned_in_smp;
	int next_fd;
	/* exec()时关闭的文件描述符链表 */
	struct embedded_fd_set close_on_exec_init;
	/* 打开的文件描述符链表 */
	struct embedded_fd_set open_fds_init;
	/* 缺省的文件对象数组,64位机器，最多容纳64个文件对象 */
	struct file * fd_array[NR_OPEN_DEFAULT]; 
};

#define files_fdtable(files) (rcu_dereference((files)->fdt))

struct file_operations;
struct vfsmount;
struct dentry;

extern int expand_files(struct files_struct *, int nr);
extern void free_fdtable_rcu(struct rcu_head *rcu);
extern void __init files_defer_init(void);

static inline void free_fdtable(struct fdtable *fdt)
{
	call_rcu(&fdt->rcu, free_fdtable_rcu);
}

/**
 * 根据fd,返回file
 */
static inline struct file * fcheck_files(struct files_struct *files, unsigned int fd)
{
	struct file * file = NULL;
	struct fdtable *fdt = files_fdtable(files);

	if (fd < fdt->max_fds)
		file = rcu_dereference(fdt->fd[fd]);
	return file;
}

/*
 * Check whether the specified fd has an open file.
 */
#define fcheck(fd)	fcheck_files(current->files, fd)

struct task_struct;

struct files_struct *get_files_struct(struct task_struct *);
void put_files_struct(struct files_struct *fs);
void reset_files_struct(struct files_struct *);
int unshare_files(struct files_struct **);
struct files_struct *dup_fd(struct files_struct *, int *);

extern struct kmem_cache *files_cachep;

#endif /* __LINUX_FDTABLE_H */
