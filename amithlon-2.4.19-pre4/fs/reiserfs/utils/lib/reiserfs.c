/*
 * Copyright 1996-2000 Hans Reiser
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <asm/types.h>
#include <sys/vfs.h>
#include <string.h>
#include <asm/byteorder.h>
#include <time.h>

#include "misc.h"
#include "vfs.h"
#include "reiserfs_fs.h"
#include "reiserfs.h"


#define reiserfs_sb(buf) ((struct reiserfs_super_block *)(buf))

static int reiserfs_magic_string (char * buf)
{
    return is_reiserfs_magic_string (reiserfs_sb (buf));    
}



/* returns 1 if buf looks like a leaf node, 0 otherwise */
#if 0 // in stree.c now
static int is_leaf (char * buf, int blocksize)
{
    struct block_head * blkh;
    struct item_head * ih;
    int used_space;
    int prev_location;
    int i;
    int nr;

    blkh = (struct block_head *)buf;
    nr = le16_to_cpu (blkh->blk_nr_item);

    if (nr != DISK_LEAF_NODE_LEVEL)
	return 0;

    if (nr < 1 || nr > ((blocksize - BLKH_SIZE) / (IH_SIZE + MIN_ITEM_LEN)))
	/* item number is too big or too small */
	return 0;

    ih = (struct item_head *)(buf + BLKH_SIZE) + nr - 1;
    used_space = BLKH_SIZE + IH_SIZE * nr + (blocksize - ih_location (ih));
    if (used_space != blocksize - le16_to_cpu (blkh->blk_free_space))
	/* free space does not match to calculated amount of use space */
	return 0;

    // FIXME: it is_leaf will hit performance too much - free_space is trustable enough

    /* check tables of item heads */
    ih = (struct item_head *)(buf + BLKH_SIZE);
    prev_location = blocksize;
    for (i = 0; i < nr; i ++, ih ++) {
	if (ih_location (ih) >= blocksize || ih_location (ih) < IH_SIZE * nr)
	    return 0;
	if (ih_item_len (ih) < 1 || ih_itme_len (ih) > MAX_ITEM_LEN (blocksize))
	    return 0;
	if (prev_location - ih_location (ih) != ih_item_len (ih))
	    return 0;
	prev_location = ih_location (ih);
    }

    /* contents of buf looks like leaf so far */
    return 1;
}


/* returns 1 if buf looks like an internal node, 0 otherwise */
static int is_internal (char * buf, int blocksize)
{
    struct block_head * blkh;
    int nr;
    int used_space;

    blkh = (struct block_head *)buf;
    if (le16_to_cpu (blkh->blk_level) <= DISK_LEAF_NODE_LEVEL ||
	le16_to_cpu (blkh->blk_level) > MAX_HEIGHT)
	/* this level is not possible for internal nodes */
	return 0;
    
    nr = le16_to_cpu (blkh->blk_nr_item);
   if (nr > (blocksize - BLKH_SIZE - DC_SIZE) / (KEY_SIZE + DC_SIZE))
	/* for internal which is not root we might check min number of keys */
	return 0;

    used_space = BLKH_SIZE + KEY_SIZE * nr + DC_SIZE * (nr + 1);
    if (used_space != blocksize - le16_to_cpu (blkh->blk_free_space))
	return 0;

    // more check can be written here

    return 1;
}
#endif // is_leaf and is_internal are in stree.c

static int is_leaf (char * buf, int blocksize)
{
    struct block_head * blkh;
    struct item_head * ih;
    int used_space;
    int prev_location;
    int i;
    int nr;

    blkh = (struct block_head *)buf;
    if (le16_to_cpu (blkh->blk_level) != DISK_LEAF_NODE_LEVEL)
	return 0;

    nr = le16_to_cpu (blkh->blk_nr_item);
    if (nr < 1 || nr > ((blocksize - BLKH_SIZE) / (IH_SIZE + MIN_ITEM_LEN)))
	/* item number is too big or too small */
	return 0;

    ih = (struct item_head *)(buf + BLKH_SIZE) + nr - 1;
    used_space = BLKH_SIZE + IH_SIZE * nr + (blocksize - ih_location (ih));
    if (used_space != blocksize - le16_to_cpu (blkh->blk_free_space))
	/* free space does not match to calculated amount of use space */
	return 0;

    // FIXME: it is_leaf will hit performance too much - we may have
    // return 1 here

    /* check tables of item heads */
    ih = (struct item_head *)(buf + BLKH_SIZE);
    prev_location = blocksize;
    for (i = 0; i < nr; i ++, ih ++) {
	if (ih_location (ih) >= blocksize || ih_location (ih) < IH_SIZE * nr)
	    return 0;
	if (ih_item_len (ih) < 1 || ih_item_len (ih) > MAX_ITEM_LEN (blocksize))
	    return 0;
	if (prev_location - ih_location (ih) != ih_item_len (ih))
	    return 0;
	prev_location = ih_location (ih);
    }

    // one may imagine much more checks
    return 1;
}


/* returns 1 if buf looks like an internal node, 0 otherwise */
static int is_internal (char * buf, int blocksize)
{
    struct block_head * blkh;
    int nr;
    int used_space;

    blkh = (struct block_head *)buf;
    if (le16_to_cpu (blkh->blk_level) <= DISK_LEAF_NODE_LEVEL ||
	le16_to_cpu (blkh->blk_level) > MAX_HEIGHT)
	/* this level is not possible for internal nodes */
	return 0;
    
    nr = le16_to_cpu (blkh->blk_nr_item);
   if (nr > (blocksize - BLKH_SIZE - DC_SIZE) / (KEY_SIZE + DC_SIZE))
	/* for internal which is not root we might check min number of keys */
	return 0;

    used_space = BLKH_SIZE + KEY_SIZE * nr + DC_SIZE * (nr + 1);
    if (used_space != blocksize - le16_to_cpu (blkh->blk_free_space))
	return 0;

    // one may imagine much more checks
    return 1;
}

/* sometimes unfomatted node looks like formatted, if we check only
   block_header. This is the reason, why it is so complicated. We
   believe only when free space and item locations are ok 
   */
int not_formatted_node (char * buf, int blocksize)
{
    struct reiserfs_journal_desc * desc;

    if (is_leaf (buf, blocksize))
	return 0;

    if (is_internal (buf, blocksize))
	return 0;

    /* super block? */
    if (reiserfs_magic_string (buf))
	return 0;

    /* journal descriptor block? */
    desc = (struct reiserfs_journal_desc *)buf;
    if (!memcmp(desc->j_magic, JOURNAL_DESC_MAGIC, 8))
	return 0;

    /* contents of buf does not look like reiserfs metadata. Bitmaps
       are possible here */
    return 1;
}


/* is this block bitmap block or block from journal or skipped area or
   super block? This works for both journal format only yet */
int not_data_block (struct super_block * s, b_blocknr_t block)
{
    int i;

    if (block < SB_JOURNAL_BLOCK (s) + JOURNAL_BLOCK_COUNT + 1)
	return 1;
    for (i = 0; i < SB_BMAP_NR (s); i ++)
	if (block == SB_AP_BITMAP (s)[i]->b_blocknr)
	    return 1;
    return 0;
}




//////////////////////////////////////////////////////////
//
// in reiserfs version 0 (undistributed bitmap)
//
static int get_journal_old_start_must (struct reiserfs_super_block * s)
{
    return 3 + s->s_bmap_nr;
}


//
// in reiserfs version 1 (distributed bitmap) journal starts at 18-th
//
static int get_journal_start_must (struct reiserfs_super_block * s)
{
    return REISERFS_DISK_OFFSET_IN_BYTES / s->s_blocksize + 2;
}


int get_journal_start (struct super_block * s)
{
    return s->u.reiserfs_sb.s_rs->s_journal_block;
}


int get_journal_size (struct super_block * s)
{
    return s->u.reiserfs_sb.s_rs->s_orig_journal_size;
}


int is_desc_block (struct buffer_head * bh)
{
    struct reiserfs_journal_desc * desc = bh_desc (bh);

    if (!memcmp(desc->j_magic, JOURNAL_DESC_MAGIC, 8))
	return 1;
    return 0;
}


int does_desc_match_commit (struct reiserfs_journal_desc * desc, 
			    struct reiserfs_journal_commit * commit)
{
    if (commit->j_trans_id != desc->j_trans_id || commit->j_len != desc->j_len || 
	commit->j_len > JOURNAL_TRANS_MAX || commit->j_len <= 0 ) {
	return 1 ;
    }
    return 0 ;
}



/* ./lib/inode.c */extern struct super_operations reiserfs_sops;

//
// FIXME: 4k only now ! 
//

int uread_super_block (struct super_block * s)
{
    struct buffer_head * bh;


    bh = bread (s->s_dev, (REISERFS_DISK_OFFSET_IN_BYTES / 4096), 4096);
    if (!bh)
	goto not_found;

    if (reiserfs_magic_string (bh->b_data) && 
	reiserfs_sb (bh->b_data)->s_journal_block == get_journal_start_must (reiserfs_sb (bh->b_data)))
	/* new super block found and correct journal start */
	goto found;

    /* new super block is not the correct one */
    brelse (bh);

    bh = bread (s->s_dev, 2, 4096);
    if (!bh)
	goto not_found;

    if (reiserfs_magic_string (bh->b_data) && 
	reiserfs_sb (bh->b_data)->s_journal_block == get_journal_old_start_must (reiserfs_sb (bh->b_data)))
	goto found;

    brelse (bh);

 not_found:
    printf ("uread_super_block: neither new nor old reiserfs format found on dev %s\n",
	    kdevname (s->s_dev));
    return 1;

 found:

    s->s_blocksize = __le16_to_cpu (reiserfs_sb (bh->b_data)->s_blocksize);
    s->s_blocksize_bits = 0;
    while ((1 << s->s_blocksize_bits) != s->s_blocksize)
	s->s_blocksize_bits ++;

    SB_BUFFER_WITH_SB (s) = bh;
    SB_DISK_SUPER_BLOCK (s) = reiserfs_sb (bh->b_data);
    s->s_op = &reiserfs_sops;
    return 0;
}


static int new_format (struct super_block * s)
{
    return (SB_JOURNAL_BLOCK (s) == get_journal_start_must (SB_DISK_SUPER_BLOCK (s)));
}



int uread_bitmaps (struct super_block * s)
{
    int i, bmp ;
    struct reiserfs_super_block * rs = SB_DISK_SUPER_BLOCK(s);

    
    SB_AP_BITMAP (s) = getmem (sizeof (struct buffer_head *) * __le16_to_cpu (rs->s_bmap_nr));
    if (!SB_AP_BITMAP (s)) {
	printf ("read_bitmaps: malloc failed\n");
	return 1;
    }

    bmp = SB_BUFFER_WITH_SB (s)->b_blocknr + 1;

    for (i = 0; i < __le16_to_cpu (rs->s_bmap_nr); i ++) {
	SB_AP_BITMAP (s)[i] = bread (s->s_dev, bmp, s->s_blocksize);
	if (!SB_AP_BITMAP (s)[i]) {
	    printf ("read_bitmaps: bread failed\n");
	    return 1;
	}
	if (new_format (s))
	    bmp = (i + 1) * (s->s_blocksize * 8);
	else
	    bmp ++;
    }
    
    return 0;
}



/* prepare stat data of new directory */
void make_dir_stat_data (struct key * dir_key, struct item_head * ih,
			 struct stat_data * sd)
{
    /* insert stat data item */
    copy_key (&(ih->ih_key), dir_key);
    ih->ih_item_len = SD_SIZE;
    ih_version (ih) = ITEM_VERSION_2;
    //    ih->u.ih_free_space = MAX_US_INT;
    //    ih->ih_reserved = 0;
/*    mark_item_unaccessed (ih);*/

    sd->sd_mode = S_IFDIR + 0755;
    sd->sd_nlink = 0;
    sd->sd_uid = 0;
    sd->sd_gid = 0;
    sd->sd_size = EMPTY_DIR_SIZE;
    sd->sd_atime = sd->sd_ctime = sd->sd_mtime = time (NULL);
    sd->u.sd_rdev = 0;
}


