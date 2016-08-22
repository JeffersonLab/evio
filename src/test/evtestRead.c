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


    int main2()
    {
        int handle, status, nevents, nwords, i;
        uint32_t  *buffer, *ip, bufLen;

        status = evOpen("/home/timmer/evioTestFiles/evioV2format.ev", "r", &handle);
        nevents = 0;

        while ((status = evReadAlloc(handle, &buffer, &bufLen)) == S_SUCCESS) {
            nevents++;

            printf("    Event #%d,  len = %d data bytes\n", nevents, 4*(buffer[0] - 1));

            ip = buffer;
            nwords = buffer[0] + 1;

            printf("      Header words\n");
            printf("        %#10.8x\n", *ip++);
            printf("        %#10.8x\n\n", *ip++);
            printf("      Data words\n");

            nwords -= 2;

            free(buffer);


//        printf("Hit Enter to continue:\n");
//        getchar();
        }

        if (status == EOF) {
            printf("    Last read, reached EOF!\n");
        }
        else {
            printf ("    Last evRead status = %d, %s\n", status, evPerror(status));
        }

        status = evClose(handle);
        printf ("    Closed /home/timmer/evioTestFiles/evioV2format.ev again, status = %d\n\n", status);
    }


    int main()
    {
        int handle, status, nevents, nwords, i;
        uint32_t  buffer[50000], *ip;

        status = evOpen("/home/timmer/evioTestFiles/evioV2format.ev", "r", &handle);
        nevents = 0;

        while ((status = evRead(handle, buffer, 50000)) == S_SUCCESS) {
            nevents++;

            printf("    Event #%d,  len = %d data bytes\n", nevents, 4*(buffer[0] - 1));

            ip = buffer;
            nwords = buffer[0] + 1;

            printf("      Header words\n");
            printf("        %#10.8x\n", *ip++);
            printf("        %#10.8x\n\n", *ip++);
            printf("      Data words\n");

            nwords -= 2;


//        printf("Hit Enter to continue:\n");
//        getchar();
        }

        if (status == EOF) {
            printf("    Last read, reached EOF!\n");
        }
        else {
            printf ("    Last evRead status = %d, %s\n", status, evPerror(status));
        }

        status = evClose(handle);
        printf ("    Closed /home/timmer/evioTestFiles/evioV2format.ev again, status = %d\n\n", status);
    }


    int main1()
{
    int handle, status, nevents, nwords, i;
    uint32_t  buffer[204800], *ip;

    status = evOpen("/tmp/fileTestSmallBigEndian.ev", "r", &handle);
    nevents = 0;

    while ((status = evRead(handle, buffer, 204800)) == S_SUCCESS) {
        nevents++;

        printf("    Event #%d,  len = %d data words\n", nevents, buffer[0] - 1);

        ip = buffer;
        nwords = buffer[0] + 1;

        printf("      Header words\n");
        printf("        %#10.8x\n", *ip++);
        printf("        %#10.8x\n\n", *ip++);
        printf("      Data words\n");

        nwords -= 2;

        for (; nwords > 0; nwords-=4) {
            for (i = MIN(nwords,4); i > 0; i--) {
                printf("        %#10.8x", *ip++);
            }
            printf("\n");
        }
        printf("\n");


//        printf("Hit Enter to continue:\n");
//        getchar();
    }

    if (status == EOF) {
        printf("    Last read, reached EOF!\n");
    }
    else {
        printf ("    Last evRead status = %d, %s\n", status, evPerror(status));
    }

    status = evClose(handle);
    printf ("    Closed /daqfs/home/timmer/coda/output-evio4.ev again, status = %d\n\n", status);
}


