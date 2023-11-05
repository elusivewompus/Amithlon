#include <sys/vfs.h>

//
// ./include/asm-i386/page.h
//
#define BUG() do { \
	printf ("BUG at %s:%d!\n", __FILE__, __LINE__); \
	*(int *)0 = 0;\
} while (0)

#include <asm/atomic.h>

//
// ./include/linux/tty.h
//
#define console_print printf

//
// ./include/linux/kdev_t.h>
//
#include <linux/kdev_t.h>

typedef unsigned long long kdev_t;
static inline kdev_t to_kdev_t(int dev)
{
    return MKDEV (MAJOR(dev), MINOR (dev));
}
#define NODEV 0

//
// ./include/asm/atomic.h
//
// typedef struct { int counter; } atomic_t;
  
#define ATOMIC_INIT(v) { (v) }
#define atomic_read(v) ((v)->counter)
#define atomic_set(v,i) (((v)->counter) = (i))
#define atomic_inc(v) ((v)->counter ++)
#define atomic_dec(v) ((v)->counter --)


//
// ./include/linux/list.h
//
struct list_head {
    struct list_head *next, *prev;
};

#define LIST_HEAD_INIT(name) { &(name), &(name) }

#define LIST_HEAD(name) \
	struct list_head name = LIST_HEAD_INIT(name)

static __inline__ void __list_add(struct list_head * new,
				  struct list_head * prev,
				  struct list_head * next)
{
    next->prev = new;
    new->next = next;
    new->prev = prev;
    prev->next = new;
}

/*
 * Insert a new entry after the specified head..
 */
static __inline__ void list_add(struct list_head *new, struct list_head *head)
{
    __list_add(new, head, head->next);
}

static __inline__ void __list_del(struct list_head * prev,
				  struct list_head * next)
{
	next->prev = prev;
	prev->next = next;
}

static __inline__ void list_del(struct list_head *entry)
{
	__list_del(entry->prev, entry->next);
}


#define list_entry(ptr, type, member) \
	((type *)((char *)(ptr)-(unsigned long)(&((type *)0)->member)))


//
// ./include/linux/wait.h
//
struct wait_queue {
};

typedef struct wait_queue wait_queue_head_t;

#define DECLARE_WAIT_QUEUE_HEAD(name) \
	wait_queue_head_t name

static inline void init_waitqueue_head(wait_queue_head_t *q)
{
}




//
// ./include/linux/sched.h
//
#define SCHED_YIELD		0x10

struct fs_struct {
    atomic_t count;
    int umask;
    struct dentry * root, * pwd;
};

struct task {
    int pid;
    int counter;
    int fsuid;
    int fsgid;
    int need_resched;
    struct fs_struct * fs;
    unsigned long policy;
    int lock_depth;
};
#define schedule() do {} while (0);

extern inline int fsuser(void)
{
    return 0;
}

#define CURRENT_TIME (time(0))

extern void __wake_up(wait_queue_head_t *q, unsigned int mode);
#define wake_up(x) __wake_up(x, 0)
void sleep_on(wait_queue_head_t *q);
extern long interruptible_sleep_on_timeout(wait_queue_head_t *q,
  signed long timeout);
extern int in_group_p(gid_t);
extern inline int capable(int cap)
{
    return 0;
}


//
// ./include/linux/capability.h
//
#define CAP_FSETID           4


//
// ./include/asm/current.h
//
extern struct task cur_task;
#define current (&cur_task)


//
// ./include/linux/dcache.h
//
struct qstr {
    const unsigned char * name;
    unsigned int len;
    unsigned int hash;
};

struct dentry {
    struct inode * d_inode;
    struct qstr d_name;
    unsigned char d_iname[256];
};

static __inline__ void d_add(struct dentry * entry, struct inode * inode)
{
    entry->d_inode = inode;
}

static __inline__ int d_unhashed(struct dentry *dentry)
{
  return 0;
}


extern void d_instantiate(struct dentry *, struct inode *);
extern void d_delete(struct dentry *);
extern void d_move(struct dentry *, struct dentry *);
extern struct dentry * d_alloc_root(struct inode *);
extern void dput(struct dentry *dentry);



//
// ./fs/namei.c
//
struct dentry * lookup_dentry(const char * name, struct dentry * base, 
			      unsigned int lookup_flags);


//
// ./include/linux/mm.h
//
struct vm_area_struct {
};

typedef struct page {
    struct address_space *mapping;
    unsigned long index;
    atomic_t count;
    int flags;
    char * address;
    struct buffer_head * buffers;
} mem_map_t;

#define PG_locked		 0
#define PG_error		 1
#define PG_referenced		 2
#define PG_uptodate		 3

#define Page_Uptodate(page) test_bit(PG_uptodate, &(page)->flags)
#define SetPageUptodate(page) set_bit(PG_uptodate, &(page)->flags)
#define LockPage(page) set_bit(PG_locked, &(page)->flags)
#define UnlockPage(page) clear_bit(PG_locked, &(page)->flags)

extern mem_map_t * mem_map;

#define GFP_KERNEL 0
#define GFP_ATOMIC 1
#define GFP_BUFFER 2

extern void vmtruncate(struct inode * inode, unsigned long offset);

//
// ./include/asm/page.h
//
#ifndef PAGE_SHIFT
#define PAGE_SHIFT 12 // for ALPHA =13
#endif

#ifndef PAGE_SIZE
#define PAGE_SIZE (1UL << PAGE_SHIFT)
#endif

#define PAGE_CACHE_SIZE PAGE_SIZE // ./include/linux/pagemap.h
#define PAGE_MASK (~(PAGE_SIZE-1))
#define PAGE_CACHE_MASK PAGE_MASK
#define PAGE_CACHE_SHIFT PAGE_SHIFT

#define MAP_NR(addr) 


//
// ./linux/include/asm-i386/pgtable.h
//
#define page_address(page) page->address


//
// ./include/linux/pipe_fs_i.h
//
struct pipe_inode_info {
};


//
// ./include/asm/semaphore.h
//
struct semaphore {
};

extern inline void down(struct semaphore * sem)
{
}

extern inline void up(struct semaphore * sem)
{
}


//
// ./include/linux/fs.h
//
#undef BLOCK_SIZE
#define BLOCK_SIZE 1024

#define READ 0
#define WRITE 1

extern void update_atime (struct inode *);
#define UPDATE_ATIME(inode) update_atime (inode)

/*
#if !defined (MS_RDONLY)
#define MS_RDONLY   1
#endif
*/

#define BH_Uptodate	0
#define BH_Dirty	1
#define BH_Lock		2
#define BH_Req		3
#define BH_Mapped	4
#define BH_New		5
#define BH__Protected	6

struct buffer_head {
    unsigned long b_blocknr;
    unsigned short b_size;
    unsigned short b_list;
    kdev_t b_dev;
    atomic_t b_count;
    unsigned long b_state;
    unsigned long b_flushtime;

    struct buffer_head * b_next;
    struct buffer_head * b_prev;
    struct buffer_head * b_hash_next;
    struct buffer_head * b_hash_prev;
    char * b_data;
    struct page * b_page;
    void (*b_end_io)(struct buffer_head *bh, int uptodate);
    struct buffer_head * b_this_page;
};

#include <asm/bitops.h>

#define buffer_uptodate(bh) test_bit(BH_Uptodate, &(bh)->b_state)
#define buffer_dirty(bh) test_bit(BH_Dirty, &(bh)->b_state)
#define buffer_locked(bh) test_bit(BH_Lock, &(bh)->b_state)
#define buffer_mapped(bh) test_bit(BH_Mapped, &(bh)->b_state)
#define buffer_req(bh) test_bit(BH_Req, &(bh)->b_state)

extern inline void mark_buffer_clean(struct buffer_head * bh)
{
    clear_bit(BH_Dirty, &(bh)->b_state);
}

extern inline void mark_buffer_dirty(struct buffer_head * bh, int flag)
{
    set_bit(BH_Dirty, &(bh)->b_state);
}
#define __mark_buffer_dirty mark_buffer_dirty
extern void balance_dirty(kdev_t);

#define ATTR_MODE	1
#define ATTR_UID	2
#define ATTR_GID	4
#define ATTR_SIZE	8
#define ATTR_ATIME	16
#define ATTR_MTIME	32
#define ATTR_CTIME	64
#define ATTR_ATIME_SET	128
#define ATTR_MTIME_SET	256
#define ATTR_FORCE	512	/* Not a change, but a change it */
#define ATTR_ATTR_FLAG	1024

struct iattr {
    unsigned int	ia_valid;
    umode_t		ia_mode;
    uid_t		ia_uid;
    gid_t		ia_gid;
    loff_t		ia_size;
    time_t		ia_atime;
    time_t		ia_mtime;
    time_t		ia_ctime;
    unsigned int	ia_attr_flags;
};

int inode_change_ok(struct inode *inode, struct iattr *attr);

struct file;
struct address_space_operations {
    int (*writepage) (struct file *, struct dentry *, struct page *);
    int (*readpage)(struct dentry *, struct page *);
    int (*sync_page)(struct page *);
    int (*prepare_write)(struct file *, struct page *, unsigned, unsigned);
    int (*commit_write)(struct file *, struct page *, unsigned, unsigned);
    /* Unfortunately this kludge is needed for FIBMAP. Don't use it */
    int (*bmap)(struct address_space *, long);
};


struct address_space {
    //struct list_head	pages;		/* list of pages */
    //unsigned long		nrpages;	/* number of pages */
    struct address_space_operations *a_ops;	/* methods */
    void			*host;		/* owner: inode, block_device */
    //struct vm_area_struct	*i_mmap;	/* list of mappings */
    //spinlock_t		i_shared_lock;  /* and spinlock protecting it */
};


#include "reiserfs_fs_i.h"

struct inode {
    struct list_head i_list;
    unsigned long i_ino;
    unsigned long i_generation;
    unsigned int i_count;
    kdev_t i_dev;
    umode_t i_mode;
    nlink_t i_nlink;
    uid_t	i_uid;
    gid_t	i_gid;
    kdev_t i_rdev;
    loff_t i_size;
    time_t i_atime;
    time_t i_mtime;
    time_t i_ctime;
    unsigned long i_blksize;
    unsigned long	i_blocks;
    struct semaphore i_sem;
    struct inode_operations * i_op;
    struct file_operations * i_fop;
    struct super_block * i_sb;
    struct address_space * i_mapping;
    struct address_space i_data;
    unsigned long i_state;
    unsigned int i_flags;
    struct inode * i_next;
    struct inode * i_prev;
    wait_queue_head_t i_wait;
    union {
	struct reiserfs_inode_info reiserfs_i;
    } u;
};


struct file {
    struct dentry	* f_dentry;
    unsigned int f_flags;
    loff_t f_pos;
    struct file_operations * f_op;
    int f_ramax, f_raend, f_rawin, f_ralen;
    int f_error;
};


#include "reiserfs_fs_sb.h"

extern struct list_head super_blocks;
#define sb_entry(list) list_entry((list), struct super_block, s_list)

struct super_block {
    struct list_head s_list;
    kdev_t s_dev;
    unsigned long s_blocksize;
    int s_blocksize_bits;
    int s_dirt;
    int s_flags;
    struct dentry * s_root;
    struct super_operations * s_op;
    union {
	struct reiserfs_sb_info reiserfs_sb;
    } u;
};


struct file_lock {
};

struct poll_table_struct;
typedef int (*filldir_t)(void *, const char *, int, off_t, ino_t);

struct file_operations {
    loff_t (*llseek) (struct file *, loff_t, int);
    ssize_t (*read) (struct file *, char *, size_t, loff_t *);
    ssize_t (*write) (struct file *, const char *, size_t, loff_t *);
    int (*readdir) (struct file *, void *, filldir_t);
    unsigned int (*poll) (struct file *, struct poll_table_struct *);
    int (*ioctl) (struct inode *, struct file *, unsigned int, unsigned long);
    int (*mmap) (struct file *, struct vm_area_struct *);
    int (*open) (struct inode *, struct file *);
    int (*flush) (struct file *);
    int (*release) (struct inode *, struct file *);
    int (*fsync) (struct file *, struct dentry *);
    int (*fasync) (int, struct file *, int);
    int (*check_media_change) (kdev_t dev);
    int (*revalidate) (kdev_t dev);
    int (*lock) (struct file *, int, struct file_lock *);
};

struct inode_operations {
    int (*create) (struct inode *,struct dentry *,int);
    struct dentry * (*lookup) (struct inode *,struct dentry *);
    int (*link) (struct dentry *,struct inode *,struct dentry *);
    int (*unlink) (struct inode *,struct dentry *);
    int (*symlink) (struct inode *,struct dentry *,const char *);
    int (*mkdir) (struct inode *,struct dentry *,int);
    int (*rmdir) (struct inode *,struct dentry *);
    int (*mknod) (struct inode *,struct dentry *,int,int);
    int (*rename) (struct inode *, struct dentry *,
		   struct inode *, struct dentry *);
    int (*readlink) (struct dentry *, char *,int);
    struct dentry * (*follow_link) (struct dentry *, struct dentry *, unsigned int);
    int (*get_block) (struct inode *, long, struct buffer_head *, int);

    int (*readpage) (struct dentry *, struct page *);
    int (*writepage) (struct dentry *, struct page *);

    void (*truncate) (struct inode *);
    int (*permission) (struct inode *, int);
    int (*revalidate) (struct dentry *);
};


struct super_operations {
    void (*read_inode) (struct inode *);
    void (*read_inode2) (struct inode *, void *) ;
    void (*write_inode) (struct inode *);
    void (*dirty_inode) (struct inode *);
    void (*put_inode) (struct inode *);
    void (*delete_inode) (struct inode *);
    int (*notify_change) (struct dentry *, struct iattr *);
    void (*put_super) (struct super_block *);
    void (*write_super) (struct super_block *);
    int (*statfs) (struct super_block *, struct statfs *);
    int (*remount_fs) (struct super_block *, int *, char *);
    void (*clear_inode) (struct inode *);
    void (*umount_begin) (struct super_block *);
};


extern inline void mark_buffer_uptodate(struct buffer_head * bh, int on)
{
    if (on)
	set_bit(BH_Uptodate, &bh->b_state);
    else
	clear_bit(BH_Uptodate, &bh->b_state);
}

typedef struct {
	size_t written;
	size_t count;
	char * buf;
	int error;
} read_descriptor_t;

typedef int (*read_actor_t)(read_descriptor_t *, struct page *, unsigned long, unsigned long);


extern char * kdevname(kdev_t);

extern void clear_inode(struct inode *);
extern struct inode * get_empty_inode(void);
extern void insert_inode_hash(struct inode *);
int is_subdir (struct dentry *, struct dentry *);
extern void sync_inodes(kdev_t);


extern struct buffer_head * get_hash_table(kdev_t, int, int);
extern struct buffer_head * getblk(kdev_t, int, int);
extern void ll_rw_block(int, int, struct buffer_head * bh[]);
extern struct buffer_head * bread(kdev_t, int, int);
extern void brelse(struct buffer_head *);
extern inline void bforget(struct buffer_head *buf);
#define set_blocksize(x,y) do { } while (0)
extern int generic_file_mmap(struct file *, struct vm_area_struct *);
extern ssize_t generic_file_read(struct file *, char *, size_t, loff_t *);
extern void do_generic_file_read(struct file * filp, loff_t *ppos, read_descriptor_t * desc, read_actor_t actor);
typedef int (*writepage_t)(struct dentry *, struct page *, unsigned long, unsigned long, const char *);
typedef int (get_block_t)(struct inode*,long,struct buffer_head*,int);
extern ssize_t generic_file_write(struct file *, const char *, size_t, loff_t *);
//extern int block_write_partial_page (struct dentry *, struct page *, unsigned long, unsigned long, const char *);
extern int block_write_full_page(struct page *page, get_block_t *get_block);
extern int block_sync_page(struct page *page);
extern int block_read_full_page(struct page * page, get_block_t * get_block);
extern int block_prepare_write(struct page *page, unsigned from, unsigned to,
			       get_block_t *get_block);
int generic_block_bmap(struct address_space *mapping, long block, get_block_t *get_block);
int generic_commit_write(struct file *file, struct page *page,
			 unsigned from, unsigned to);
extern void init_fifo(struct inode *);
extern void invalidate_buffers(kdev_t);

extern int fsync_dev(kdev_t);


#define I_DIRTY	1
#define I_LOCK 	2
static inline void mark_inode_dirty(struct inode *inode)
{
    inode->i_state |= I_DIRTY;
}



extern struct inode *iget4(struct super_block *sb, unsigned long ino, void * notused, void * dirino);
extern void iput(struct inode *);

extern struct inode_operations chrdev_inode_operations;
extern struct inode_operations blkdev_inode_operations;

#define ERR_PTR(err)       ((void *)((long)(err)))
extern int file_fsync(struct file *, struct dentry *);


//
// ./include/linux/locks.h
//
extern inline void wait_on_buffer(struct buffer_head * bh)
{
}

extern inline void unlock_buffer(struct buffer_head *bh)
{
    clear_bit(BH_Lock, &bh->b_state);

}

extern inline void lock_super(struct super_block * sb)
{
}

extern inline void unlock_super(struct super_block * sb)
{
}

#define __wait_on_buffer wait_on_buffer

extern struct inode_operations page_symlink_inode_operations;

extern int generic_buffer_fdatasync(struct inode *inode, unsigned long start_idx, unsigned long end_idx);

extern ssize_t generic_read_dir(struct file *, char *, size_t, loff_t *);
extern void make_bad_inode(struct inode *);


//
// ./include/linux/pagemap.h
//
#define page_cache_release(x) {freemem((x)->address);freemem(x);}
extern inline void wait_on_page(struct page * page)
{
}
extern struct page * grab_cache_page (struct address_space *, unsigned long);


//
// ./include/linux/kernel.h
//
#define printk printf

//
// ./include/linux/byteorder/generic.h
//
#include <asm/byteorder.h>

#define le16_to_cpu __le16_to_cpu
#define cpu_to_le16 __cpu_to_le16
#define le32_to_cpu __le32_to_cpu
#define cpu_to_le32 __cpu_to_le32
#define le64_to_cpu __le64_to_cpu
#define cpu_to_le64 __cpu_to_le64


//
// ./include/linux/module.h
//
#define MOD_INC_USE_COUNT do { } while (0)
#define MOD_DEC_USE_COUNT do { } while (0)


//
// ./include/asm-i386/processor.h
//
extern int kernel_thread(int (*fn)(void *), void * arg, unsigned long flags); 


//
// ./include/asm-i386/uaccess.h
//
#define copy_to_user(to,from,n) memcpy(to,from,n)
#define put_user(x,ptr) (*(ptr) = x)

//
// ./include/asm-i386/param.h
//
#define HZ 100





//
// ./include/linux/slab.h
//
#define kmalloc(x,y) getmem(x)
#define kfree(x) freemem(x)


//
// ./include/linux/blkdev.h
//
#define MAX_BLKDEV      255
extern int * blksize_size[MAX_BLKDEV];


//
// ./include/linux/tqueue.h
//
#define run_task_queue(tq) do {} while (0)
#define queue_task(a,b)  do {} while (0)


//
// 
//
extern inline void bwrite (struct buffer_head * bh)
{
    /* this does not lock buffer */
    ll_rw_block (WRITE, 1, &bh);
    mark_buffer_clean (bh);
}


//
// to be deleted
//
#define mark_buffer_dirty_balance_dirty(x,y) do { } while (0)
#define reiserfs_brelse(bh) do { } while (0)

extern inline int reiserfs_remove_page_from_flush_list(struct reiserfs_transaction_handle * th,
						       struct inode * inode)
{
  return 0;
}
extern inline int reiserfs_add_page_to_flush_list(struct reiserfs_transaction_handle * th,
						  struct inode * inode, struct buffer_head * bh)
{
  return 0;
}

extern inline void reiserfs_check_lock_depth (char * caller)
{
}



