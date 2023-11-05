VPATH = ../bin
vpath %.c  $(REISERFS_KERNEL_SOURCE) $(REISERFS_LIB) 


# kernel files needed for emu
KERNEL_C = inode.c namei.c stree.c do_balan.c lbalance.c ibalance.c prints.c item_ops.c tail_conversion.c file.c dir.c fix_node.c bitmap.c objectid.c hashes.c super.c buffer2.c
KERNEL_OBJS = inode.o namei.o stree.o do_balan.o lbalance.o ibalance.o prints.o item_ops.o tail_conversion.o file.o dir.o fix_node.o bitmap.o objectid.o hashes.o super.o buffer2.o

# files from utils's lib directory needed for emu
LIB_C = misc.c vfs.c reiserfs.c version.c
LIB_OBJS = misc.o vfs.o reiserfs.o version.o


EMU_OBJS = emu.o  $(LIB_OBJS)  $(KERNEL_OBJS)

EMU = $(TMPBINDIR)/emu

all: $(EMU)

.c.o:
	$(CC) $(CFLAGS) $<

$(EMU): $(EMU_OBJS)
	$(CC) $(LFLAGS) -o $(EMU) $(EMU_OBJS)

clean:
	rm -f *.o $(EMU) *~

dep:
	gcc -MM $(IDIRS) *.c > .depend
	for i in $(KERNEL_C); do gcc -MM $(IDIRS) $(REISERFS_KERNEL_SOURCE)/$$i >> .depend ; done
	for i in $(LIB_C); do gcc -MM $(IDIRS) ../lib/$$i >> .depend ; done




ifeq (.depend,$(wildcard .depend))
include .depend
endif
