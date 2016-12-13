/*-----------------------------------------------------------------------------
 * Copyright (c) 1991,1992 Southeastern Universities Research Association,
 *                         Continuous Electron Beam Accelerator Facility
 *
 * This software was developed under a United States Government license
 * described in the NOTICE file included as part of this distribution.
 *
 * CEBAF Data Acquisition Group, 12000 Jefferson Ave., Newport News, VA 23606
 * Email: coda@cebaf.gov  Tel: (804) 249-7101  Fax: (804) 249-7363
 *-----------------------------------------------------------------------------
 * 
 * Description:
 *	Event I/O test program
 *	
 * Author:  Chip Watson, CEBAF Data Acquisition Group
 *
 */
 
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "evio.h"

/* 1st event */
uint32_t data1[] = {
        0x00000005,
        0x00010b01,
        0x00000001,
        0x00000002,
        0x00000003,
        0x00000004
};

/* 2nd event */
uint32_t data2[] = {
        0x00000006,
        0x000a0b0a,
        0x00000000,
        0x00000002,
        0x00000004,
        0x00000006,
        0x00000008
};


int main() {
    int handle, status, nevents, nwords, blkNum=0;
    uint32_t *ip, *pBuf;
    int i, maxEvBlk = 2;
    uint32_t *blkHeader, blkWordSize;

    int useFile = 1;


    printf("\nEvent I/O test, write /home/timmer/fileTestSmall.ev\n");

    if (useFile) {
        status = evOpen("/home/timmer/fileTestSmall.ev", "w", &handle);
    }

    printf("     handle = %d\n", handle);

    blkWordSize = 16;
    status = evIoctl(handle, "B", &blkWordSize);
    if (status != S_SUCCESS) {
        printf("evIoctl error setting block size\n");
    }

    for (i = 0; i < 6; i++) {
        // Write event to file
        status = evWrite(handle, data1);

        status = evIoctl(handle, "H", &blkHeader);

        printf("Event #%d, Block #%d\n", (i + 1), blkHeader[1]);
        free(blkHeader);

        //evFlush(handle);
        //printf("    Flushed file, status = %d\n", status);

        // How much room do I have left in the buffer now?
        if (!useFile) {
            printf("Buffer has %d bytes left\n", 1);
        }
    }

    status = evWrite(handle, data2);
    status = evIoctl(handle, "H", &blkHeader);
    printf("Event #7, Block #%d\n", blkHeader[1]);
    free(blkHeader);

//    evFlush(handle);
//    evFlush(handle);
//    evFlush(handle);
//    evFlush(handle);
    printf("    Flushed file, status = %d\n", status);

    status = evWrite(handle, data1);
    status = evIoctl(handle, "H", &blkHeader);
    printf("Event #8, Block #%d\n", blkHeader[1]);
    free(blkHeader);
//    evFlush(handle);

    if (1) {
        // All done writing
        evClose(handle);
        printf("    Closed file, status = %d\n", status);
    }


    printf("\nReopen /home/timmer/fileTestSmall.ev and append\n");
    if (useFile) {
        status = evOpen("/home/timmer/fileTestSmall.ev", "a", &handle);
    }

    for (i = 0; i < 3; i++) {
        // Write event to file
        status = evWrite(handle, data1);

        status = evIoctl(handle, "H", &blkHeader);
        printf("Event #%d, Block #%d\n", (i + 1), blkHeader[1]);
        free(blkHeader);
    }

    evClose(handle);
    printf("    Closed file again , status = %d\n", status);
    return 0;
}


