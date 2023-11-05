VPATH = ../bin
vpath %.c $(REISERFS_KERNEL_SOURCE) $(REISERFS_LIB)


# kernel files needed for debugreiserfs/unpack
KERNEL_C = prints.c item_ops.c
KERNEL_OBJ = prints.o item_ops.o

# files from utils's lib directory needed for debugreiserfs/unpack
LIB_C = misc.c vfs.c reiserfs.c version.c
LIB_OBJ = misc.o vfs.o reiserfs.o version.o


DUMP_OBJS = debugreiserfs.o $(LIB_OBJ) $(KERNEL_OBJ)
UNPACK_OBJS = unpack.o $(LIB_OBJ) $(KERNEL_OBJ)

DUMPFS = $(TMPBINDIR)/debugreiserfs
UNPACK = $(TMPBINDIR)/unpackreiserfs


all: $(DUMPFS)
# $(UNPACK)

.c.o:
	$(CC) $(CFLAGS) $<

$(DUMPFS): $(DUMP_OBJS)
	$(CC) -O2 $(LFLAGS) -o $(DUMPFS) $(DUMP_OBJS)

$(UNPACK): $(UNPACK_OBJS)
	$(CC) $(LFLAGS) -o $(UNPACK) $(UNPACK_OBJS)

clean:
	rm -f *.o $(DUMPFS) $(UNPACK) *~

dep:
	gcc -MM $(IDIRS) *.c > .depend
	for i in $(KERNEL_C); do gcc -MM $(IDIRS) $(REISERFS_KERNEL_SOURCE)/$$i >> .depend ; done
	for i in $(LIB_C); do gcc -MM $(IDIRS) ../lib/$$i >> .depend ; done

install:
	cp -f $(DUMPFS) $(SBIN)

uninstall:
	rm -f $(SBIN)/debugreiserfs

ifeq (.depend,$(wildcard .depend))
include .depend
endif




