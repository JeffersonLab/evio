/*
 * This program is for testing the automatic generation of file names.
 *
 * author: Carl Timmer
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "evio.h"

#define EV_WRITEFILE    4


int main (int argc, char **argv)
{
    int err, specifierCount=0;

    EVFILE a;

    char* baseName;
    char* runType  = "myRunType";

    /* The following will substitute the env var USER into file name,
     * while %s will be substituted with runType. */

    char* baseFileNames[4];
    baseFileNames[0] = "File_for_$(USER)_%s__";
/*    baseFileNames[1] = "File_for_$(USER)_%s_0x%03x_";*/
    baseFileNames[1] = "File_for_$(USER)_%s_%03d_";
    baseFileNames[2] = "File_for_$(USER)_%s_%3d_%05d__";
    baseFileNames[3] = "File_for_$(USER)_%s_%3d_%05d_%07d";

    uint32_t runNumber   = 33;
    uint32_t splitNumber = 100;
    uint32_t streamId    = 77;
    uint32_t streamCount;
    int splitting, debug = 1;

    evFileStructInit(&a);
    a.rw = EV_WRITEFILE;

    for (int i=0; i < 4; i++) {

        err = evGenerateBaseFileName(baseFileNames[i], &baseName, &specifierCount);
        if (err != S_SUCCESS) {
            printf("\nevGenerateBaseFileName: bad filename/format-specifiers or more that 3 specifiers found\n\n");
            return 1;
        }

        printf("\norig base = %s, env var subbed = %s, specifier count = %d\n\n",
               baseFileNames[i], baseName, specifierCount);

        a.baseFileName = baseName;

        splitting = 1;
        streamCount = 1;

        evGenerateFileName(&a, specifierCount, runNumber, splitting, splitNumber,
                                    runType, streamId, streamCount, debug);

        streamCount = 2;
        evGenerateFileName(&a, specifierCount, runNumber, splitting, splitNumber,
                                    runType, streamId, streamCount, debug);

        splitting = 0;
        streamCount = 1;

        evGenerateFileName(&a, specifierCount, runNumber, splitting, splitNumber,
                                    runType, streamId, streamCount, debug);

        streamCount = 2;
        evGenerateFileName(&a, specifierCount, runNumber, splitting, splitNumber,
                                    runType, streamId, streamCount, debug);
    }


    return(0);
}

