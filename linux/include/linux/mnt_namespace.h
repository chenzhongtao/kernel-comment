#ifndef _NAMESPACE_H_
#define _NAMESPACE_H_
#ifdef __KERNEL__

#include <linux/path.h>
#include <linux/seq_file.h>
#include <linux/wait.h>

 
/* 单进程命名空间，每个进程有唯一的安装文件系统--唯一的根目录和唯一的文件系统层次结构 
默认情况下，进程共享同样的命名空间 (inux 内核的设计与实现13.14)*/
struct mnt_namespace {
	/* 使用该命名空间的计数 */
	atomic_t		count;
	/*根目录的安装点对象  */
	struct vfsmount *	root;
	/* 链表表头，指向该命名空间中所有文件系统的vfsmount实例 */
	struct list_head	list;
	/* 轮询的等待队列 */
	wait_queue_head_t poll;
	/* 事件计数 */
	int event;
};

struct proc_mounts {
	struct seq_file m; /* must be the first element */
	struct mnt_namespace *ns;
	struct path root;
	int event;
};

struct fs_struct;

extern struct mnt_namespace *create_mnt_ns(struct vfsmount *mnt);
extern struct mnt_namespace *copy_mnt_ns(unsigned long, struct mnt_namespace *,
		struct fs_struct *);
extern void put_mnt_ns(struct mnt_namespace *ns);
static inline void get_mnt_ns(struct mnt_namespace *ns)
{
	atomic_inc(&ns->count);
}

extern const struct seq_operations mounts_op;
extern const struct seq_operations mountinfo_op;
extern const struct seq_operations mountstats_op;

#endif
#endif
