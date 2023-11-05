#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <asm/types.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>


#include "vfs.h"
#include "misc.h"
#include "reiserfs_fs.h"

//
// ./kernel/sched.h
//
struct fs_struct fs = {{0}, 022, 0, 0};
struct task cur_task = {0, 0, 0, 0, 0, &fs};

void __wake_up(wait_queue_head_t *q, unsigned int mode)
{
    return;
}

void sleep_on(wait_queue_head_t *q)
{
    return;
}

long interruptible_sleep_on_timeout(wait_queue_head_t *q,
				    signed long timeout)
{
    return 0;
}

//
// ./kernel/sys.c
//
int in_group_p(gid_t grp)
{
    return 0;
}


//
// ./mm/page_alloc.c
//
unsigned long get_free_page (void)
{
    return (unsigned long)getmem (PAGE_SIZE);
}


//
// ./mm/filemap.c
//
struct page *grab_cache_page(struct address_space *mapping, unsigned long index)
{
    return get_free_page ();
}


int generic_file_mmap(struct file * file, struct vm_area_struct * vma)
{
    return 0;
}


ssize_t generic_file_write(struct file * file, const char * buf, size_t count,
			   loff_t * ppos)
{
	struct dentry	*dentry = file->f_dentry; 
	struct inode	*inode = dentry->d_inode; 
	struct address_space *mapping = inode->i_mapping;
	//	unsigned long	limit = current->rlim[RLIMIT_FSIZE].rlim_cur;
	loff_t		pos;
	struct page	*page, *cached_page;
	unsigned long	written;
	long		status = 0;
	int		err;

	cached_page = NULL;
	down(&inode->i_sem);

	pos = *ppos;
	err = -EINVAL;
	err = file->f_error;
	if (err) {
		file->f_error = 0;
		goto out;
	}

	written = 0;

	//	if (file->f_flags & O_APPEND)
	//pos = inode->i_size;

	while (count) {
		unsigned long bytes, index, offset;
		char *kaddr;

		/*
		 * Try to find the page in the cache. If it isn't there,
		 * allocate a free page.
		 */
		offset = (pos & (PAGE_CACHE_SIZE -1)); /* Within page */
		index = pos >> PAGE_CACHE_SHIFT;
		bytes = PAGE_CACHE_SIZE - offset;
		if (bytes > count)
			bytes = count;

		status = -ENOMEM;	/* we'll assign it later anyway */
		//page = __grab_cache_page(mapping, index, &cached_page);
		page = grab_cache_page(mapping, index);
		if (!page)
			break;

		/* We have exclusive IO access to the page.. */
		//		if (!PageLocked(page)) {
		//	PAGE_BUG(page);
		//}

		status = mapping->a_ops->prepare_write(file, page, offset, offset+bytes);
		if (status)
			goto unlock;
		kaddr = (char*)page_address(page);
		memcpy/*copy_from_user*/(kaddr+offset, buf, bytes);
		//if (status)
		//goto fail_write;
		status = mapping->a_ops->commit_write(file, page, offset, offset+bytes);
		if (!status)
			status = bytes;

		if (status >= 0) {
			written += status;
			count -= status;
			pos += status;
			buf += status;
			if (pos > inode->i_size)
				inode->i_size = pos;
		}
unlock:
		/* Mark it unlocked again and drop the page.. */
		UnlockPage(page);
		page_cache_release(page);

		if (status < 0)
			break;
	}
	*ppos = pos;

	//if (cached_page)
	//page_cache_free(cached_page);

	err = written ? written : status;
out:
	up(&inode->i_sem);
	return err;
//fail_write:
	//status = -EFAULT;
	//	ClearPageUptodate(page);
	//	kunmap(page);
	goto unlock;
}


//
// ../mm/memiry.c
//
void vmtruncate(struct inode * inode, unsigned long offset)
{
}


//
// ./fs/devices.c
//
struct inode_operations chrdev_inode_operations = {0,};
struct inode_operations blkdev_inode_operations = {0,};

char * kdevname(kdev_t dev)
{
    static char name[20];
    struct stat st;

    if (fstat (dev, &st) == -1)
	die ("kdevname: fstat failed: %s", strerror (errno));
    sprintf (name, "[%Lx:%Lx]", MAJOR (st.st_rdev), MINOR (st.st_rdev));
    return name;
}


//
// ./fs/fifo.c
//
void init_fifo(struct inode * inode)
{
}


//
// ./fs/attr.c
//
int inode_change_ok(struct inode *inode, struct iattr *attr)
{
    return 0;
}


//
// ./fs/inode.c
//
LIST_HEAD(inode_in_use);

#define NR_INODES 1000

struct inode * first_inode;
int inodes = 0;

struct inode * find_inode (unsigned long ino)
{
  struct inode * inode;

  inode = first_inode;
  if (inode == 0)
    return 0;

  while (1) {
    if (inode->i_ino == ino) {
      inode->i_count ++;
      return inode;
    }
    inode = inode->i_next;
    if (inode == first_inode)
      break;
  }
  return 0;
}


static void clean_inode(struct inode *inode)
{
    static struct address_space_operations empty_aops = {};
    static struct inode_operations empty_iops = {};
    static struct file_operations empty_fops = {};
    memset(&inode->u, 0, sizeof(inode->u));
    inode->i_op = &empty_iops;
    inode->i_fop = &empty_fops;
    inode->i_nlink = 1;
    inode->i_size = 0;
    inode->i_data.a_ops = &empty_aops;
    inode->i_data.host = (void*)inode;
    inode->i_mapping = &inode->i_data;
}

struct inode * get_empty_inode (void)
{
  struct inode * inode, * prev, * next;

  if (inodes == NR_INODES) {
    first_inode->i_sb->s_op->write_inode (first_inode);

    /* set all but i_next and i_prev to 0 */
    next = first_inode->i_next;
    prev = first_inode->i_prev;
    memset (first_inode, 0, sizeof (struct inode));
    first_inode->i_next = next;
    first_inode->i_prev = prev;
    
    /* move to end of list */
    first_inode = first_inode->i_next;
    return first_inode->i_prev;
  }
  /* allocate new inode */
  inode = getmem (sizeof (struct inode));
  if (!inode)
    return 0;

    /* add to end of list */
    if (first_inode) {
	inode->i_prev = first_inode->i_prev;
	inode->i_next = first_inode;
	first_inode->i_prev->i_next = inode;
	first_inode->i_prev = inode;
    } else {
	first_inode = inode->i_next = inode->i_prev = inode;
    }
    inode->i_count = 1;
    clean_inode (inode);
    return inode;
}


void insert_inode_hash (struct inode * inode)
{
}

static struct inode * get_new_inode (struct super_block *sb, unsigned long ino,
				     unsigned long dirino)
{
  struct inode * inode;

  inode = get_empty_inode ();
  if (inode) {
    inode->i_sb = sb;
    inode->i_dev = sb->s_dev;
    inode->i_ino = ino;
    sb->s_op->read_inode2 (inode, (void *)dirino);
    return inode;
  }
  return 0;
}



struct inode *iget4 (struct super_block *sb, unsigned long ino, void * notused,
		     void * dirino)
{
    struct inode * inode;

    inode = find_inode (ino);
    if (inode)
	return inode;
    return get_new_inode (sb, ino, (unsigned long)dirino);
    return 0;
}

void iput (struct inode *inode)
{
    if (inode) {
	if (inode->i_count == 0)
	    die ("iput: can not free free inode");

	if (inode->i_op && inode->i_fop && 
	    inode->i_fop->release)
	  inode->i_fop->release (inode, 0);
	if (inode->i_sb->s_op->put_inode)
	  inode->i_sb->s_op->put_inode (inode);
	inode->i_count --;

	if (inode->i_nlink == 0) {
	    inode->i_sb->s_op->delete_inode (inode);
	    return;
	}
	if (inode->i_state & I_DIRTY) {
	    inode->i_sb->s_op->write_inode (inode);
	    inode->i_state &= ~I_DIRTY;
	}
    }
}


void clear_inode (struct inode *inode)
{
}


void sync_inodes(kdev_t dev)
{
}


void update_atime (struct inode *inode)
{
}

void __wait_on_inode(struct inode * inode)
{
}

//
// ./fs/bad_ionde.c
//
void make_bad_inode(struct inode * inode)
{
}



//
// ./fs/super.c
//
LIST_HEAD(super_blocks);


//
// ./fs/read_write.c
//
ssize_t generic_read_dir(struct file *filp, char *buf, size_t siz, loff_t *ppos)
{
    return -EINVAL;
}

//
// ./fs/dcache.c
//
void d_instantiate(struct dentry *entry, struct inode * inode)
{
    entry->d_inode = inode;
}

void d_delete(struct dentry * dentry)
{
}

void d_move(struct dentry * d1, struct dentry * d2)
{
}

int is_subdir(struct dentry * new_dentry, struct dentry * old_dentry)
{
    return 0;
}

struct dentry * d_alloc_root(struct inode * inode)
{
    return 0;
}

void dput(struct dentry *dentry)
{
}


//
// ./fs/namei.c
//
struct dentry * lookup_dentry(const char * name, struct dentry * base, unsigned int lookup_flags)
{
  return 0;
}

struct inode_operations page_symlink_inode_operations = {
};

//
// ./fs/buffer.c
//
#define MAX_NR_BUFFERS 4000
static int g_nr_buffers;

#define NR_HASH_QUEUES 20
static struct buffer_head * g_a_hash_queues [NR_HASH_QUEUES];
static struct buffer_head * g_buffer_list_head;
static struct buffer_head * g_buffer_heads;

static void insert_into_hash_queue (struct buffer_head * bh)
{
    int index = bh->b_blocknr % NR_HASH_QUEUES;

    if (bh->b_hash_prev || bh->b_hash_next)
	die ("insert_into_hash_queue: hash queue corrupted");

    if (g_a_hash_queues[index]) {
	g_a_hash_queues[index]->b_hash_prev = bh;
	bh->b_hash_next = g_a_hash_queues[index];
    }
    g_a_hash_queues[index] = bh;

/*  check_hash_queues ();*/
}


static void remove_from_hash_queue (struct buffer_head * bh)
{
    if (bh->b_hash_next == 0 && bh->b_hash_prev == 0 && bh != g_a_hash_queues[bh->b_blocknr % NR_HASH_QUEUES])
	/* (b_dev == 0) ? */
	return;

    if (bh == g_a_hash_queues[bh->b_blocknr % NR_HASH_QUEUES]) {
	if (bh->b_hash_prev != 0)
	    die ("remove_from_hash_queue: hash queue corrupted");
	g_a_hash_queues[bh->b_blocknr % NR_HASH_QUEUES] = bh->b_hash_next;
    }
    if (bh->b_hash_next)
	bh->b_hash_next->b_hash_prev = bh->b_hash_prev;

    if (bh->b_hash_prev)
	bh->b_hash_prev->b_hash_next = bh->b_hash_next;

    bh->b_hash_prev = bh->b_hash_next = 0;

/*  check_hash_queues ();*/
}


static void put_buffer_list_end (struct buffer_head * bh)
{
    struct buffer_head * last = 0;

    if (bh->b_prev || bh->b_next)
	die ("put_buffer_list_end: buffer list corrupted");

    if (g_buffer_list_head == 0) {
	bh->b_next = bh;
	bh->b_prev = bh;
	g_buffer_list_head = bh;
    } else {
	last = g_buffer_list_head->b_prev;
    
	bh->b_next = last->b_next;
	bh->b_prev = last;
	last->b_next->b_prev = bh;
	last->b_next = bh;
    }
}


static void remove_from_buffer_list (struct buffer_head * bh)
{
    if (bh == bh->b_next) {
	g_buffer_list_head = 0;
    } else {
	bh->b_prev->b_next = bh->b_next;
	bh->b_next->b_prev = bh->b_prev;
	if (bh == g_buffer_list_head)
	    g_buffer_list_head = bh->b_next;
    }

    bh->b_next = bh->b_prev = 0;
}


static void put_buffer_list_head (struct buffer_head * bh)
{
    put_buffer_list_end (bh);
    g_buffer_list_head = bh;
}


#define GROW_BUFFERS__NEW_BUFERS_PER_CALL 10
/* creates number of new buffers and insert them into head of buffer list 
 */
static int grow_buffers (int size)
{
    int i;
    struct buffer_head * bh, * tmp;

    if (g_nr_buffers + GROW_BUFFERS__NEW_BUFERS_PER_CALL > MAX_NR_BUFFERS)
	return 0;

    /* get memory for array of buffer heads */
    bh = (struct buffer_head *)getmem (GROW_BUFFERS__NEW_BUFERS_PER_CALL * sizeof (struct buffer_head) + sizeof (struct buffer_head *));
    if (g_buffer_heads == 0)
	g_buffer_heads = bh;
    else {
	/* link new array to the end of array list */
	tmp = g_buffer_heads;
	while (*(struct buffer_head **)(tmp + GROW_BUFFERS__NEW_BUFERS_PER_CALL) != 0)
	    tmp = *(struct buffer_head **)(tmp + GROW_BUFFERS__NEW_BUFERS_PER_CALL);
	*(struct buffer_head **)(tmp + GROW_BUFFERS__NEW_BUFERS_PER_CALL) = bh;
    }

    for (i = 0; i < GROW_BUFFERS__NEW_BUFERS_PER_CALL; i ++) {

	tmp = bh + i;
	memset (tmp, 0, sizeof (struct buffer_head));
	tmp->b_data = getmem (size);
	if (tmp->b_data == 0)
	    die ("grow_buffers: no memory for new buffer data");
	tmp->b_dev = 0;
	tmp->b_size = size;
	put_buffer_list_head (tmp);

	g_nr_buffers ++;
    }
    return GROW_BUFFERS__NEW_BUFERS_PER_CALL;
}


static struct buffer_head * find_buffer (int dev, int block, 
					 int size)
{		
  struct buffer_head * next;

  next = g_a_hash_queues[block % NR_HASH_QUEUES];
  for (;;) {
    struct buffer_head *tmp = next;
    if (!next)
      break;
    next = tmp->b_hash_next;
    if (tmp->b_blocknr != block || tmp->b_size != size || tmp->b_dev != dev)
      continue;
    next = tmp;
    break;
  }
  return next;
}




struct buffer_head * get_hash_table(kdev_t dev, int block, int size)
{
  struct buffer_head * bh;

  bh = find_buffer (dev, block, size);
  if (bh) {
    atomic_inc(&bh->b_count);
  }
  return bh;
}


/* if sync != 0, then do not free memory used by buffer cache */
static void sync_buffers (int size, int sync)
{
    struct buffer_head * next = g_buffer_list_head;
    int written = 0;

    for (;;) {
	if (!next)
	    die ("sync_buffers: buffer list is corrupted");
    
	if ((!size || next->b_size == size) && buffer_dirty (next) && 
	    buffer_uptodate (next) && atomic_read (&next->b_count) == 0) {
	    written ++;
	    bwrite (next);
	    if (size && written == 10)
		/* when size is not 0, write only 10 blocks */
		return;
	}
    
	next = next->b_next;
	if (next == g_buffer_list_head)
	    break;
    }

    if (!sync) {
	int i = 0;
	next = g_buffer_list_head;
	for (;;) {
	    if (!next)
		die ("sync_buffers: buffer list is corrupted");
	    if (atomic_read (&next->b_count) != 0)
		die ("sync_buffers: not free buffer (%d, %d, %d)",
		     next->b_blocknr, next->b_size, atomic_read (&next->b_count));

	    if (buffer_dirty (next) && buffer_uptodate (next))
		die ("sync_buffers: dirty buffer found");

	    freemem (next->b_data);
	    i ++;
	    next = next->b_next;
	    if (next == g_buffer_list_head)
		break;
	}
	if (i != g_nr_buffers)
	    die ("sync_buffers: found %d buffers, must be %d", i, g_nr_buffers);

	/* free buffer heads */
	while ((next = g_buffer_heads)) {
	    g_buffer_heads = *(struct buffer_head **)(next + GROW_BUFFERS__NEW_BUFERS_PER_CALL);
	    freemem (next);
	}
    }
}

void invalidate_buffers(kdev_t dev)
{ 
}

void show_buffers (int dev, int size)
{
    int all = 0;
    int dirty = 0;
    int in_use = 0; /* count != 0 */
    int free = 0;
    struct buffer_head * next = g_buffer_list_head;

    for (;;) {
	if (!next)
	    die ("show_buffers: buffer list is corrupted");
	if (next->b_dev == dev && next->b_size == size) {
	    all ++;
	    if (atomic_read (&next->b_count) != 0) {
		in_use ++;
	    }
	    if (buffer_dirty (next)) {
		dirty ++;
	    }
	    if (!buffer_dirty (next) && atomic_read (&next->b_count) == 0) {
		free ++;
	    }
	}
	next = next->b_next;
	if (next == g_buffer_list_head)
	    break;
    }

    printf ("show_buffers (dev %d, size %d): free %d, count != 0 %d, dirty %d, all %d\n",
	    dev, size, free, in_use, dirty, all);
}

static struct buffer_head * get_free_buffer (int size)
{
    struct buffer_head * next;
    int growed = 0;

 repeat:
    next = g_buffer_list_head;
    if (!next)
	goto grow;
    
    for (;;) {
	if (!next)
	    die ("get_free_buffer: buffer list is corrupted");
	if (atomic_read (&next->b_count) == 0 && !buffer_dirty (next)
	    && next->b_size == size) {
	    remove_from_hash_queue (next);
	    remove_from_buffer_list (next);
	    put_buffer_list_end (next);
	    return next;
	}
	next = next->b_next;
	if (next == g_buffer_list_head)
	    break;
    }

 grow:
    if (grow_buffers (size) == 0) {
	/* this write dirty buffers and they become reusable if their
           b_count == 0 */
	sync_buffers (size, 10);
    }
    if (growed == 0) {
	growed = 1;
	goto repeat;
    }

    return 0;
}


struct buffer_head * getblk (kdev_t dev, int block, int size)
{
  struct buffer_head * bh;

  bh = find_buffer (dev, block, size);
  if (bh) {
    if (!buffer_uptodate (bh))
      die ("getblk: buffer must be uptodate");
    atomic_inc (&bh->b_count);
    return bh;
  }

  bh = get_free_buffer (size);
  if (!bh)
      die ("getblk: no free buffers");
  atomic_set (&bh->b_count, 1);
  bh->b_dev = dev;
  bh->b_size = size;
  bh->b_blocknr = block;
  memset (bh->b_data, 0, size);
  clear_bit(BH_Dirty, &bh->b_state);
  clear_bit(BH_Uptodate, &bh->b_state);

  insert_into_hash_queue (bh);

  return bh;
}


struct buffer_head * bread (kdev_t dev, int block, int size)
{
    struct buffer_head * bh;

    bh = getblk (dev, block, size);
    if (buffer_uptodate (bh))
	return bh;

    ll_rw_block (READ, 1, &bh);
    mark_buffer_uptodate (bh, 1);
    return bh;
}


void brelse (struct buffer_head * bh)
{
    if (bh == 0)
	return;
    if (atomic_read (&bh->b_count) == 0)
	die ("brelse: can not free a free buffer");

    atomic_dec (&bh->b_count);
}


void bforget (struct buffer_head * bh)
{
    if (bh) {
	brelse (bh);
	remove_from_hash_queue (bh);
	remove_from_buffer_list (bh);
	put_buffer_list_head (bh);
    }
}

void balance_dirty(kdev_t dev)
{
}

int fsync_dev (kdev_t dev)
{
    sync_buffers (0, dev);
    return 0;
}


int file_fsync(struct file * file, struct dentry * dentry)
{
    return 0;
}


//int block_write_partial_page (struct dentry * dentry, struct page *page, unsigned long offset, unsigned long bytes, const char * buf)
//{
//    return 0;
//}

int block_write_full_page (struct page *page, get_block_t *get_block)
{
    return 0;
}
int block_sync_page (struct page *page) 
{
    return 0;
}


int block_read_full_page (struct page * page, get_block_t * get_block)
{
    struct inode * inode = (struct inode*)page->mapping->host;
    struct buffer_head * bh;

    if (inode->i_sb->s_blocksize != PAGE_SIZE)
	die ("block_read_full_page: block size must be 4k");
    bh = get_free_buffer (inode->i_sb->s_blocksize);
    if (!bh)
	return -ENOMEM;
    atomic_set (&(bh->b_count), 1);
    get_block(inode, page->index, bh, 0);
    if (!buffer_mapped (bh)) {
      memset(bh->b_data, 0, bh->b_size);
      set_bit(BH_Uptodate, &bh->b_state);
    } else {
      ll_rw_block (READ, 1, &bh);
    }
    memcpy (page->address, bh->b_data, bh->b_size);
    SetPageUptodate (page);
    UnlockPage (page);
    brelse (bh);
    return 0;
}

int block_prepare_write(struct page *page, unsigned from, unsigned to,
			get_block_t *get_block)
{
    struct buffer_head bh = {0,};
    struct buffer_head * pbh = &bh;
    struct inode *inode = (struct inode*)page->mapping->host;
    int block;
    int nr;

    bh.b_data = getmem (inode->i_sb->s_blocksize);
    bh.b_size = inode->i_sb->s_blocksize;
    block = page->index << PAGE_CACHE_SHIFT;

    get_block (inode, block, &bh, 1);
    nr = bh.b_size;
    if ((to - from + 1) < nr)
	nr = to - from + 1;
    if (nr != bh.b_size) {
	ll_rw_block (READ, 1, &pbh);
	wait_on_buffer (&bh);
    }

    return 0;
}


int generic_block_bmap(struct address_space *mapping, long block, get_block_t *get_block)
{
    struct buffer_head tmp;
    struct inode *inode = (struct inode*)mapping->host;
    tmp.b_state = 0;
    tmp.b_blocknr = 0;
    get_block(inode, block, &tmp, 0);
    return tmp.b_blocknr;
}

int generic_commit_write(struct file *file, struct page *page,
		unsigned from, unsigned to)
{
    return 0;
}

int generic_buffer_fdatasync(struct inode *inode, unsigned long start_idx, unsigned long end_idx)
{
    return 0;
}

//
// ./mm/filemap.c
//
void do_generic_file_read(struct file * filp, loff_t *ppos, read_descriptor_t * desc, read_actor_t actor)
{
    struct inode * inode = filp->f_dentry->d_inode;
    struct address_space *mapping = inode->i_mapping;
    struct page * page;
    unsigned long index, offset;

    index = *ppos >> PAGE_SHIFT;
    offset = *ppos & ~PAGE_CACHE_MASK;

    while (1) {
	unsigned long end_index, nr;

	end_index = inode->i_size >> PAGE_SHIFT;
	if (index > end_index)
	    break;
	nr = PAGE_SIZE;
	if (index == end_index) {
	    nr = inode->i_size & ~PAGE_CACHE_MASK;
	    if (nr <= offset)
		break;
	}

	nr = nr - offset;

	/* allocate page to read in */
	page = getmem (sizeof (struct page));
	page->address = getmem (PAGE_SIZE);
	page->index = index;
	page->mapping = mapping;
	if (mapping->a_ops->readpage(filp->f_dentry, page) != 0)
	//if (inode->i_op->readpage (filp->f_dentry, page) != 0)
	    die ("do_generic_file_read: readpage failed");
	
	/* actor */
	if (nr > desc->count)
	    nr = desc->count;

	if (actor) {
	    actor (desc, page, offset, nr);
	} else {
	    memcpy (desc->buf, page_address (page) + offset, nr);
	    desc->buf += nr;
	    desc->count -= nr;
	    desc->written += nr;
	}
/*	memcpy (buf, page_address (page) + offset, nr);*/
	offset += nr;
/*	buf += nr;
	count -= nr;*/
/*	retval += nr;*/

	index += offset >> PAGE_SHIFT;
	offset &= ~PAGE_CACHE_MASK;
 	page_cache_release (page);
	if (!desc->count)
	    break;
    }

    *ppos = ((loff_t) index << PAGE_SHIFT) + offset;
    return;
}


ssize_t generic_file_read(struct file * filp, char * buf, size_t count, loff_t *ppos)
{
    read_descriptor_t desc;

    desc.buf = buf;
    desc.written = 0;
    desc.count = count;
    desc.error = 0;
    do_generic_file_read (filp, ppos, &desc, 0);
    return desc.written;
}


//
// ??????????????
//
int kernel_thread(int (*fn)(void *), void * arg, unsigned long flags)
{
    return 0;
}



//
// ./drivers/block/ll_rw_blk.c
//
int * blksize_size[MAX_BLKDEV] = { NULL, NULL, };
void ll_rw_block(int rw, int nr, struct buffer_head * pbh[])
{
    int i;
    
    for (i = 0; i < nr; i ++) {
	if (rw == WRITE) {
	    if (!buffer_dirty (pbh[i]) || !buffer_uptodate (pbh[i]))
		die ("ll_rw_block: do not write not uptodate or clean buffer");
	    block_write (pbh[i]->b_dev, pbh[i]->b_blocknr, pbh[i]->b_size, pbh[i]->b_data);
	} else {
	    block_read (pbh[i]->b_dev, pbh[i]->b_blocknr, pbh[i]->b_size, pbh[i]->b_data);
	}
    }
}




//
// util versions of reiserfs kernel functions 
//

//void wait_buffer_until_released (struct buffer_head * bh)
//{
//  assert (0);
//}


//int reiserfs_sync_file (struct file * file, struct dentry * dentry)
//{
//  return 0;
//}

void reiserfs_check_buffers (kdev_t dev)
{
}

DECLARE_TASK_QUEUE(reiserfs_end_io_tq) ;

//
// used by utils who works with reiserfs directly (reiserfsck, mkreiserfs, etc)
//



//
// to be deleted
//
#if 0
int is_buffer_suspected_recipient (struct super_block * s, struct buffer_head * bh)
{
  return 0;
}

struct tree_balance;
void preserve_shifted (struct tree_balance * a, struct buffer_head ** b, 
		       struct buffer_head * c, int d, struct buffer_head * e)
{
}

inline void unpreserve (struct super_block * s, struct buffer_head * bh)
{
}

inline void mark_suspected_recipient (struct super_block * sb, struct buffer_head * bh)
{
}

inline void unmark_suspected_recipient (struct super_block * sb, struct buffer_head * bh)
{
}

void add_to_preserve (unsigned long blocknr, struct super_block * sb)
{
}

int maybe_free_preserve_list (struct super_block * sb, int x)
{
  return 0;
}

int get_space_from_preserve_list (struct super_block * s)
{
  return 0;
}


void preserve_invalidate (struct tree_balance * tb, struct buffer_head * bh, struct buffer_head * bh1)
{
}

inline void mark_buffer_unwritten (struct buffer_head * bh)
{
}

int ready_preserve_list (struct tree_balance * tb, struct buffer_head * bh)
{
  return 0;
}

//
// delete the above
//

#endif
