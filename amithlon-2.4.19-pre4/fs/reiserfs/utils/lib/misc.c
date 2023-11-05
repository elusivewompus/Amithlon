/*
 * Copyright 1996, 1997, 1998 Hans Reiser
 */
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <mntent.h>
#include <errno.h>
#include <asm/types.h>
#include <sys/vfs.h>
#include <unistd.h>
#include <linux/unistd.h>

#include "misc.h"
#include "vfs.h"



#ifdef __alpha__

int set_bit (int nr, void * addr)
{
    __u8 * p, mask;
    int retval;

    p = (__u8 *)addr;
    p += nr >> 3;
    mask = 1 << (nr & 0x7);
    /*cli();*/
    retval = (mask & *p) != 0;
    *p |= mask;
    /*sti();*/
    return retval;
}


int clear_bit (int nr, void * addr)
{
    __u8 * p, mask;
    int retval;

    p = (__u8 *)addr;
    p += nr >> 3;
    mask = 1 << (nr & 0x7);
    /*cli();*/
    retval = (mask & *p) != 0;
    *p &= ~mask;
    /*sti();*/
    return retval;
}

int test_bit(int nr, const void * addr)
{
    __u8 * p, mask;
  
    p = (__u8 *)addr;
    p += nr >> 3;
    mask = 1 << (nr & 0x7);
    return ((mask & *p) != 0);
}

int find_first_zero_bit (const void *vaddr, unsigned size)
{
    const __u8 *p = vaddr, *addr = vaddr;
    int res;

    if (!size)
	return 0;

    size = (size >> 3) + ((size & 0x7) > 0);
    while (*p++ == 255) {
	if (--size == 0)
	    return (p - addr) << 3;
    }
  
    --p;
    for (res = 0; res < 8; res++)
	if (!test_bit (res, p))
	    break;
    return (p - addr) * 8 + res;
}


int find_next_zero_bit (const void *vaddr, unsigned size, unsigned offset)
{
    const __u8 *addr = vaddr;
    const __u8 *p = addr + (offset >> 3);
    int bit = offset & 7, res;
  
    if (offset >= size)
	return size;
  
    if (bit) {
	/* Look for zero in first char */
	for (res = bit; res < 8; res++)
	    if (!test_bit (res, p))
		return (p - addr) * 8 + res;
	p++;
    }
    /* No zero yet, search remaining full bytes for a zero */
    res = find_first_zero_bit (p, size - 8 * (p - addr));
    return (p - addr) * 8 + res;
}
#endif /* __alpha__ */



/*int test_and_set_bit (int nr, void * addr)
{
  int oldbit = test_bit (nr, addr);
  set_bit (nr, addr);
  return oldbit;
}


int test_and_clear_bit (int nr, void * addr)
{
  int oldbit = test_bit (nr, addr);
  clear_bit (nr, addr);
  return oldbit;
}*/


void die (char * fmt, ...)
{
    static char buf[1024];
    va_list args;

    va_start (args, fmt);
    vsprintf (buf, fmt, args);
    va_end (args);

    fprintf (stderr, "\n%s\n\n\n", buf);
    exit (-1);
}



#define MEM_BEGIN "membegi"
#define MEM_END "mem_end"
#define MEM_FREED "__free_"
#define CONTROL_SIZE (strlen (MEM_BEGIN) + 1 + sizeof (int) + strlen (MEM_END) + 1)


static int get_mem_size (const char * p)
{
    const char * begin;

    begin = p - strlen (MEM_BEGIN) - 1 - sizeof (int);
    return *(int *)(begin + strlen (MEM_BEGIN) + 1);
}


static void checkmem (const char * p, int size)
{
    const char * begin;
    const char * end;
  
    begin = p - strlen (MEM_BEGIN) - 1 - sizeof (int);
    if (strcmp (begin, MEM_BEGIN))
	die ("checkmem: memory corrupted - invalid head sign");

    if (*(int *)(begin + strlen (MEM_BEGIN) + 1) != size)
	die ("checkmem: memory corrupted - invalid size");

    end = begin + size + CONTROL_SIZE - strlen (MEM_END) - 1;
    if (strcmp (end, MEM_END))
	die ("checkmem: memory corrupted - invalid end sign");
}



void * getmem (int size)
{
    char * p;
    char * mem;

    p = (char *)malloc (CONTROL_SIZE + size);
    if (!p)
	die ("getmem: no more memory (%d)", size);

    strcpy (p, MEM_BEGIN);
    p += strlen (MEM_BEGIN) + 1;
    *(int *)p = size;
    p += sizeof (int);
    mem = p;
    memset (mem, 0, size);
    p += size;
    strcpy (p, MEM_END);

    checkmem (mem, size);

    return mem;
}


void * expandmem (void * vp, int size, int by)
{
    int allocated;
    char * mem, * p = vp;
    int expand_by = by;

    if (p) {
	checkmem (p, size);
	allocated = CONTROL_SIZE + size;
	p -= (strlen (MEM_BEGIN) + 1 + sizeof (int));
    } else {
	allocated = 0;
	/* add control bytes to the new allocated area */
	expand_by += CONTROL_SIZE;
    }
    p = realloc (p, allocated + expand_by);
    if (!p)
	die ("expandmem: no more memory (%d)", size);
    if (!vp) {
	strcpy (p, MEM_BEGIN);
    }
    mem = p + strlen (MEM_BEGIN) + 1 + sizeof (int);

    *(int *)(p + strlen (MEM_BEGIN) + 1) = size + by;
    /* fill new allocated area by 0s */
    memset (mem + size, 0, by);
    strcpy (mem + size + by, MEM_END);

    checkmem (mem, size + by);

    return mem;
}


void freemem (const void * vp)
{
    int size;
  
    if (!vp)
	return;
    size = get_mem_size (vp);
    checkmem (vp, size);

    vp -= (strlen (MEM_BEGIN) + 1 + sizeof (int));
    free ((void *)vp);
}


int is_mounted (char * device_name)
{
    FILE *f;
    struct mntent *mnt;

    if ((f = setmntent (MOUNTED, "r")) == NULL)
	return 0;

    while ((mnt = getmntent (f)) != NULL)
	if (strcmp (device_name, mnt->mnt_fsname) == 0)
	    return 1;
    endmntent (f);

    return 0;
}




static char * strs[] =
{"0%",".",".",".",".","20%",".",".",".",".","40%",".",".",".",".","60%",".",".",".",".","80%",".",".",".",".","100%"};

static char progress_to_be[1024];
static char current_progress[1024];

static void str_to_be (char * buf, int prosents)
{
    int i;
    prosents -= prosents % 4;
    buf[0] = 0;
    for (i = 0; i <= prosents / 4; i ++)
	strcat (buf, strs[i]);
}


void print_how_far (__u32 * passed, __u32 total)
{
    int n;

    if (*passed == 0)
	current_progress[0] = 0;

    if (*passed >= total) {
	fprintf/*die*/ (stderr, "\nprint_how_far: total %u has been reached already. cur=%u\n", total, ++(*passed));
	return;
    }

    (*passed) ++;
    n = ((double)((double)(*passed) / (double)total) * (double)100);

    str_to_be (progress_to_be, n);

    if (strlen (current_progress) != strlen (progress_to_be)) {
	fprintf (stderr, "%s", progress_to_be + strlen (current_progress));
    }

    strcat (current_progress, progress_to_be + strlen (current_progress));


    fflush (stdout);
}



_syscall5 (int,  _llseek,  uint,  fd, ulong, hi, ulong, lo,
	   loff_t *, res, uint, wh);

loff_t reiserfs_llseek (unsigned int fd, loff_t offset, unsigned int origin)
{
    loff_t retval, result;
  
    retval = _llseek (fd, ((unsigned long long) offset) >> 32,
		      ((unsigned long long) offset) & 0xffffffff,
		      &result, origin);
    return (retval != 0 ? (loff_t)-1 : result);
  
}


int block_read (int dev, int block, int blocksize, char * data)
{
    loff_t pos;
    ssize_t rd;
    
    pos = (loff_t)block * blocksize;

    if (reiserfs_llseek (dev, pos, SEEK_SET) == (loff_t)-1)
	die ("block_read: lseek failed: %s", strerror (errno));

    if ((rd = read (dev, data, blocksize)) != blocksize) {
	if (rd == -1)
	    die ("block_read: write failed: %s", strerror (errno));
	else
	    die ("block_read: %d bytes written (should be %d)", rd, blocksize);
    }
    return 0;
}

int block_write (int dev, int block, int blocksize, char * data)
{
    loff_t pos = 0;
    ssize_t wr;

    pos = (loff_t)block * blocksize;
    if (reiserfs_llseek (dev, pos, SEEK_SET) == (loff_t)-1)
	die ("block_write: lseek failed: %s", strerror (errno));
    
    if ((wr = write (dev, data, blocksize)) != blocksize) {
	if (wr == -1)
	    die ("block_write: write failed: %s", strerror (errno));
	else
	    die ("block_write: %d bytes written (should be %d)", wr, blocksize);
    }
    return 0;
}

/*
 * For the benefit of those who are trying to port Linux to another
 * architecture, here are some C-language equivalents.  You should
 * recode these in the native assmebly language, if at all possible.
 *
 * C language equivalents written by Theodore Ts'o, 9/26/92.
 * Modified by Pete A. Zaitcev 7/14/95 to be portable to big endian
 * systems, as well as non-32 bit systems.
 *
 * taken from ext2fs_progs, was ext2fs_set_bit
 */

int reiserfs_progs_set_le_bit(int nr,void * addr)
{
	int		mask, retval;
	unsigned char	*local_addr = (unsigned char *) addr;

	local_addr += nr >> 3;
	mask = 1 << (nr & 0x07);
	retval = (mask & *local_addr) != 0;
	*local_addr |= mask;
	return retval;
}

/* taken from ext2fs progs, was ext2fs_test_bit */
int reiserfs_progs_test_le_bit(int nr, const void * addr)
{
	int			mask;
	const unsigned char	*local_addr = (const unsigned char *) addr;

	local_addr += nr >> 3;
	mask = 1 << (nr & 0x07);
	return ((mask & *local_addr) != 0);
}
