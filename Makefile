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
#    Revision 1.1  1996/09/19 18:25:21  chen
#    Initial revision
#
#
#
CC = cc
CFLAGS = -g -DSYSV -DSVR4

OBJS = evio.o swap_util.o
CODALIB = $(CODA_LIB)/libcoda.a

all: evio.o swap_util.o

install:
	ar ruv $(CODALIB) $(OBJS)
	ranlib $(CODALIB)

.c.o:
	rm -f $@
	$(CC) $(CFLAGS) -c $< -o $@

evio.o:	evio.c evfile_msg.h
	rm -f $@
	$(CC) $(CFLAGS) -c $< -o $@

evtest:	evtest.o  $(CODALIB)
	$(CC) $(CFLAGS) -o $@ evtest.o $(CODALIB)

evfile_msg.h:	evfile.msg 
	makeMsgTbl

clean:
	$(RM) *.o evfile_msg.h ceMsgTbl.c evtest *.dat
