/*
 * This program is for testing the new string manipulation
 * routines which facilitate the splitting and automatic
 * naming of files.
 *
 * author: Carl Timmer
 */
#include <stdlib.h>
#include <stdio.h>
#include "evio.h"

#define MAXBUFLEN  4096
#define MIN(a,b) (a<b)? a : b

extern int evOpenFake(char *filename, char *flags, int *handle, char **evf);



int main (int argc, char **argv)
{
    int err, handle, specifierCount=-1;
    EVFILE *a;
   
    char *orig = "My_%s_%3d_$(BLAH)_%4x";
    char *replace = "X";
    char *with = "$(BLAH)";
    
    char *tmp, *result = evStrReplace(orig, replace, with);
    
    if (argc > 1) orig = argv[1];
    
    printf("String = %s\n", orig);
    printf("OUT    = %s\n", result);

    tmp = evStrReplaceEnvVar(result);
    printf("ENV    = %s\n", tmp);
    free(tmp);
    free(result);
    
    result = evStrReplaceSpecifier(orig, &specifierCount);
    if (result == NULL) {
        printf("error in evStrReplaceSpecifier routine\n");
    }
    else {
        printf("SPEC    = %s, count = %d\n", result, specifierCount);
    }
    
    /* Simulate evOpen() */
    err = evOpenFake(strdup(orig), "w", &handle, &tmp);
    if (err != S_SUCCESS) {
        printf("Error in evOpenfake(), err = %x\n", err);
        exit(0);
    }
    
    a = (EVFILE *) tmp;
    
    printf("opened file = %s\n", a->filename);
   
    
    err = evGenerateBaseFileName(handle, "runType", &specifierCount);
    if (err != S_SUCCESS) {
        printf("Error in evGenerateBaseFileName(), err = %x\n", err);
        exit(0);
    }

    printf("BASE   = %s, count = %d\n", a->filename, specifierCount);
    
    result = evGenerateFileName(handle, specifierCount, 7, 0, 666);
    if (result == NULL) {
        printf("Error in evGenerateFileName()\n");
        exit(0);
    }


    printf("FINAL = %s, count = %d\n", result, specifierCount);
    free(result);

    exit(0);
}
