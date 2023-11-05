/*
 * Copyright 1999-2000  Hans Reiser
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <asm/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>


#include "misc.h"
#include "vfs.h"
#include "reiserfs_fs.h"
#include "reiserfs.h"


struct super_block g_sb;



#define print_usage_and_exit() die ("Usage: %s device\n\n", argv[0])



void reiserfs_prepare_for_journal(struct super_block *p_s_sb, 
                                  struct buffer_head *bh, int wait) {
}

void reiserfs_restore_prepared_buffer(struct super_block *p_s_sb, 
                                      struct buffer_head *bh) {
}


/* de->d_iname contains name */
static struct inode * get_inode_by_name (struct inode * dir, struct dentry * dentry)
{
  dentry->d_name.len = strlen (dentry->d_iname);
  dentry->d_name.name = dentry->d_iname;

  reiserfs_lookup (dir, dentry);

  return dentry->d_inode;
}


struct inode * g_pwd;

static void do_create (char * args)
{
  struct dentry de;

  if (sscanf (args, "%255s", de.d_iname) != 1) {
    reiserfs_warning ("create: usage: create filename\n");
    return;
  }

  if (get_inode_by_name (g_pwd, &de) == 0) {
    reiserfs_create (g_pwd, &de, 0100644);
    iput (de.d_inode);
  } else
    reiserfs_warning ("create: file %s exists\n", de.d_name.name);

}

static int do_mkdir (char * args)
{
  struct dentry de;

  if (sscanf (args, "%255s", de.d_iname) != 1) {
    reiserfs_warning ("mkdir: usage: mkdir dirname\n");
    return 1;
  }

  if (get_inode_by_name (g_pwd, &de) == 0) {
    reiserfs_mkdir (g_pwd, &de, 0100644);
    iput (de.d_inode);
    return 0;
  }
  reiserfs_warning ("mkdir: dir %s exists\n", de.d_name.name);
  iput (de.d_inode);
  return 1;
}

static void do_rmdir (char * args)
{
  struct dentry de;

  if (sscanf (args, "%255s", de.d_iname) != 1) {
    reiserfs_warning ("rmdir: usage: rmdir dirname\n");
    return;
  }

  if (get_inode_by_name (g_pwd, &de) != 0) {
    reiserfs_rmdir (g_pwd, &de);
    iput (de.d_inode);
  } else
    reiserfs_warning ("rmdir: dir %s is not exists\n", de.d_name.name);
}


static ssize_t _do_write (struct inode * inode, ssize_t count, loff_t offset, char * buf)
{
    struct buffer_head bh = {0,};
    struct buffer_head * pbh = &bh;
    loff_t pos;
    ssize_t retval = count;
    int nr;

    bh.b_data = getmem (g_sb.s_blocksize);
    bh.b_size = g_sb.s_blocksize;
    //    atomic_set (&(bh.b_count), 1);

    pos = offset;
    while (count) {
	inode->i_op->get_block (inode, offset / g_sb.s_blocksize, &bh, 1);
	nr = bh.b_size - offset % bh.b_size;
	if (count < nr)
	    nr = count;
	if (nr != bh.b_size) {
	    ll_rw_block (READ, 1, &pbh);
	    wait_on_buffer (&bh);
	}
	memcpy (bh.b_data + offset % bh.b_size, buf, nr);
	mark_buffer_uptodate (&bh, 1);
	mark_buffer_dirty (&bh, 1);
	bwrite (&bh);
	buf += nr;
	count -= nr;
	offset += nr;
	pos += nr;
    }
    
    freemem (bh.b_data);
    if (pos > inode->i_size) {
	inode->i_size = pos;
	mark_inode_dirty (inode);
    }
    return retval;
}


static void do_write (char * args)
{
    int i;
    int count;
    loff_t offset;
    char * buf;
    struct dentry de;
    struct inode * inode;
    struct file file;

    if (sscanf (args, "%255s %Ld %d", de.d_iname, &offset, &count) != 3) {
	reiserfs_warning ("write: usage: write filename offset count\n");
	return;
    }

    buf = (char *)malloc (count);
    if (buf == 0)
	reiserfs_panic (&g_sb, "do_write: no memory, or function not defined");
    for (i = 0; i < count; i ++)
	buf[i] = '0' + i % 10;

    if ((inode = get_inode_by_name (g_pwd, &de)) != 0) {
	file.f_error = 0;
	file.f_dentry = &de;
	file.f_pos = offset;
	generic_file_write (&file, buf, count, &file.f_pos);
	//_do_write (inode, count, offset, buf);
	iput (inode);
    } else {
	reiserfs_warning ("do_write: file \'%s\' does not exist\n", de.d_iname);
    }

}

static void do_truncate (char * args)
{
  int size;
  struct file file;
  struct dentry de;

  if (sscanf (args, "%255s %d", de.d_iname, &size) != 2) {
    reiserfs_warning ("usage: truncate filename size\n");
    return;
  }

  if (get_inode_by_name (g_pwd, &de)) {
    file.f_dentry = &de;
    file.f_flags = 0;
    /* if regular file */
    file.f_op = de.d_inode->i_fop;

    de.d_inode->i_size = size;
    mark_inode_dirty (de.d_inode);
    de.d_inode->i_op->truncate (de.d_inode);
    iput (de.d_inode);
  }
}

static void do_read (char * args)
{
  int count;
  loff_t offset;
  struct file file;
  char * buf;
  struct dentry de;
  struct inode * inode;

  if (sscanf (args, "%255s %Ld %d", de.d_iname, &offset, &count) != 3) {
    reiserfs_warning ("do_read: usage: read filename offset count\n");
    return;
  }

  if ((inode = get_inode_by_name (g_pwd, &de)) != 0) {
    file.f_dentry = &de;
    file.f_flags = 0;
    file.f_pos = 0;
    /* if regular file */
    file.f_op = de.d_inode->i_fop;
    buf = (char *)malloc (count);
    if (buf == 0)
      reiserfs_panic (&g_sb, "do_read: no memory, or function not defined");
    memset (buf, 0, count);

    file.f_op->read (&file, buf, count, &offset);
    iput (inode);
    free (buf);
  }
}



static void do_fcopy (char * args)
{
    char * src;
    char * dest;
    int fd_source;
    int rd, bufsize;
    loff_t offset;
    char * buf;
    struct dentry de;
    struct inode * inode;


    src = args;
    src [strlen (src) - 1] = 0;
    dest = strrchr (args, '/') + 1;
    if (dest == 0)
	die ("/ must be in the name of source");

    fd_source = open (src, O_RDONLY);
    if (fd_source == -1) 
	die ("fcopy: could not open \"%s\": %s", 
	     src, strerror (errno));

    bufsize = 1024;
    buf = (char *)malloc (bufsize);
    if (buf == 0)
	reiserfs_panic (&g_sb, "fcopy: no memory, or function not defined");

    strcpy (de.d_iname, dest);

    if ((inode = get_inode_by_name (g_pwd, &de)) == 0) {
	reiserfs_create (g_pwd, &de, 0100644);
	inode = de.d_inode;
    } else {
	reiserfs_warning ("fcopy: file %s exists\n", de.d_name.name);
	return;
    }

    offset = 0;
    while ((rd = read(fd_source, buf, bufsize)) > 0)
	offset += _do_write (inode, rd, offset, buf);

    iput (inode);
    free (buf);
  
    close(fd_source);
}


struct linux_dirent {
	unsigned long	d_ino;
	unsigned long	d_off;
	unsigned short	d_reclen;
	char		d_name[1];
};

struct getdents_callback {
	struct linux_dirent * current_dir;
	struct linux_dirent * previous;
	int count;
	int error;
};

int dir_size;

#define NAME_OFFSET(de) ((int) ((de)->d_name - (char *) (de)))
#define ROUND_UP_2(x) (((x)+sizeof(long)-1) & ~(sizeof(long)-1))


static int filldir(void * __buf, const char * name, int namlen, off_t offset, ino_t ino)
{
  struct linux_dirent * dirent;
  struct getdents_callback * buf = (struct getdents_callback *) __buf;
  int reclen = ROUND_UP_2(12 + namlen + 1);

  buf->error = -EINVAL;
  if (reclen > buf->count)
    return -EINVAL;
  dirent = buf->previous;
  if (dirent)
    put_user(offset, &dirent->d_off);

  dirent = buf->current_dir;
  buf->previous = dirent;
  put_user(ino, &dirent->d_ino);
  put_user(reclen, &dirent->d_reclen);
  copy_to_user(dirent->d_name, name, namlen);
  put_user(0, dirent->d_name + namlen);

  ((char *) dirent) += reclen;
  buf->current_dir = dirent;
  buf->count -= reclen;

  dir_size += DEH_SIZE + namlen;

  return 0;
}


int emu_getdents (struct file * file, void * dirbuf, int dirbuf_size)
{
  struct getdents_callback buf;
  struct linux_dirent * lastdirent;
  int error;

  buf.current_dir = (struct linux_dirent *) dirbuf;
  buf.previous = NULL;
  buf.count = dirbuf_size;
  buf.error = 0;
  file->f_op->readdir (file, &buf, filldir);
  error = buf.error;
  lastdirent = buf.previous;
  if (lastdirent) {
    put_user(file->f_pos, &lastdirent->d_off);
    error = dirbuf_size - buf.count;
  }

  return error;
}


void do_readdir (void)
{
    struct dentry de;
    struct file file;
/*  struct dirent * dirent;*/
    struct linux_dirent * p;
    int entry_nr = 0;
    int retval, i;
    char * dirbuf;
  
    dir_size = 0;

    de.d_inode = g_pwd;
    file.f_dentry = &de;
    file.f_pos = 0;
    file.f_op = de.d_inode->i_fop;

    dirbuf = malloc (3933);
    while ((retval = emu_getdents (&file, dirbuf, 3933)) > 0) {
	p = (struct linux_dirent *)dirbuf;
	i = 0;
	do {
	    p = (struct linux_dirent *)(dirbuf + i);
	    printf ("%s\n", p->d_name);
	    retval -= p->d_reclen;
	    i += p->d_reclen;
	    entry_nr ++;
	} while (retval > 0);
#if 0
	while (p->d_reclen && (char *)p + p->d_reclen < dirbuf + retval) {
/*      printf ("linux/drivers/scsi/%s\n", p->d_name);*/
	    printf ("%s\n", p->d_name);
	    p = (struct linux_dirent *)((char *)p + p->d_reclen);
	    entry_nr ++;
	}
#endif
    }
    if (retval != 0)
	printf ("getdents failed: %s", strerror (retval));
    free (dirbuf);

    printf ("%d entries, size %d\n", entry_nr, dir_size);
}


/* iput pwd inode, iget new pwd inode */
static int do_cd (char * args)
{
    struct inode * dir;
    struct dentry de;

    if (sscanf (args, "%255s", de.d_iname) != 1) {
	reiserfs_warning ("do_cd: usage: cd dirname\n");
	return 1;
    }
    dir = get_inode_by_name (g_pwd, &de);
    if (dir != 0 && S_ISDIR (dir->i_mode)) {
	iput (g_pwd);
	g_pwd = dir;
	return 0;
    }
    reiserfs_warning ("do_cd: no such file or not a directory \"%s\"\n", de.d_iname);
    return 1;
}

char buf1[1024], buf2[1024];

/* path is in buf1 */
static int do_path_cd (char * path)
{
    char * p, * slash;

    strcpy (buf2, path);
    p = buf2;
/*
    while ((slash = strchr (p, '/'))) {
	*slash = 0;
	if (do_cd (p)) {
	    printf ("cd: wrong path element: %s\n", p);
	    return 1;
	}
	p = slash + 1;
    }
    if (do_cd (p)) {
    }
*/
    while (1) {
	slash = strchr (p, '/');
	if (slash)
	    *slash = 0;	
	if (do_cd (p)) {
	    printf ("cd: wrong path element: %s\n", p);
	    return 1;
	}
	if (!slash)
	    break;
	p = slash + 1;
    }
    return 0;
}






static int do_symlink (char * args)
{
    struct dentry de;

    if (sscanf (args, "%255s %s", de.d_iname, buf1) != 2) {
	reiserfs_warning ("symlink: usage: symlink filename\n");
	return 0;
    }

    if (get_inode_by_name (g_pwd, &de) == 0) {
	reiserfs_symlink (g_pwd, &de, buf1);
	iput (de.d_inode);
    } else
	reiserfs_warning ("symlink: file %s exists\n", de.d_name.name);
    return 0;
}


static int do_readlink (char * args)
{
    struct dentry de;

    if (sscanf (args, "%255s", de.d_iname) != 1) {
	reiserfs_warning ("readlink: usage: readlink filename\n");
	return 0;
    }

    if (get_inode_by_name (g_pwd, &de)) {
	if (S_ISLNK (de.d_inode->i_mode)) {
	    de.d_inode->i_op->readlink (&de, buf1, sizeof (buf1));
	    buf1[de.d_inode->i_size] = 0;
	    reiserfs_warning ("The name in symlink: %s\n", buf1);
	} else
	    reiserfs_warning ("readlink: %s is not a symlink\n", de.d_name.name);
	iput (de.d_inode);
    } else
	reiserfs_warning ("readlink: file %s exists\n", de.d_name.name);

    return 0;
}



#include <dirent.h>

void do_dcopy (char * args)
{
  char name[256], * p;
  char command [256];
  DIR * d;
  struct dirent * de;
  struct stat st;

  if (sscanf (args, "%255s", name) != 1) {
    reiserfs_warning ("do_dcopy: usage: dcopy dirname\n");
    return;
  }
  if ((d = opendir (name)) == NULL || chdir (name) == -1) {
    printf ("opendir failed: %s\n", strerror (errno));
    return;
  }

  p = strrchr (name, '/');
  p ++;
  if (do_mkdir (p))
    return;
  if (do_cd (p))
    return;

  while ((de = readdir (d)) != NULL) {
    if (lstat (de->d_name, &st) == -1) {
      printf ("%s\n", strerror (errno));
      return;
    }
    if (S_ISREG (st.st_mode)) {
      printf ("%s/%s\n", name, de->d_name);
      sprintf (command, "%s/%s\n", name, de->d_name);
      do_fcopy (command);
      continue;
    }
    if (S_ISLNK (st.st_mode)) {
      if (readlink (de->d_name, buf1, sizeof (buf1)) == -1) {
	printf ("readlink failed: %s\n", strerror (errno));
	continue;
      }
      buf1[st.st_size] = 0;
      printf ("%s/%s->%s\n", name, de->d_name, buf1);
      sprintf (command, "%s %s\n", de->d_name, buf1);
      do_symlink (command);
      continue;
    }
  }
  
}



void do_diff (char * args)
{
  char orig[256];
  int fd, rd1, rd2;
  struct file file;
  struct dentry de;

  if (sscanf (args, "%80s %255s", de.d_iname, orig) != 2) {
    reiserfs_warning ("diff: usage: diff filename sourcefilename\n");
    return;
  }

  fd = open (orig, O_RDONLY);
  if (fd == -1) {
    printf ("%s\n", strerror (errno));
    return;
  }

  /* open file on reiserfs */
  if (get_inode_by_name (g_pwd, &de)) {
    file.f_dentry = &de;
    file.f_flags = 0;
    file.f_pos = 0;
    /* if regular file */
    file.f_op = de.d_inode->i_fop;
  } else {
    printf ("No such file or directory\n");
    return;
  }
  while ((rd1 = read (fd, buf1, 1024)) > 0) {
    rd2 = file.f_op->read (&file, buf2, 1024, &file.f_pos);
    if (rd1 != rd2) {
      printf ("Read error 1\n");
      return;
    }
    if (memcmp (buf1, buf2, rd1)) {
      printf ("Read error 2\n");
      return;
    }
  }
}


int do_delete (char * args)
{
  struct dentry de;

  if (sscanf (args, "%255s", de.d_iname) != 1) {
    reiserfs_warning ("delete: usage: delete filename\n");
    return 1;
  }
  if (get_inode_by_name (g_pwd, &de) == 0 || !(S_ISREG (de.d_inode->i_mode) ||
					       S_ISLNK (de.d_inode->i_mode))) {
    reiserfs_warning ("delete: file %s does not exist or not a regular or symlink\n",
		      de.d_name.name);
    return 1;
  }	
  reiserfs_unlink (g_pwd, &de);
  reiserfs_delete_inode (de.d_inode);
  return 0;
}


void do_for_each_name (void)
{
  struct dentry de;
  struct file file;
  char * buf;
  struct linux_dirent * p;

  de.d_inode = g_pwd;
  file.f_dentry = &de;
  file.f_pos = 0;
  file.f_op = de.d_inode->i_fop;

  buf = (char *)malloc (1024);
  while (emu_getdents (&file, buf, 1024) != 0) {
    p = (struct linux_dirent *)buf;
    while (p->d_reclen && (char *)p + p->d_reclen < (buf + 1024)) {
      printf ("Deleting %s.. %s\n", p->d_name, 
	      do_delete (p->d_name) ? "skipped" : "done");
      
      p = (struct linux_dirent *)((char *)p + p->d_reclen);
    }
  }

  free (buf);
}


void do_rm_rf (char * args)
{
    struct dentry de;

    if (sscanf (args, "%255s", de.d_iname) != 1) {
	reiserfs_warning ("rm_rf: usage: rm_rf dirname\n");
	return;
    }

    if (do_cd (de.d_iname))
	return;
    do_for_each_name ();
}


static void do_cd_root (void)
{
    struct reiserfs_iget4_args args ;

    args.objectid = REISERFS_ROOT_PARENT_OBJECTID;
    if (g_pwd)
	iput (g_pwd);
    g_pwd = iget4 (&g_sb, REISERFS_ROOT_OBJECTID, 0, (void *)(&args));
}


/* args is name of file which contains list of files to be copied and
   directories to be created */
void do_batch (char * args)
{
    FILE * list;
    char * path;
    char * name;

    args[strlen (args) - 1] = 0;
    list = fopen (args, "r");
    if (list == 0) {
	printf ("do_batch: fopen failed on \'%s\': %s\n", args, 
		strerror (errno));
	return;
    }
    while (fgets (buf1, sizeof (buf1), list) != 0) {
	do_cd_root ();

	/* remove ending \n */
       	buf1[strlen (buf1) - 1] = 0;
	
	/* select last name */
	path = buf1;
	name = path + strlen (buf1) - 1;
	if (*name == '/')
	    name --;
	while (*name != '/' && name != path)
	    name --;
	if (*name == '/')
	    *name++ = 0;
	if (name == path)
	    path = 0;

	printf ("cd to %s..", path);
	if (path && do_path_cd (path)) {
	    printf ("do_batch: cd failed\n");
	    return;
	}
	printf ("ok, ");

	if (name [strlen (name) - 1] == '/') {
	    name [strlen (name) - 1] = 0;
	    printf ("mkdir %s..", name);
	    do_mkdir (name);
	} else {
	    printf ("cp %s..", name);
	    sprintf (buf2, "%s/%s\n", path, name);
	    do_fcopy (buf2);
	}
	printf ("done\n");
    }
    printf ("Ok\n");
    fclose (list);
}



void do_help (void)
{
  printf (" create filename\n");
  printf (" mkdir dirname\n");
  printf (" rmdir dirname\n");
  printf (" write filename offset count\n");  
  printf (" read filename offset count\n");  
  printf (" fcopy filename\n");  
  printf (" ls\n");
  printf (" cd dirname\n");
  printf (" dcopy dirname\n");
  printf (" diff filename1(created by emu) filename2(original file)\n");
  printf (" delete file\n");
  printf (" truncate filename newsize\n");
  printf (" rm_rf dirname\n");
  printf (" symlink filename pointed-name\n");
  printf (" readlink filename\n");
  printf (" batch filelist\n");
  printf (" quit\n");
}


void release_bitmaps (struct super_block * s)
{
  int i;

  for (i = 0; i < SB_BMAP_NR (s); i ++) {
    brelse (SB_AP_BITMAP (s)[i]);
  }
  
  freemem (SB_AP_BITMAP (s));
}


struct dentry d_root;

int main (int argc, char * argv [])
{
  char cmd[256];
  char * file_name;
  int dev;

  printf ("\n<----------- REISERFSv2 EMU ----------->\n");

  if(argc < 2)
    print_usage_and_exit ();


  file_name = argv[1];

  /* open_device will die if it could not open device */
  dev = open (file_name, O_RDWR);
  if (dev == -1)
    reiserfs_panic (0, "emu: can not open '%s': %s", file_name, strerror (errno));

/*  init_buffer_cache ();*/
  g_sb.s_dev = dev;
  uread_super_block (&g_sb);
  uread_bitmaps (&g_sb);
  
  do_cd_root ();
  g_sb.s_root = &d_root;
  g_sb.s_root->d_inode = g_pwd;
  g_sb.u.reiserfs_sb.s_hash_function = hash_function (&g_sb);

 
  /* check whether device contains mounted tree file system */
  if (is_mounted (file_name))
    reiserfs_warning ("emu: '%s' contains a not mounted file system\n", file_name);



  while (1) {
    printf ("Enter command: >");
    fgets (cmd, 255, stdin);
    if (strncasecmp (cmd, "create ", 7) == 0)
      do_create (cmd + 7);
    else if (strncasecmp (cmd, "delete ", 7) == 0)
      do_delete (cmd + 7);
    else if (strncasecmp (cmd, "write ", 6) == 0)
      do_write (cmd + 6);    
    else if (strncasecmp (cmd, "truncate ", 8) == 0)
      do_truncate (cmd + 8);    
    else if (strncasecmp (cmd, "read ", 5) == 0)
      do_read (cmd + 5);    
    else if (strncasecmp (cmd, "mkdir ", 6) == 0)
      do_mkdir (cmd + 6);    
    else if (strncasecmp (cmd, "rmdir ", 6) == 0)
      do_rmdir (cmd + 6);    
    else if (strncasecmp (cmd, "dcopy ", 6) == 0)
      do_dcopy (cmd + 6);
    else if (strncasecmp (cmd, "fcopy ", 6) == 0)
      do_fcopy (cmd + 6);    
    else if (strncasecmp (cmd, "ls", 2) == 0)
      do_readdir ();
    else if (strncasecmp (cmd, "cd ", 3) == 0)
      do_cd (cmd + 3);
    else if (strncasecmp (cmd, "diff ", 5) == 0)
      do_diff (cmd + 5);
    else if (strncasecmp (cmd, "rm_rf ", 6) == 0)
      do_rm_rf (cmd + 6);
    else if (strncasecmp (cmd, "symlink ", 8) == 0)
      do_symlink (cmd + 8);
    else if (strncasecmp (cmd, "readlink ", 9) == 0)
      do_readlink (cmd + 9);
    else if (strncasecmp (cmd, "batch ", 6) == 0)
      do_batch (cmd + 6);
    else if (strncmp (cmd, "QUIT", strlen ("QUIT")) == 0)
      break;
    else if (strncmp (cmd, "q", strlen ("q")) == 0)
      break;
    else {
      do_help ();
    }
  }
  sync_inodes (0);
  release_bitmaps (&g_sb);
  brelse (g_sb.u.reiserfs_sb.s_sbh);
  fsync_dev (0);
  return 0;
}

