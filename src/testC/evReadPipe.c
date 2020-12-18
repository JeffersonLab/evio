/*-----------------------------------------------------------------------------
 * Copyright (c) 2012  Jefferson Science Associates
 *
 * This software was developed under a United States Government license
 * described in the NOTICE file included as part of this distribution.
 *
 * Data Acquisition Group, 12000 Jefferson Ave., Newport News, VA 23606
 * Email: timmer@jlab.org  Tel: (757) 249-7100  Fax: (757) 269-6248
 *-----------------------------------------------------------------------------
 * 
 * Description:
 *  Event I/O test program
 *  
 * Author:  Carl Timmer, Data Acquisition Group
 *
 */
 
#include <stdlib.h>
#include <stdio.h>
#include "evio.h"

#define MIN(a,b) (a<b)? a : b


 /**
  * @file
  * This program can be used to read stdin and therefore the output
  * of evWritePipe.c. It's purpose is to test using pipes with evio.

  The function popen does a fork and exec to execute the cmdstring,
  and returns a standard I/O file pointer. If type is "r", the file
  pointer is connected to the standard output of cmdstring
  (result of fp = popen(cmdstring, "r")):

      parent                  cmdstring (child)
 _______________              _________________
 |             |              |               |
 |          fp | <----------- | stdout        |
 |_____________|              |_______________|

  If type is "w", the file pointer is connected to the standard input
  of cmdstring (result of fp = popen(cmdstring, "w")):

      parent                  cmdstring (child)
 _______________              _________________
 |             |              |               |
 |          fp | -----------> | stdin         |
 |_____________|              |_______________|
 
 This program can be used in 2 ways:
    1) a) at command line, run: evWritePipe "|evReadPipe"
         evWritePipe needs to set filename = "-" internally.
         Thus the rest of the cmd line is consumed as the argument to evWritePipe.
         evReadPipe needs to set filename = "-" internally (read from stdin).

      b) at cmd line, run: evWritePipe
         evWritePipe needs to set filename = "|evReadPipe" internally.
         evReadPipe  needs to set filename = "-" internally (read from stdin).

   Make sure the evio lib is not printing stuff to stdout.

   2) a) at command line, run: evReadPipe
      evWritePipe will need to set filename = "-"  internally (write to stdout).
      And evReadPipe needs to set filename = "|evWritePipe" internally.
      Be sure that this program is NOT printing out anything in this case.

      b) at command line, run: evReadPipe "|evWritePipe"
      evWritePipe will need to set filename = "|evReadPipe"  internally (write to stdout).
      evReadPipe needs to set filename = "-" internally.
      Be sure that this program is NOT printing out anything in this case.

*/


static void printEvent(uint32_t eventNum, const uint32_t *buffer) {
    int i, nwords;

    if (buffer == NULL) return;
    nwords = buffer[0] + 1;

    printf("   Event #%u,  len = %u data words\n", eventNum, nwords - 2);
    printf("      Header words\n");
    printf("         %#10.8x\n", *buffer++);
    printf("         %#10.8x\n\n", *buffer++);
    printf("      Data words\n");
   
    nwords -= 2;
        
    for (; nwords > 0; nwords-=4) {
        for (i = MIN(nwords,4); i > 0; i--) {
            printf("         %#10.8x", *buffer++);
        }
        printf("\n");
    }
    printf("\n");
}


int main(int argc, char **argv)
{
    int handle, status, debug=0;
    uint32_t buffer[204800], *buf2,  nevents, bufLen;
    const uint32_t *buf;
    
    /* Tell evio to read from stdin */
    char *filename = "-";
    //filename = "|evWritePipe";
    
    if (debug) printf("evReadPipe: try to evOpen()\n");
    status = evOpen(filename, "r", &handle);
    if (status != S_SUCCESS) {
        if (1) printf ("    Error in evOpen so exit, status = 0x%0x, %s\n",
                           status, evPerror(status));
        exit(-1);
    }
    nevents = 0;
    debug = 1;

    if (debug) printf("evReadPipe: read events\n");


    status = evRead(handle, buffer, 204800);
    if (status == EOF) {
        if (debug) printf("    Last read, reached EOF!\n"); goto end;
    }
    else if (status != S_SUCCESS) {
        if (debug) printf ("    Last evRead status = 0x%0x, %s\n", status, evPerror(status)); goto end;
    }
    if (debug) printEvent(++nevents, buffer);


    
    status = evReadAlloc(handle, &buf2, &bufLen);
    if (status == EOF) {
        if (debug) printf("    Last read, reached EOF!\n"); goto end;
    }
    else if (status != S_SUCCESS) {
        if (debug) printf ("    Last evRead status = 0x%0x, %s\n", status, evPerror(status)); goto end;
    }
    else {
        
        if (debug) printEvent(++nevents, buf2);
        free(buf2);
    }


    
    status = evReadNoCopy(handle, &buf, &bufLen);
    if (status == EOF) {
        if (debug) printf("    Last read, reached EOF!\n"); goto end;
    }
    else if (status != S_SUCCESS) {
        if (debug) printf ("    Last evRead status = 0x%0x, %s\n", status, evPerror(status)); goto end;
    }
    else {
        if (debug) printEvent(++nevents, buf);
    }

    /*
    while ((status = evRead(handle, buffer, 204800)) == S_SUCCESS) {
        if (debug) printEvent(++nevents, buffer);
//        printf("Hit Enter to continue:\n");
//        getchar();
    }
    */
    
    if (status == EOF) {
        if (debug) printf("    Last read, reached EOF!\n");
    }
    else if (status != S_SUCCESS) {
        if (debug) printf ("    Last evRead status = 0x%0x, %s\n", status, evPerror(status));
    }
   
    end:
    status = evClose(handle);
    if (debug) printf ("    evReadPipe: closed file, status = 0x%0x\n\n", status);
}


