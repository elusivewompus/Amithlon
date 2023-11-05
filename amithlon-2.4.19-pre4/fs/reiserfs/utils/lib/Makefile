OBJS = vfs.o misc.o reiserfs.o
#OBJS = misc.o io.o inode.o 
#hlam.o

MISC = $(TMPBINDIR)/libmisc.a

all: $(MISC)

.c.o:
	$(CC) $(CFLAGS) $<

$(MISC): $(OBJS)
	ar -r $(MISC)  $(OBJS)

clean:
	rm -f *.o $(MISC) *~

dep:
	gcc -MM $(IDIRS) *.c > .depend

ifeq (.depend,$(wildcard .depend))
include .depend
endif



