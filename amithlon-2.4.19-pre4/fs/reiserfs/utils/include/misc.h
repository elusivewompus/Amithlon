/*
 * Copyright 1996, 1997, 1998, 1999 Hans Reiser
 */


void die (char * fmt, ...);
void * getmem (int size);
void freemem (const void * p);
void * expandmem (void * p, int size, int by);
int is_mounted (char * device_name);
void check_and_free_mem (void);
void print_how_far (__u32 * passed, __u32 total);
int block_write (int dev, int block, int blocksize, char * data);
int block_read (int dev, int block, int blocksize, char * data);

loff_t reiserfs_llseek (unsigned int fd, loff_t offset, unsigned int origin);
int reiserfs_progs_set_le_bit(int, void *) ;
int reiserfs_progs_test_le_bit(int, const void *) ;


#if !(__GLIBC__ > 1 && __GLIBC_MINOR__ > 0)
typedef unsigned short int uint16_t;
typedef unsigned int uint32_t;
#endif
