/*
 *  evio2xml.c
 *
 *   Converts evio data to xml, full support for tagsegments and long long
 *
 *   NOTE:  does NOT handle VAX float or double, packets, or repeating structures
 *
 *   This code will eventually be reorganized into a library
 *
 *
 *  Descriptors used in output XML:
 *
 *   term         size in bytes
 *   ----         -------------
 *   byte              1
 *   short             2
 *   int               4
 *   float             4 IEEE format
 *   long              8
 *   double            8 IEEE format
 *
 *
 *   Author: Elliott Wolin, JLab, 5-jun-2001
*/

/* still to do
 * -----------
*/



/* for posix */
#define _POSIX_SOURCE_ 1
#define __EXTENSIONS__


/*  misc macros */
#define MAXBUFLEN 100000
#define INDENT_SIZE 3


/* include files */
#include <stdio.h>
#include <stdlib.h>
#define min(a, b)  ((a) > (b) ? (b) : (a))


/*  misc variables */
char* filename;
int nevent         = 0;
int skip_event     = 0;
int max_event      = 0;
int debug          = 0;
int nindent        = 0;
int no_indent      = 0;


/* prototypes */
void decode_command_line(int argc, char **argv);
void evio2xml(unsigned long *buf);
void dump_bank(unsigned long *bank);
void dump_segment(unsigned long *segment);
void dump_tagsegment(unsigned long *tagsegment);
void dump_data(unsigned long *data, int type, int length);
int get_ndata(int type, int nwords);
char *get_type(int type);
void indent(void);

int evOpen(const char *filename, const char *mode, int *handle);
int evRead(int handle, unsigned long *buf, int maxbuflen );
int evClose(int handle);


/*--------------------------------------------------------------------------*/


int main (int argc, char **argv) {

  int handle,status;
  unsigned long buf[MAXBUFLEN];
  

  /* decode command line */
  decode_command_line(argc,argv);


  /* open file */
  if((status=evOpen(filename,"r",&handle))!=0) {
    printf("/n ?Unable to open file %s, status=$d\n\n",filename,status);
    exit(EXIT_FAILURE);
  }


  /* loop over events, perhaps skip some, dump up to max_event events */
  nevent=0;
  while ((status=evRead(handle,buf,MAXBUFLEN))==0) {
    nevent++;
    if(skip_event>=nevent)continue;
    evio2xml(buf);
    if((nevent>=max_event+skip_event)&&(max_event!=0))break;
  }

  /* done */
  evClose(handle);
  exit(EXIT_SUCCESS);
}


/*---------------------------------------------------------------- */


void evio2xml(unsigned long *buf) {

  printf("\n\n<!-- ===================== Event %d contains %d words (%d bytes) ===================== -->\n\n",
	 nevent,buf[0]+1,4*(buf[0]+1));
  printf("<event type=\"evio\" count=\"%d\">\n\n",nevent);

  /* event is always a bank */
  dump_bank(buf);

  printf("\n</event>\n\n");

  return;
}


/*---------------------------------------------------------------- */


void dump_bank(unsigned long *bank) {

  int length  = bank[0]+1;         /* of fragment, including header word */
  int tag     = bank[1]>>16;
  int type    = (bank[1]>>8)&0xff;
  int num     = bank[1]&0xff;
  int i;


  /* open tag */
  indent();
  printf("<bank ");
  printf(" tag=\"%d\"",tag);
  printf(" type=\"%s\"",get_type(type));
  printf(" data_type=\"0x%x\"",type);
  printf(" num=\"%d\"",num);
  if(debug!=0)printf(" length=\"%d\"",length);
  printf(">\n");
  

  /* dump data part */
  dump_data(&bank[2],type,length-2);


  /* close tag */
  indent();
  printf("</bank>\n");

  return;
}


/*---------------------------------------------------------------- */


void dump_segment(unsigned long *segment) {

  int length  = (segment[0]&0xffff)+1;    /* of fragment, including header word */
  int type    = (segment[0]>>16)&0xff;
  int tag     = (segment[0]>>24)&0xff;
  int i;


  /* open tag */
  indent();
  printf("<segment ");
  printf(" tag=\"%d\"",tag);
  printf(" type=\"%s\"",get_type(type));
  printf(" data_type=\"0x%x\"",type);
  if(debug!=0)printf(" length=\"%d\"",length);
  printf(">\n");
  

  /* dump data part */
  dump_data(&segment[1],type,length-1);


  /* close tag */
  indent();
  printf("</segment>\n");

  return;
}


/*---------------------------------------------------------------- */


void dump_tagsegment(unsigned long *tagsegment) {

  int length  = (tagsegment[0]&0xffff)+1;    /* of fragment, including header word */
  int type    = (tagsegment[0]>>16)&0xf;
  int tag     = (tagsegment[0]>>20)&0xfff;
  int i;


  /* open tag */
  indent();
  printf("<tagsegment ");
  printf(" tag=\"%d\"",tag);
  printf(" type=\"%s\"",get_type(type));
  printf(" data_type=\"0x%x\"",type);
  if(debug!=0)printf(" length=\"%d\"",length);
  printf(">\n");
  

  /* dump data part */
  dump_data(&tagsegment[1],type,length-1);


  /* close tag */
  indent();
  printf("</tagsegment>\n");

  return;
}


/*---------------------------------------------------------------- */


void dump_data(unsigned long *data, int type, int length) {

  int i,j;
  int p=0;
  short *s;
  char *c;



  /* increase indent */
  nindent+=INDENT_SIZE;


  switch (type) {

  case 0x0:
    for(i=0; i<length; i+=5) {
      indent();
      for(j=i; j<min((i+5),length); j++) {
	printf("%#14x ",data[j]);
      }
      printf("\n");
    }
    break;

  case 0x1:
    for(i=0; i<length; i+=5) {
      indent();
      for(j=i; j<min((i+5),length); j++) {
	printf("%#14x ",data[j]);
      }
      printf("\n");
    }
    break;

  case 0x2:
    for(i=0; i<length; i+=5) {
      indent();
      for(j=i; j<min(i+5,length); j++) {
	printf("%#14f ",*(float*)&data[j]);
      }
      printf("\n");
    }
    break;

  case 0x3:
    indent();
    printf("%s\n",(char*)&data[0]);
    break;

  case 0x4:
    s=(short*)&data[0];
    for(i=0; i<2*length; i+=4) {
      indent();
      for(j=i; j<min(i+4,2*length); j++) {
	printf("%14hd ",s[j]);
      }
      printf("\n");
    }
    break;

  case 0x5:
    s=(short*)&data[0];
    for(i=0; i<2*length; i+=4) {
      indent();
      for(j=i; j<min(i+4,2*length); j++) {
	printf("%#14hx ",s[j]);
      }
      printf("\n");
    }
    break;

  case 0x6:
  case 0x7:
    c=(char*)&data[0];
    for(i=0; i<4*length; i+=8) {
      indent();
      for(j=i; j<min(i+8,4*length); j++) {
	printf("   %#4hx ",c[j]);
      }
      printf("\n");
     }
    break;

  case 0x8:
    for(i=0; i<length/2; i+=2) {
      indent();
      for(j=i; j<min(i+2,length/2); j++) {
	printf("%#28.20e ",*(double*)&data[2*j]);
      }
      printf("\n");
    }
    break;

  case 0x9:
    for(i=0; i<length/2; i+=2) {
      indent();
      for(j=i; j<min(i+2,length/2); j++) {
	printf("%28lld ",*(long long*)&data[2*j]);
      }
      printf("\n");
    }
    break;

  case 0xa:
    for(i=0; i<length/2; i+=2) {
      indent();
      for(j=i; j<min(i+2,length/2); j++) {
	printf("%28llx ",*(long long*)&data[2*j]);
      }
      printf("\n");
    }
    break;

  case 0xb:
    for(i=0; i<length; i+=5) {
      indent();
      for(j=i; j<min((i+5),length); j++) {
	printf("%#14d ",data[j]);
      }
      printf("\n");
    }
    break;

  case 0xc:
    while(p<length) {
      dump_tagsegment(&data[p]);
      p+=(data[p]&0xffff)+1;
    }
    break;

  case 0xd:
    while(p<length) {
      dump_segment(&data[p]);
      p+=(data[p]&0xffff)+1;
    }
    break;

  case 0xe:
    while(p<length) {
      dump_bank(&data[p]);
      p+=data[p]+1;
    }
    break;

  case 0x10:
    while(p<length) {
      dump_bank(&data[p]);
      p+=data[p]+1;
    }
    break;

  case 0x20:
    while(p<length) {
      dump_segment(&data[p]);
      p+=(data[p]&0xffff)+1;
    }
    break;

  case 0x40:
    while(p<length) {
      dump_tagsegment(&data[p]);
      p+=(data[p]&0xffff)+1;
    }
    break;

  default:
    for(i=0; i<length; i+=5) {
      indent();
      for(j=i; j<min(i+5,length); j++) {
	printf("%#14x ",(unsigned long)data[j]);
      }
      printf("\n");
    }
    break;
  }


  /* decrease indent */
  nindent-=INDENT_SIZE;
  return;
}


/*---------------------------------------------------------------- */


int get_ndata(int type, int length) {


  switch (type) {

  case 0x0:
  case 0x1:
  case 0x2:
    return(length);
    break;

  case 0x3:
    return(1);
    break;

  case 0x4:
  case 0x5:
    return(2*length);
    break;

  case 0x6:
  case 0x7:
    return(4*length);
    break;

  case 0x8:
  case 0x9:
  case 0xa:
    return(length/2);
    break;

  case 0xb:
  case 0xc:
  case 0xd:
  case 0xe:
  case 0xf:
  case 0x10:
  case 0x20:
  case 0x40:
  default:
    return(length);
    break;

  }
}


/*---------------------------------------------------------------- */


char *get_type(int type) {



  switch (type) {

  case 0x0:
    return("unknown");
    break;

  case 0x1:
    return("unsigned int");
    break;

  case 0x2:
    return("float");
    break;

  case 0x3:
    return("string");
    break;

  case 0x4:
    return("signed short");
    break;

  case 0x5:
    return("unsigned short");
    break;

  case 0x6:
    return("signed byte");
    break;

  case 0x7:
    return("unsigned byte");
    break;

  case 0x8:
    return("double");
    break;

  case 0x9:
    return("signed long");
    break;

  case 0xa:
    return("unsigned long");
    break;

  case 0xb:
    return("signed int");
    break;

  case 0xc:
    return("tagsegment");
    break;

  case 0xd:
    return("segment");
    break;

  case 0xe:
    return("bank");
    break;

  case 0xf:
    return("repeating structure");
    break;

  case 0x10:
    return("bank");
    break;

  case 0x20:
    return("segment");
    break;

  case 0x40:
    return("tagsegment");
    break;

  default:
    return("unknown");
    break;
  }
}


/*---------------------------------------------------------------- */


void decode_command_line(int argc, char**argv) {
  
  const char *help = 
    "\nusage:\n\n  evio2xml [-max max_event] [-skip skip_event] [-no_indent] filename\n";
  int i;
    
    
  if(argc<2) {
    printf("%s\n",help);
    exit(EXIT_SUCCESS);
  } 


  /* loop over arguments */
  i=1;
  while (i<argc) {
    if (strncasecmp(argv[i],"-h",2)==0) {
      printf("%s\n",help);
      exit(EXIT_SUCCESS);

    } else if (strncasecmp(argv[i],"-debug",6)==0) {
      debug=1;
      i=i+1;

    } else if (strncasecmp(argv[i],"-no_indent",10)==0) {
      no_indent=1;
      i=i+1;

    } else if (strncasecmp(argv[i],"-max",4)==0) {
      max_event=atoi(argv[i+1]);
      i=i+2;

    } else if (strncasecmp(argv[i],"-skip",5)==0) {
      skip_event=atoi(argv[i+1]);
      i=i+2;

    } else {
      break;
    }
  }
  
  /* last arg must be filename */
  filename=argv[argc-1];

  return;
}


/*---------------------------------------------------------------- */

void indent() {

  int i;

  if(no_indent!=0)return;
  for(i=0; i<nindent; i++)printf(" ");
  return;
}

/*---------------------------------------------------------------- */
