/*
 * Copyright 2000 by Hans Reiser, licensing governed by reiserfs/README
 */

#include <linux/sched.h>
#include <linux/reiserfs_fs.h>

// this contains item handlers for old item types: sd, direct,
// indirect, directory

/* and where are the comments? how about saying where we can find an
   explanation of each item handler method? -Hans */

//////////////////////////////////////////////////////////////////////////////
// stat data functions
//
static int sd_bytes_number (struct item_head * ih, int block_size)
{
  return 0;
}

static void sd_decrement_key (struct cpu_key * key)
{
    key->on_disk_key.k_objectid --;
    set_cpu_key_k_type (key, TYPE_ANY);
    set_cpu_key_k_offset(key, (loff_t)(-1));
}

static int sd_is_left_mergeable (struct key * key, unsigned long bsize)
{
    return 0;
}



static char * print_time (time_t t)
{
    static char timebuf[256];

    sprintf (timebuf, "%ld", t);
    return timebuf;
}


static void sd_print_item (struct item_head * ih, char * item)
{
    printk ("\tmode | size | nlinks | first direct | mtime\n");
    if (stat_data_v1 (ih)) {
      	struct stat_data_v1 * sd = (struct stat_data_v1 *)item;

	printk ("\t0%-6o | %6u | %2u | %d | %s\n", sd->sd_mode, sd->sd_size,
		sd->sd_nlink, sd->sd_first_direct_byte, print_time (sd->sd_mtime));
    } else {
	struct stat_data * sd = (struct stat_data *)item;

	printk ("\t0%-6o | %6Lu | %2u | %d | %s\n", sd->sd_mode, (unsigned long long)(sd->sd_size),
		sd->sd_nlink, sd->u.sd_rdev, print_time (sd->sd_mtime));
    }
}

static void sd_check_item (struct item_head * ih, char * item)
{
    // FIXME: type something here!
}


static int sd_create_vi (struct virtual_node * vn,
			 struct virtual_item * vi, 
			 int is_affected, 
			 int insert_size)
{
    vi->vi_index = TYPE_STAT_DATA;
    //vi->vi_type |= VI_TYPE_STAT_DATA;// not needed?
    return 0;
}


static int sd_check_left (struct virtual_item * vi, int free, 
			  int start_skip, int end_skip)
{
    if (start_skip || end_skip)
	BUG ();
    return -1;
}


static int sd_check_right (struct virtual_item * vi, int free)
{
    return -1;
}

static int sd_part_size (struct virtual_item * vi, int first, int count)
{
    if (count)
	BUG ();
    return 0;
}

static int sd_unit_num (struct virtual_item * vi)
{
    return vi->vi_item_len - IH_SIZE;
}


static void sd_print_vi (struct virtual_item * vi)
{
    reiserfs_warning ("STATDATA, index %d, type 0x%x, %h\n", 
		      vi->vi_index, vi->vi_type, vi->vi_ih);
}

struct item_operations stat_data_ops = {
    sd_bytes_number,
    sd_decrement_key,
    sd_is_left_mergeable,
    sd_print_item,
    sd_check_item,

    sd_create_vi,
    sd_check_left,
    sd_check_right,
    sd_part_size,
    sd_unit_num,
    sd_print_vi
};



//////////////////////////////////////////////////////////////////////////////
// direct item functions
//
static int direct_bytes_number (struct item_head * ih, int block_size)
{
  return le16_to_cpu (ih->ih_item_len);
}


// FIXME: this should probably switch to indirect as well
static void direct_decrement_key (struct cpu_key * key)
{
    cpu_key_k_offset_dec (key);
    if (cpu_key_k_offset (key) == 0)
	set_cpu_key_k_type (key, TYPE_STAT_DATA);	
}


static int direct_is_left_mergeable (struct key * key, unsigned long bsize)
{
    int version = le_key_version (key);
    return ((le_key_k_offset (version, key) & (bsize - 1)) != 1);
}


static void direct_print_item (struct item_head * ih, char * item)
{
    int j = 0;

//    return;
    printk ("\"");
    while (j < ih->ih_item_len)
	printk ("%c", item[j++]);
    printk ("\"\n");
}


static void direct_check_item (struct item_head * ih, char * item)
{
    // FIXME: type something here!
}


static int direct_create_vi (struct virtual_node * vn,
			     struct virtual_item * vi, 
			     int is_affected, 
			     int insert_size)
{
    vi->vi_index = TYPE_DIRECT;
    //vi->vi_type |= VI_TYPE_DIRECT;
    return 0;
}

static int direct_check_left (struct virtual_item * vi, int free,
			      int start_skip, int end_skip)
{
    int bytes;

    bytes = free - free % 8;
    return bytes ?: -1;    
}


static int direct_check_right (struct virtual_item * vi, int free)
{
    return direct_check_left (vi, free, 0, 0);
}

static int direct_part_size (struct virtual_item * vi, int first, int count)
{
    return count;
}


static int direct_unit_num (struct virtual_item * vi)
{
    return vi->vi_item_len - IH_SIZE;
}


static void direct_print_vi (struct virtual_item * vi)
{
    reiserfs_warning ("DIRECT, index %d, type 0x%x, %h\n", 
		      vi->vi_index, vi->vi_type, vi->vi_ih);
}

struct item_operations direct_ops = {
    direct_bytes_number,
    direct_decrement_key,
    direct_is_left_mergeable,
    direct_print_item,
    direct_check_item,

    direct_create_vi,
    direct_check_left,
    direct_check_right,
    direct_part_size,
    direct_unit_num,
    direct_print_vi
};



//////////////////////////////////////////////////////////////////////////////
// indirect item functions
//

static int indirect_bytes_number (struct item_head * ih, int block_size)
{
  return le16_to_cpu (ih->ih_item_len) / UNFM_P_SIZE * block_size; //- get_ih_free_space (ih);
}


// decrease offset, if it becomes 0, change type to stat data
static void indirect_decrement_key (struct cpu_key * key)
{
    cpu_key_k_offset_dec (key);
    if (cpu_key_k_offset (key) == 0)
	set_cpu_key_k_type (key, TYPE_STAT_DATA);
}


// if it is not first item of the body, then it is mergeable
static int indirect_is_left_mergeable (struct key * key, unsigned long bsize)
{
    int version = le_key_version (key);
    return (le_key_k_offset (version, key) != 1);
}


// printing of indirect item
static void start_new_sequence (__u32 * start, int * len, __u32 new)
{
    *start = new;
    *len = 1;
}


static int sequence_finished (__u32 start, int * len, __u32 new)
{
    if (start == INT_MAX)
	return 1;

    if (start == 0 && new == 0) {
	(*len) ++;
	return 0;
    }
    if (start != 0 && (start + *len) == new) {
	(*len) ++;
	return 0;
    }
    return 1;
}

static void print_sequence (__u32 start, int len)
{
    if (start == INT_MAX)
	return;

    if (len == 1)
	printk (" %d", start);
    else
	printk (" %d(%d)", start, len);
}


static void indirect_print_item (struct item_head * ih, char * item)
{
    int j;
    __u32 * unp, prev = INT_MAX;
    int num;

    unp = (__u32 *)item;

    if (ih->ih_item_len % UNFM_P_SIZE)
	printk ("indirect_print_item: invalid item len");  

    printk ("%d pointers\n[ ", (int)I_UNFM_NUM (ih));
    for (j = 0; j < I_UNFM_NUM (ih); j ++) {
	if (sequence_finished (prev, &num, unp[j])) {
	    print_sequence (prev, num);
	    start_new_sequence (&prev, &num, unp[j]);
	}
    }
    print_sequence (prev, num);
    printk ("]\n");
}

static void indirect_check_item (struct item_head * ih, char * item)
{
    // FIXME: type something here!
}


static int indirect_create_vi (struct virtual_node * vn,
			       struct virtual_item * vi, 
			       int is_affected, 
			       int insert_size)
{
    vi->vi_index = TYPE_INDIRECT;
    //vi->vi_type |= VI_TYPE_INDIRECT;
    return 0;
}

static int indirect_check_left (struct virtual_item * vi, int free,
				int start_skip, int end_skip)
{
    int bytes;

    bytes = free - free % UNFM_P_SIZE;
    return bytes ?: -1;    
}


static int indirect_check_right (struct virtual_item * vi, int free)
{
    return indirect_check_left (vi, free, 0, 0);
}



// return size in bytes of 'units' units. If first == 0 - calculate from the head (left), otherwise - from tail (right)
static int indirect_part_size (struct virtual_item * vi, int first, int units)
{
    // unit of indirect item is byte (yet)
    return units;
}

static int indirect_unit_num (struct virtual_item * vi)
{
    // unit of indirect item is byte (yet)
    return vi->vi_item_len - IH_SIZE;
}

static void indirect_print_vi (struct virtual_item * vi)
{
    reiserfs_warning ("INDIRECT, index %d, type 0x%x, %h\n", 
		      vi->vi_index, vi->vi_type, vi->vi_ih);
}

struct item_operations indirect_ops = {
    indirect_bytes_number,
    indirect_decrement_key,
    indirect_is_left_mergeable,
    indirect_print_item,
    indirect_check_item,

    indirect_create_vi,
    indirect_check_left,
    indirect_check_right,
    indirect_part_size,
    indirect_unit_num,
    indirect_print_vi
};


//////////////////////////////////////////////////////////////////////////////
// direntry functions
//


static int direntry_bytes_number (struct item_head * ih, int block_size)
{
    reiserfs_warning ("vs-16090: direntry_bytes_number: "
		      "bytes number is asked for direntry");
    return 0;
}

static void direntry_decrement_key (struct cpu_key * key)
{
    cpu_key_k_offset_dec (key);
    if (cpu_key_k_offset (key) == 0)
	set_cpu_key_k_type (key, TYPE_STAT_DATA);	
}


static int direntry_is_left_mergeable (struct key * key, unsigned long bsize)
{
    if (le32_to_cpu (key->u.k_offset_v1.k_offset) == DOT_OFFSET)
	return 0;
    return 1;
	
}


static void direntry_print_item (struct item_head * ih, char * item)
{
    int i;
    int namelen;
    struct reiserfs_de_head * deh;
    char * name;
    static char namebuf [80];


    printk ("\n # %-15s%-30s%-15s%-15s%-15s\n", "Name", "Key of pointed object", "Hash", "Gen number", "Status");

    deh = (struct reiserfs_de_head *)item;

    for (i = 0; i < I_ENTRY_COUNT (ih); i ++, deh ++) {
	namelen = (i ? ((deh - 1)->deh_location) : ih->ih_item_len) - deh->deh_location;
	name = item + deh->deh_location;
	if (name[namelen-1] == 0)
	  namelen = strlen (name);
	namebuf[0] = '"';
	if (namelen > sizeof (namebuf) - 3) {
	    strncpy (namebuf + 1, name, sizeof (namebuf) - 3);
	    namebuf[sizeof (namebuf) - 2] = '"';
	    namebuf[sizeof (namebuf) - 1] = 0;
	} else {
	    memcpy (namebuf + 1, name, namelen);
	    namebuf[namelen + 1] = '"';
	    namebuf[namelen + 2] = 0;
	}

	printk ("%d:  %-15s%-15d%-15d%-15Ld%-15Ld(%s)\n", 
		i, namebuf,
		deh->deh_dir_id, deh->deh_objectid,
		GET_HASH_VALUE (deh_offset (deh)), GET_GENERATION_NUMBER ((deh_offset (deh))),
		(de_hidden (deh)) ? "HIDDEN" : "VISIBLE");
    }
}


static void direntry_check_item (struct item_head * ih, char * item)
{
    int i;
    struct reiserfs_de_head * deh;

    // FIXME: type something here!
    deh = (struct reiserfs_de_head *)item;
    for (i = 0; i < I_ENTRY_COUNT (ih); i ++, deh ++) {
	;
    }
}



#define DIRENTRY_VI_FIRST_DIRENTRY_ITEM 1

struct direntry_uarea {
    int flags;
    short entry_count;
    short entry_sizes[1];
};


/*
 * function returns old entry number in directory item in real node
 * using new entry number in virtual item in virtual node */
static inline int old_entry_num (int is_affected, int virtual_entry_num, int pos_in_item, int mode)
{
    if ( mode == M_INSERT || mode == M_DELETE)
	return virtual_entry_num;
    
    if (!is_affected)
	/* cut or paste is applied to another item */
	return virtual_entry_num;

    if (virtual_entry_num < pos_in_item)
	return virtual_entry_num;

    if (mode == M_CUT)
	return virtual_entry_num + 1;

    RFALSE( mode != M_PASTE || virtual_entry_num == 0,
	    "vs-8015: old_entry_num: mode must be M_PASTE (mode = \'%c\'", mode);
    
    return virtual_entry_num - 1;
}




/* Create an array of sizes of directory entries for virtual
   item. Return space used by an item. FIXME: no control over
   consuming of space used by this item handler */
static int direntry_create_vi (struct virtual_node * vn,
			       struct virtual_item * vi, 
			       int is_affected, 
			       int insert_size)
{
    struct direntry_uarea * dir_u = vi->vi_uarea;
    int i, j;
    int size = sizeof (struct direntry_uarea);
    struct reiserfs_de_head * deh;
  
    vi->vi_index = TYPE_DIRENTRY;

    if (!(vi->vi_ih) || !vi->vi_item)
	BUG ();


    dir_u->flags = 0;
    if (le_ih_k_offset (vi->vi_ih) == DOT_OFFSET)
	dir_u->flags |= DIRENTRY_VI_FIRST_DIRENTRY_ITEM;

    deh = (struct reiserfs_de_head *)(vi->vi_item);
    
    
    /* virtual directory item have this amount of entry after */
    dir_u->entry_count = ih_entry_count (vi->vi_ih) + 
	((is_affected) ? ((vn->vn_mode == M_CUT) ? -1 :
			  (vn->vn_mode == M_PASTE ? 1 : 0)) : 0);
    
    for (i = 0; i < dir_u->entry_count; i ++) {
	j = old_entry_num (is_affected, i, vn->vn_pos_in_item, vn->vn_mode);
	dir_u->entry_sizes[i] = (j ? le16_to_cpu (deh[j - 1].deh_location) : le16_to_cpu (vi->vi_ih->ih_item_len)) -
	    le16_to_cpu (deh[j].deh_location) + DEH_SIZE;
    }

    size += (dir_u->entry_count * sizeof (short));
    
    /* set size of pasted entry */
    if (is_affected && vn->vn_mode == M_PASTE)
	dir_u->entry_sizes[vn->vn_pos_in_item] = insert_size;


#ifdef CONFIG_REISERFS_CHECK
    /* compare total size of entries with item length */
    {
	int k, l;
    
	l = 0;
	for (k = 0; k < dir_u->entry_count; k ++)
	    l += dir_u->entry_sizes[k];
    
	if (l + IH_SIZE != vi->vi_item_len + 
	    ((is_affected && (vn->vn_mode == M_PASTE || vn->vn_mode == M_CUT)) ? insert_size : 0) ) {
	    reiserfs_panic (0, "vs-8025: set_entry_sizes: (mode==%c, insert_size==%d), invalid length of directory item",
			    vn->vn_mode, insert_size);
	}
    }
#endif

    return size;


}


//
// return number of entries which may fit into specified amount of
// free space, or -1 if free space is not enough even for 1 entry
//
static int direntry_check_left (struct virtual_item * vi, int free,
				int start_skip, int end_skip)
{
    int i;
    int entries = 0;
    struct direntry_uarea * dir_u = vi->vi_uarea;

    for (i = start_skip; i < dir_u->entry_count - end_skip; i ++) {
	if (dir_u->entry_sizes[i] > free)
	    /* i-th entry doesn't fit into the remaining free space */
	    break;
		  
	free -= dir_u->entry_sizes[i];
	entries ++;
    }

    if (entries == dir_u->entry_count) {
	printk ("free spze %d, entry_count %d\n", free, dir_u->entry_count);
	BUG ();
    }

    /* "." and ".." can not be separated from each other */
    if (start_skip == 0 && (dir_u->flags & DIRENTRY_VI_FIRST_DIRENTRY_ITEM) && entries < 2)
	entries = 0;
    
    return entries ?: -1;
}


static int direntry_check_right (struct virtual_item * vi, int free)
{
    int i;
    int entries = 0;
    struct direntry_uarea * dir_u = vi->vi_uarea;
    
    for (i = dir_u->entry_count - 1; i >= 0; i --) {
	if (dir_u->entry_sizes[i] > free)
	    /* i-th entry doesn't fit into the remaining free space */
	    break;
	
	free -= dir_u->entry_sizes[i];
	entries ++;
    }
    if (entries == dir_u->entry_count)
	BUG ();

    /* "." and ".." can not be separated from each other */
    if ((dir_u->flags & DIRENTRY_VI_FIRST_DIRENTRY_ITEM) && entries > dir_u->entry_count - 2)
	entries = dir_u->entry_count - 2;

    return entries ?: -1;
}


/* sum of entry sizes between from-th and to-th entries including both edges */
static int direntry_part_size (struct virtual_item * vi, int first, int count)
{
    int i, retval;
    int from, to;
    struct direntry_uarea * dir_u = vi->vi_uarea;
    
    retval = 0;
    if (first == 0)
	from = 0;
    else
	from = dir_u->entry_count - count;
    to = from + count - 1;

    for (i = from; i <= to; i ++)
	retval += dir_u->entry_sizes[i];

    return retval;
}

static int direntry_unit_num (struct virtual_item * vi)
{
    struct direntry_uarea * dir_u = vi->vi_uarea;
    
    return dir_u->entry_count;
}



static void direntry_print_vi (struct virtual_item * vi)
{
    int i;
    struct direntry_uarea * dir_u = vi->vi_uarea;

    reiserfs_warning ("DIRENTRY, index %d, type 0x%x, %h, flags 0x%x\n", 
		      vi->vi_index, vi->vi_type, vi->vi_ih, dir_u->flags);
    printk ("%d entries: ", dir_u->entry_count);
    for (i = 0; i < dir_u->entry_count; i ++)
	printk ("%d ", dir_u->entry_sizes[i]);
    printk ("\n");
}

struct item_operations direntry_ops = {
    direntry_bytes_number,
    direntry_decrement_key,
    direntry_is_left_mergeable,
    direntry_print_item,
    direntry_check_item,

    direntry_create_vi,
    direntry_check_left,
    direntry_check_right,
    direntry_part_size,
    direntry_unit_num,
    direntry_print_vi
};


//////////////////////////////////////////////////////////////////////////////
//
//
#if ! (TYPE_STAT_DATA == 0 && TYPE_INDIRECT == 1 && TYPE_DIRECT == 2 && TYPE_DIRENTRY == 3)
  do not compile
#endif

struct item_operations * item_ops [4] = {
  &stat_data_ops,
  &indirect_ops,
  &direct_ops,
  &direntry_ops
};



