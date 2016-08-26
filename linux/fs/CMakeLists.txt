cmake_minimum_required(VERSION 3.5)
project(fs)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c99")

include_directories( "F:\\code\\github\\chenzhongtao\\linux-2.6.11.12\\linux\\include" )
include_directories( "/home/code/github/chenzhongtao/linux-2.6.11.12/linux/include" )

set(SOURCE_FILES
    # debugfs/file.c
    # debugfs/inode.c
    # debugfs/Makefile
    ext2/acl.c
    ext2/acl.h
    ext2/balloc.c
    ext2/bitmap.c
    ext2/dir.c
    ext2/ext2.h
    ext2/file.c
    ext2/fsync.c
    ext2/ialloc.c
    ext2/inode.c
    ext2/ioctl.c
    ext2/namei.c
    ext2/super.c
    ext2/symlink.c
    ext2/xattr.c
    ext2/xattr.h
    ext2/xattr_security.c
    ext2/xattr_trusted.c
    ext2/xattr_user.c
    # ext3/acl.c
    # ext3/acl.h
    # ext3/balloc.c
    # ext3/bitmap.c
    # ext3/dir.c
    # ext3/file.c
    # ext3/fsync.c
    # ext3/hash.c
    # ext3/ialloc.c
    # ext3/inode.c
    # ext3/ioctl.c
    # ext3/namei.c
    # ext3/resize.c
    # ext3/super.c
    # ext3/symlink.c
    # ext3/xattr.c
    # ext3/xattr.h
    # ext3/xattr_security.c
    # ext3/xattr_trusted.c
    # ext3/xattr_user.c
    # proc/array.c
    # proc/base.c
    # proc/generic.c
    # proc/inode.c
    # proc/internal.h
    # proc/kcore.c
    # proc/kmsg.c
    # proc/mmu.c
    # proc/nommu.c
    # proc/proc_devtree.c
    # proc/proc_misc.c
    # proc/proc_tty.c
    # proc/root.c
    # proc/task_mmu.c
    # proc/task_nommu.c
    # sysfs/bin.c
    # sysfs/dir.c
    # sysfs/file.c
    # sysfs/group.c
    # sysfs/inode.c
    # sysfs/mount.c
    # sysfs/symlink.c
    # sysfs/sysfs.h
    # xfs/linux-2.6/kmem.c
    # xfs/linux-2.6/kmem.h
    # xfs/linux-2.6/mrlock.h
    # xfs/linux-2.6/mutex.h
    # xfs/linux-2.6/sema.h
    # xfs/linux-2.6/spin.h
    # xfs/linux-2.6/sv.h
    # xfs/linux-2.6/time.h
    # xfs/linux-2.6/xfs_aops.c
    # xfs/linux-2.6/xfs_buf.c
    # xfs/linux-2.6/xfs_buf.h
    # xfs/linux-2.6/xfs_cred.h
    # xfs/linux-2.6/xfs_export.c
    # xfs/linux-2.6/xfs_file.c
    # xfs/linux-2.6/xfs_fs_subr.c
    # xfs/linux-2.6/xfs_fs_subr.h
    # xfs/linux-2.6/xfs_globals.c
    # xfs/linux-2.6/xfs_globals.h
    # xfs/linux-2.6/xfs_ioctl.c
    # xfs/linux-2.6/xfs_ioctl32.c
    # xfs/linux-2.6/xfs_ioctl32.h
    # xfs/linux-2.6/xfs_iops.c
    # xfs/linux-2.6/xfs_iops.h
    # xfs/linux-2.6/xfs_linux.h
    # xfs/linux-2.6/xfs_lrw.c
    # xfs/linux-2.6/xfs_lrw.h
    # xfs/linux-2.6/xfs_stats.c
    # xfs/linux-2.6/xfs_stats.h
    # xfs/linux-2.6/xfs_super.c
    # xfs/linux-2.6/xfs_super.h
    # xfs/linux-2.6/xfs_sysctl.c
    # xfs/linux-2.6/xfs_sysctl.h
    # xfs/linux-2.6/xfs_version.h
    # xfs/linux-2.6/xfs_vfs.c
    # xfs/linux-2.6/xfs_vfs.h
    # xfs/linux-2.6/xfs_vnode.c
    # xfs/linux-2.6/xfs_vnode.h
    # xfs/quota/xfs_dquot.c
    # xfs/quota/xfs_dquot.h
    # xfs/quota/xfs_dquot_item.c
    # xfs/quota/xfs_dquot_item.h
    # xfs/quota/xfs_qm.c
    # xfs/quota/xfs_qm.h
    # xfs/quota/xfs_qm_bhv.c
    # xfs/quota/xfs_qm_stats.c
    # xfs/quota/xfs_qm_stats.h
    # xfs/quota/xfs_qm_syscalls.c
    # xfs/quota/xfs_quota_priv.h
    # xfs/quota/xfs_trans_dquot.c
    # xfs/support/debug.c
    # xfs/support/debug.h
    # xfs/support/ktrace.c
    # xfs/support/ktrace.h
    # xfs/support/move.c
    # xfs/support/move.h
    # xfs/support/qsort.c
    # xfs/support/qsort.h
    # xfs/support/uuid.c
    # xfs/support/uuid.h
    # xfs/xfs.h
    # xfs/xfs_acl.c
    # xfs/xfs_acl.h
    # xfs/xfs_ag.h
    # xfs/xfs_alloc.c
    # xfs/xfs_alloc.h
    # xfs/xfs_alloc_btree.c
    # xfs/xfs_alloc_btree.h
    # xfs/xfs_arch.h
    # xfs/xfs_attr.c
    # xfs/xfs_attr.h
    # xfs/xfs_attr_leaf.c
    # xfs/xfs_attr_leaf.h
    # xfs/xfs_attr_sf.h
    # xfs/xfs_behavior.c
    # xfs/xfs_behavior.h
    # xfs/xfs_bit.c
    # xfs/xfs_bit.h
    # xfs/xfs_bmap.c
    # xfs/xfs_bmap.h
    # xfs/xfs_bmap_btree.c
    # xfs/xfs_bmap_btree.h
    # xfs/xfs_btree.c
    # xfs/xfs_btree.h
    # xfs/xfs_buf_item.c
    # xfs/xfs_buf_item.h
    # xfs/xfs_cap.h
    # xfs/xfs_clnt.h
    # xfs/xfs_da_btree.c
    # xfs/xfs_da_btree.h
    # xfs/xfs_dfrag.c
    # xfs/xfs_dfrag.h
    # xfs/xfs_dinode.h
    # xfs/xfs_dir.c
    # xfs/xfs_dir.h
    # xfs/xfs_dir2.c
    # xfs/xfs_dir2.h
    # xfs/xfs_dir2_block.c
    # xfs/xfs_dir2_block.h
    # xfs/xfs_dir2_data.c
    # xfs/xfs_dir2_data.h
    # xfs/xfs_dir2_leaf.c
    # xfs/xfs_dir2_leaf.h
    # xfs/xfs_dir2_node.c
    # xfs/xfs_dir2_node.h
    # xfs/xfs_dir2_sf.c
    # xfs/xfs_dir2_sf.h
    # xfs/xfs_dir2_trace.c
    # xfs/xfs_dir2_trace.h
    # xfs/xfs_dir_leaf.c
    # xfs/xfs_dir_leaf.h
    # xfs/xfs_dir_sf.h
    # xfs/xfs_dmapi.h
    # xfs/xfs_dmops.c
    # xfs/xfs_error.c
    # xfs/xfs_error.h
    # xfs/xfs_extfree_item.c
    # xfs/xfs_extfree_item.h
    # xfs/xfs_fs.h
    # xfs/xfs_fsops.c
    # xfs/xfs_fsops.h
    # xfs/xfs_ialloc.c
    # xfs/xfs_ialloc.h
    # xfs/xfs_ialloc_btree.c
    # xfs/xfs_ialloc_btree.h
    # xfs/xfs_iget.c
    # xfs/xfs_imap.h
    # xfs/xfs_inode.c
    # xfs/xfs_inode.h
    # xfs/xfs_inode_item.c
    # xfs/xfs_inode_item.h
    # xfs/xfs_inum.h
    # xfs/xfs_iocore.c
    # xfs/xfs_iomap.c
    # xfs/xfs_iomap.h
    # xfs/xfs_itable.c
    # xfs/xfs_itable.h
    # xfs/xfs_log.c
    # xfs/xfs_log.h
    # xfs/xfs_log_priv.h
    # xfs/xfs_log_recover.c
    # xfs/xfs_log_recover.h
    # xfs/xfs_mac.h
    # xfs/xfs_macros.c
    # xfs/xfs_macros.h
    # xfs/xfs_mount.c
    # xfs/xfs_mount.h
    # xfs/xfs_qmops.c
    # xfs/xfs_quota.h
    # xfs/xfs_refcache.h
    # xfs/xfs_rename.c
    # xfs/xfs_rtalloc.c
    # xfs/xfs_rtalloc.h
    # xfs/xfs_rw.c
    # xfs/xfs_rw.h
    # xfs/xfs_sb.h
    # xfs/xfs_trans.c
    # xfs/xfs_trans.h
    # xfs/xfs_trans_ail.c
    # xfs/xfs_trans_buf.c
    # xfs/xfs_trans_extfree.c
    # xfs/xfs_trans_inode.c
    # xfs/xfs_trans_item.c
    # xfs/xfs_trans_priv.h
    # xfs/xfs_trans_space.h
    # xfs/xfs_types.h
    # xfs/xfs_utils.c
    # xfs/xfs_utils.h
    # xfs/xfs_vfsops.c
    # xfs/xfs_vnodeops.c
    aio.c
    attr.c
    bad_inode.c
    binfmt_aout.c
    binfmt_elf.c
    binfmt_elf_fdpic.c
    binfmt_em86.c
    binfmt_flat.c
    binfmt_misc.c
    binfmt_script.c
    binfmt_som.c
    bio.c
    block_dev.c
    buffer.c
    char_dev.c
    compat.c
    compat_ioctl.c
    dcache.c
    dcookies.c
    direct-io.c
    dnotify.c
    dquot.c
    eventpoll.c
    exec.c
    fcntl.c
    fifo.c
    file.c
    file_table.c
    filesystems.c
    fs-writeback.c
    inode.c
    ioctl.c
    libfs.c
    locks.c
    mbcache.c
    mpage.c
    namei.c
    namespace.c
    nfsctl.c
    open.c
    pipe.c
    posix_acl.c
    quota.c
    quota_v1.c
    quota_v2.c
    read_write.c
    readdir.c
    select.c
    seq_file.c
    stat.c
    super.c
    xattr.c
    xattr_acl.c
	)

add_executable(fs ${SOURCE_FILES})