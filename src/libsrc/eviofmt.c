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
 *  Code to translate a string containing a data format
 *  into an array of integer codes. To be used with the
 *  function eviofmtswap for swapping data held in this
 *  format.
 *  
 * Author:  Sergey Boiarinov, Hall B
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include "evio.h"

#define MAX(a,b)  ( (a) > (b) ? (a) : (b) )

#define debugprint(ii) \
  printf("  [%3d] %3d (16 * %2d + %2d), nr=%d\n",ii,ifmt[ii],ifmt[ii]/16,ifmt[ii]-(ifmt[ii]/16)*16,nr)

#undef DEBUG

/**
 * <pre>
 *  This routine transforms a composite, format-containing
 *  ASCII string to an unsigned char array. It is to be used
 *  in conjunction with {@link #eviofmtswap} to swap the endianness of
 *  composite data.
 * 
 *   format code bits <- format in ascii form
 *    [15:14] [13:8] [7:0]
 *      Nnm      #     0           #'('
 *        0      0     0            ')'
 *      Nnm      #     1           #'i'   unsigned int
 *      Nnm      #     2           #'F'   floating point
 *      Nnm      #     3           #'a'   8-bit char (C++)
 *      Nnm      #     4           #'S'   short
 *      Nnm      #     5           #'s'   unsigned short
 *      Nnm      #     6           #'C'   char
 *      Nnm      #     7           #'c'   unsigned char
 *      Nnm      #     8           #'D'   double (64-bit float)
 *      Nnm      #     9           #'L'   long long (64-bit int)
 *      Nnm      #    10           #'l'   unsigned long long (64-bit int)
 *      Nnm      #    11           #'I'   int
 *      Nnm      #    12           #'A'   hollerit (4-byte char with int endining)
 *
 *   NOTES:
 *    1. The number of repeats '#' must be the number between 2 and 63, number 1 assumed by default
 *    2. If the number of repeats is symbol 'N' instead of the number, it will be taken from data assuming 'int32' format;
 *       if the number of repeats is symbol 'n' instead of the number, it will be taken from data assuming 'int16' format;
 *       if the number of repeats is symbol 'm' instead of the number, it will be taken from data assuming 'int8' format;
 *       Two bits Nnm [15:14], if not zero, requires to take the number of repeats from data in appropriate format:
 *            [01] means that number is integer (N),
 *            [10] - short (n),
 *            [11] - char (m)
 *    3. If format ends but end of data did not reach, format in last parenthesis
 *       will be repeated until all data processed; if there are no parenthesis
 *       in format, data processing will be started from the beginnig of the format
 *       (FORTRAN agreement)
 * </pre>
 * 
 *  @param fmt     null-terminated composite data format string
 *  @param ifmt    unsigned short array to hold transformed format
 *  @param ifmtLen length of unsigned short array, ifmt, in # of shorts
 * 
 *  @return the number of shorts in ifmt[] (positive)
 *  @return -1 to -8 for improper format string
 *  @return -9 if unsigned char array is too small
 */
int
eviofmt(char *fmt, unsigned short *ifmt, int ifmtLen)
{
    char ch;
    int  l, n, kf, lev, nr, nn, nb;
    size_t fmt_len;

    n   = 0; /* ifmt[] index */
    nr  = 0;
    nn  = 1;
    lev = 0;
    nb = 0; /* the number of bytes in length taken from data */

#ifdef DEBUG
    printf("\n=== eviofmt start, fmt >%s< ===\n",fmt);
#endif

    /* loop over format string */
    fmt_len = strlen(fmt);
    if (fmt_len > INT_MAX) return (-1);
    for (l=0; l < (int)fmt_len; l++)
    {
      ch = fmt[l];
      if (ch == ' ') continue;
#ifdef DEBUG
      printf("%c\n",ch);
#endif
      /* if digit, we have hard coded 'number', following before komma will be repeated 'number' times;
      forming 'nr' */
      if (isdigit(ch))
      {
        if (nr < 0) return(-1);
        nr = 10*MAX(0,nr) + atoi((char *)&ch);
        if (nr > 15) return(-2);
#ifdef DEBUG
        printf("the number of repeats nr=%d\n",nr);
#endif
      }

      /* a left parenthesis -> 16*nr + 0 */
      else if (ch == '(')
      {
        if (nr < 0) return(-3);
        if (--ifmtLen < 0) return(-9);
        lev++;
#ifdef DEBUG
        printf("111: nn=%d nr=%d\n",nn,nr);
#endif

        if (nn == 0) /*special case: if #repeats is in data, set bits [15:14] */
		{
          if(nb==4)      ifmt[n++] = (1<<14);
          else if(nb==2) ifmt[n++] = (2<<14);
          else if(nb==1) ifmt[n++] = (3<<14);
          else {printf("eviofmt ERROR: unknown nb=%d\n",nb);exit(0);}
		}
        else /* #repeats hardcoded */
		{
          ifmt[n++] = (MAX(nn,nr)&0x3F) << 8;
		}

        nn = 1;
        nr = 0;
#ifdef DEBUG
        debugprint(n-1);
#endif
      }
      /* a right parenthesis -> (0<<8) + 0 */
      else if (ch == ')')
      {
        if (nr >= 0) return(-4);
        if (--ifmtLen < 0) return(-9);
        lev--;
        ifmt[n++] = 0;
        nr = -1;
#ifdef DEBUG
        debugprint(n-1);
#endif
      }
      /* a komma, reset nr */
      else if (ch == ',')
      {
        if (nr >= 0) return(-5);
        nr = 0;
#ifdef DEBUG
        printf("komma, nr=%d\n",nr);
#endif
      }
      /* variable length format (int32) */
      else if (ch == 'N')
      {
        nn = 0;
        nb = 4;
#ifdef DEBUG
        printf("N, nb=%d\n",nb);
#endif
      }
      /* variable length format (int16) */
      else if (ch == 'n')
      {
        nn = 0;
        nb = 2;
#ifdef DEBUG
        printf("n, nb=%d\n",nb);
#endif
      }
      /* variable length format (int8) */
      else if (ch == 'm')
      {
        nn = 0;
        nb = 1;
#ifdef DEBUG
        printf("m, nb=%d\n",nb);
#endif
      }
      /* actual format */
      else
      {
        if(     ch == 'i') kf = 1;  /* 32 */
        else if(ch == 'F') kf = 2;  /* 32 */
        else if(ch == 'a') kf = 3;  /*  8 */
        else if(ch == 'S') kf = 4;  /* 16 */
        else if(ch == 's') kf = 5;  /* 16 */
        else if(ch == 'C') kf = 6;  /*  8 */
        else if(ch == 'c') kf = 7;  /*  8 */
        else if(ch == 'D') kf = 8;  /* 64 */
        else if(ch == 'L') kf = 9;  /* 64 */
        else if(ch == 'l') kf = 10; /* 64 */
        else if(ch == 'I') kf = 11; /* 32 */
        else if(ch == 'A') kf = 12; /* 32 */
        else               kf = 0;

        if (kf != 0)
        {
          if (nr < 0) return(-6);
          if (--ifmtLen < 0) return(-9);
#ifdef DEBUG
          printf("222: nn=%d nr=%d\n",nn,nr);
#endif


          ifmt[n++] = ((MAX(nn,nr)&0x3F)<<8) + kf;


		  if(nb>0)
		  {
            if(nb==4)      ifmt[n-1] |= (1<<14);
            else if(nb==2) ifmt[n-1] |= (2<<14);
            else if(nb==1) ifmt[n-1] |= (3<<14);
            else {printf("eviofmt ERROR: unknown nb=%d\n",nb);exit(0);}
		  }


          nn=1;
#ifdef DEBUG
          debugprint(n-1);
#endif
        }
        else
        {
          /* illegal character */
          return(-7);
        }
        nr = -1;
      }
    }

    if (lev != 0) return(-8);

#ifdef DEBUG
    printf("=== eviofmt end ===\n");
#endif

    return(n);
}
