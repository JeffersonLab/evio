/*
 *  evio2xml.c
 *
 *   Converts binary evio data to xml, full support for tagsegments and 64-bit
 *
 *   NOTE:  does NOT handle VAX float or double, packets, or repeating structures
 *
 *
 *  Definitions of terms used in output XML:
 *
 *   name         size in bytes
 *   ----         -------------
 *   byte              1
 *   int16             2
 *   int32             4
 *   float32           4 IEEE format
 *   int64             8
 *   float64           8 IEEE format
 *
 *  The prefix 'u' means unsigned
 *
 *
 *   Author: Elliott Wolin, JLab, 6-sep-2001
*/

/* still to do
 * -----------
 *  xml boilerplate and xml-schema specification
 *
*/



/* for posix */
#define _POSIX_SOURCE_ 1
#define __EXTENSIONS__


/*  misc macros, etc. */
#define MAXEVIOBUF 100000
#define MAXXMLBUF  10000
#define MAXDICT    5000
#define MAXDEPTH   512
#define min(a, b)  ((a) > (b) ? (b) : (a))


/* include files */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <expat.h>


/* xml tag dictionary */
static char *dictfilename = NULL;
static XML_Parser dictParser;
static char xmlbuf[MAXXMLBUF];
typedef struct {
  int ntag;
  int *tag;
  char *name;
} dict_entry;
static dict_entry dict[MAXDICT];
static int ndict          = 0;


/*  misc variables */
static char *filename;
static char *main_tag     = (char*)"evio-data";
static char *event_tag    = (char*)"event";
static int nevent         = 0;
static int skip_event     = 0;
static int max_event      = 0;
static int max_depth      = -1;
static int depth          = 0;
static int tagstack[MAXDEPTH];
static int nevok          = 0;
static int evok[100];
static int nnoev          = 0;
static int noev[100];
static int nfragok        = 0;
static int fragok[100];
static int nnofrag        = 0;
static int nofrag[100];
static int no_typename    = 0;
static int verbose        = 0;
static int pause          = 0;
static int debug          = 0;
static int done           = 0;
static int nindent        = 0;
static int indent_size    = 3;
static char temp[2048];


/* formatting info */
static int xtod         = 0;
static int n8           = 8;
static int n16          = 8;
static int n32          = 5;
static int n64          = 2;
static int w8           = 4;
static int w16          = 9;
static int w32          = 14;
static int w64          = 28;


/* fragment info */
char *fragment_name[] = {"bank","segment","tagsegment"};
int fragment_offset[] = {2,1,1};
enum {
  BANK = 0,
  SEGMENT,
  TAGSEGMENT,
};


/* prototypes */
void decode_command_line(int argc, char **argv);
void create_dictionary(void);
void startDictElement(void *userData, const char *name, const char **atts);
void evio2xml(unsigned long *buf);
int user_event_select(unsigned long *buf);
int user_frag_select(int tag);
void dump_fragment(unsigned long *buf, int fragment_type);
void dump_bank(unsigned long *buf);
void dump_segment(unsigned long *buf);
void dump_tagsegment(unsigned long *buf);
void dump_data(unsigned long *data, int type, int length, int noexpand);
int get_ndata(int type, int nwords);
const char *get_typename(int type);
const char *get_tagname();
int isContainer(int type);
void indent(void);
int get_int_att(const char **atts, const int natt, const char *tag, int *val);
const char *get_char_att(const char **atts, const int natt, const char *tag);
int evOpen(const char *filename, const char *mode, int *handle);
int evRead(int handle, unsigned long *buf, int maxbuflen );
int evClose(int handle);


/*--------------------------------------------------------------------------*/


int main (int argc, char **argv) {

  int handle,status;
  unsigned long buf[MAXEVIOBUF];
  

  /* decode command line */
  decode_command_line(argc,argv);


  /* create tag dictionary */
  create_dictionary();


  /* open evio input file */
  if((status=evOpen(filename,"r",&handle))!=0) {
    printf("\n ?Unable to open file %s, status=%d\n\n",filename,status);
    exit(EXIT_FAILURE);
  }


  /* opening xml boilerplate */
  printf("<!-- xml boilerplate needs to go here -->\n\n",main_tag);
  printf("<%s>\n\n",main_tag);


  /* loop over events, perhaps skip some, dump up to max_event events */
  nevent=0;
  while ((status=evRead(handle,buf,MAXEVIOBUF))==0) {
    nevent++;
    if(skip_event>=nevent)continue;
    if(user_event_select(buf)==0)continue;
    evio2xml(buf);
    if((done!=0)||((nevent>=max_event+skip_event)&&(max_event!=0)))break;
  }


  /* closing xml boilerplate */
  printf("</%s>\n\n",main_tag);


  /* done */
  evClose(handle);
  exit(EXIT_SUCCESS);
}


/*---------------------------------------------------------------- */


void create_dictionary(void) {

  FILE *dictfile;
  int len;


  /* is dictionary file specified */
  if(dictfilename==NULL)return;


  /* read add'l tags from xml file */
  if((dictfile=fopen(dictfilename,"r"))==NULL) {
    printf("\n?unable to open dictionary file %s\n\n",dictfilename);
    exit(EXIT_FAILURE);
  }


  /* create parser and set callbacks */
  dictParser=XML_ParserCreate(NULL);
  XML_SetElementHandler(dictParser,startDictElement,NULL);


  /* read and parse dictionary file */
  do {
    len=fread(xmlbuf,1,MAXXMLBUF,dictfile);
    XML_Parse(dictParser,xmlbuf,len,len<MAXXMLBUF);
  } while (len==MAXXMLBUF);
  

  close(dictfile);
  return;
}


/*---------------------------------------------------------------- */


void startDictElement(void *userData, const char *name, const char **atts) {

  int natt=XML_GetSpecifiedAttributeCount(dictParser);
  char *tagtext,*p;
  int i,npt;
  int *ip;


  if(strcasecmp(name,"evioDictEntry")!=0)return;

  ndict++;
  if(ndict>MAXDICT) {
    printf("\n?dictionary too large\n\n");
    exit(EXIT_FAILURE);
  }
  

  /* allocate array for sub-tags */
  tagtext=strdup(get_char_att(atts,natt,"tag"));
  npt=0; for(i=0; i<strlen(tagtext); i++) if(tagtext[i]=='.')npt++;
  ip=(int*)malloc((npt+1)*sizeof(int));
  

  /* fill array with sub-tags */
  i=0; 
  p=tagtext-1;
  do {
    ip[i++]=atoi(++p);
    p=strchr(p,'.');
  } while (p!=NULL);


  /* store dictionary info */
  dict[ndict-1].ntag = npt+1;
  dict[ndict-1].tag  = ip;
  dict[ndict-1].name = strdup(get_char_att(atts,natt,"name"));

  return;
}


/*---------------------------------------------------------------- */


int user_event_select(unsigned long *buf) {

  int i;
  int event_tag = buf[1]>>16;


  if((nevok<=0)&&(nnoev<=0)) {
    return(1);

  } else if(nevok>0) {
    for(i=0; i<nevok; i++) if(event_tag==evok[i])return(1);
    return(0);
    
  } else {
    for(i=0; i<nnoev; i++) if(event_tag==noev[i])return(0);
    return(1);
  }

}


/*---------------------------------------------------------------- */


int user_frag_select(int tag) {

  int i;

  if((nfragok<=0)&&(nnofrag<=0)) {
    return(1);

  } else if(nfragok>0) {
    for(i=0; i<nfragok; i++) if(tag==fragok[i])return(1);
    return(0);
    
  } else {
    for(i=0; i<nnofrag; i++) if(tag==nofrag[i])return(0);
    return(1);
  }

}


/*---------------------------------------------------------------- */


void evio2xml(unsigned long *buf) {

  char s[256];

  printf("\n\n<!-- ===================== Event %d contains %d words (%d bytes) "
	 "===================== -->\n\n",nevent,buf[0]+1,4*(buf[0]+1));
  
  /* event is always a bank */
  depth=0;
  dump_fragment(buf,BANK);

  if(pause!=0) {
    printf("\n\nHit return to continue, q to quit: ");
    fgets(s,sizeof(s),stdin);
    if(tolower(s[strspn(s," \t")])=='q')done=1;
  }

  return;
}


/*---------------------------------------------------------------- */


void dump_fragment(unsigned long *buf, int fragment_type) {

  int length,tag,type,num,is_container,noexpand;
  char *myname;


  /* get type-dependent info */
  switch(fragment_type) {
  case 0:
    length  	= buf[0]+1;
    tag     	= buf[1]>>16;
    type    	= (buf[1]>>8)&0xff;
    num     	= buf[1]&0xff;
    break;

  case 1:
    length  	= (buf[0]&0xffff)+1;
    type    	= (buf[0]>>16)&0xff;
    tag     	= (buf[0]>>24)&0xff;
    num     	= 0;
    break;
    
  case 2:
    length  	= (buf[0]&0xffff)+1;
    type    	= (buf[0]>>16)&0xf;
    tag     	= (buf[0]>>20)&0xfff;
    num     	= 0;
    break;

  default:
    printf("?illegal fragment_type in dump_fragment: %d",fragment_type);
    exit(EXIT_FAILURE);
    break;
  }


  /* user selection on fragment tags (not on event tag) */
  if((depth>0)&&(user_frag_select(tag)==0))return;


  /* update depth, tagstack, etc. */
  depth++;
  if(depth>(sizeof(tagstack)/sizeof(int))) {
    printf("?error...tagstack overflow\n");
    exit(EXIT_FAILURE);
  }
  tagstack[depth-1]=tag;
  is_container=isContainer(type);
  myname=(char*)get_tagname();
  noexpand=is_container&&(max_depth>=0)&&(depth>max_depth);


  /* print header word (as comment) if requested */
  if(verbose!=0) {
    printf("\n"); indent();
    if(fragment_type==BANK) {
      printf("<!-- header words: %d, %#x -->\n",buf[0],buf[1]);
    } else {
      printf("<!-- header word: %#x -->\n",buf[0]);
    }
  }


  /* fragment opening xml tag */
  indent();
  if((fragment_type==BANK)&&(depth==1)) {
    printf("<%s format=\"evio\" count=\"%d\"",event_tag,nevent);
    printf(" content=\"%s\"",get_typename(type));
  } else if(myname!=NULL) {
    printf("<%s",myname);
    printf(" content=\"%s\"",get_typename(type));
  } else if(is_container||no_typename) {
    printf("<%s",fragment_name[fragment_type]);
    printf(" content=\"%s\"",get_typename(type));
  } else {
    printf("<%s",get_typename(type));
  }
  printf(" data_type=\"0x%x\"",type);
  printf(" tag=\"%d\"",tag);
  if(fragment_type==BANK)printf(" num=\"%d\"",num);
  if(verbose!=0) {
    printf(" length=\"%d\" ndata=\"%d\"",length,
	   get_ndata(type,length-fragment_offset[fragment_type]));
  }
  if(noexpand)printf(" opt=\"noexpand\"");
  printf(">\n");
  

  /* fragment data */
  dump_data(&buf[fragment_offset[fragment_type]],type,
	    length-fragment_offset[fragment_type],noexpand);


  /* fragment close tag */
  indent();
  if((fragment_type==BANK)&&(depth==1)) {
    printf("</%s>\n\n",event_tag);
  } else if(myname!=NULL) {
    printf("</%s>\n",myname);
  } else if(is_container||no_typename) {
    printf("</%s>\n",fragment_name[fragment_type]);
  } else {
    printf("</%s>\n",get_typename(type));
  }


  /* decrement stack depth */
  depth--;


  return;
}


/*---------------------------------------------------------------- */


void dump_data(unsigned long *data, int type, int length, int noexpand) {

  int i,j,len;
  int p=0;
  short *s;
  char *c;
  unsigned char *uc;
  char format[132];


  nindent+=indent_size;


  /* dump data in if no expansion, even if this is a container */
  if(noexpand) {
    sprintf(format,"%%#%d%s ",w32,(xtod==0)?"x":"d");
    for(i=0; i<length; i+=n32) {
      indent();
      for(j=i; j<min((i+n32),length); j++) {
	printf(format,data[j]);
      }
      printf("\n");
    }
    nindent-=indent_size;
    return;
  }


  /* dump the data or call dump_fragment */
  switch (type) {

  case 0x0:
  case 0x1:
    sprintf(format,"%%#%d%s ",w32,(xtod==0)?"x":"d");
    for(i=0; i<length; i+=n32) {
      indent();
      for(j=i; j<min((i+n32),length); j++) {
	printf(format,data[j]);
      }
      printf("\n");
    }
    break;

  case 0x2:
    sprintf(format,"%%#%df ",w32);
    for(i=0; i<length; i+=n32) {
      indent();
      for(j=i; j<min(i+n32,length); j++) {
	printf(format,*(float*)&data[j]);
      }
      printf("\n");
    }
    break;

  case 0x3:
    c=(char*)&data[0];
    len=length*4;
    sprintf(format,"<![CDATA[\n%%.%ds\n]]>\n",len);  /* handle unterminated strings */
    printf(format,c);
    break;

  case 0x4:
    sprintf(format,"%%%dhd ",w16);
    s=(short*)&data[0];
    for(i=0; i<2*length; i+=n16) {
      indent();
      for(j=i; j<min(i+n16,2*length); j++) {
	printf(format,s[j]);
      }
      printf("\n");
    }
    break;

  case 0x5:
    sprintf(format,"%%#%d%s ",w16,(xtod==0)?"hx":"d");
    s=(short*)&data[0];
    for(i=0; i<2*length; i+=n16) {
      indent();
      for(j=i; j<min(i+n16,2*length); j++) {
	printf(format,s[j]);
      }
      printf("\n");
    }
    break;

  case 0x6:
    sprintf(format,"   %%%dd ",w8);
    c=(char*)&data[0];
    for(i=0; i<4*length; i+=n8) {
      indent();
      for(j=i; j<min(i+n8,4*length); j++) {
	printf(format,c[j]);
      }
      printf("\n");
     }
    break;

  case 0x7:
    sprintf(format,"   %%#%d%s ",w8,(xtod==0)?"x":"d");
    uc=(unsigned char*)&data[0];
    for(i=0; i<4*length; i+=n8) {
      indent();
      for(j=i; j<min(i+n8,4*length); j++) {
	printf(format,uc[j]);
      }
      printf("\n");
     }
    break;

  case 0x8:
    sprintf(format,"%%#%d.20e ",w64);
    for(i=0; i<length/2; i+=n64) {
      indent();
      for(j=i; j<min(i+n64,length/2); j++) {
	printf(format,*(double*)&data[2*j]);
      }
      printf("\n");
    }
    break;

  case 0x9:
    sprintf(format,"%%%dlld ",w64);
    for(i=0; i<length/2; i+=n64) {
      indent();
      for(j=i; j<min(i+n64,length/2); j++) {
	printf(format,*(long long*)&data[2*j]);
      }
      printf("\n");
    }
    break;

  case 0xa:
    sprintf(format,"%%#%dll%s ",w64,(xtod==0)?"x":"d");
    for(i=0; i<length/2; i+=n64) {
      indent();
      for(j=i; j<min(i+n64,length/2); j++) {
	printf(format,*(long long*)&data[2*j]);
      }
      printf("\n");
    }
    break;

  case 0xb:
    sprintf(format,"%%#%dd ",w32);
    for(i=0; i<length; i+=n32) {
      indent();
      for(j=i; j<min((i+n32),length); j++) {
	printf(format,data[j]);
      }
      printf("\n");
    }
    break;

  case 0xe:
  case 0x10:
    while(p<length) {
      dump_fragment(&data[p],BANK);
      p+=data[p]+1;
    }
    break;

  case 0xd:
  case 0x20:
    while(p<length) {
      dump_fragment(&data[p],SEGMENT);
      p+=(data[p]&0xffff)+1;
    }
    break;

  case 0xc:
  case 0x40:
    while(p<length) {
      dump_fragment(&data[p],TAGSEGMENT);
      p+=(data[p]&0xffff)+1;
    }
    break;

  default:
    sprintf(format,"%%#%d%s ",w32,(xtod==0)?"x":"d");
    for(i=0; i<length; i+=n32) {
      indent();
      for(j=i; j<min(i+n32,length); j++) {
	printf(format,(unsigned long)data[j]);
      }
      printf("\n");
    }
    break;
  }


  /* decrease indent */
  nindent-=indent_size;
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


const char *get_typename(int type) {

  switch (type) {

  case 0x0:
    return("unknown32");
    break;

  case 0x1:
    return("uint32");
    break;

  case 0x2:
    return("float32");
    break;

  case 0x3:
    return("string");
    break;

  case 0x4:
    return("int16");
    break;

  case 0x5:
    return("uint16");
    break;

  case 0x6:
    return("byte");
    break;

  case 0x7:
    return("ubyte");
    break;

  case 0x8:
    return("float64");
    break;

  case 0x9:
    return("int64");
    break;

  case 0xa:
    return("uint64");
    break;

  case 0xb:
    return("int32");
    break;

  case 0xf:
    return("repeating");
    break;

  case 0xe:
  case 0x10:
    return("bank");
    break;

  case 0xd:
  case 0x20:
    return("segment");
    break;

  case 0xc:
  case 0x40:
    return("tagsegment");
    break;

  default:
    return("unknown");
    break;
  }
}


/*---------------------------------------------------------------- */


void indent() {

  int i;

  for(i=0; i<nindent; i++)printf(" ");
  return;
}


/*---------------------------------------------------------------- */


int isContainer(int type) {

  switch (type) {

  case (0xc):
  case (0xd):
  case (0xe):
  case (0x10):
  case (0x20):
  case (0x40):
    return(1);

  default:
    return(0);
  }
}


/*---------------------------------------------------------------- */


int get_int_att(const char **atts, const int natt, const char *name, int *val) {

  int i;

  for(i=0; i<natt; i+=2) {
    if(strcasecmp(atts[i],name)==0) {
      if(strncmp(atts[i+1],"0x",2)==0) {
	sscanf(atts[i+1],"%x",val);
      } else if(strncmp(atts[i+1],"0X",2)==0) {
	sscanf(atts[i+1],"%X",val);
      } else {
	sscanf(atts[i+1],"%d",val);
      }
      return(1);
    }
  }

  return(0);
}


/*---------------------------------------------------------------- */


const char *get_char_att(const char **atts, const int natt, const char *name) {

  int i;

  for(i=0; i<natt; i+=2) {
    if(strcasecmp(atts[i],name)==0)return((const char*)atts[i+1]);
  }

  return(NULL);
}


/*---------------------------------------------------------------- */


const char *get_tagname() {

  int i,j,ntd,nt;
  int match;


  /* search dictionary for match with current tag */
  for(i=0; i<ndict; i++) {
    
    ntd=dict[i].ntag;
    nt=min(ntd,depth);

    match=1;
    for(j=0; j<nt; j++) {
      if(dict[i].tag[ntd-j-1]!=tagstack[depth-j-1]) {
	match=0;
	break;
      }
    }
    if(match)return(dict[i].name);
  }

  return(NULL);
}


/*---------------------------------------------------------------- */


void decode_command_line(int argc, char**argv) {
  
  const char *help = 
    "\nusage:\n\n  evio2xml [-max max_event] [-pause] [-skip skip_event] [-dict dictfilename]\n"
    "           [-ev evtag] [-noev evtag] [-frag frag] [-nofrag frag] [-max_depth max_depth]\n"
    "           [-n8 n8] [-n16 n16] [-n32 n32] [-n64 n64]\n"
    "           [-w8 w8] [-w16 w16] [-w32 w32] [-w64 w64]\n"
    "           [-verbose] [-xtod] [-m main_tag] [-e event_tag]\n"
    "           [-indent indent_size] [-no_typename] [-debug] filename\n";
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

    } else if (strncasecmp(argv[i],"-pause",6)==0) {
      pause=1;
      i=i+1;

    } else if (strncasecmp(argv[i],"-debug",6)==0) {
      debug=1;
      i=i+1;

    } else if (strncasecmp(argv[i],"-verbose",8)==0) {
      verbose=1;
      i=i+1;

    } else if (strncasecmp(argv[i],"-no_typename",12)==0) {
      no_typename=1;
      i=i+1;

    } else if (strncasecmp(argv[i],"-max_depth",10)==0) {
      max_depth=atoi(argv[i+1]);
      i=i+2;

    } else if (strncasecmp(argv[i],"-indent",7)==0) {
      indent_size=atoi(argv[i+1]);
      i=i+2;

    } else if (strncasecmp(argv[i],"-max",4)==0) {
      max_event=atoi(argv[i+1]);
      i=i+2;

    } else if (strncasecmp(argv[i],"-skip",5)==0) {
      skip_event=atoi(argv[i+1]);
      i=i+2;

    } else if (strncasecmp(argv[i],"-dict",5)==0) {
      dictfilename=strdup(argv[i+1]);
      i=i+2;

    } else if (strncasecmp(argv[i],"-xtod",5)==0) {
      xtod=1;
      i=i+1;

    } else if (strncasecmp(argv[i],"-ev",3)==0) {
      if(nevok<(sizeof(evok)/sizeof(int))) {
	evok[nevok++]=atoi(argv[i+1]);
	i=i+2;
      } else {
	printf("?too many ev flags: %s\n",argv[i+1]);
      }

    } else if (strncasecmp(argv[i],"-noev",5)==0) {
      if(nnoev<(sizeof(noev)/sizeof(int))) {
	noev[nnoev++]=atoi(argv[i+1]);
	i=i+2;
      } else {
	printf("?too many noev flags: %s\n",argv[i+1]);
      }

    } else if (strncasecmp(argv[i],"-frag",5)==0) {
      if(nfragok<(sizeof(fragok)/sizeof(int))) {
	fragok[nfragok++]=atoi(argv[i+1]);
	i=i+2;
      } else {
	printf("?too many frag flags: %s\n",argv[i+1]);
      }

    } else if (strncasecmp(argv[i],"-nofrag",7)==0) {
      if(nnofrag<(sizeof(nofrag)/sizeof(int))) {
	nofrag[nnofrag++]=atoi(argv[i+1]);
	i=i+2;
      } else {
	printf("?too many nofrag flags: %s\n",argv[i+1]);
      }

    } else if (strncasecmp(argv[i],"-n8",3)==0) {
      n8=atoi(argv[i+1]);
      i=i+2;

    } else if (strncasecmp(argv[i],"-n16",4)==0) {
      n16=atoi(argv[i+1]);
      i=i+2;

    } else if (strncasecmp(argv[i],"-n32",4)==0) {
      n32=atoi(argv[i+1]);
      i=i+2;

    } else if (strncasecmp(argv[i],"-n64",4)==0) {
      n64=atoi(argv[i+1]);
      i=i+2;

    } else if (strncasecmp(argv[i],"-w8",3)==0) {
      w8=atoi(argv[i+1]);
      i=i+2;

    } else if (strncasecmp(argv[i],"-w16",4)==0) {
      w16=atoi(argv[i+1]);
      i=i+2;

    } else if (strncasecmp(argv[i],"-w32",4)==0) {
      w32=atoi(argv[i+1]);
      i=i+2;

    } else if (strncasecmp(argv[i],"-w64",4)==0) {
      w64=atoi(argv[i+1]);
      i=i+2;

    } else if (strncasecmp(argv[i],"-m",2)==0) {
      main_tag=strdup(argv[i+1]);
      i=i+2;

    } else if (strncasecmp(argv[i],"-e",2)==0) {
      event_tag=strdup(argv[i+1]);
      i=i+2;

    } else if (strncasecmp(argv[i],"-",1)==0) {
      printf("\n  ?unknown command line arg: %s\n\n",argv[i]);
      exit(EXIT_FAILURE);

    } else {
      break;
    }
  }
  
  /* last arg better be filename */
  filename=argv[argc-1];

  return;
}


/*---------------------------------------------------------------- */
