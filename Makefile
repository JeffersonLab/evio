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

EXPAT_INC = $(EXPAT_HOME)/xmlparse
EXPAT_LIB = $(EXPAT_HOME)/xmlparse


ifeq ($(ARCH),VXWORKS68K51)
CC = cc68k
AR = ar68k
RANLIB = ranlib68k
DEFS = -DCPU=MC68040 -DVXWORKS -DVXWORKS68K51
VXINC = $(WIND_BASE)/target/h
INCS = -w -Wall -fvolatile -fstrength-reduce -nostdinc -I. -I$(VXINC) -I$(EXPAT_INC)
CFLAGS = -O $(DEFS) $(INCS)
endif

ifeq ($(ARCH),VXWORKSPPC)
CC = ccppc
AR = arppc
RANLIB = ranlibppc
DEFS = -mcpu=604 -DCPU=PPC604 -DVXWORKS -D_GNU_TOOL -DVXWORKSPPC
VXINC = $(WIND_BASE)/target/h
INCS = -w -Wall -fno-for-scope -fno-builtin -fvolatile -fstrength-reduce -nostdinc -I. -I$(VXINC) -I$(EXPAT_INC)
endif

ifeq ($(ARCH),SunOS)
CC = cc
AR = ar
RANLIB = touch
DEFS = -DSYSV -DSVR4
INCS = -I. -I$(EXPAT_INC)
endif

ifeq ($(ARCH),Linux)
CC = gcc
AR = ar
RANLIB = ranlib
DEFS = -DSYSV -DSVR4
INCS = -I. -I$(EXPAT_INC)
endif


CFLAGS = -O $(DEFS) $(INCS)
OBJS = evio.o swap_util.o evioswap.o
LIBS = libcoda.a
PROGS = evio2xml xml2evio eviocopy


all: $(LIBS) $(PROGS)

install: libcoda.a
	cp $(LIBS)  $(CODA)/$(ARCH)/lib/
	cp $(PROGS) $(CODA)/$(ARCH)/bin/

.c.o:
	rm -f $@
	$(CC) -c $(CFLAGS) $<

libcoda.a: $(OBJS)
	$(AR) ruv $(LIBS) $(OBJS)
	$(RANLIB) $(LIBS)

evio2xml: evio2xml.o
	$(CC) $(CFLAGS) $< -o $@ -L$(EXPAT_LIB) -lexpat -L. -lcoda

xml2evio: xml2evio.o
	$(CC) $(CFLAGS) $< -o $@ -L$(EXPAT_LIB) -lexpat -L. -lcoda

eviocopy: eviocopy.o
	$(CC) $(CFLAGS) $< -o $@ -L. -lcoda

clean:
	$(RM) *.o $(LIBS) $(PROGS)
