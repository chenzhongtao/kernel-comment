#ifndef _LINUX_PATH_H
#define _LINUX_PATH_H

struct dentry;
struct vfsmount;
 
/* path由 文件系统安装点 和 目录项组成 */
struct path {
	struct vfsmount *mnt;
	struct dentry *dentry;
};

extern void path_get(struct path *);
extern void path_put(struct path *);

#endif  /* _LINUX_PATH_H */
