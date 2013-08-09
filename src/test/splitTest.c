/*
 * This program is for testing the new string manipulation
 * routines which facilitate the splitting and automatic
 * naming of files.
 *
 * author: Carl Timmer
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "evio.h"

extern int evOpenFake(char *filename, char *flags, int *handle, char **evf);

uint32_t eventBuffer1[] =
{
    0x00000007,
    0x00011001,
    0x00000005,
    0x00020b02,
    0x00000000,
    0x00000001,
    0x00000002,
    0x00000003
}; /* len = 8 words */

uint32_t eventBuffer2[] =
{
    0x00000011,
    0x00011001,
    0x0000000f,
    0x00020b02,
    0x00000000,
    0x00000001,
    0x00000002,
    0x00000003,
    0x00000004,
    0x00000005,
    0x00000006,
    0x00000007,
    0x00000008,
    0x00000009,
    0x0000000a,
    0x0000000b,
    0x0000000c,
    0x0000000d
};/* len = 18 words */

uint32_t eventBuffer3[] =
{
    0x0000000f,
    0x00011001,
    0x0000000d,
    0x00020b02,
    0x00000000,
    0x00000001,
    0x00000002,
    0x00000003,
    0x00000004,
    0x00000005,
    0x00000006,
    0x00000007,
    0x00000008,
    0x00000009,
    0x0000000a,
    0x0000000b
};/* len = 16 words */


//char *filename = "/daqfs/home/timmer/coda/evio-4.1/myFile000000";
//char *filename = "/daqfs/home/timmer/coda/evio-4.1/myFile";
char *filename = "/daqfs/home/timmer/coda/evio-4.1/my$(FILE_ENV)run_%d_.dat_%4d";


/* xml dictionary */
char *xmlDictionary =
        "<xmlDict>\n"
        "  <bank name=\"My Event\"       tag=\"1\"   num=\"1\">\n"
        "     <bank name=\"Segments\"    tag=\"2\"   num=\"2\">\n"
        "       <leaf name=\"My Shorts\" tag=\"3\"   />\n"
        "     </bank>\n"
        "     <bank name=\"Banks\"       tag=\"1\"   num=\"1\">\n"
        "       <leaf name=\"My chars\"  tag=\"5\"   num=\"5\"/>\n"
        "     </bank>\n"
        "  </bank>\n"
        "  <dictEntry name=\"Last Bank\" tag=\"33\"  num=\"66\"/>\n"
        "  <dictEntry name=\"Test Bank\" tag=\"1\" />\n"
        "</xmlDict>";



int main (int argc, char **argv)
{
    int i, err, handle, arg;
    int evCount = 3;
    uint64_t split;

    //if (argc > 1) orig = argv[1];

    /* open file for splitting */
    err = evOpen(filename, "s", &handle);
    if (err != S_SUCCESS) {
        printf("Error in evOpen(), err = %x\n", err);
        exit(0);
    }

    /* split at 4*24 bytes */
    split = 159L;
    evIoctl(handle, "S", &split);

    /* 1 event per block max */
//    arg = 1;
//    evIoctl(handle, "N", &arg);

    /* target block size = 8 header + 16 event words */
//    arg = 24;
//    evIoctl(handle, "B", &arg);
   
    /* buffer size = 2-8 headers + 16 event words or */
//    arg = 24 + 8;
//    evIoctl(handle, "W", &arg);
   

    for (i=0; i < evCount; i++) {
printf("\n\nsplitTest: write event %d ...\n", (i+1));
        err = evWrite(handle, eventBuffer1);
        if (err != S_SUCCESS) {
            printf("Error in evWrite(), err = %x\n", err);
            exit(0);
        }
    }
    
//     printf("\n\nsplitTest: write 1 big event ...\n");
//     err = evWrite(handle, eventBuffer2);
//     if (err != S_SUCCESS) {
//         printf("Error in evWrite(), err = %x\n", err);
//         exit(0);
//     }

    err = evClose(handle);
    if (err != S_SUCCESS) {
        printf("Error in evClose(), err = %x\n", err);
    }

    exit(0);
}
