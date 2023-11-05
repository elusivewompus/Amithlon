#ifndef _REISER_FS_I
#define _REISER_FS_I

#define REISERFS_N_BLOCKS 10

#include <linux/list.h>

struct reiserfs_inode_info {
  //struct pipe_inode_info reserved;
  __u32 i_key [4];/* key is still 4 32 bit integer */
  int i_version;  // this says whether file is old or new

  // FIXME: do we need both now?
  atomic_t i_is_being_converted;

  int i_pack_on_close ; // file might need tail packing on close 
  //atomic_t i_read_sync_counter;
  //atomic_t i_converted;
  /* this is set when direct2indirect has been
			   performed. Unset when i_count gets to 0 in
			   iput */

  __u32 i_first_direct_byte; // offset of first byte stored in direct item.
  //int i_has_tail; // 1 if file has bytes stored in direct item(s), 0 otherwise

  int i_data_length;
  __u32 i_data [REISERFS_N_BLOCKS];

  /* pointer to the page that must be flushed before 
  ** the current transaction can commit.
  **
  ** this pointer is only used when the tail is converted back into
  ** a direct item, or the file is deleted
  */
  struct reiserfs_page_list *i_converted_page ;

  /* we save the id of the transaction when we did the direct->indirect
  ** conversion.  That allows us to flush the buffers to disk
  ** without having to upate this inode to zero out the converted
  ** page variable
  */
  int i_conversion_trans_id ;

				/* So is this an extent, or what? What
                                   happens when the disk is badly
                                   fragmented? I would prefer a list
                                   of blocks, and I would prefer that
                                   you simply take the first
                                   PREALLOC_SIZE blocks after
                                   search_start and put them on the
                                   list, contiguous or not. Maybe I
                                   don't fully understand this code. I
                                   really prefer allocate on flush
                                   conceptually..... -Hans */
  //For preallocation
  int i_prealloc_block;
  int i_prealloc_count;

};


#endif
