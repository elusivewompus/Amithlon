/*
 * Copyright 1996, 1997, 1998 Hans Reiser, see reiserfs/README for licensing and copyright details
 */

#include <linux/sched.h>
#include <linux/reiserfs_fs.h>
#include <linux/locks.h>
#include <linux/smp_lock.h>
#include <asm/uaccess.h>
#include <asm/unaligned.h>

#else

#include "nokernel.h"

#endif


static int reiserfs_get_block (struct inode * inode, long block,
			       struct buffer_head * bh_result, int create);
//
// initially this function was derived from minix or ext2's analog and
// evolved as the prototype did
//
void reiserfs_delete_inode (struct inode * inode)
{
    int jbegin_count = JOURNAL_PER_BALANCE_CNT * 2; 
    int windex ;
    struct reiserfs_transaction_handle th ;

  
    lock_kernel() ; 

    /* The = 0 happens when we abort creating a new inode for some reason like lack of space.. */
    if (INODE_PKEY(inode)->k_objectid != 0) { /* also handles bad_inode case */
	down (&inode->i_sem); 

	journal_begin(&th, inode->i_sb, jbegin_count) ;
	reiserfs_update_inode_transaction(inode) ;
	windex = push_journal_writer("delete_inode") ;

	reiserfs_delete_object (&th, inode);
	pop_journal_writer(windex) ;

	journal_end(&th, inode->i_sb, jbegin_count) ;

        up (&inode->i_sem);

        /* all items of file are deleted, so we can remove "save" link */
	remove_save_link (inode, 0/* not truncate */);
    } else {
	/* no object items are in the tree */
	;
    }
    clear_inode (inode); /* note this must go after the journal_end to prevent deadlock */
    inode->i_blocks = 0;
    unlock_kernel() ;
}

#if 0
static void copy_data_blocks_to_inode (struct inode * inode, struct item_head * ih, __u32 * ind_item)
{
  int first_log_block = (ih->ih_key.k_offset - 1) / inode->i_sb->s_blocksize; /* first log block addressed by indirect item */
  int i, j;
  
  for (i = first_log_block, j = 0; i < REISERFS_N_BLOCKS && j < I_UNFM_NUM (ih); i ++, j ++) {
#ifdef CONFIG_REISERFS_CHECK
    if (inode->u.reiserfs_i.i_data [i] && inode->u.reiserfs_i.i_data [i] != ind_item [j])
      reiserfs_panic (inode->i_sb, "vs-13000: copy_data_blocks_to_inode: "
							 "log block %d, data block %d is seet and doe not match to unfmptr %d",
							 i, inode->u.reiserfs_i.i_data [i], ind_item [j]);
#endif
    inode->u.reiserfs_i.i_data [i] = ind_item [j];
  }
}
#endif/*0*/



static void _make_cpu_key (struct cpu_key * key, int version, __u32 dirid, __u32 objectid, 
	       loff_t offset, int type, int length )
{
    key->version = version;

    key->on_disk_key.k_dir_id = dirid;
    key->on_disk_key.k_objectid = objectid;
    set_cpu_key_k_offset (key, offset);
    set_cpu_key_k_type (key, type);  
    key->key_length = length;
}


/* take base of inode_key (it comes from inode always) (dirid, objectid) and version from an inode, set
   offset and type of key */
void make_cpu_key (struct cpu_key * key, const struct inode * inode, loff_t offset,
	      int type, int length )
{
  _make_cpu_key (key, get_inode_item_key_version (inode), le32_to_cpu (INODE_PKEY (inode)->k_dir_id),
		 le32_to_cpu (INODE_PKEY (inode)->k_objectid), 
		 offset, type, length);
}


//
// when key is 0, do not set version and short key
//
inline void make_le_item_head (struct item_head * ih, const struct cpu_key * key,
			       int version,
			       loff_t offset, int type, int length, 
			       int entry_count/*or ih_free_space*/)
{
    if (key) {
	ih->ih_key.k_dir_id = cpu_to_le32 (key->on_disk_key.k_dir_id);
	ih->ih_key.k_objectid = cpu_to_le32 (key->on_disk_key.k_objectid);
    }
    put_ih_version( ih, version );
    set_le_ih_k_offset (ih, offset);
    set_le_ih_k_type (ih, type);
    put_ih_item_len( ih, length );
    /*    set_ih_free_space (ih, 0);*/
    // for directory items it is entry count, for directs and stat
    // datas - 0xffff, for indirects - 0
    put_ih_entry_count( ih, entry_count );
}

static void add_to_flushlist(struct inode *inode, struct buffer_head *bh) {
    struct inode *jinode = &(SB_JOURNAL(inode->i_sb)->j_dummy_inode) ;

    buffer_insert_inode_queue(bh, jinode) ;
}

//
// FIXME: we might cache recently accessed indirect item (or at least
// first 15 pointers just like ext2 does
//
static int got_from_inode (struct inode * inode, b_blocknr_t * pblock)
{
  return 0;
}

/* people who call journal_begin with a page locked must call this
** BEFORE calling journal_begin
*/
static int prevent_flush_page_lock(struct super_block *s,
                                   struct page *page, 
				   struct inode *inode) {
  struct reiserfs_page_list *pl ;
  /* we don't care if the inode has a stale pointer from an old
  ** transaction
  */
  if(inode->u.reiserfs_i.i_conversion_trans_id != SB_JOURNAL(s)->j_trans_id) {
    return 0 ;
  }
  pl = inode->u.reiserfs_i.i_converted_page ;
  if (pl && pl->page == page) {
    pl->do_not_lock = 1 ;
  }
  return 0 ;
 
}
/* people who call journal_end with a page locked must call this
** AFTER calling journal_end
*/
static int allow_flush_page_lock(struct super_block *s,
                                   struct page *page, 
				   struct inode *inode) {

  struct reiserfs_page_list *pl ;
  /* we don't care if the inode has a stale pointer from an old
  ** transaction
  */
  if(inode->u.reiserfs_i.i_conversion_trans_id != SB_JOURNAL(s)->j_trans_id) {
    return 0 ;
  }
  pl = inode->u.reiserfs_i.i_converted_page ;
  if (pl && pl->page == page) {
    pl->do_not_lock = 0 ;
  }
  return 0 ;
 
}



/* we need to allocate a block for new unformatted node.  Try to figure out
   what point in bitmap reiserfs_new_blocknrs should start from. */
static b_blocknr_t find_tag (struct buffer_head * bh, struct item_head * ih,
			     __u32 * item, int pos_in_item)
{
  __u32 block ;
  if (!is_indirect_le_ih (ih))
	 /* something more complicated could be here */
	 return bh->b_blocknr;

  /* for indirect item: go to left and look for the first non-hole entry in
	  the indirect item */
  if (pos_in_item == I_UNFM_NUM (ih))
	 pos_in_item --;
  while (pos_in_item >= 0) {
	 block = get_block_num(item, pos_in_item) ;
	 if (block)
		return block ;
	 pos_in_item --;
  }
  return bh->b_blocknr;
}


/* reiserfs_get_block does not need to allocate a block only if it has been
   done already or non-hole position has been found in the indirect item */
static inline int allocation_needed (int retval, b_blocknr_t allocated, 
				     struct item_head * ih,
				     __u32 * item, int pos_in_item)
{
  if (allocated)
	 return 0;
  if (retval == POSITION_FOUND && is_indirect_le_ih (ih) && 
      get_block_num(item, pos_in_item))
	 return 0;
  return 1;
}

static inline int indirect_item_found (int retval, struct item_head * ih)
{
  return (retval == POSITION_FOUND) && is_indirect_le_ih (ih);
}


static inline void set_block_dev_mapped (struct buffer_head * bh, 
					 b_blocknr_t block, struct inode * inode)
{
  bh->b_dev = inode->i_dev;
  bh->b_blocknr = block;
  bh->b_state |= (1UL << BH_Mapped);
}


//
// files which were created in the earlier version can not be longer,
// than 2 gb
//
static int file_capable (struct inode * inode, long block)
{
    if (get_inode_item_key_version (inode) != KEY_FORMAT_3_5 || // it is new file.
	block < (1 << (31 - inode->i_sb->s_blocksize_bits))) // old file, but 'block' is inside of 2gb
	return 1;

    return 0;
}

static void restart_transaction(struct reiserfs_transaction_handle *th,
				struct inode *inode, struct path *path) {
  struct super_block *s = th->t_super ;
  int len = th->t_blocks_allocated ;

  pathrelse(path) ;
  reiserfs_update_sd(th, inode) ;
  journal_end(th, s, len) ;
  journal_begin(th, s, len) ;
  reiserfs_update_inode_transaction(inode) ;
}

// it is called by get_block when create == 0. Returns block number
// for 'block'-th logical block of file. When it hits direct item it
// returns 0 (being called from bmap) or read direct item into piece
// of page (bh_result)
static void _get_block_create_0 (struct inode * inode, long block,
				 struct buffer_head * bh_result,
				 int read_direct)
{
    INITIALIZE_PATH (path);
    struct cpu_key key;
    struct buffer_head * bh;
    struct item_head * ih;
    int blocknr;
    char * p;
    int chars;


    if (got_from_inode (inode, &bh_result->b_blocknr)) {
	bh_result->b_dev = inode->i_dev;
	//bh_result->b_blocknr = block;
	bh_result->b_state |= (1UL << BH_Mapped);
	return;
    }

    // prepare the key to look for the 'block'-th block of file
    make_cpu_key (&key, inode,
		  (loff_t)block * inode->i_sb->s_blocksize + 1, TYPE_ANY, 3);

    wait_on_tail (inode);
    lock_tail (inode, READ_TAIL_LOCK);

    if (search_for_position_by_key (inode->i_sb, &key, &path) != POSITION_FOUND) {
	pathrelse (&path);
	unlock_tail(inode) ;
	return;
    }
    
    //
    bh = get_last_bh (&path);
    ih = get_ih (&path);
    if (is_indirect_le_ih (ih)) {
	__u32 * ind_item = (__u32 *)B_I_PITEM (bh, ih);
	
	/* FIXME: here we could cache indirect item or part of it in
	   the inode to avoid search_by_key in case of subsequent
	   access to file */
	blocknr = le32_to_cpu (ind_item [path.pos_in_item]);
	if (blocknr) {
	    bh_result->b_dev = inode->i_dev;
	    bh_result->b_blocknr = blocknr;
	    bh_result->b_state |= (1UL << BH_Mapped);
	}
	unlock_tail (inode);
	pathrelse (&path);
	return;
    }


    // requested data are in direct item(s)
    if (!read_direct) {
	// we are called by bmap. FIXME: we can not map block of file
	// when it is stored in direct item(s)
	unlock_tail (inode);
	pathrelse (&path);	
	return;
    }

    /* if we've got a direct item, and the buffer was uptodate,
    ** we don't want to pull data off disk again.  skip to the
    ** end, where we map the buffer and return
    */
    if (buffer_uptodate(bh_result)) {
        goto finished ;
    } else 
	/*
	** grab_tail_page can trigger calls to reiserfs_get_block on up to date
	** pages without any buffers.  If the page is up to date, we don't want
	** read old data off disk.  Set the up to date bit on the buffer instead
	** and jump to the end
	*/
	    if (Page_Uptodate(bh_result->b_page)) {
		mark_buffer_uptodate(bh_result, 1);
		goto finished ;
    }

    // read file tail into part of page
    p = bh_result->b_data;
    memset (p, 0, inode->i_sb->s_blocksize);
    do {
	if (!is_direct_le_ih (ih))
	    BUG ();
	chars = le16_to_cpu (ih->ih_item_len) - path.pos_in_item;
	memcpy (p, B_I_PITEM (bh, ih) + path.pos_in_item, chars);
	p += chars;

	if (PATH_LAST_POSITION (&path) != (B_NR_ITEMS (bh) - 1))
	    // we done, if read direct item is not the last item of
	    // node FIXME: we could try to check right delimiting key
	    // to see whether direct item continues in the right
	    // neighbor or rely on i_size
	    break;

	// update key to look for the next piece
	set_cpu_key_k_offset (&key, cpu_key_k_offset (&key) + chars);
	if (search_for_position_by_key (inode->i_sb, &key, &path) != POSITION_FOUND)
	    // we read something from tail, even if now we got IO_ERROR
	    break;
	bh = get_last_bh (&path);
	ih = get_ih (&path);
    } while (1);

    unlock_tail (inode);
    pathrelse (&path);
    
    // FIXME: b_blocknr == 0 here. but b_data contains correct data
    // from tail. ll_rw_block will skip uptodate buffers
    bh_result->b_blocknr = 0 ;
    bh_result->b_dev = inode->i_dev;
    mark_buffer_uptodate (bh_result, 1);
    bh_result->b_state |= (1UL << BH_Mapped);

    return;
}


// this is called to create file map. So, _get_block_create_0 will not
// read direct item
int reiserfs_bmap (struct inode * inode, long block,
		   struct buffer_head * bh_result, int create)
{
    if (!file_capable (inode, block))
	return -EFBIG;

    lock_kernel() ;
    _get_block_create_0 (inode, block, bh_result, 0/*do not read direct item*/);
    unlock_kernel() ;
    return 0;
}


static inline int _allocate_block(struct reiserfs_transaction_handle *th,
                           struct inode *inode, 
			   b_blocknr_t *allocated_block_nr, 
			   unsigned long tag,
			   int flags) {
  
#ifdef REISERFS_PREALLOCATE
    if (!(flags & GET_BLOCK_NO_ISEM)) {
        return reiserfs_new_unf_blocknrs2(th, inode, allocated_block_nr, tag);
    }
#endif
    return reiserfs_new_unf_blocknrs (th, allocated_block_nr, tag);
}
//
// initially this function was derived from minix or ext2's analog and
// evolved as the prototype did
//
static int reiserfs_get_block (struct inode * inode, long block,
			       struct buffer_head * bh_result, int create)
{
    int repeat, retval;
    unsigned long tag;
    b_blocknr_t allocated_block_nr = 0;// b_blocknr_t is unsigned long
    INITIALIZE_PATH(path);
    int pos_in_item;
    struct cpu_key key;
    struct buffer_head * bh, * unbh = 0;
    struct item_head * ih, tmp_ih;
    __u32 * item;
    int done;
    int fs_gen;
    int windex ;
    struct reiserfs_transaction_handle th ;
    int jbegin_count = JOURNAL_PER_BALANCE_CNT * 3 ;
    int version;

    lock_kernel() ;

    version = inode_items_version (inode);

    if (!file_capable (inode, block)) {
	unlock_kernel() ;
	return -EFBIG;
    }

    /* if !create, we aren't changing the FS, so we don't need to
    ** log anything, so we don't need to start a transaction
    */
    if (!create) {
	/* find number of block-th logical block of the file */
	_get_block_create_0 (inode, block, bh_result, 1/*read direct item*/);
	unlock_kernel() ;
	return 0;
    }

    if (block < 0) {
	unlock_kernel();
	return -EIO;
    }

    // note: tail can not be convert_locked here. It can not get locked by
    // another process either while we are in this vfs_write->get_block
    if (is_tail_convert_locked (inode))
	BUG ();

    prevent_flush_page_lock(inode->i_sb, bh_result->b_page, inode) ;
    journal_begin(&th, inode->i_sb, jbegin_count) ;
    inode->u.reiserfs_i.i_pack_on_close = 1 ;
    windex = push_journal_writer("reiserfs_get_block") ;
  
    /* set the key of the first byte in the 'block'-th block of file */
    make_cpu_key (&key, inode,
		  (loff_t)block * inode->i_sb->s_blocksize + 1, // k_offset
		  TYPE_ANY, 3/*key length*/);

 research:

    retval = search_for_position_by_key (inode->i_sb, &key, &path);
    if (retval == IO_ERROR) {
	retval = -EIO;
	goto failure;
    }
	
    bh = get_last_bh (&path);
    ih = get_ih (&path);
    item = get_item (&path);
    pos_in_item = path.pos_in_item;

    fs_gen = get_generation (inode->i_sb);

    if (allocation_needed (retval, allocated_block_nr, ih, item, pos_in_item)) {
	/* we have to allocate block for the unformatted node */
	copy_item_head (&tmp_ih, ih);
	tag = find_tag (bh, ih, item, pos_in_item);
#ifdef REISERFS_PREALLOCATE
	repeat = reiserfs_new_unf_blocknrs2 (&th, inode, &allocated_block_nr, tag);
#else
	repeat = reiserfs_new_unf_blocknrs (&th, &allocated_block_nr, tag);
#endif

	if (repeat == NO_DISK_SPACE) {
	    /* restart the transaction to give the journal a chance to free
	    ** some blocks.  releases the path, so we have to go back to
	    ** research if we succeed on the second try
	    */
	    restart_transaction(&th, inode, &path) ; 
	    repeat = _allocate_block(&th, inode,&allocated_block_nr,tag,create);

	    if (repeat != NO_DISK_SPACE) {
		allocated_block_nr = 0 ; /* just in case it got changed somehow */
		goto research ;
	    }
	    retval = -ENOSPC;
	    goto failure;
	}

	if (fs_changed (fs_gen, inode->i_sb) && item_moved (&tmp_ih, &path)) {
	    goto research;
	}
    }

    if (indirect_item_found (retval, ih)) {
        b_blocknr_t unfm_ptr;
	/* 'block'-th block is in the file already (there is
	   corresponding cell in some indirect item). But it may be
	   zero unformatted node pointer (hole) */
        unfm_ptr = get_block_num (item, pos_in_item);
	if (unfm_ptr == 0) {
	    /* use allocated block to plug the hole */
	    reiserfs_prepare_for_journal(inode->i_sb, bh, 1) ;
	    if (fs_changed (fs_gen, inode->i_sb) && item_moved (&tmp_ih, &path)) {
		reiserfs_restore_prepared_buffer(inode->i_sb, bh) ;
		goto research;
	    }
	    bh_result->b_state |= (1UL << BH_New);
	    item[pos_in_item] = cpu_to_le32 (allocated_block_nr);
	    journal_mark_dirty (&th, inode->i_sb, bh);
	}
	set_block_dev_mapped (bh_result, le32_to_cpu (item[pos_in_item]), inode);
	pathrelse (&path);
	pop_journal_writer(windex) ;
	journal_end(&th, inode->i_sb, jbegin_count) ;
	allow_flush_page_lock(inode->i_sb, bh_result->b_page, inode) ;
	unlock_kernel() ;
	 
	/* the item was found, so the file was not grown or changed. 
	** there is no need to make sure the inode is updated with this 
	** transaction
	*/
	return 0;
    }


    /* desired position is not found or is in the direct item. We have
       to append file with holes up to 'block'-th block converting
       direct items to indirect one if necessary */
    done = 0;
    do {
	if (is_statdata_le_ih (ih)) {
	    __u32 unp = 0;
	    struct cpu_key tmp_key;

	    /* indirect item has to be inserted */
	    make_le_item_head (&tmp_ih, &key, version, 1, TYPE_INDIRECT, 
			       UNFM_P_SIZE, 0/* free_space */);

	    if (cpu_key_k_offset (&key) == 1) {
		/* we are going to add 'block'-th block to the file. Use
		   allocated block for that */
		unp = cpu_to_le32 (allocated_block_nr);
		set_block_dev_mapped (bh_result, allocated_block_nr, inode);
		bh_result->b_state |= (1UL << BH_New);
		done = 1;
	    }
	    tmp_key = key; // ;)
	    set_cpu_key_k_offset (&tmp_key, 1);
	    PATH_LAST_POSITION(&path) ++;

	    retval = reiserfs_insert_item (&th, &path, &tmp_key, &tmp_ih, (char *)&unp);
	    if (retval) {
		reiserfs_free_block (&th, allocated_block_nr);
		goto failure; // retval == -ENOSPC or -EIO or -EEXIST
	    }
	    if (unp)
		inode->i_blocks += inode->i_sb->s_blocksize / 512;
	    //mark_tail_converted (inode);
	} else if (is_direct_le_ih (ih)) {
	    /* direct item has to be converted */
	    loff_t tail_offset;

	    tail_offset = ((le_ih_k_offset (ih) - 1) & ~(inode->i_sb->s_blocksize - 1)) + 1;
	    if (tail_offset == cpu_key_k_offset (&key)) {
		/* direct item we just found fits into block we have
                   to map. Convert it into unformatted node: use
                   bh_result for the convertion */
		set_block_dev_mapped (bh_result, allocated_block_nr, inode);
		unbh = bh_result;
		done = 1;
	    } else {
		/* we have to padd file tail stored in direct item(s)
		   up to block size and convert it to unformatted
		   node. FIXME: this should also get into page cache */
		if (!unbh) {
		    copy_item_head (&tmp_ih, ih);
			 
		    fs_gen = get_generation (inode->i_sb);

		    get_new_buffer (&th, bh, &unbh, &path);
		    if (!unbh) {
			/* restart_transaction calls pathrelse for us */
			restart_transaction(&th, inode, &path) ;
			get_new_buffer (&th, bh, &unbh, &path);
			if (unbh) {
			    goto search_again ;
			}
			reiserfs_free_block (&th, allocated_block_nr);

#ifdef REISERFS_PREALLOCATE
			reiserfs_discard_prealloc (&th, inode); 
#endif
			retval = -ENOSPC;
			goto failure;
		    }
		    //unbh->b_state |= (1UL << BH_New);
		    if (fs_changed (fs_gen, inode->i_sb) && 
		        item_moved (&tmp_ih, &path)) {
			goto search_again ;
		    }
		}
	    }
	    mark_buffer_uptodate (unbh, 1);
	    retval = direct2indirect (&th, inode, &path, unbh, tail_offset);
	    if (retval) {
		if (!done) {
		    // free what has been allocated by get_new_buffer
		    unsigned long tmp = unbh->b_blocknr;

		    bforget (unbh);
		    reiserfs_free_block (&th, tmp);
#ifdef REISERFS_PREALLOCATE
		    reiserfs_discard_prealloc (&th, inode); 
#endif

		}
		reiserfs_free_block (&th, allocated_block_nr);

#ifdef REISERFS_PREALLOCATE
		reiserfs_discard_prealloc (&th, inode); 
#endif
		goto failure;
	    }
	    /* we've converted the tail, so we must 
	    ** flush unbh before the transaction commits
	    */
	    add_to_flushlist(inode, unbh) ;

	    /* mark it dirty now to prevent commit_write from adding
	    ** this buffer to the inode's dirty buffer list
	    */
	    __mark_buffer_dirty(unbh) ;
		  
	    if (!done) {
		// unbh was acquired by get_new_buffer
		/* adding the page to the flush list sets the do not lock
		** flag that gets check when flushing the page at transaction
		** end.  We only want this for pages passed in to get_block, 
		** not for buffer cache pages returned by get_new_buffer().  
		** So, we allow flush page to lock this page before brelsing 
		** the buffer.
		*/
		allow_flush_page_lock(inode->i_sb, unbh->b_page, inode) ;
		brelse (unbh);
	    }
	    //inode->i_blocks += inode->i_sb->s_blocksize / 512;
	    //mark_tail_converted (inode);
	} else {
	    /* append indirect item with holes if needed, when appending
	       pointer to 'block'-th block use block, which is already
	       allocated */
	    struct cpu_key tmp_key;
	    struct unfm_nodeinfo un = {0, 0};

	    RFALSE( pos_in_item != ih_item_len(ih) / UNFM_P_SIZE,
		    "vs-804: invalid position for append");
	    /* indirect item has to be appended, set up key of that position */
	    make_cpu_key (&tmp_key, inode,
			  le_key_k_offset (version, &(ih->ih_key)) + op_bytes_number (ih, inode->i_sb->s_blocksize),
			  //pos_in_item * inode->i_sb->s_blocksize,
			  TYPE_INDIRECT, 3);// key type is unimportant
		  
	    if (cpu_key_k_offset (&tmp_key) == cpu_key_k_offset (&key)) {
		/* we are going to add target block to the file. Use allocated
		   block for that */
		un.unfm_nodenum = cpu_to_le32 (allocated_block_nr);
		set_block_dev_mapped (bh_result, allocated_block_nr, inode);
		bh_result->b_state |= (1UL << BH_New);
		done = 1;
	    } else {
		/* paste hole to the indirect item */
	    }
	    retval = reiserfs_paste_into_item (&th, &path, &tmp_key, (char *)&un, UNFM_P_SIZE);
	    if (retval) {
		reiserfs_free_block (&th, allocated_block_nr);
		goto failure;
	    }
	    if (un.unfm_nodenum)
		inode->i_blocks += inode->i_sb->s_blocksize / 512;
	    //mark_tail_converted (inode);
	}
		
	if (done == 1)
	    break;
	 
	/* this loop could log more blocks than we had originally asked
	** for.  So, we have to allow the transaction to end if it is
	** too big or too full.  Update the inode so things are 
	** consistent if we crash before the function returns
	**
	** release the path so that anybody waiting on the path before
	** ending their transaction will be able to continue.
	*/
	if (journal_transaction_should_end(&th, th.t_blocks_allocated)) {
	  int orig_len_alloc = th.t_blocks_allocated ;
	  pathrelse (&path);
	  reiserfs_update_sd(&th, inode) ;
	  journal_end(&th, inode->i_sb, orig_len_alloc) ;
	  journal_begin(&th, inode->i_sb, orig_len_alloc) ;
	}
search_again:

	retval = search_for_position_by_key (inode->i_sb, &key, &path);
	if (retval == IO_ERROR) {
	    retval = -EIO;
	    goto failure;
	}
	if (retval == POSITION_FOUND) {
	    reiserfs_warning ("vs-825: reiserfs_get_block: "
			      "%K should not be found\n", &key);
	    retval = -EEXIST;
	    goto failure;
	}
	bh = get_last_bh (&path);
	ih = get_ih (&path);
	item = get_item (&path);
	pos_in_item = path.pos_in_item;

    } while (1);


    retval = 0;
    reiserfs_check_path(&path) ;

    //    pathrelse (&path);
    //reiserfs_update_sd(&th, inode) ;
    //pop_journal_writer(windex) ;
    //journal_end(&th, inode->i_sb, jbegin_count) ;
    //allow_flush_page_lock(inode->i_sb, bh_result->b_page, inode) ;
    //unlock_kernel() ;
    //return 0;
    
 failure:
    reiserfs_update_sd(&th, inode) ;
    pop_journal_writer(windex) ;
    journal_end(&th, inode->i_sb, jbegin_count) ;
    allow_flush_page_lock(inode->i_sb, bh_result->b_page, inode) ;
    unlock_kernel() ;
    reiserfs_check_path(&path) ;
    return retval;
}


//
// BAD: new directories have stat data of new type and all other items
// of old type. Version stored in the inode says about body items, so
// in update_stat_data we can not rely on inode, but have to check
// item version directly
//

// called by read_inode
static void init_inode (struct inode * inode, struct path * path)
{
    struct buffer_head * bh;
    struct item_head * ih;
    __u32 rdev;
    //int version = ITEM_VERSION_1;

    bh = PATH_PLAST_BUFFER (path);
    ih = PATH_PITEM_HEAD (path);


    copy_key (INODE_PKEY (inode), &(ih->ih_key));
    inode->i_generation = INODE_PKEY (inode)->k_dir_id;

    INIT_LIST_HEAD(&inode->u.reiserfs_i.i_prealloc_list) ;

    if (stat_data_v1 (ih)) {
	struct stat_data_v1 * sd = (struct stat_data_v1 *)B_I_PITEM (bh, ih);

	inode_items_version (inode) = ITEM_VERSION_1;
	//version = ITEM_VERSION_1;
	inode->i_mode = le16_to_cpu (sd->sd_mode);
	inode->i_nlink = le16_to_cpu (sd->sd_nlink);
	inode->i_uid = le16_to_cpu (sd->sd_uid);
	inode->i_gid = le16_to_cpu (sd->sd_gid);
	inode->i_size = le32_to_cpu (sd->sd_size);
	inode->i_atime = le32_to_cpu (sd->sd_atime);
	inode->i_mtime = le32_to_cpu (sd->sd_mtime);
	inode->i_ctime = le32_to_cpu (sd->sd_ctime);
	inode->i_blocks = le32_to_cpu (sd->u.sd_blocks);
	if (!inode->i_blocks)
	    // files created by <=3.5.16 have 0 here. Calculate
	    // i_blocks using i_size (this gives incorrect value for
	    // files with holes)
	    inode->i_blocks = (inode->i_size + 511) >> 9;
	rdev = le32_to_cpu (sd->u.sd_rdev);
	inode->u.reiserfs_i.i_first_direct_byte = le32_to_cpu (sd->sd_first_direct_byte);
    } else {
	// new stat data found, but object may have old items
	// (directories and symlinks)
	struct stat_data * sd = (struct stat_data *)B_I_PITEM (bh, ih);

	/* both old and new directories have old keys */
	//version = (S_ISDIR (sd->sd_mode) ? ITEM_VERSION_1 : ITEM_VERSION_2);
	if (S_ISDIR (sd->sd_mode) || S_ISLNK (sd->sd_mode))
	    inode_items_version (inode) = ITEM_VERSION_1;
	else
	    inode_items_version (inode) = ITEM_VERSION_2;
	inode->i_mode = le16_to_cpu (sd->sd_mode);
	inode->i_nlink = le32_to_cpu (sd->sd_nlink);
	inode->i_uid = le32_to_cpu (sd->sd_uid);
	inode->i_size = le64_to_cpu (sd->sd_size);
	inode->i_gid = le32_to_cpu (sd->sd_gid);
	inode->i_mtime = le32_to_cpu (sd->sd_mtime);
	inode->i_atime = le32_to_cpu (sd->sd_atime);
	inode->i_ctime = le32_to_cpu (sd->sd_ctime);
	inode->i_blksize = inode->i_sb->s_blocksize;
	inode->i_blocks = le32_to_cpu (sd->sd_blocks);
	rdev = le32_to_cpu (sd->u.sd_rdev);
	if( S_ISCHR( inode -> i_mode ) || S_ISBLK( inode -> i_mode ) )
	    inode->i_generation = INODE_PKEY (inode)->k_dir_id;
	else
	    inode->i_generation = le32_to_cpu( sd->u.sd_generation );
    }

    pathrelse (path);
    if (S_ISREG (inode->i_mode)) {
	inode->i_op = &reiserfs_file_inode_operations;
	inode->i_fop = &reiserfs_file_operations;
	inode->i_mapping->a_ops = &reiserfs_address_space_operations ;
    } else if (S_ISDIR (inode->i_mode)) {
	inode->i_op = &reiserfs_dir_inode_operations;
	inode->i_fop = &reiserfs_dir_operations;
    } else if (S_ISLNK (inode->i_mode)) {
	inode->i_op = &page_symlink_inode_operations;
	inode->i_mapping->a_ops = &reiserfs_address_space_operations;
    } else {
	inode->i_blocks = 0;
	init_special_inode(inode, inode->i_mode, rdev) ;
    }

    //inode_items_version (inode) = version;

}


// update new stat data with inode fields
static void inode2sd (void * sd, struct inode * inode)
{
    struct stat_data * sd_v2 = (struct stat_data *)sd;
    __u16 flags;

    set_sd_v2_mode(sd_v2, inode->i_mode );
    set_sd_v2_nlink(sd_v2, inode->i_nlink );
    set_sd_v2_uid(sd_v2, inode->i_uid );
    set_sd_v2_size(sd_v2, inode->i_size );
    set_sd_v2_gid(sd_v2, inode->i_gid );
    set_sd_v2_mtime(sd_v2, inode->i_mtime );
    set_sd_v2_atime(sd_v2, inode->i_atime );
    set_sd_v2_ctime(sd_v2, inode->i_ctime );
    set_sd_v2_blocks(sd_v2, inode->i_blocks );
    if (S_ISCHR(inode->i_mode) || S_ISBLK(inode->i_mode))
        set_sd_v2_rdev(sd_v2, inode->i_rdev );
    else
        set_sd_v2_generation(sd_v2, inode->i_generation);
    flags = inode -> u.reiserfs_i.i_attrs;
    i_attrs_to_sd_attrs( inode, &flags );
    set_sd_v2_attrs( sd_v2, flags );
}


// used to copy inode's fields to old stat data
static void inode2sd_v1 (void * sd, struct inode * inode)
{
    struct stat_data_v1 * sd_v1 = (struct stat_data_v1 *)sd;

    set_sd_v1_mode(sd_v1, inode->i_mode );
    set_sd_v1_uid(sd_v1, inode->i_uid );
    set_sd_v1_gid(sd_v1, inode->i_gid );
    set_sd_v1_nlink(sd_v1, inode->i_nlink );
    set_sd_v1_size(sd_v1, inode->i_size );
    set_sd_v1_atime(sd_v1, inode->i_atime );
    set_sd_v1_ctime(sd_v1, inode->i_ctime );
    set_sd_v1_mtime(sd_v1, inode->i_mtime );

    if (S_ISCHR(inode->i_mode) || S_ISBLK(inode->i_mode))
        set_sd_v1_rdev(sd_v1, inode->i_rdev );
    else
        set_sd_v1_blocks(sd_v1, inode->i_blocks );

    // Sigh. i_first_direct_byte is back
    set_sd_v1_first_direct_byte(sd_v1, inode->u.reiserfs_i.i_first_direct_byte);
}


/* NOTE, you must prepare the buffer head before sending it here,
** and then log it after the call
*/
static void update_stat_data (struct path * path, struct inode * inode)
{
    struct buffer_head * bh;
    struct item_head * ih;
  
    bh = PATH_PLAST_BUFFER (path);
    ih = PATH_PITEM_HEAD (path);

    if (!is_statdata_le_ih (ih))
	reiserfs_panic (inode->i_sb, "vs-13065: update_stat_data: key %k, found item %h",
			INODE_PKEY (inode), ih);
  
    if (stat_data_v1 (ih)) {
	// path points to old stat data
	inode2sd_v1 (B_I_PITEM (bh, ih), inode);
    } else {
	inode2sd (B_I_PITEM (bh, ih), inode);
    }

    return;
}


void reiserfs_update_sd (struct reiserfs_transaction_handle *th, 
			 struct inode * inode)
{
    struct cpu_key key;
    INITIALIZE_PATH(path);
    struct buffer_head *bh ;
    int fs_gen ;
    struct item_head *ih, tmp_ih ;
    int retval;

    make_cpu_key (&key, inode, SD_OFFSET, TYPE_STAT_DATA, 3);//key type is unimportant
    
    for(;;) {
	/* look for the object's stat data */
	retval = search_item (inode->i_sb, &key, &path);
	if (retval == IO_ERROR) {
	    reiserfs_warning ("vs-13050: reiserfs_update_sd: "
			      "i/o failure occurred trying to update %K stat data",
			      &key);
	    return;
	}
	if (retval == ITEM_NOT_FOUND) {
	    pathrelse(&path) ;
	    if (inode->i_nlink == 0) {
		/*printk ("vs-13050: reiserfs_update_sd: i_nlink == 0, stat data not found\n");*/
		return;
	    }
	    reiserfs_warning ("vs-13060: reiserfs_update_sd: "
			      "stat data of object %k (nlink == %d) not found (pos %d)\n", 
			      INODE_PKEY (inode), inode->i_nlink);
	    reiserfs_check_path(&path) ;
	    return;
	}
	
	/* sigh, prepare_for_journal might schedule.  When it schedules the
	** FS might change.  We have to detect that, and loop back to the
	** search if the stat data item has moved
	*/
	bh = get_last_bh(&path) ;
	ih = get_ih(&path) ;
	copy_item_head (&tmp_ih, ih);
	fs_gen = get_generation (inode->i_sb);
	reiserfs_prepare_for_journal(inode->i_sb, bh, 1) ;
	if (fs_changed (fs_gen, inode->i_sb) && item_moved(&tmp_ih, &path)) {
	    reiserfs_restore_prepared_buffer(inode->i_sb, bh) ;
	    continue ;	/* Stat_data item has been moved after scheduling. */
	}
	break;
    }
    update_stat_data (&path, inode);
    journal_mark_dirty(th, th->t_super, bh) ; 
    pathrelse (&path);
    return;
}

void reiserfs_read_inode(struct inode *inode) {
    make_bad_inode(inode) ;
}


//
// initially this function was derived from minix or ext2's analog and
// evolved as the prototype did
//

/* looks for stat data in the tree, and fills up the fields of in-core
   inode stat data fields */
void reiserfs_read_inode2 (struct inode * inode, void *p)
{
    INITIALIZE_PATH (path_to_sd);
    struct cpu_key key;
    struct reiserfs_iget4_args *args = (struct reiserfs_iget4_args *)p ;
    unsigned long dirino;
    int retval;

    dirino = args->objectid ;

    /* set version 1, version 2 could be used too, because stat data
       key is the same in both versions */
    key.version = KEY_FORMAT_3_5;
    key.on_disk_key.k_dir_id = dirino;
    key.on_disk_key.k_objectid = inode->i_ino;
    key.on_disk_key.u.k_offset_v1.k_offset = SD_OFFSET;
    key.on_disk_key.u.k_offset_v1.k_uniqueness = SD_UNIQUENESS;

    /* look for the object's stat data */
    retval = search_item (inode->i_sb, &key, &path_to_sd);
    if (retval == IO_ERROR) {
	reiserfs_warning ("vs-13070: reiserfs_read_inode2: "
                    "i/o failure occurred trying to find stat data of %K\n",
                    &key);
	make_bad_inode(inode) ;
	return;
    }
    if (retval != ITEM_FOUND) {
	/* a stale NFS handle can trigger this without it being an error */
	pathrelse (&path_to_sd);
	make_bad_inode(inode) ;
	inode->i_nlink = 0;
	return;
    }

    init_inode (inode, &path_to_sd);
   
    /* It is possible that knfsd is trying to access inode of a file
       that is being removed from the disk by some other thread. As we
       update sd on unlink all that is required is to check for nlink
       here. This bug was first found by Sizif when debugging
       SquidNG/Butterfly, forgotten, and found again after Philippe
       Gramoulle <philippe.gramoulle@mmania.com> reproduced it. 

       More logical fix would require changes in fs/inode.c:iput() to
       remove inode from hash-table _after_ fs cleaned disk stuff up and
       in iget() to return NULL if I_FREEING inode is found in
       hash-table. */
    /* Currently there is one place where it's ok to meet inode with
       nlink==0: processing of open-unlinked and half-truncated files
       during mount (fs/reiserfs/super.c:finish_unfinished()). */
    if( ( inode -> i_nlink == 0 ) && 
	! inode -> i_sb -> u.reiserfs_sb.s_is_unlinked_ok ) {
	    reiserfs_warning( "vs-13075: reiserfs_read_inode2: "
			      "dead inode read from disk %K. "
			      "This is likely to be race with knfsd. Ignore\n", 
			      &key );
	    make_bad_inode( inode );
    }

    reiserfs_check_path(&path_to_sd) ; /* init inode should be relsing */

}

/**
 * reiserfs_find_actor() - "find actor" reiserfs supplies to iget4().
 *
 * @inode:    inode from hash table to check
 * @inode_no: inode number we are looking for
 * @opaque:   "cookie" passed to iget4(). This is &reiserfs_iget4_args.
 *
 * This function is called by iget4() to distinguish reiserfs inodes
 * having the same inode numbers. Such inodes can only exist due to some
 * error condition. One of them should be bad. Inodes with identical
 * inode numbers (objectids) are distinguished by parent directory ids.
 *
 */
static int reiserfs_find_actor( struct inode *inode, 
				unsigned long inode_no, void *opaque )
{
    struct reiserfs_iget4_args *args;

    args = opaque;
    /* args is already in CPU order */
    return le32_to_cpu(INODE_PKEY(inode)->k_dir_id) == args -> objectid;
}

struct inode * reiserfs_iget (struct super_block * s, const struct cpu_key * key)
{
    struct inode * inode;
    struct reiserfs_iget4_args args ;

    args.objectid = key->on_disk_key.k_dir_id ;
    inode = iget4 (s, key->on_disk_key.k_objectid, 
		   reiserfs_find_actor, (void *)(&args));
    if (!inode) 
	return ERR_PTR(-ENOMEM) ;

    if (comp_short_keys (INODE_PKEY (inode), key) || is_bad_inode (inode)) {
	/* either due to i/o error or a stale NFS handle */
	iput (inode);
	inode = 0;
    }
    return inode;
}

struct dentry *reiserfs_fh_to_dentry(struct super_block *sb, __u32 *data,
				     int len, int fhtype, int parent) {
    struct cpu_key key ;
    struct inode *inode = NULL ;
    struct list_head *lp;
    struct dentry *result;

    /* fhtype happens to reflect the number of u32s encoded.
     * due to a bug in earlier code, fhtype might indicate there
     * are more u32s then actually fitted.
     * so if fhtype seems to be more than len, reduce fhtype.
     * Valid types are:
     *   2 - objectid + dir_id - legacy support
     *   3 - objectid + dir_id + generation
     *   4 - objectid + dir_id + objectid and dirid of parent - legacy
     *   5 - objectid + dir_id + generation + objectid and dirid of parent
     *   6 - as above plus generation of directory
     * 6 does not fit in NFSv2 handles
     */
    if (fhtype > len) {
	    if (fhtype != 6 || len != 5)
		    printk(KERN_WARNING "nfsd/reiserfs, fhtype=%d, len=%d - odd\n",
			   fhtype, len);
	    fhtype = 5;
    }
    if (fhtype < 2 || (parent && fhtype < 4)) 
	goto out ;

    if (! parent) {
	    /* this works for handles from old kernels because the default
	    ** reiserfs generation number is the packing locality.
	    */
	    key.on_disk_key.k_objectid = data[0] ;
	    key.on_disk_key.k_dir_id = data[1] ;
	    inode = reiserfs_iget(sb, &key) ;
	    if (inode && !IS_ERR(inode) && (fhtype == 3 || fhtype >= 5) &&
		data[2] != inode->i_generation) {
		    iput(inode) ;
		    inode = NULL ;
	    }
    } else {
	    key.on_disk_key.k_objectid = data[fhtype>=5?3:2] ;
	    key.on_disk_key.k_dir_id = data[fhtype>=5?4:3] ;
	    inode = reiserfs_iget(sb, &key) ;
	    if (inode && !IS_ERR(inode) && fhtype == 6 &&
		data[5] != inode->i_generation) {
		    iput(inode) ;
		    inode = NULL ;
	    }
    }
out:
    if (IS_ERR(inode))
	return ERR_PTR(PTR_ERR(inode));
    if (!inode)
        return ERR_PTR(-ESTALE) ;

    /* now to find a dentry.
     * If possible, get a well-connected one
     */
    spin_lock(&dcache_lock);
    for (lp = inode->i_dentry.next; lp != &inode->i_dentry ; lp=lp->next) {
	    result = list_entry(lp,struct dentry, d_alias);
	    if (! (result->d_flags & DCACHE_NFSD_DISCONNECTED)) {
		    dget_locked(result);
		    result->d_vfs_flags |= DCACHE_REFERENCED;
		    spin_unlock(&dcache_lock);
		    iput(inode);
		    return result;
	    }
    }
    spin_unlock(&dcache_lock);
    result = d_alloc_root(inode);
    if (result == NULL) {
	    iput(inode);
	    return ERR_PTR(-ENOMEM);
    }
    result->d_flags |= DCACHE_NFSD_DISCONNECTED;
    return result;

}

int reiserfs_dentry_to_fh(struct dentry *dentry, __u32 *data, int *lenp, int need_parent) {
    struct inode *inode = dentry->d_inode ;
    int maxlen = *lenp;
    
    if (maxlen < 3)
        return 255 ;

    data[0] = inode->i_ino ;
    data[1] = le32_to_cpu(INODE_PKEY (inode)->k_dir_id) ;
    data[2] = inode->i_generation ;
    *lenp = 3 ;
    /* no room for directory info? return what we've stored so far */
    if (maxlen < 5 || ! need_parent)
        return 3 ;

    inode = dentry->d_parent->d_inode ;
    data[3] = inode->i_ino ;
    data[4] = le32_to_cpu(INODE_PKEY (inode)->k_dir_id) ;
    *lenp = 5 ;
    if (maxlen < 6)
	    return 5 ;
    data[5] = inode->i_generation ;
    *lenp = 6 ;
    return 6 ;
}


//
// initially this function was derived from minix or ext2's analog and
// evolved as the prototype did
//
/* looks for stat data, then copies fields to it, marks the buffer
   containing stat data as dirty */
void reiserfs_write_inode (struct inode * inode, int do_sync) {
    int windex ;
    struct reiserfs_transaction_handle th ;
    int jbegin_count = JOURNAL_PER_BALANCE_CNT * 3; 

    lock_kernel() ;
    journal_begin(&th, inode->i_sb, jbegin_count) ;
    windex = push_journal_writer("write_inode") ;
    reiserfs_update_sd (&th, inode);
    pop_journal_writer(windex) ;
    if (do_sync) 
	journal_end_sync(&th, inode->i_sb, jbegin_count) ;
    else
	journal_end(&th, inode->i_sb, jbegin_count) ;
    unlock_kernel() ;
}


/* FIXME: no need any more. right? */
int reiserfs_sync_inode (struct reiserfs_transaction_handle *th, struct inode * inode)
{
  int err = 0;

  reiserfs_update_sd (th, inode);
  return err;
}


/* stat data of new object is inserted already, this inserts the item
   containing "." and ".." entries */
static int reiserfs_new_directory (struct reiserfs_transaction_handle *th, 
				   struct item_head * ih, struct path * path,
				   const struct inode * dir)
{
    struct super_block * sb = th->t_super;
    char empty_dir [EMPTY_DIR_SIZE];
    char * body = empty_dir;
    struct cpu_key key;
    int retval;
    
    _make_cpu_key (&key, KEY_FORMAT_3_5, le32_to_cpu (ih->ih_key.k_dir_id),
		   le32_to_cpu (ih->ih_key.k_objectid), DOT_OFFSET, TYPE_DIRENTRY, 3/*key length*/);
    
    /* compose item head for new item. Directories consist of items of
       old type (ITEM_VERSION_1). Do not set key (second arg is 0), it
       is done by reiserfs_new_inode */
    if (old_format_only (sb)) {
	make_le_item_head (ih, 0, KEY_FORMAT_3_5, DOT_OFFSET, TYPE_DIRENTRY, EMPTY_DIR_SIZE_V1, 2);
	
	make_empty_dir_item_v1 (body, ih->ih_key.k_dir_id, ih->ih_key.k_objectid,
				INODE_PKEY (dir)->k_dir_id, 
				INODE_PKEY (dir)->k_objectid );
    } else {
	make_le_item_head (ih, 0, KEY_FORMAT_3_5, DOT_OFFSET, TYPE_DIRENTRY, EMPTY_DIR_SIZE, 2);
	
	make_empty_dir_item (body, ih->ih_key.k_dir_id, ih->ih_key.k_objectid,
		   		INODE_PKEY (dir)->k_dir_id, 
		   		INODE_PKEY (dir)->k_objectid );
    }
    
    /* look for place in the tree for new item */
    retval = search_item (sb, &key, path);
    if (retval == IO_ERROR) {
	reiserfs_warning ("vs-13080: reiserfs_new_directory: "
			  "i/o failure occurred creating new directory\n");
	return -EIO;
    }
    if (retval == ITEM_FOUND) {
	pathrelse (path);
	reiserfs_warning ("vs-13070: reiserfs_new_directory: "
			  "object with this key exists (%k)", &(ih->ih_key));
	return -EEXIST;
    }

    /* insert item, that is empty directory item */
    return reiserfs_insert_item (th, path, &key, ih, body);
}


/* stat data of object has been inserted, this inserts the item
   containing the body of symlink */
static int reiserfs_new_symlink (struct reiserfs_transaction_handle *th, 
				 struct item_head * ih,
				 struct path * path, const char * symname, int item_len)
{
    struct super_block * sb = th->t_super;
    struct cpu_key key;
    int retval;

    _make_cpu_key (&key, KEY_FORMAT_3_5, 
		   le32_to_cpu (ih->ih_key.k_dir_id), 
		   le32_to_cpu (ih->ih_key.k_objectid),
		   1, TYPE_DIRECT, 3/*key length*/);

    make_le_item_head (ih, 0, KEY_FORMAT_3_5, 1, TYPE_DIRECT, item_len, 0/*free_space*/);

    /* look for place in the tree for new item */
    retval = search_item (sb, &key, path);
    if (retval == IO_ERROR) {
	reiserfs_warning ("vs-13080: reiserfs_new_symlinik: "
			  "i/o failure occurred creating new symlink\n");
	return -EIO;
    }
    if (retval == ITEM_FOUND) {
	pathrelse (path);
	reiserfs_warning ("vs-13080: reiserfs_new_symlink: "
			  "object with this key exists (%k)", &(ih->ih_key));
	return -EEXIST;
    }

    /* insert item, that is body of symlink */
    return reiserfs_insert_item (th, path, &key, ih, symname);
}


/* inserts the stat data into the tree, and then calls
   reiserfs_new_directory (to insert ".", ".." item if new object is
   directory) or reiserfs_new_symlink (to insert symlink body if new
   object is symlink) or nothing (if new object is regular file) */
struct inode * reiserfs_new_inode (struct reiserfs_transaction_handle *th,
				   const struct inode * dir, int mode, 
				   const char * symname, 
				   int i_size, /* 0 for regular, EMTRY_DIR_SIZE for dirs,
						  strlen (symname) for symlinks)*/
				   struct dentry *dentry, struct inode *inode, int * err)
{
    struct super_block * sb;
    INITIALIZE_PATH (path_to_key);
    struct cpu_key key;
    struct item_head ih;
    struct stat_data sd;
    int retval;
  
    if (!dir || !dir->i_nlink) {
	*err = -EPERM;
	iput(inode) ;
	return NULL;
    }

    sb = dir->i_sb;
    inode -> u.reiserfs_i.i_attrs = 
	    dir -> u.reiserfs_i.i_attrs & REISERFS_INHERIT_MASK;
    sd_attrs_to_i_attrs( inode -> u.reiserfs_i.i_attrs, inode );

    /* item head of new item */
    ih.ih_key.k_dir_id = INODE_PKEY (dir)->k_objectid;
    ih.ih_key.k_objectid = cpu_to_le32 (reiserfs_get_unused_objectid (th));
    if (!ih.ih_key.k_objectid) {
	iput(inode) ;
	*err = -ENOMEM;
	return NULL;
    }
    if (old_format_only (sb))
      /* not a perfect generation count, as object ids can be reused, but this
      ** is as good as reiserfs can do right now.
      ** note that the private part of inode isn't filled in yet, we have
      ** to use the directory.
      */
      inode->i_generation = le32_to_cpu (INODE_PKEY (dir)->k_objectid);
    else
#if defined( USE_INODE_GENERATION_COUNTER )
      inode->i_generation = 
	le32_to_cpu( sb -> u.reiserfs_sb.s_rs -> s_inode_generation );
#else
      inode->i_generation = ++event;
#endif
    if (old_format_only (sb))
	make_le_item_head (&ih, 0, KEY_FORMAT_3_5, SD_OFFSET, TYPE_STAT_DATA, SD_V1_SIZE, MAX_US_INT);
    else
	make_le_item_head (&ih, 0, KEY_FORMAT_3_6, SD_OFFSET, TYPE_STAT_DATA, SD_SIZE, MAX_US_INT);


    /* key to search for correct place for new stat data */
    _make_cpu_key (&key, KEY_FORMAT_3_6, le32_to_cpu (ih.ih_key.k_dir_id),
		   le32_to_cpu (ih.ih_key.k_objectid), SD_OFFSET, TYPE_STAT_DATA, 3/*key length*/);

    /* find proper place for inserting of stat data */
    retval = search_item (sb, &key, &path_to_key);
    if (retval == IO_ERROR) {
	iput (inode);
	*err = -EIO;
	return NULL;
    }
    if (retval == ITEM_FOUND) {
	pathrelse (&path_to_key);
	iput (inode);
	*err = -EEXIST;
	return NULL;
    }

    /* fill stat data */
    inode->i_mode = mode;
    inode->i_nlink = (S_ISDIR (mode) ? 2 : 1);
    inode->i_uid = current->fsuid;
    if (dir->i_mode & S_ISGID) {
	inode->i_gid = dir->i_gid;
	if (S_ISDIR(mode))
	    inode->i_mode |= S_ISGID;
    } else
	inode->i_gid = current->fsgid;

    /* symlink cannot be immutable or append only, right? */
    if( S_ISLNK( inode -> i_mode ) )
	    inode -> i_flags &= ~ ( S_IMMUTABLE | S_APPEND );

    inode->i_mtime = inode->i_atime = inode->i_ctime = CURRENT_TIME;
    inode->i_size = i_size;
    inode->i_blocks = (inode->i_size + 511) >> 9;
    inode->u.reiserfs_i.i_first_direct_byte = S_ISLNK(mode) ? 1 : 
      U32_MAX/*NO_BYTES_IN_DIRECT_ITEM*/;

    INIT_LIST_HEAD(&inode->u.reiserfs_i.i_prealloc_list) ;

    if (old_format_only (sb)) {
	if (inode->i_uid & ~0xffff || inode->i_gid & ~0xffff) {
	    pathrelse (&path_to_key);
	    /* i_uid or i_gid is too big to be stored in stat data v3.5 */
	    iput (inode);
	    *err = -EINVAL;
	    return NULL;
	}
	inode2sd_v1 (&sd, inode);
    } else
	inode2sd (&sd, inode);

    // these do not go to on-disk stat data
    inode->i_ino = le32_to_cpu (ih.ih_key.k_objectid);
    inode->i_blksize = PAGE_SIZE;
    inode->i_dev = sb->s_dev;
    inode->i_blksize = sb->s_blocksize;
  
    // store in in-core inode the key of stat data and version all
    // object items will have (directory items will have old offset
    // format, other new objects will consist of new items)
    memcpy (INODE_PKEY (inode), &(ih.ih_key), KEY_SIZE);
    if (old_format_only (sb) || S_ISDIR(mode) || S_ISLNK(mode))
        set_inode_item_key_version (inode, KEY_FORMAT_3_5);
    else
        set_inode_item_key_version (inode, KEY_FORMAT_3_6);
    if (old_format_only (sb))
	set_inode_sd_version (inode, STAT_DATA_V1);
    else
	set_inode_sd_version (inode, STAT_DATA_V2);
    
    /* insert the stat data into the tree */
    retval = reiserfs_insert_item (th, &path_to_key, &key, &ih, (char *)(&sd));
    if (retval) {
	iput (inode);
	*err = retval;
	reiserfs_check_path(&path_to_key) ;
	return NULL;
    }

    if (S_ISDIR(mode)) {
	/* insert item with "." and ".." */
	retval = reiserfs_new_directory (th, &ih, &path_to_key, dir);
    }

    if (S_ISLNK(mode)) {
	/* insert body of symlink */
	if (!old_format_only (sb))
	    i_size = ROUND_UP(i_size);
	retval = reiserfs_new_symlink (th, &ih, &path_to_key, symname, i_size);
    }
    if (retval) {
	iput (inode);
	*err = retval;
	reiserfs_check_path(&path_to_key) ;
	return NULL;
    }

    insert_inode_hash (inode);
    // we do not mark inode dirty: on disk content matches to the
    // in-core one
    reiserfs_check_path(&path_to_key) ;
    return inode;
}

/* If this page has a file tail in it, and
** it was read in by get_block_create_0, the page data is valid,
** but tail is still sitting in a direct item, and we can't write to
** it.  So, look through this page, and check all the mapped buffers
** to make sure they have valid block numbers.  Any that don't need
** to be unmapped, so that block_prepare_write will correctly call
** reiserfs_get_block to convert the tail into an unformatted node
*/
static inline void fix_tail_page_for_writing(struct page *page) {
    struct buffer_head *head, *next, *bh ;

    if (page && page->buffers) {
	head = page->buffers ;
	bh = head ;
	do {
	    next = bh->b_this_page ;
	    if (buffer_mapped(bh) && bh->b_blocknr == 0) {
	        reiserfs_unmap_buffer(bh) ;
	    }
	    bh = next ;
	} while (bh != head) ;
    }
}

//
// this is exactly what 2.3.99-pre9's ext2_readpage is
//
static int reiserfs_readpage (struct file *f, struct page * page)
{
    return block_read_full_page (page, reiserfs_get_block);
}


//
// this is exactly what 2.3.99-pre9's ext2_writepage is
//
static int reiserfs_writepage (struct file *f, struct page * page)
{
    fix_tail_page_for_writing(page) ;
    return block_write_full_page (page, reiserfs_get_block);
}


//
// from ext2_prepare_write, but modified
//
static int reiserfs_prepare_write(struct file *f, struct page *page, unsigned from, unsigned to) {
    fix_tail_page_for_writing(page) ;
    return block_prepare_write(page, from, to, reiserfs_get_block) ;
}


//
// this is exactly what 2.3.99-pre9's ext2_bmap is
//
static int reiserfs_aop_bmap(struct address_space *as, long block) {
  return generic_block_bmap(as, block, reiserfs_bmap) ;
}


static int reiserfs_commit_write(struct file *f, struct page *page, 
                                 unsigned from, unsigned to) {
    struct inode *inode = (struct inode *)(page->mapping->host) ;
    loff_t pos = inode->i_size ;
    int ret = generic_commit_write(f, page, from, to) ;

    /* ok, generic_commit_write might change the inode's sizes
    ** this change will be lost if we don't dirty the inode
    */
    if (pos != inode->i_size) {
	prevent_flush_page_lock(inode->i_sb, page, inode) ;
	reiserfs_write_inode(inode, 0) ;
	allow_flush_page_lock(inode->i_sb, page, inode) ;
    }

    return ret ;

}
struct address_space_operations reiserfs_address_space_operations = {
    writepage: reiserfs_writepage,
    readpage: reiserfs_readpage, 
    sync_page: block_sync_page,
    prepare_write: reiserfs_prepare_write,
    commit_write: reiserfs_commit_write,
    bmap: reiserfs_aop_bmap
} ;
