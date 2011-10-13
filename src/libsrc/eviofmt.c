/*
   eviofmt.c - translate Format from ASCII string form to unsigned char array

   input:  null-terminated character string fmt
   output: unsigned char ifmt[]
   return: the number of bytes in ifmt[], or negative value on error

     fmt code bits    fmt in ascii form
     [7:4] [3:0]
       #     0           #'('
       0     0            ')'
       #     1           #'i'   unsigned int
       #     2           #'F'   floating point
       #     3           #'a'   8-bit char (C++)
       #     4           #'S'   short
       #     5           #'s'   unsigned short
       #     6           #'C'   char
       #     7           #'c'   unsigned char
       #     8           #'D'   double (64-bit float)
       #     9           #'L'   long long (64-bit int)
       #    10           #'l'   unsigned long long (64-bit int)
       #    11           #'I'   int
       #    12           #'A'   hollerit (4-byte char with int endining)

   NOTES:
    1. If format ends but end of data did not reach, format in last parenthesis
       will be repeated until all data processed; if there are no parenthesis
       in format, data processing will be started from the beginnig of the format
       (FORTRAN agreement)
    2. The number of repeats '#' must be the number between 2 and 15; if the number
       of repeats is symbol 'N' instead of the number, it will be taken from data
       assuming 'int' format
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX(a,b)  ( (a) > (b) ? (a) : (b) )

#define debugprint(ii) \
  printf("  [%3d] %3d (16 * %2d + %2d), nr=%d\n",ii,ifmt[ii],ifmt[ii]/16,ifmt[ii]-(ifmt[ii]/16)*16,nr)

#undef DEBUG

int
eviofmt(char *fmt, unsigned char *ifmt)
{
  int l;
  int n, kf, lev, nr, nn;
  char ch;

  n = 0; /* ifmt[] indef */
  lev = 0;
  nr = 0;
  nn = 1;

#ifdef DEBUG
  printf("\nfmt >%s<\n",fmt);
#endif

  /* loop over format string */
  for(l=0; l<strlen(fmt); l++)
  {
    ch = fmt[l];
    if(ch == ' ') continue;
#ifdef DEBUG
    printf("%c\n",ch);
#endif

    if(isdigit(ch))     /* if digit, following before komma will be repeated 'number' times */
    {
      if(nr < 0) return(-1);
      nr = 10*MAX(0,nr) + atoi((char *)&ch);
      if(nr > 15) return(-2);
#ifdef DEBUG
      printf("the number of repeats nr=%d\n",nr);
#endif
    }
    else if(ch == '(')  /* a left parenthesis -> 16*nr + 0 */
    {
      if(nr < 0) return(-3);
      lev ++;
      ifmt[n++] = 16*MAX(nn,nr);
      nn = 1;
      nr = 0;
#ifdef DEBUG
      debugprint(n-1);
#endif
    }
    else if(ch == ')')  /* a right parenthesis -> 16*0 + 0 */
    {
      if(nr >= 0) return(-4);
      lev --;
      ifmt[n++] = 0;
      nr = -1;
#ifdef DEBUG
      debugprint(n-1);
#endif
    }
    else if(ch == ',')  /* a komma, reset nr */
    {
      if(nr >= 0) return(-5);
      nr = 0;
#ifdef DEBUG
      printf("komma, nr=%d\n",nr);
#endif
    }
    else if(ch == 'N')  /* variable length format */
    {
      nn = 0;
#ifdef DEBUG
      printf("nn\n");
#endif
    }
    else                /* actual format */
    {
      if(     ch == 'i') kf = 1;  /*32*/
      else if(ch == 'F') kf = 2;  /*32*/
      else if(ch == 'a') kf = 3;  /*8*/
      else if(ch == 'S') kf = 4;  /*16*/
      else if(ch == 's') kf = 5;  /*16*/
      else if(ch == 'C') kf = 6;  /*8*/
      else if(ch == 'c') kf = 7;  /*8*/
      else if(ch == 'D') kf = 8;  /*64*/
      else if(ch == 'L') kf = 9;  /*64*/
      else if(ch == 'l') kf = 10; /*64*/
      else if(ch == 'I') kf = 11; /*32*/
      else if(ch == 'A') kf = 12; /*32*/
      else kf = 0;

      if(kf != 0)
      {
        if(nr < 0) return(-6);
        ifmt[n++] = 16*MAX(nn,nr) + kf;
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

  if(lev != 0) return(-8);

#ifdef DEBUG
  printf("\n");
#endif

  return(n);
}
