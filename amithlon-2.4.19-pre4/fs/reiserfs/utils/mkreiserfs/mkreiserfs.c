/*
 * Copyright 1996-1999  Hans Reiser
 */

/* mkreiserfs is very simple. It supports only 4K blocks. It skips
   first 64K of device, and then write super block, first bitmap
   block, journal, root block. All this must fit into number of blocks
   pointed by one bitmap. */
/* TODO: bad block specifying */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <asm/types.h>
#include <sys/vfs.h>

#include "misc.h"
#include "vfs.h"
#include "reiserfs_fs.h"


#define print_usage_and_exit() die ("Usage: %s [ -f ] [-b block-size] [-h rupasov|tea|r5] device [blocks-count]\n\n", argv[0])


#define DEFAULT_BLOCKSIZE 4096
#define MIN_BLOCK_AMOUNT 10000


struct buffer_head * g_sb_bh;
struct buffer_head * g_rb_bh;


int g_block_size = DEFAULT_BLOCKSIZE;
int g_journal_size = JOURNAL_BLOCK_COUNT;
int g_hash = DEFAULT_HASH;
int g_block_number;
int g_blocks_on_device;
struct buffer_head ** g_bmap;


/* Given a file descriptor and an offset, check whether the offset is
   a valid offset for the file - return 0 if it isn't valid or 1 if it
   is */
int valid_offset( int fd, loff_t offset )
{
    char ch;

    if (reiserfs_llseek (fd, offset, 0) < 0)
	return 0;

    if (read (fd, &ch, 1) < 1)
	return 0;

    return 1;
}


/* calculates number of blocks on device */
unsigned long count_blocks (char * filename, int blocksize)
{
    loff_t high, low;
    int fd;

    fd = open (filename, O_RDONLY);
    if (fd < 0)
	die ("count_blocks: open failed (%s)", strerror (errno));


#if defined (BLKGETSIZE)
    {
	long size;
    
	if (ioctl (fd, BLKGETSIZE, &size) >= 0) {
	    close (fd);
	    return  size / (blocksize / 512);
	}
    }
#endif

    low = 0;
    for( high = 1; valid_offset (fd, high); high *= 2 )
	low = high;
    while (low < high - 1) {
	const loff_t mid = ( low + high ) / 2;
      
	if (valid_offset (fd, mid))
	    low = mid;
	else
	    high = mid;
    }
    valid_offset (fd, 0);
    close (fd);
  
    return (low + 1) / (blocksize);
}


static void mark_block_used (b_blocknr_t block, struct buffer_head ** bmap)
{
    int i, j;

    i = block / (g_block_size * 8);
    j = block % (g_block_size * 8);
    reiserfs_progs_set_le_bit (j, bmap[i]->b_data);
    mark_buffer_dirty (bmap[i], 0);
}


static void make_super_block (int dev)
{
    struct reiserfs_super_block * sb;
    __u32 * oids;
    b_blocknr_t blocknr;
    int i;
  
    if (SB_SIZE > g_block_size || g_block_size > REISERFS_DISK_OFFSET_IN_BYTES)
	die ("mkreiserfs: blocksize (%d) too small or too big", g_block_size);
    
    blocknr = REISERFS_DISK_OFFSET_IN_BYTES / g_block_size;

    /* get buffer for super block */
    g_sb_bh = getblk (dev, blocknr, g_block_size);
    sb = (struct reiserfs_super_block *)g_sb_bh->b_data;

    memset(g_sb_bh->b_data, 0, g_block_size) ;

    sb->s_block_count = cpu_to_le32 (g_block_number);	/* size of filesystem in blocks */
    sb->s_bmap_nr = cpu_to_le16 ((g_block_number + (g_block_size * 8 - 1)) / (g_block_size * 8));/* number of needed bitmaps */

    /* used blocks are: blocknr-s skipped, super block, bitmap blocks, journal, root block */
    sb->s_free_blocks = cpu_to_le32 (g_block_number - blocknr -
				     1/* super block */ - g_journal_size - 1/* journal head */
				     - le16_to_cpu (sb->s_bmap_nr) - 1/* root */);
    /* root block is after skipped (blocknr), super block (1), first
       bitmap(1), journal (g_journal_size), journal header(1) */
    sb->s_root_block = cpu_to_le32 (blocknr + 1 + 1 + g_journal_size + 1);
  
    sb->s_version = cpu_to_le16 (REISERFS_VERSION_2);
    sb->s_blocksize = cpu_to_le16 (g_block_size);
    sb->s_state = cpu_to_le16 (REISERFS_VALID_FS);
    sb->s_tree_height = cpu_to_le16 (2);
    sb->s_journal_dev = cpu_to_le32 (0) ;
    sb->s_orig_journal_size = cpu_to_le32 (g_journal_size) ;
    sb->s_journal_trans_max = cpu_to_le32 (0) ;
    sb->s_journal_block_count = cpu_to_le32 (0) ;
    sb->s_journal_max_batch = cpu_to_le32 (0) ;
    sb->s_journal_max_commit_age = cpu_to_le32 (0) ;
    sb->s_journal_max_trans_age = cpu_to_le32 (0) ;
    memcpy (sb->s_magic, REISER2FS_SUPER_MAGIC_STRING, sizeof (REISER2FS_SUPER_MAGIC_STRING));
    sb->s_hash_function_code = cpu_to_le32 (g_hash); 

    /* initialize object map */
    oids = (__u32 *)(sb + 1);

    oids[0] = cpu_to_le32 (1);
    oids[1] = cpu_to_le32 (REISERFS_ROOT_OBJECTID + 1);	/* objectids > REISERFS_ROOT_OBJECTID are free */
    sb->s_oid_cursize = cpu_to_le16 (2);
    sb->s_oid_maxsize = cpu_to_le16 ((g_block_size - SB_SIZE) / sizeof (__u32) / 2 * 2);

    mark_buffer_dirty (g_sb_bh, 0);
    mark_buffer_uptodate (g_sb_bh, 1);

    /* allocate bitmap blocks */
    blocknr ++;
    g_bmap = (struct buffer_head **)getmem (le16_to_cpu (sb->s_bmap_nr) * sizeof (struct buffer_head *));
    for (i = 0; i < sb->s_bmap_nr; i ++) {
	g_bmap[i] = getblk (dev, blocknr, sb->s_blocksize);
	mark_buffer_dirty (g_bmap[i], 0);
	mark_buffer_uptodate (g_bmap[i], 1);
	blocknr = (i + 1) * sb->s_blocksize * 8;
    }

    /* mark skipped blocks busy */
    for (i = 0; i < g_sb_bh->b_blocknr; i ++)
	mark_block_used (i, g_bmap);

    /* mark super block */
    mark_block_used (i++, g_bmap);

    /* first bitmap block */
    mark_block_used (i++, g_bmap);
  
    /* first cautious bitmap block */ /* DELETE */
/*    mark_block_used (i++, g_bmap);*/

    sb->s_journal_block = cpu_to_le32 (i);
    for (; i <= g_sb_bh->b_blocknr + 2 + g_journal_size; i ++)
	mark_block_used (i, g_bmap);

    /* make sure, that all journal blocks pointed by first bitmap */
    if (i >= g_block_size * 8)
	die ("mkreiserfs: Journal too big");
    /* make sure, that we still have some amount (100 blocks) of free space */
    if (i + 100 > g_block_number)
	die ("mkreiserfs: size of file system is too small");
    /* mark root block as used */
    mark_block_used (i, g_bmap);


    /* set up other bitmap blocks */
    for (i = 1; i < le16_to_cpu (sb->s_bmap_nr); i ++) {
	mark_block_used (i * g_block_size * 8, g_bmap);
	/* cautious bitmap block */ /* DELETE */
/*	mark_block_used (i * g_block_size * 8 + 1, g_bmap);*/
    }

    /* unused space of last bitmap is filled by 1s */
    for (i = sb->s_bmap_nr * sb->s_blocksize * 8; --i >= sb->s_block_count; )
	mark_block_used (i, g_bmap);

    return;
}


void zero_journal_blocks(int dev, int start, int len) {
    int i ;
    struct buffer_head *bh ;
    int done = 0;

    printf ("Initializing journal - "); fflush (stdout);

    for (i = 0 ; i < len ; i++) {
	print_how_far (&done, len);
	bh = getblk (dev, start + i, g_block_size) ;
	memset(bh->b_data, 0, g_block_size) ;
	mark_buffer_dirty(bh,0) ;
	mark_buffer_uptodate(bh, 1) ;
	bwrite (bh);
	brelse(bh) ;
    }
    printf ("\n"); fflush (stdout);
}


/* form the root block of the tree (the block head, the item head, the
   root directory) */
void make_root_block ()
{
    struct reiserfs_super_block * sb = (struct reiserfs_super_block *)g_sb_bh->b_data;
    char * rb;
    struct block_head * blkh;
    struct item_head * ih;
    struct stat_data * sd;
    struct key maxkey = {0xffffffff, 0xffffffff, {{0xffffffff, 0xffffffff}, }};


    /* get root block */
    g_rb_bh = getblk (g_sb_bh->b_dev, le32_to_cpu (sb->s_root_block), g_sb_bh->b_size);
    rb = g_rb_bh->b_data;

    /* first item is stat data item of root directory */
    ih = (struct item_head *)(rb + BLKH_SIZE);
    ih_version (ih) = cpu_to_le16 (ITEM_VERSION_2);
    ih->ih_key.k_dir_id = cpu_to_le32 (REISERFS_ROOT_PARENT_OBJECTID);
    ih->ih_key.k_objectid = cpu_to_le32 (REISERFS_ROOT_OBJECTID);
    set_le_ih_k_offset (ih, SD_OFFSET);
    set_le_ih_k_type (ih, TYPE_STAT_DATA);
    ih->ih_item_len = cpu_to_le16 (SD_SIZE);
    ih->ih_item_location = cpu_to_le16 (le16_to_cpu (sb->s_blocksize)
					- SD_SIZE);
    set_ih_free_space (ih, MAX_US_INT);

    /* fill NEW stat data */
    sd = (struct stat_data *)(rb + ih->ih_item_location);
    sd->sd_mode = cpu_to_le16 (S_IFDIR + 0755);
    sd->sd_nlink = cpu_to_le16 (3);
    sd->sd_uid = cpu_to_le32 (getuid ());
    sd->sd_gid = cpu_to_le32 (getgid ());
    sd->sd_size = cpu_to_le64 (EMPTY_DIR_SIZE);
    sd->sd_atime = sd->sd_ctime = sd->sd_mtime = cpu_to_le32 (time (NULL));
    sd->u.sd_rdev = cpu_to_le32 (0);


    /* second item is root directory item, containing "." and ".." */
    ih ++;
    ih_version (ih) = cpu_to_le16 (ITEM_VERSION_1);
    ih->ih_key.k_dir_id = cpu_to_le32 (REISERFS_ROOT_PARENT_OBJECTID);
    ih->ih_key.k_objectid = cpu_to_le32 (REISERFS_ROOT_OBJECTID);
    set_le_ih_k_offset (ih, DOT_OFFSET);
    set_le_ih_k_type (ih, TYPE_DIRENTRY);
    ih->ih_item_len = cpu_to_le16 (DEH_SIZE * 2 + ROUND_UP (strlen (".")) +
				   ROUND_UP (strlen ("..")));
    ih->ih_item_location = cpu_to_le16 (le16_to_cpu ((ih-1)->ih_item_location) - 
					le16_to_cpu (ih->ih_item_len));
    ih->u.ih_entry_count = cpu_to_le32 (2);
  
    /* compose item itself */
    make_empty_dir_item (rb + ih->ih_item_location, 
			 REISERFS_ROOT_PARENT_OBJECTID, REISERFS_ROOT_OBJECTID,
			 0, REISERFS_ROOT_PARENT_OBJECTID);

    /* block head */
    blkh = (struct block_head *)rb;
    blkh->blk_level = cpu_to_le16 (DISK_LEAF_NODE_LEVEL);
    blkh->blk_nr_item = cpu_to_le16 (2);
    blkh->blk_free_space = cpu_to_le16 (le32_to_cpu (sb->s_blocksize) - BLKH_SIZE - 
					2 * IH_SIZE - SD_SIZE - le16_to_cpu (ih->ih_item_len));
    // kept just for compatibility only (not used)
    blkh->blk_right_delim_key = maxkey;

    mark_buffer_dirty (g_rb_bh, 0);
    mark_buffer_uptodate (g_rb_bh, 1);
    return;
}


/*
 *  write the super block, the bitmap blocks and the root of the tree
 */
static void make_new_filesystem (void)
{
    struct reiserfs_super_block * sb = (struct reiserfs_super_block *)g_sb_bh->b_data;
    int i;

    printf ("journal size %d (from %d)\n", 
	    sb->s_orig_journal_size, sb->s_journal_block);

    zero_journal_blocks (g_sb_bh->b_dev, sb->s_journal_block, 
			 sb->s_orig_journal_size + 1);

    /* bitmap blocks */
    for (i = 0; i < sb->s_bmap_nr; i ++) {
	bwrite (g_bmap[i]);
	brelse (g_bmap[i]);
    }

    /* root block */
    bwrite (g_rb_bh);
    brelse (g_rb_bh);

    /* super block */
    bwrite (g_sb_bh);
    brelse (g_sb_bh);
}


void report (void)
{
    struct reiserfs_super_block * sb = (struct reiserfs_super_block *)g_sb_bh->b_data;
    unsigned int i;

    printf ("Block size %d bytes\n", le16_to_cpu (sb->s_blocksize));
    printf ("Block count %d\n", g_block_number);
    printf ("Used blocks %d\n", g_block_number - le32_to_cpu (sb->s_free_blocks));
    printf ("\tJournal - %d blocks (%d-%d), journal header is in block %d\n",
	    g_journal_size, 
	    le32_to_cpu (sb->s_journal_block),
	    le32_to_cpu (sb->s_journal_block) + g_journal_size - 1,
	    le32_to_cpu (sb->s_journal_block) + g_journal_size) ;
    printf ("\tBitmaps: ");
    for (i = 0; i < le16_to_cpu (sb->s_bmap_nr); i ++)
	printf ("%ld%s", g_bmap[i]->b_blocknr, 
		(i == le16_to_cpu (sb->s_bmap_nr) - 1) ? "\n" : ", ");
    printf ("\tRoot block %d\n", le32_to_cpu (sb->s_root_block));
    printf ("Hash function \"%s\"\n", g_hash == TEA_HASH ? "tea" :
	    ((g_hash == YURA_HASH) ? "rupasov" : "r5"));
    fflush (stdout);
}


static void discard_old_filesystems (int dev)
{
    struct buffer_head * bh;

    /* discard vfat/msods (0-th 512 byte sector) and ext2 (1-st 1024
       byte block) */
    bh = getblk (dev, 0, 2048);
    memset (bh->b_data, 0, 2048);
    mark_buffer_uptodate (bh, 1);
    mark_buffer_dirty (bh, 1);
    bwrite (bh);
    brelse (bh);
    
    /* discard super block of reiserfs of old format (8-th 1024 byte block) */
    bh = getblk (dev, 8, 1024);
    memset (bh->b_data, 0, 1024);
    mark_buffer_uptodate (bh, 1);
    mark_buffer_dirty (bh, 1);
    bwrite (bh);
    brelse (bh);
  
}


static void set_block_size (char * str)
{
    char * tmp;

    g_block_size = (int) strtol (str, &tmp, 0);
    if (*tmp)
	die ("mkreiserfs: wrong block size specified: %s\n", str);
      
    if (g_block_size != 4)
	die ("mkreiserfs: %dk is wrong block size", g_block_size);

    g_block_size *= 1024;
}


/* journal must fit into number of blocks pointed by first bitmap */
static void set_journal_size (char * str)
{
    char * tmp;

    g_journal_size = (int) strtol (str, &tmp, 0);
    if (*tmp)
	die ("mkreiserfs: wrong journal size specified: %s\n", str);

    if (g_journal_size < JOURNAL_BLOCK_COUNT / 2 || 
	g_journal_size > JOURNAL_BLOCK_COUNT * 2)
	die ("mkreiserfs: wrong journal size specified: %s\n", str);
}


/* journal size is specified already */
static void set_block_number (char * str)
{
    char * tmp;

    g_block_number = (int) strtol (str, &tmp, 0);
    if (*tmp)
	die ("mkreiserfs: wrong block number specified: %s\n", str);
      
    if (g_block_number > g_blocks_on_device)
      die ("mkreiserfs: there are not so many blocks on device", g_block_number);

}


static void set_hash_function (char * str)
{
    if (!strcmp (str, "tea"))
	g_hash = TEA_HASH;
    else if (!strcmp (str, "rupasov"))
	g_hash = YURA_HASH;
    else if (!strcmp (str, "r5"))
	g_hash = R5_HASH;
    else
	printf ("mkreiserfs: wrong hash type specified. Using default\n");
}

int main (int argc, char **argv)
{
    int dev;
    int force = 0;
    struct stat statbuf;
    char * device_name;
    char c;

    printf ("\n\n<----------- MKREISERFSv2 ----------->\n\n");

#if 1
    clear_inode (0);
    if (0) {
	/* ???? */
	getblk (0,0,0);
	iput (0);
    }
#endif

    if (argc < 2)
	print_usage_and_exit ();

    while ( ( c = getopt( argc, argv, "fb:c:h:" ) ) != EOF )
	switch( c ) {
	case 'f' :                 /* force if file is not a block device */
	    force = 1;
	    break;

	case 'b' :                  /* -b n - where n is 1,2 or 4 */
	    set_block_size (optarg);
	    break;

	case 'c':
	    set_journal_size (optarg);
	    break;

	case 'h':
	    set_hash_function (optarg);
	    break;

	default :
	    print_usage_and_exit ();
	}
    device_name = argv [optind];
    if (is_mounted (device_name))
	die ("mkreiserfs: '%s' contains a mounted file system\n", device_name);
  

    /* get block number for file system */
    g_blocks_on_device = count_blocks (device_name, g_block_size);
    g_block_number = g_blocks_on_device;
    if (optind == argc - 2)
	set_block_number (argv[optind + 1]);
    else if (optind != argc - 1)
	print_usage_and_exit ();

    g_block_number = g_block_number / 8 * 8;
    
    if (g_block_number < MIN_BLOCK_AMOUNT)
	die ("mkreiserfs: block number %d (truncated to n*8) is too low", 
	     g_block_number);

    if (stat (device_name, &statbuf) < 0)
	die ("mkreiserfs: unable to stat %s", device_name);
  
    if (!S_ISBLK (statbuf.st_mode) && ( force == 1 ))
	die ("mkreiserfs: '%s (%o)' is not a block device", device_name, statbuf.st_mode);
    else        /* Ignore any 'full' fixed disk devices */
	if ( statbuf.st_rdev == 0x0300 || statbuf.st_rdev == 0x0340 
	     || statbuf.st_rdev == 0x0400 || statbuf.st_rdev == 0x0410
	     || statbuf.st_rdev == 0x0420 || statbuf.st_rdev == 0x0430
	     || statbuf.st_rdev == 0x0d00 || statbuf.st_rdev == 0x0d40 )
	    /* ???? */
	    die ("mkreiserfs: will not try to make filesystem on '%s'", device_name);
    dev = open (device_name, O_RDWR);
    if (dev == -1)
	die ("mkreiserfs: can not open '%s': %s", device_name, strerror (errno));

    /* these fill buffers (super block, first bitmap, root block) with
       reiserfs structures */
    make_super_block (dev);
    make_root_block ();
  
    report ();

    printf ("ATTENTION: ALL DATA WILL BE LOST ON '%s'! (y/n)", device_name);
    c = getchar ();
    if (c != 'y' && c != 'Y')
	die ("mkreiserfs: Disk was not formatted");

    discard_old_filesystems (dev);
    make_new_filesystem ();

    

    /* 0 means: write all buffers and free memory */
    fsync_dev (0);

    printf ("Syncing.."); fflush (stdout);
    sync ();
    printf ("done.\n\n");

    return 0;

}
