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
#include "evio.h"

int32_t *makeEvent();
#define MIN(a,b) (a<b)? a : b


int main()
{
    int handle, status, nevents, nlong, handle2;
    int buffer[2048], i, maxEvBlk = 2;
    int *ip, *pBuf;

    printf("\nEvent I/O tests...\n");
    status = evOpen("single.dat", "w", &handle);
    printf ("    Opened single.dat, status = %d\n", status);

    pBuf = ip = makeEvent();
    
    printf ("    Will write single.dat, status = %d\n",status);
    status = evWrite(handle, ip);

    status = evClose(handle);
    printf ("    Closed single.dat, status = %d\n\n", status);

    status = evOpen("single.dat", "r", &handle2);
    nevents = 0;
    
    while ((status = evRead(handle2, buffer, 2048)) == S_SUCCESS) {
        nevents++;
        printf("    Event #%d,  len = %d words\n", nevents, buffer[0]);
        if (nevents <= 4) {
            nlong = buffer[0] + 1;
        }
        
        for (ip=buffer; nlong > 0; nlong-=8) {
            for (i = MIN(nlong,8); i > 0; i--) {
                printf("      %#.8x\n", *ip++);
            }
        }
        
        printf("\n");
    }
    
    status = evClose(handle2);
    printf ("    Closed single.dat again, status = %d\n\n", status);

    ip = pBuf;
    
    printf ("    Try opening multiple.dat\n");
    status = evOpen("multiple.dat", "w", &handle);
    printf ("    Opened multiple.dat, status = %d\n", status);

    status = evIoctl(handle, "N", (void *) (&maxEvBlk));
    printf ("    Changed max events/block to %d, status = %#x\n", maxEvBlk, status);
    
    printf ("    Will write multiple.dat\n");
    evWrite(handle,ip);
    evWrite(handle,ip);
    evWrite(handle,ip);

    status = evClose(handle);
    printf ("    Closed multiple.dat status %d\n\n", status);

    status = evOpen("multiple.dat", "r", &handle);
    nevents = 0;
    
    while ((status = evRead(handle, buffer, 2048)) == S_SUCCESS) {
        nevents++;
        printf("    Event #%d,  len = %d words\n", nevents, buffer[0]);
        if (nevents <= 4) {
            nlong = buffer[0] + 1;
        }
        
        for (ip=buffer; nlong > 0; nlong-=8) {
            for (i = MIN(nlong,8); i > 0; i--) {
                printf("      %#.8x\n", *ip++);
            }
        }
        
        printf("\n");
    }
    
    printf("\n    Last read, status = %x\n", status);
    if (status == EOF) {
        printf("    Last read, reached EOF!\n");
    }
    
    evClose(handle);

    free(pBuf);
}


int32_t *makeEvent()
{
    int32_t *bank, *segment;
    short *word;


    bank = (int *) calloc(1, 11*sizeof(int32_t));
    bank[0] = 10;                    /* event length = 9 */
    bank[1] = 1 << 16 | 0x20 << 8;   /* tag = 1, bank 1 contains segments */

    segment = &(bank[2]);
    segment[0] = 2 << 24 | 0xb << 16 | 2; /* tag = 2, seg 1 has 2 - 32 bit ints, len = 2 */
    segment[1] = 0x1;
    segment[2] = 0x2;
        
    segment += 3;
        
    segment[0] = 3 << 24 | 2 << 22 | 4 << 16 | 2; /* tag = 3, 2 bytes padding, seg 2 has 3 shorts, len = 2 */
    word = (short *) &(segment[1]);
    word[0] = 0x3;
    word[1] = 0x4;
    word[2] = 0x5;

    segment += 3;

    /* HI HO - 2 strings */
    segment[0] =    4 << 24 | 0x3 << 16 | 2; /* tag = 4, seg 3 has 2 strings, len = 2 */
    segment[1] = 0x48 << 24 | 0 << 16   | 0x49 << 8 | 0x48 ;   /* H \0 I H */
    segment[2] =   4  << 24 | 4 << 16   | 0 << 8    | 0x4F ;   /* \4 \0 \0 O */

    return(bank);
}


int *makeEventOrig()
{
    int *bank;
    int *segment, *longword;
    short *word;
    short *packet;
    float data;

    bank = (int *) malloc(80*sizeof(int));
    bank[0] = 24;			/* event length = 18 */
    bank[1] = 1<<16 | 0x20<<8;	/* bank 1 contains segments */
    {
        segment = &(bank[2]);
        segment[0] = 1<<24 | 0x20<<16 | 6; /* segment 1 has 2 segments of len 3 */
        {
            segment = &(segment[1]);
            segment[0] = 2<<24 |  1<<16 | 2; /* segment 2 has 2 longwords */
            segment[1] = 0x11111111;
            segment[2] = 0x22222222;
            segment += 3;
            segment[0] = 3<<24 |  4<<16 | 2; /* segment 3 has 2 longwords of shorts */
            {
                word = (short *) &(segment[1]);
                word[0] = 0x0000;
                word[1] = 0x1111;
                word[2] = 0x2222;
                word[3] = 0x3333;
            }
        }
        segment = &(bank[2]) + 7;	/* point past segment 1 */
        segment[0] = 4<<24 | 0x34<<16 | 3; /* seg 4 has I*2 packets */
        {
            packet = (short *) &(segment[1]);
            packet[0] = 1<<8 | 2;	/* packet 1 */
            packet[1] = 0x1111;
            packet[2] = 0x2222;
            packet += 3;
            packet[0] = 2<<8 | 2;	/* packet 2 */
            packet[1] = 0x1111;
            packet[2] = 0x2222;
        }
        segment += 4;
        segment[0] = 5<<24 | 0xF<<16 | 8; /* seg 5 contains repeating structures */
        {
            word = (short *) &(segment[1]);
            word[0] = 2;
            word[1] = 2<<8 | 2;	/* 2(a,b) */
            word[2] = 0x8000 | 2<<4 | 1; /* 2I */
            word[3] = 0x8000 | 1<<4 | 2; /* 1F */
            longword = &(segment[3]);
            data = 123.456;
            longword[0] = 0x1111;
            longword[1] = 0x2222;
            longword[2] = *(int *)&data;
            longword[3] = 0x11111111;
            longword[4] = 0x22222222;
            longword[5] = *(int *)&data;
        }
    }
    return(bank);
}
