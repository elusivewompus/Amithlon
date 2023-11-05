VPATH = ../bin
vpath %.c $(REISERFS_KERNEL_SOURCE) $(REISERFS_LIB)

# files from utils's lib directory needed for mkreiserfs
LIB_C = misc.c vfs.c version.c
LIB_OBJS = misc.o vfs.o version.o

MKFS_OBJS = mkreiserfs.o  $(LIB_OBJS)

MKFS = $(TMPBINDIR)/mkreiserfs

all: $(MKFS)

.c.o:
	$(CC) $(CFLAGS) $<

$(MKFS): $(MKFS_OBJS)
	$(CC) $(LFLAGS) -o $(MKFS) $(MKFS_OBJS)

clean:
	rm -f *.o $(MKFS) *~

dep:
	gcc -MM $(IDIRS) *.c > .depend
	for i in $(LIB_C); do gcc -MM $(IDIRS) ../lib/$$i >> .depend ; done

install:
	cp -f $(MKFS) $(SBIN)
	if [ -d $(MANDIR) ] ; then cp mkreiserfs.8 $(MANDIR) ; gzip -9 -f $(MANDIR)/mkreiserfs.8 ; fi


uninstall:
	rm -f $(MANDIR)/mkreiserfs.8.gz $(SBIN)/mkreiserfs


ifeq (.depend,$(wildcard .depend))
include .depend
endif









