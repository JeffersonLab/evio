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
#       Must define EXPAT_HOME for xml...E.Wolin, 13-sep-01
# 	
# 	
#  Author:  Chip Watson, CEBAF Data Acquisition Group
# 
#  Revision History:
#    $Log$
#    Revision 1.17  2004/05/06 20:04:58  wolin
#    Using gzopen, etc.
#
#    Revision 1.16  2004/04/07 17:29:06  wolin
#    Split evio2xml into main plus library xml_util.c
#
#    Revision 1.15  2004/02/19 16:14:06  wolin
#    Added Evioswap.java
#
#    Revision 1.14  2003/12/24 15:18:16  abbottd
#      changes for vxWorks compiles
#
#    Revision 1.13  2003/10/30 21:05:24  wolin
#    Make install now makes everything
#
#    Revision 1.12  2003/08/26 13:46:41  wolin
#    Default EXPAT_HOME = /apps/expat
#
#    Revision 1.11  2003/08/26 13:32:58  wolin
#    New expat
#
#    Revision 1.10  2002/06/21 14:26:51  wolin
#    Added evioswap to libcoda
#
#    Revision 1.9  2001/09/13 19:59:08  wolin
#    Added EXPAT_HOME
#
#    Revision 1.8  2001/09/13 19:25:09  wolin
#    Added xml2evio, eviocopy
#
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


ifndef EXPAT_HOME
  EXPAT_HOME = /apps/expat
endif
EXPAT_INC = $(EXPAT_HOME)/include
EXPAT_LIB = $(EXPAT_HOME)/lib


ifeq ($(ARCH),VXWORKS68K51)
CC = cc68k
AR = ar68k
RANLIB = ranlib68k
DEFS = -DCPU=MC68040 -DVXWORKS -DVXWORKS68K51
VXINC = $(WIND_BASE)/target/h
INCS = -w -Wall -fvolatile -fstrength-reduce -nostdinc -I. -I$(VXINC)
CFLAGS = -O $(DEFS) $(INCS)
OBJS = 
LIBS = 
PROGS =
endif

ifeq ($(ARCH),VXWORKSPPC)
CC = ccppc
AR = arppc
RANLIB = ranlibppc
DEFS = -mcpu=604 -DCPU=PPC604 -DVXWORKS -D_GNU_TOOL -DVXWORKSPPC
VXINC = $(WIND_BASE)/target/h
INCS = -w -Wall -fno-for-scope -fno-builtin -fvolatile -fstrength-reduce -mlongcall -I. -I$(VXINC)
CFLAGS = -O $(DEFS) $(INCS)
OBJS = 
LIBS = 
PROGS =
endif

ifeq ($(ARCH),SunOS)
CC = cc
AR = ar
RANLIB = touch
DEFS = -DSYSV -DSVR4
INCS = -I. -I$(EXPAT_INC)
CFLAGS = -O $(DEFS) $(INCS)
OBJS = evio.o swap_util.o evioswap.o xml_util.o
LIBS = libcoda.a
PROGS = evio2xml xml2evio eviocopy Evioswap.class
endif

ifeq ($(ARCH),Linux)
CC = gcc
AR = ar
RANLIB = ranlib
DEFS = -DSYSV -DSVR4
INCS = -I. -I$(EXPAT_INC)
CFLAGS = -O $(DEFS) $(INCS)
OBJS = evio.o swap_util.o evioswap.o xml_util.o
LIBS = libcoda.a
PROGS = evio2xml xml2evio eviocopy Evioswap.class
endif


all: $(LIBS) $(PROGS)


install: $(LIBS) $(PROGS)
	cp $(LIBS)  $(CODA)/$(ARCH)/lib/
	cp $(PROGS) $(CODA)/$(ARCH)/bin/

install-vxworks: $(LIBS)
	cp $(LIBS)  $(CODA)/$(ARCH)/lib/


.c.o:
	rm -f $@
	$(CC) -c $(CFLAGS) $<

libcoda.a: $(OBJS)
	$(AR) ruv $(LIBS) $(OBJS)
	$(RANLIB) $(LIBS)

evio2xml: evio2xml.o
	$(CC) $(CFLAGS) $< -o $@ -L$(EXPAT_LIB) -lexpat -L. -lcoda -lz

xml2evio: xml2evio.o
	$(CC) $(CFLAGS) $< -o $@ -L$(EXPAT_LIB) -lexpat -L. -lcoda -lz

eviocopy: eviocopy.o
	$(CC) $(CFLAGS) $< -o $@ -L. -lcoda

Evioswap.class: Evioswap.java
	javac $<

clean:
	$(RM) *.o $(LIBS) $(PROGS)
