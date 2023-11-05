/*
 * this is to be included by all kernel files if __KERNEL__ undefined
 */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <asm/types.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <malloc.h>
#include <sys/vfs.h>
#include <time.h>
#include <sys/mount.h>
#include <limits.h>

#ifndef __alpha__
#include <asm/bitops.h>
#endif

#include "misc.h"
#include "vfs.h"


#define ext2_set_bit test_and_set_bit
#define ext2_clear_bit test_and_clear_bit
#define ext2_test_bit test_bit
#define ext2_find_next_zero_bit find_next_zero_bit

#include "reiserfs_fs.h"

#define unlock_kernel() {;}
#define lock_kernel() {;}


//
// util versions of reiserfs kernel functions (./lib/vfs.c)
//
extern inline struct buffer_head * reiserfs_bread (kdev_t dev, int block, int size)
{
  return bread (dev, block, size);
}

extern inline struct buffer_head * reiserfs_getblk (kdev_t dev, int block, int size)
{
  return getblk (dev, block, size);
}


#define init_special_inode(a,b,c) {;}
#define list_empty(a) 0

//
// fs/reiserfs/buffer.c
//
//#define reiserfs_file_buffer(bh,state) do {} while (0)
//#define reiserfs_journal_end_io 0
//#define reiserfs_end_buffer_io_sync 0

//
// fs/reiserfs/journal.c
//
#define journal_mark_dirty(th,s,bh) mark_buffer_dirty (bh, 1)
#define journal_mark_dirty_nolog(th,s,bh) mark_buffer_dirty (bh, 1)
#define mark_buffer_journal_new(bh) mark_buffer_dirty (bh, 1)

extern inline int flush_old_commits (struct super_block * s, int i)
{
  return 0;
}

#define journal_begin(th,s,n) do {int fu = n;fu++;(th)->t_super = s;} while (0)
#define journal_release(th,s) do {} while (0)
#define journal_release_error(th,s) do {} while (0)
#define journal_init(s) 0
#define journal_end(th,s,n) do {s=s;} while (0)
#define buffer_journaled(bh) 0
#define journal_lock_dobalance(s) do {} while (0)
#define journal_unlock_dobalance(s) do {} while (0)
#define journal_transaction_should_end(th,n) 1
#define push_journal_writer(s) 1
#define pop_journal_writer(n) do {} while (0)
#define journal_end_sync(th,s,n) do {} while (0)
#define journal_mark_freed(th,s,n) do {} while (0)
#define reiserfs_in_journal(a,b,c,d,e,f) 0
#define flush_async_commits(s) do {} while (0)

//
// fs/reiserfs/resize.c
//
#define reiserfs_resize(s,n) do {} while (0)
#define simple_strtoul strtol

//
//
//
#define EHASHCOLLISION 125

#define S_IRWXUGO (S_IRWXU|S_IRWXG|S_IRWXO)

extern struct super_block g_sb;
#define get_super(a) (&g_sb)
