#-----------------------------------------------------------------------------
#  Copyright (c) 1991,1992 Southeastern Universities Research Association,
#                          Continuous Electron Beam Accelerator Facility
# 
#  This software was developed under a United States Government license
#  described in the NOTICE file included as part of this distribution.
# 
#  CEBAF Data Acquisition Group, 12000 Jefferson Ave., Newport News, VA 23606
#  Email: coda@cebaf.gov  Tel: (804) 249-7101  Fax: (804) 249-7363
# -----------------------------------------------------------------------------
#  
#  Description:
# 	Makefile for event file I/O library, line mode dump utility
# 	
#  Author:  Chip Watson, CEBAF Data Acquisition Group
# 
#  Revision History:
#    $Log$
#    Revision 1.7  2001/06/21 18:30:43  wolin
#    added evio2xml.c
#
#    Revision 1.6  1999/01/22 15:43:57  rwm
#    make install depend on libcoda.a.
#
#    Revision 1.5  1998/11/13 16:20:41  timmer
#    get rid of Makefile.in
#
#    Revision 1.3  1998/09/21 15:06:08  abbottd
#    Makefile for vxWorks build of libcoda.a
#
#    Revision 1.2  1997/05/12 14:19:16  heyes
#    remove evfile_msg.h
#
#    Revision 1.1.1.1  1996/09/19 18:25:21  chen
#    original port to solaris
#
#
#

ifndef ARCH
  ARCH := $(subst -,_,$(shell uname))
endif

ifeq ($(ARCH),VXWORKS68K51)
CC = cc68k
AR = ar68k
RANLIB = ranlib68k
DEFS = -DCPU=MC68040 -DVXWORKS -DVXWORKS68K51
VXINC = $(WIND_BASE)/target/h
INCS = -w -Wall -fvolatile -fstrength-reduce -nostdinc -I. -I$(VXINC)
CFLAGS = -O $(DEFS) $(INCS)
endif

ifeq ($(ARCH),VXWORKSPPC)
CC = ccppc
AR = arppc
RANLIB = ranlibppc
DEFS = -mcpu=604 -DCPU=PPC604 -DVXWORKS -D_GNU_TOOL -DVXWORKSPPC
VXINC = $(WIND_BASE)/target/h
INCS = -w -Wall -fno-for-scope -fno-builtin -fvolatile -fstrength-reduce -nostdinc -I. -I$(VXINC)
endif

ifeq ($(ARCH),SunOS)
CC = cc
AR = ar
RANLIB = touch
DEFS = -DSYSV -DSVR4
INCS = -I.
endif

ifeq ($(ARCH),Linux)
CC = gcc
AR = ar
RANLIB = ranlib
DEFS = -DSYSV -DSVR4
INCS = -I.
endif

CFLAGS = -O $(DEFS) $(INCS)
OBJS = evio.o swap_util.o
CODALIB = libcoda.a

all: libcoda.a evio2xml

install: libcoda.a
	cp $(CODALIB) $(CODA)/$(ARCH)/lib/
	cp evio2xml $(CODA)/$(ARCH)/bin/

.c.o:
	rm -f $@
	$(CC) $(CFLAGS) -c $< -o $@

libcoda.a: evio.o swap_util.o
	$(AR) ruv $(CODALIB) $(OBJS)
	$(RANLIB) $(CODALIB)

evio.o:	evio.c 
	rm -f $@
	$(CC) $(CFLAGS) -c $< -o $@

evio2xml: evio2xml.o
	$(CC) $(CFLAGS) $< -o $@ -L. -lcoda

clean:
	$(RM) *.o libcoda.a evio2xml

evtest2: evtest2.c
	rm -f $@ 
	$(CC) $(CFLAGS) -L$(CODA_LIB) $< -o $@ -lcoda
