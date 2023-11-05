/* 
 * Copyright 1999 Hans Reiser, see README file for licensing details.
 */

#define print_usage_and_exit()\
 die ("Usage: %s  -s[+|-]#[M|K] [-fvn] device", argv[0])
 

/* reiserfs_resize.c */
int expand_fs(void);

/* fe.c */
int resize_fs_online(char * devname, unsigned long block);
