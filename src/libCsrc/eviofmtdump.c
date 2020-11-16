/*
     eviofmtdump.c - dumps data into XML array

     input: iarr[] - data array (words)
            nwrd - length of data array in words
            ifmt[] - format (as produced by eviofmt.c)
            nfmt - the number of elements in ifmt[]
     return: the number of bytes in 'xml' if everything fine, negative if error

        Converts the data of array (iarr[i], i=0...nwrd-1)
        using the format code      ifmt[j], j=0...nfmt-1)

     algorithm description:
        data processed inside while(ib < nwrd) loop, where 'ib' is iarr[] index; loop breaks when 'ib'
        reaches the number of elements in iarr[]   
*/

#include <stdio.h>
#include <stdint.h>

#define MIN(a,b) ( (a) < (b) ? (a) : (b) )

/*
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
*/

#define SWAP64(x) (x)
#define SWAP32(x) (x)
#define SWAP16(x) (x)

typedef struct
{
  int left;    /* index of ifmt[] element containing left parenthesis */
  int nrepeat; /* how many times format in parenthesis must be repeated */
  int irepeat; /* right parenthesis counter, or how many times format
                  in parenthesis already repeated */
} LV;


#define PRINT

#undef DEBUG

#define NWORDS 1000000
static int32_t iarr[NWORDS+10];

int
eviofmtdump(int32_t *arr, int nwrd, unsigned short *ifmt, int nfmt, int nextrabytes, char *xml)
{
  int i, imt, ncnf, kcnf, mcnf, lev/*, iterm*/;
  int64_t *b64, *b64end;
  int32_t *b32, *b32end;
  int16_t *b16, *b16end;
  int8_t  *b8, *b8end;
  LV lv[10];
  char *xml1 = xml;


  if(nwrd <= 0 || nfmt<=0 || nwrd>NWORDS)
  {
    printf("ERROR in eviofmtdump: nwrd=%d, nfmt=%d, nwrd=%d\n",nwrd,nfmt,NWORDS);
    return(0);
  }
  for(i=0; i<nwrd; i++) iarr[i] = arr[i]; /* do not modify original buffer ! */


  imt = 0;   /* ifmt[] index */
  ncnf = 0;  /* how many times must repeat a format */
  lev  = 0;  /* parenthesis level */
  /* iterm = 0;  */

  b8 = (int8_t *)&iarr[0]; /* beginning of data */
  b8end = (int8_t *)&iarr[nwrd] - nextrabytes; /* end of data + 1 */

#ifdef DEBUG
  printf("\n=== eviofmtdump start (xml=0x%016lx) ===\n",(long unsigned int)xml);
  printf("iarr[0]=0x%08x, nwrd=%d\n",iarr[0],nwrd);
#endif

#ifdef PRINT
  xml += sprintf(xml,"         <row>\n");
#endif
  while(b8 < b8end)
  {
#ifdef DEBUG
    printf("+++ 0x%016lx 0x%016lx - getting next format code\n",
           (long unsigned int)b8,(long unsigned int)b8end);
#endif
    while(1) /* get next format code */
    {
      imt++;
#ifdef DEBUG
      printf("imt=%d, nfmt=%d\n",imt,nfmt);
#endif
      if(imt > nfmt) /* end of format statement reached, back to iterm - last parenthesis or format begining */
      {
        imt = 0/*iterm*/; /* Sergey: always start from the beginning of the format for now, will think about it ...*/
#ifdef PRINT
        xml += sprintf(xml,"         </row>\n");
        xml += sprintf(xml,"         <row>\n");
#endif
#ifdef DEBUG
        printf("======================= end of format statement reached, set imt=%d\n",imt);
#endif
#ifdef DEBUG
        printf("1\n");
#endif
      }
      else if(ifmt[imt-1] == 0) /* meet right parenthesis, so we're finished processing format(s) in parenthesis */
      {
        lv[lev-1].irepeat ++; /* increment counter */
        if(lv[lev-1].irepeat >= lv[lev-1].nrepeat) /* if format in parenthesis was processed */
        {                                          /* required number of times */
          //iterm = lv[lev-1].left - 1; /* store left parenthesis index minus 1
          //                            (if will meet end of format, will start from format index imt=iterm;
          //                             by default we continue from the beginning of the format (iterm=0)) */
          lev--; /* done with this level - decrease parenthesis level */
#ifdef PRINT
          xml += sprintf(xml,"          )\n");
#endif
#ifdef DEBUG
          printf("meet right parenthesis\n");
#endif
        }
        else /* go for another round of processing by setting 'imt' to the left parenthesis */
        {
          imt = lv[lev-1].left; /* points ifmt[] index to the left parenthesis from the same level 'lev' */
#ifdef PRINT
          xml += sprintf(xml,"\n");
#endif
#ifdef DEBUG
          printf("go for another round of processing by setting 'imt' to the left parenthesis\n");
#endif
        }
      }
      else
      {
        ncnf = (ifmt[imt-1]>>8)&0x3F; /* how many times to repeat format code */
        kcnf = ifmt[imt-1]&0xFF; /* format code */
        mcnf = (ifmt[imt-1]>>14)&0x3; /* repeat code */

        if(kcnf == 0) /* left parenthesis - set new lv[] */
        {

          /* check repeats for left parenthesis */

          if(mcnf==1) /* left parenthesis, SPECIAL case: #repeats must be taken from int32 data */
          {
            mcnf = 0;
            b32 = (int32_t *)b8;
            ncnf = *b32 = SWAP32(*b32); /* get #repeats from data */
#ifdef PRINT
            xml += sprintf(xml,"          %d(\n",ncnf);
            /*printf("ncnf(: %d\n",ncnf);*/
#endif
            b8 += 4;
#ifdef DEBUG
            printf("ncnf from data = %d (code 15)\n",ncnf);
#endif
          }

          if(mcnf==2) /* left parenthesis, SPECIAL case: #repeats must be taken from int16 data */
          {
            mcnf = 0;
            b16 = (int16_t *)b8;
            ncnf = *b16 = SWAP16(*b16); /* get #repeats from data */
#ifdef PRINT
            xml += sprintf(xml,"          %d(\n",ncnf);
            /*printf("ncnf(: %d\n",ncnf);*/
#endif
            b8 += 2;
#ifdef DEBUG
            printf("ncnf from data = %d (code 14)\n",ncnf);
#endif
          }

          if(mcnf==3) /* left parenthesis, SPECIAL case: #repeats must be taken from int8 data */
          {
            mcnf = 0;
            ncnf = *((int8_t *)b8); /* get #repeats from data */
#ifdef PRINT
            xml += sprintf(xml,"          %d(\n",ncnf);
            /*printf("ncnf(: %d\n",ncnf);*/
#endif
            b8 ++;
#ifdef DEBUG
            printf("ncnf from data = %d (code 13)\n",ncnf);
#endif
          }


          lv[lev].left = imt; /* store ifmt[] index */
          lv[lev].nrepeat = ncnf; /* how many time will repeat format code inside parenthesis */
          lv[lev].irepeat = 0; /* cleanup place for the right parenthesis (do not get it yet) */
          lev++; /* increase parenthesis level */
#ifdef DEBUG
          printf("left parenthesis - set new lv[]\n");
#endif
        }
        else /* format F or I or ... */
        {
          if(lev == 0) /* there are no parenthesis, just simple format, go to processing */
          {
#ifdef DEBUG
            printf("there are no parenthesis, just simple format, go to processing\n");
#endif
            break;
          }
          else if(imt != (nfmt-1)) /* have parenthesis, NOT the pre-last format element (last assumed ')' ?) */
          {
#ifdef DEBUG
            printf("have parenthesis, NOT the pre-last format element: %d %d\n",imt,nfmt-1);
#endif
            break;
          }
          else if(imt != lv[lev-1].left+1) /* have parenthesis, NOT the first format after the left parenthesis */
          {
#ifdef DEBUG
            printf("have parenthesis, NOT the first format after the left parenthesis: %d %d\n",imt,nfmt-1);
#endif
            break;
          }
          else
          {
            /* if none of above, we are in the end of format */
            ncnf = 999999999; /* set format repeat to the big number */
#ifdef DEBUG
            printf("none of above, we are in the end of format\n");
#endif
            break;
          }
        }
      }
    }


    /* if 'ncnf' is zero, get it from data */
	/*printf("ncnf=%d\n",ncnf);fflush(stdout);*/
    if(ncnf==0)
    {
      if(mcnf==1)
	  {
        b32 = (int32_t *)b8;
        ncnf = *b32 = SWAP32(*b32);
		/*printf("ncnf32=%d\n",ncnf);fflush(stdout);*/
        b8 += 4;
	  }
      else if(mcnf==2)
	  {
        b16 = (int16_t *)b8;
        ncnf = *b16 = SWAP16(*b16);
		/*printf("ncnf16=%d\n",ncnf);fflush(stdout);*/
        b8 += 2;
	  }
      else if(mcnf==3)
	  {
        ncnf = *b8;
		/*printf("ncnf08=%d\n",ncnf);fflush(stdout);*/
        b8 += 1;
	  }
      /*else printf("ncnf00=%d\n",ncnf);fflush(stdout);*/

#ifdef PRINT
	  xml += sprintf(xml,"          %d:\n",ncnf);
      /*printf("ncnf: %d\n",ncnf);*/
#endif
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
      b64 = (int64_t *)b8;
      b64end = b64 + ncnf;
      if(b64end > (int64_t *)b8end) b64end = (int64_t *)b8end;
      while(b64 < b64end)
	  {
#ifdef PRINT
        xml += sprintf(xml,"             64bit: 0x%llx(%lld)\n",
                       (long long unsigned int)*b64,(long long int)*b64);
#endif
#ifdef DEBUG
        printf("64bit: 0x%llx(%lld)\n",(unsigned long long)*b64,(long long)*b64);
#endif
        *b64 = SWAP64(*b64);
        b64++;
	  }
      b8 = (int8_t *)b64;
#ifdef DEBUG
      printf("64bit: %d elements\n",ncnf);
#endif
    }
    else if(kcnf==1||kcnf==2||kcnf==11||kcnf==12) /* 32-bit */
    {
      b32 = (int32_t *)b8;
      b32end = b32 + ncnf;
      if(b32end > (int32_t *)b8end) b32end = (int32_t *)b8end;
      while(b32 < b32end)
	  {
#ifdef PRINT
        xml += sprintf(xml,"             32bit: 0x%08x(%d)\n",*b32,*b32);
#endif
#ifdef DEBUG
        printf("32bit: 0x%08x(%d)\n",*b32,*b32);
#endif
        *b32 = SWAP32(*b32);
        b32++;
	  }
      b8 = (int8_t *)b32;
#ifdef DEBUG
      printf("32bit: %d elements\n",ncnf);
#endif
    }
    else if(kcnf==4||kcnf==5)       /* 16 bits */
    {
      b16 = (int16_t *)b8;
      b16end = b16 + ncnf;
      if(b16end > (int16_t *)b8end) b16end = (int16_t *)b8end;
#ifdef PRINT
      xml += sprintf(xml,"             16bit:");
#endif
#ifdef DEBUG
      printf("16bit:");
#endif
      while(b16 < b16end)
	  {
#ifdef PRINT
        xml += sprintf(xml," 0x%04x(%d)",(unsigned short)*b16,*b16);
#endif
#ifdef DEBUG
        printf(" 0x%04x(%d)",*b16,*b16);
#endif
        *b16 = SWAP16(*b16);
        b16++;
	  }
#ifdef PRINT
      xml += sprintf(xml,"\n");
#ifdef DEBUG
      printf("\n");
#endif
#endif
      b8 = (int8_t *)b16;
#ifdef DEBUG
      printf("16bit: %d elements\n",ncnf);
#endif
    }
    else if(kcnf==6||kcnf==7||kcnf==3)       /* 8 bits */
    {
#ifdef PRINT
      {
        int jjj;
#ifdef DEBUG
        printf("08b:");
#endif
        xml += sprintf(xml,"             08bit:");
        for(jjj=0; jjj<ncnf; jjj++)
		{
#ifdef DEBUG
          printf(" 0x%02x(%d)",b8[jjj],b8[jjj]);
#endif
          xml += sprintf(xml," 0x%02x(uchar=%u char=%d)",((unsigned char *)b8)[jjj],((unsigned char *)b8)[jjj],b8[jjj]);
		}
#ifdef DEBUG
        printf("\n");
#endif
        xml += sprintf(xml,"\n");
	  }
#endif
      b8 += ncnf;
#ifdef DEBUG
      printf("8bit: %d elements\n",ncnf);
#endif
      /*do nothing for characters*/
    }

  } /*while*/

#ifdef PRINT
  xml += sprintf(xml,"         </row>\n");
#endif

#ifdef DEBUG
  printf("\n=== eviofmtdump end (xml1=0x%016lx, xml=0x%016lx, len=%ld) ===\n",
         (long unsigned int)xml1,(long unsigned int)xml,xml-xml1);fflush(stdout);
#endif

  return (int)(xml-xml1);
}
