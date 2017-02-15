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

/* Main event */
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

/* First event */
uint32_t dataFirst[] = {
        0x00000007,
        0x000a0b0a,
        0x00000100,
        0x00000200,
        0x00000300,
        0x00000400,
        0x00000500,
        0x00000600
};

static char *xmlDictionary =
        "<xmlDict>\n"
        "  <bank name=\"My Event\"       tag=\"1\"   num=\"1\">\n"
        "     <bank name=\"Ints\"    tag=\"2\"   num=\"2\">\n"
        "       <leaf name=\"My Shorts\" tag=\"3\"   />\n"
        "     </bank>\n"
        "     <bank name=\"Banks\"       tag=\"4\"   num=\"4\">\n"
        "       <leaf name=\"My chars\"  tag=\"5\"   num=\"5\"/>\n"
        "     </bank>\n"
        "  </bank>\n"
        "  <dictEntry name=\"First Event\" tag=\"100\"  num=\"100\"/>\n"
        "  <dictEntry name=\"Test Bank\" tag=\"1\" />\n"
        "</xmlDict>";

int mainAppend() {

    int handle, status, i;
    uint32_t *blkHeader, blkWordSize, bufferSize;
    uint64_t splitSize;

    int useFile = 1;

    printf("\nReopen /home/timmer/fileAppend and append\n");
    if (useFile) {
        status = evOpen("/home/timmer/fileAppend", "a", &handle);
    }

    blkWordSize = 29;
    status = evIoctl(handle, "B", &blkWordSize);
    if (status != S_SUCCESS) {
        printf("evIoctl error setting block size\n");
    }


    splitSize = 160;
    status = evIoctl(handle, "S", &splitSize);
    if (status != S_SUCCESS) {
        printf("evIoctl error setting split size\n");
    }


    bufferSize = blkWordSize + 8; // 37
    status = evIoctl(handle, "W", &bufferSize);
    if (status != S_SUCCESS) {
        printf("evIoctl error setting internal buffer size in words\n");
    }


    // Write first event to file
    status = evWriteFirstEvent(handle, dataFirst);
    if (status != S_SUCCESS) {
        printf("error writing first event\n");
    }


    // set mode to "r"
//    uint32_t buf[10];
//    memset((void *)buf, 0, sizeof(buf));
//    int err = evRead(handle, buf, 10);
//    if (err != S_SUCCESS) {
//        printf("ERROR reading event\n");
//    }
//    evPrintBuffer(buf, 10, 0);

    for (i = 0; i < 3; i++) {
        // Write event to file
        status = evWrite(handle, data1);
    }

    evClose(handle);
    printf("    Closed file again , status = %d\n", status);
    return 0;
}

int main() {
    int handle, status;
    uint32_t *ip, *pBuf;
    int i;
    uint32_t *blkHeader, blkWordSize, lockOff=0, lockOn=1, bufferSize;
    uint64_t splitSize;

    int useFile = 1;


    printf("\nEvent I/O test, write /home/timmer/fileTestSmall.ev\n");

    if (useFile) {
        status = evOpen("/home/timmer/fileTestSmall.ev", "s", &handle);
    }

    printf("     handle = %d\n", handle);

    blkWordSize = 29;
    status = evIoctl(handle, "B", &blkWordSize);
    if (status != S_SUCCESS) {
        printf("evIoctl error setting block size\n");
    }


    splitSize = 160;
    status = evIoctl(handle, "S", &splitSize);
    if (status != S_SUCCESS) {
        printf("evIoctl error setting split size\n");
    }


    bufferSize = blkWordSize + 8; // 37
    status = evIoctl(handle, "W", &bufferSize);
    if (status != S_SUCCESS) {
        printf("evIoctl error setting internal buffer size in words\n");
    }


    status = evWriteDictionary(handle, xmlDictionary);
    if (status != S_SUCCESS) {
        printf("error writing dictionary\n");
    }


    // Write first event to file
    status = evWriteFirstEvent(handle, dataFirst);
    if (status != S_SUCCESS) {
        printf("error writing first event\n");
    }


    for (i = 0; i < 6; i++) {
        // Write event to file
        status = evWrite(handle, data1);

        status = evIoctl(handle, "H", &blkHeader);

        printf("Event #%d, Block #%d\n", (i + 1), blkHeader[1]);
        free(blkHeader);

//        if (i%2 == 0) {
//            status = evIoctl(handle, "X", &lockOff);
//        }
//        else {
//            status = evIoctl(handle, "X", &lockOn);
//        }

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

//    status = evIoctl(handle, "X", &lockOn);
//    status = evIoctl(handle, "X", &lockOn);
//    status = evIoctl(handle, "X", &lockOff);
//    status = evIoctl(handle, "X", &lockOff);

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


//    printf("\nReopen /home/timmer/fileTestSmall.ev and append\n");
//    if (useFile) {
//        status = evOpen("/home/timmer/fileTestSmall.ev", "a", &handle);
//    }
//    // set mode to "r"
////    uint32_t buf[10];
////    memset((void *)buf, 0, sizeof(buf));
////    int err = evRead(handle, buf, 10);
////    if (err != S_SUCCESS) {
////        printf("ERROR reading event\n");
////    }
////    evPrintBuffer(buf, 10, 0);
//
//    for (i = 0; i < 3; i++) {
//        // Write event to file
//        status = evWrite(handle, data1);
//
//        status = evIoctl(handle, "H", &blkHeader);
//        printf("Event #%d, Block #%d\n", (i + 1), blkHeader[1]);
//        free(blkHeader);
//    }
//
//    evClose(handle);
//    printf("    Closed file again , status = %d\n", status);
    return 0;
}



