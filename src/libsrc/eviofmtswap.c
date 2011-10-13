/*
     eviofmtswap.c - converts between IEEE (big endian) and DECS (little endian)

     input: iarr[] - data array (words)
            nwrd - length of data array in words
            ifmt[] - format (as produced by eviofmt.c)
            nfmt - the number of bytes in ifmt[]
     return: 0 if everything fine, negative if error

        Converts the data of array (iarr[i], i=0...nwrd-1)
        using the format code      ifmt[j], j=0...nfmt-1)

     algorithm description:
        data processed inside while(ib < nwrd) loop, where 'ib' is iarr[] index; loop breaks when 'ib'
        reaches the number of elements in iarr[]   
*/

#include <stdio.h>

#define MIN(a,b) ( (a) < (b) ? (a) : (b) )

#define SWAP64(x) ( (((x) & 0x00000000000000ff) << 56) | \
                    (((x) & 0x000000000000ff00) << 40) | \
                    (((x) & 0x0000000000ff0000) << 24) | \
                    (((x) & 0x00000000ff000000) <<  8) | \
                    (((x) & 0x000000ff00000000) >>  8) | \
                    (((x) & 0x0000ff0000000000) >> 24) | \
                    (((x) & 0x00ff000000000000) >> 40) | \
                    (((x) & 0xff00000000000000) >> 56) )

#define SWAP32(x) ( (((x) & 0x000000ff) << 24) | \
                    (((x) & 0x0000ff00) <<  8) | \
                    (((x) & 0x00ff0000) >>  8) | \
                    (((x) & 0xff000000) >> 24) )

#define SWAP16(x) ( (((x) & 0x00ff) << 8) | \
                    (((x) & 0xff00) >> 8) )

typedef struct
{
  int left;    /* index of ifmt[] element containing left parenthesis */
  int nrepeat; /* how many times format in parenthesis must be repeated */
  int irepeat; /* right parenthesis counter, or how many times format
                  in parenthesis already repeated */
} LV;

#undef DEBUG

int
eviofmtswap(int *iarr, int nwrd, unsigned char *ifmt, int nfmt)
{
  int imt, ncnf, kcnf, lev, iterm;
  long long *b64, *b64end;
  int *b32, *b32end;
  short *b16, *b16end;
  char *b8, *b8end;
  LV lv[10];

  if(nwrd <= 0 || nfmt<=0) return(-1);

  imt = 0;   /* ifmt[] index */
  ncnf = 0;  /* how many times must repeat a format */
  lev  = 0;  /* parenthesis level */
  iterm = 0; /*  */

  b8 = (char *)&iarr[0]; /* beginning of data */
  b8end = (char *)&iarr[nwrd]; /* end of data + 1 */

  while(b8 < b8end)
  {
#ifdef DEBUG
    printf("+++ 0x%08x 0x%08x\n",b8,b8end);
#endif
    while(1) /* get next format code */
    {
      imt++;
      if(imt > nfmt) /* end of format statement reached, back to iterm - last parenthesis or format begining */
      {
        imt = iterm;
#ifdef DEBUG
        printf("1\n");
#endif
      }
      else if(ifmt[imt-1] == 0) /* meet right parenthesis, so we're finished processing format(s) in parenthesis */
      {
        lv[lev-1].irepeat ++; /* increment counter */
        if(lv[lev-1].irepeat >= lv[lev-1].nrepeat) /* if format in parenthesis was processed */
        {                                          /* required number of times */
          iterm = lv[lev-1].left - 1; /* store left parenthesis index minus 1
                                      (if will meet end of format, will start from format index imt=iterm;
                                       by default we continue from the beginning of the format (iterm=0)) */
          lev--; /* done with this level - decrease parenthesis level */
#ifdef DEBUG
          printf("2\n");
#endif
        }
        else /* go for another round of processing by setting 'imt' to the left parenthesis */
        {
          imt = lv[lev-1].left; /* points ifmt[] index to the left parenthesis from the same level 'lev' */
#ifdef DEBUG
          printf("3\n");
#endif
        }
      }
      else
      {
        ncnf = ifmt[imt-1]/16; /* how many times to repeat format code */
        kcnf = ifmt[imt-1]-16*ncnf; /* format code */
        if(kcnf == 0) /* left parenthesis - set new lv[] */
        {
          lv[lev].left = imt; /* store ifmt[] index */
          lv[lev].nrepeat = ncnf; /* how many time will repeat format code inside parenthesis */
          lv[lev].irepeat = 0; /* cleanup place for the right parenthesis (do not get it yet) */
          lev++; /* increase parenthesis level */
#ifdef DEBUG
          printf("4\n");
#endif
        }
        else /* format F or I or ... */
        {
          if(lev == 0) /* there are no parenthesis, just simple format, go to processing */
          {
#ifdef DEBUG
            printf("51\n");
#endif
            break;
          }
          else if(imt != (nfmt-1)) /* have parenthesis, NOT the pre-last format element (last assumed ')' ?) */
          {
#ifdef DEBUG
            printf("52: %d %d\n",imt,nfmt-1);
#endif
            break;
          }
          else if(imt != lv[lev-1].left+1) /* have parenthesis, NOT the first format after the left parenthesis */
          {
#ifdef DEBUG
            printf("53: %d %d\n",imt,nfmt-1);
#endif
            break;
          }
          else
          {
            /* if none of above, we are in the end of format */
            ncnf = 999999999; /* set format repeat to the big number */
#ifdef DEBUG
            printf("54\n");
#endif
            break;
          }
        }
      }
    }


    /* if 'ncnf' is zero, get it from data (alwas in 'int' format) */
    if(ncnf==0)
    {
      b32 = (int *)b8;
      ncnf = *b32 = SWAP32(*b32);
      b8 += 4;
#ifdef DEBUG
      printf("ncnf from data = %d\n",ncnf);
#endif
    }


    /* at that point we are ready to process next piece of data; we have following entry info:
         kcnf - format type
         ncnf - how many times format repeated
         b8 - starting data pointer (it comes from previously processed piece)
    */

    /* Convert data in according to type kcnf */
    if(kcnf==8||kcnf==9||kcnf==10) /* 64-bit */
    {
      b64 = (long long *)b8;
      b64end = b64 + ncnf;
      if(b64end > (long long *)b8end) b64end = (long long *)b8end;
      while(b64 < b64end) *b64++ = SWAP64(*b64);
      b8 = (char *)b64;
#ifdef DEBUG
      printf("64bit: %d elements\n",ncnf);
#endif
    }
    else if(kcnf==1||kcnf==2||kcnf==11||kcnf==12) /* 32-bit */
    {
      b32 = (int *)b8;
      b32end = b32 + ncnf;
      if(b32end > (int *)b8end) b32end = (int *)b8end;
      while(b32 < b32end) *b32++ = SWAP32(*b32);
      b8 = (char *)b32;
#ifdef DEBUG
      printf("F,I: %d elements\n",ncnf);
#endif
    }
    else if(kcnf==4||kcnf==5)       /* 16 bits */
    {
      b16 = (short *)b8;
      b16end = b16 + ncnf;
      if(b16end > (short *)b8end) b16end = (short *)b8end;
      while(b16 < b16end) *b16++ = SWAP16(*b16);
      b8 = (char *)b16;
#ifdef DEBUG
      printf("S: %d elements\n",ncnf);
#endif
    }
    else if(kcnf==6||kcnf==7||kcnf==3)       /* 8 bits */
    {
      b8 += ncnf;
#ifdef DEBUG
      printf("C: %d elements\n",ncnf);
#endif
      /*do nothing for characters*/
    }

  } /*while*/

  return(0);
}
