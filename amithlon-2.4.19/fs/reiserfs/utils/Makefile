#
# Warning : reiserfsck and some others utils are broken currently.
#           We hope that it will be ready not later reiserfs-3.6.8
#
.EXPORT_ALL_VARIABLES:

TOPDIR = $(shell pwd)
# separate utils package detection
SEPARATE_UTILS_PACKAGE=$(shell [ -d reiserfs-src ] && echo yes)
TMPBINDIR = $(TOPDIR)/bin
INCLUDEDIR = $(TOPDIR)/include

ifeq ($(SEPARATE_UTILS_PACKAGE),yes)
	INCLUDEDIR2 = /usr/include/linux
else
	INCLUDEDIR2 = $(TOPDIR)/../../../include/linux
endif

SBIN = /sbin
MANDIR = /usr/man/man8

IDIRS = -I$(INCLUDEDIR) -I$(INCLUDEDIR2) -I-

#CFLAGS = -Wall -c -O -pg -g
#CFLAGS = -Wall -c -O2 -g -pg $(IDIRS)
CFLAGS = -Wall -c -O2 -g $(IDIRS)
#CFLAGS = -Wall -c -g $(IDIRS)
LFLAGS = -g -L$(TMPBINDIR)


# FIXME: path to kernel reiserfs .c files
ifeq ($(SEPARATE_UTILS_PACKAGE),yes)
	REISERFS_KERNEL_SOURCE = $(TOPDIR)/reiserfs-src
else	
	REISERFS_KERNEL_SOURCE = $(TOPDIR)/..
endif

REISERFS_LIB = ../lib

#ALL_SUB_DIRS = lib obj mkreiserfs debugreiserfs emu fsck
ALL_SUB_DIRS = mkreiserfs debugreiserfs  emu fsck
ALL_PROGS = mkreiserfs debugreiserfs emu reiserfsck

all:
	mkdir -p bin
	set -e; for i in $(ALL_SUB_DIRS); do $(MAKE) -C $$i ; done

dep:
	set -e; for i in $(ALL_SUB_DIRS); do $(MAKE) -C $$i dep ; done

clean:
	set -e; for i in $(ALL_SUB_DIRS); do $(MAKE) -C $$i clean ; done

install:
	for i in $(ALL_PROGS); do $(MAKE) -C $$i install ; done

uninstall:
	set -e; for i in $(ALL_PROGS); do $(MAKE) -C $$i uninstall ; done

tags:   
	:> TAGS
#	cd mkreiserfs; etags *.[ch] ../lib/*.c ../include/*.h ../../*.c ../../../../include/linux/reiserfs*h
#	cd fsck; etags *.[ch] ../lib/*.c ../include/*.h ../../../../include/linux/reiserfs*h
#	cd print_disk_layout; etags *.[ch] ../lib/*.c ../include/*.h ../../../../include/linux/reiserfs*h
#	cd lib; etags *.[ch] ../lib/*.c ../include/*.h ../../../../include/linux/reiserfs*h
#	cd include; etags *.[ch] ../lib/*.c ../include/*.h ../../../../include/linux/reiserfs*h
#	cd emu; etags *.[ch] ../lib/*.c ../include/*.h ../../../../include/linux/reiserfs*h
#	cd obj; etags ../../*.[ch] ../../../../include/linux/reiserfs*h
	rm -f TAGS; etags ../*.[ch] ../../../include/linux/reiserfs*h include/*.h lib/*.c debugreiserfs/*.c mkreiserfs/*.c emu/*.c fsck/*.c








