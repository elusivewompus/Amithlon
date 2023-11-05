/*
 * Copyright 1996-2000 Hans Reiser, see reiserfs/README for licensing and copyright details
 */


#include <linux/sched.h>
#include <linux/reiserfs_fs.h>
#include <linux/smp_lock.h>

/*
** We pack the tails of files on file close, not at the time they are written.
** This implies an unnecessary copy of the tail and an unnecessary indirect item
** insertion/balancing, for files that are written in one write.
** It avoids unnecessary tail packings (balances) for files that are written in
** multiple writes and are small enough to have tails.
** 
** file_release is called by the VFS layer when the file is closed.  If
** this is the last open file descriptor, and the file
** small enough to have a tail, and the tail is currently in an
** unformatted node, the tail is converted back into a direct item.
** 
** Since reiserfs_file_truncate involves the same checks and conversions
** we just call truncate on the file without changing the file size.
** The file is not truncated at all.
*/
static int reiserfs_file_release (struct inode * inode, struct file * filp)
{

    struct reiserfs_transaction_handle th ;
    int windex ;

    /* don't pack if we aren't the last holder */
    if (atomic_read(&inode->i_count) > 1) {
	return 0;
    }

    if (!S_ISREG (inode->i_mode))
	BUG ();

    /* fast out for when nothing needs to be done */
    if ((!inode->u.reiserfs_i.i_pack_on_close || 
         !tail_has_to_be_packed(inode))       && 
	inode->u.reiserfs_i.i_prealloc_count <= 0) {
	return 0;
    }    
    
    lock_kernel() ;
    down (&inode->i_sem); 
    journal_begin(&th, inode->i_sb, JOURNAL_PER_BALANCE_CNT * 3) ;
    reiserfs_update_inode_transaction(inode) ;

#ifdef REISERFS_PREALLOCATE
    reiserfs_discard_prealloc (&th, inode);
#endif

    if (tail_has_to_be_packed (inode)) {
	/* if regular file is released by last holder and it has been
	   appended (we append by unformatted node only) or its direct
	   item(s) had to be converted, then it may have to be
	   indirect2direct converted */
	windex = push_journal_writer("file_release") ;
	reiserfs_do_truncate (&th, inode, 0/*no timestamp updates*/); 
	pop_journal_writer(windex) ;
    }
    journal_end(&th, inode->i_sb, JOURNAL_PER_BALANCE_CNT * 3) ;
    up (&inode->i_sem); 
    unlock_kernel() ;
    return 0;
}


/* Sync a reiserfs file. */
static int reiserfs_sync_file(
			      struct file   * p_s_filp,
			      struct dentry * p_s_dentry,
			      int datasync
			      ) {
  struct inode * p_s_inode = p_s_dentry->d_inode;
  int n_err;

  lock_kernel() ;

  if (!S_ISREG(p_s_inode->i_mode))
      BUG ();

  /* step one, flush all dirty buffers in the file's page map to disk */
  n_err = generic_buffer_fdatasync(p_s_inode, 0, ~0UL) ;
  
  /* step two, commit the current transaction to flush any metadata
  ** changes
  */
  journal_begin(&th, p_s_inode->i_sb, jbegin_count) ;
  windex = push_journal_writer("sync_file") ;
  reiserfs_update_sd(&th, p_s_inode);
  pop_journal_writer(windex) ;
  journal_end_sync(&th, p_s_inode->i_sb,jbegin_count) ;
  unlock_kernel() ;
  return ( n_err < 0 ) ? -EIO : 0;
}


/*
** vfs version of truncate file.  Must NOT be called with
** a transaction already started.
*/
static void reiserfs_truncate_file(struct inode *p_s_inode) {
  struct reiserfs_transaction_handle th ;
  int windex ;

  journal_begin(&th, p_s_inode->i_sb,  JOURNAL_PER_BALANCE_CNT * 2 ) ;
  windex = push_journal_writer("resierfs_vfs_truncate_file") ;
  reiserfs_do_truncate (&th, p_s_inode, 1/*update timestamps*/) ;
  pop_journal_writer(windex) ;
  journal_end(&th, p_s_inode->i_sb,  JOURNAL_PER_BALANCE_CNT * 2 ) ;
}

static int reiserfs_setattr(struct dentry *dentry, struct iattr *attr) {
    struct inode *inode = dentry->d_inode ;
    int error ;
    if (attr->ia_valid & ATTR_SIZE) {
	/* version 2 items will be caught by the s_maxbytes check
	** done for us in vmtruncate
	*/
	if (get_inode_item_key_version(inode) == KEY_FORMAT_3_5 &&
	    attr->ia_size > MAX_NON_LFS)
            return -EFBIG ;

	/* fill in hole pointers in the expanding truncate case. */
        if (attr->ia_size > inode->i_size) {
	    error = generic_cont_expand(inode, attr->ia_size) ;
	    if (inode->u.reiserfs_i.i_prealloc_count > 0) {
		struct reiserfs_transaction_handle th ;
		/* we're changing at most 2 bitmaps, inode + super */
		journal_begin(&th, inode->i_sb, 4) ;
		reiserfs_discard_prealloc (&th, inode);
		journal_end(&th, inode->i_sb, 4) ;
	    }
	    if (error)
	        return error ;
	}
    }

    if ((((attr->ia_valid & ATTR_UID) && (attr->ia_uid & ~0xffff)) ||
	 ((attr->ia_valid & ATTR_GID) && (attr->ia_gid & ~0xffff))) &&
	(get_inode_sd_version (inode) == STAT_DATA_V1))
		/* stat data of format v3.5 has 16 bit uid and gid */
	    return -EINVAL;

    error = inode_change_ok(inode, attr) ;
    if (!error)
        inode_setattr(inode, attr) ;

    return error ;
}

struct file_operations reiserfs_file_operations = {
    read:	generic_file_read,
    write:	generic_file_write,
    mmap:	generic_file_mmap,
    release:	reiserfs_file_release,
    fsync:	reiserfs_sync_file,
};


struct  inode_operations reiserfs_file_inode_operations = {
    truncate:	reiserfs_truncate_file,
};


