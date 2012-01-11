/*
 *  xml_util.c
 *
 *   Utility for converting binary evio data to xml
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
 *   Author:  Elliott Wolin, JLab, 6-sep-2001
 *   Revised: Elliott Wolin, 5-apr-2004 convert to library, print to string
 *   Revised: Elliott Wolin, 8-feb-2008 added brief mode and num chaining in dictionary
*/

/* still to do
 * -----------
 *  check string length, use snprintf
 *
*/


/* container types used locally */
enum {
  BANK = 0,
  SEGMENT,
  TAGSEGMENT,
};



/* for posix */
#define _POSIX_SOURCE_ 1
#define __EXTENSIONS__


/*  misc macros, etc. */
#define MAXXMLBUF  10000
#define MAXDICT    5000
#define MAXDEPTH   512
#define min(a, b)  ( ( (a) > (b) ) ? (b) : (a) )


/* include files */
#include <evio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <expat.h>


/* xml tag dictionary */
static XML_Parser dictParser;
static char xmlbuf[MAXXMLBUF];
typedef struct {
  char *name;
  int ntag;
  int *tag;
  int nnum;
  int *num;
} dict_entry;
static dict_entry dict[MAXDICT];
static int ndict          = 0;


/* fragment info */
char *fragment_name[] = {"bank","segment","tagsegment"};
int fragment_offset[] = {2,1,1};


/* formatting info */
static int xtod         = 0;
static int n8           = 8;
static int n16          = 8;
static int n32          = 5;
static int n64          = 2;
static int w8           = 4;
static int w16          = 9;
static int w32          = 14;
static int p32          = 6;
static int w64          = 28;
static int p64          = 20;


/*  misc variables */
static int nbuf;
static char *event_tag    = "event";
static char *bank2_tag    = "bank";
static char *dicttagname  = NULL;
static int max_depth      = -1;
static int depth          = 0;
static int tagstack[MAXDEPTH];
static int numstack[MAXDEPTH];
static int no_typename    = 0;
static int verbose        = 0;
static int brief          = 0;
static int no_data        = 0;
static int nindent        = 0;
static int indent_size    = 3;
static char *xml;
static int xmllen;


/* prototypes */
static void create_dictionary(char *dictfilename);
static void startDictElement(void *userData, const char *name, const char **atts);
static void dump_fragment(unsigned int *buf, int fragment_type);
static void dump_data(unsigned int *data, int type, int length, int padding, int noexpand);
static int get_ndata(int type, int nwords, int padding);
static const char *get_matchname();
static void indent(int extra);
static const char *get_char_att(const char **atts, const int natt, const char *tag);

/* user supplied fragment select function via set_user_frag_select_func(int (*f) (int tag)) */
static int (*USER_FRAG_SELECT_FUNC) (int tag) = NULL;


/*--------------------------------------------------------------------------*/


void evio_xmldump_init(char *dictfilename, char *dtagname) {

  dicttagname=strdup(dtagname);
  create_dictionary(dictfilename);

  return;
}


/*---------------------------------------------------------------- */


static void create_dictionary(char *dictfilename) {

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
  

  fclose(dictfile);
  return;
}


/*---------------------------------------------------------------- */


static void startDictElement(void *userData, const char *name, const char **atts) {

  int natt=XML_GetSpecifiedAttributeCount(dictParser);
  char *tagtext,*p,*numtext;
  const char *cp;
  int i,nt,nn;
  int *ip, *in;


  if(strcasecmp(name,dicttagname)!=0)return;

  ndict++;
  if(ndict>MAXDICT) {
    printf("\n?too many dictionary entries in file\n\n");
    exit(EXIT_FAILURE);
  }
  

  /* store tags */
  nt=0;
  ip=NULL;
  cp=get_char_att(atts,natt,"tag");
  if(cp!=NULL) {
    nt=1;
    tagtext=strdup(cp);
    for(i=0; i<strlen(tagtext); i++) if(tagtext[i]=='.')nt++;
    ip=(int*)malloc(nt*sizeof(int));
    
    i=0; 
    p=tagtext-1;
    do {
      ip[i++]=atoi(++p);
      p=strchr(p,'.');
    } while (p!=NULL);
  }


  /* store nums */
  nn=0;
  in=NULL;
  cp=get_char_att(atts,natt,"num");
  if(cp!=NULL) {
    nn=1;
    numtext=strdup(cp);
    for(i=0; i<strlen(numtext); i++) if(numtext[i]=='.')nn++;
    in=(int*)malloc(nn*sizeof(int));
    
    i=0; 
    p=numtext-1;
    do {
      in[i++]=atoi(++p);
      p=strchr(p,'.');
    } while (p!=NULL);
  }
    

  /* store dictionary info */
  dict[ndict-1].name = strdup(get_char_att(atts,natt,"name"));
  dict[ndict-1].ntag = nt;
  dict[ndict-1].tag  = ip;
  dict[ndict-1].nnum = nn;
  dict[ndict-1].num  = in;

  return;
}



/**
 * This routine creates an xml representation of an evio event.
 *
 * @param buf    buffer with evio event data
 * @param bufnum buffer id number
 * @param string buffer in which to place the resulting xml
 * @param len    length of xml string buffer
 */
void evio_xmldump(unsigned int *buf, int bufnum, char *string, int len) {

  nbuf=bufnum;
  xml=string;
  xmllen=len;

  xml+=sprintf(xml,"\n\n<!-- ===================== Buffer %d contains %d words (%d bytes) "
	       "===================== -->\n\n",nbuf,buf[0]+1,4*(buf[0]+1));
  
  depth=0;
  dump_fragment(buf,BANK);


  return;
}


/*---------------------------------------------------------------- */


void set_user_frag_select_func( int (*f) (int tag) ) {
  USER_FRAG_SELECT_FUNC = f;
  return;
}



/**
 * This routine puts evio container data into an xml string representing an evio event.
 *
 * @param buf            pointer to data to put in xml string
 * @param fragment_type  container type of evio data (bank=0, segment=1, or tagsegment=2)
 */
static void dump_fragment(unsigned int *buf, int fragment_type) {

  int length,type,is_a_container,noexpand, padding=0;
  unsigned short tag;
  unsigned char num;

  char *myname;


  /* get type-dependent info */
  switch(fragment_type) {
  case BANK:
    length  	= buf[0]+1;
    tag     	= (buf[1]>>16)&0xffff;
    type        = (buf[1]>>8)&0x3f;
    padding     = (buf[1]>>14)&0x3;
    num     	= buf[1]&0xff;
    break;

  case SEGMENT:
    length  	= (buf[0]&0xffff)+1;
    type    	= (buf[0]>>16)&0x3f;
    padding     = (buf[0]>>22)&0x3;
    tag     	= (buf[0]>>24)&0xff;
    num     	= -1;  /* doesn't have num */
    break;
    
  case TAGSEGMENT:
    length  	= (buf[0]&0xffff)+1;
    type    	= (buf[0]>>16)&0xf;
    tag     	= (buf[0]>>20)&0xfff;
    num     	= -1;   /* doesn't have num */
    break;

  default:
    printf("?illegal fragment_type in dump_fragment: %d",fragment_type);
    exit(EXIT_FAILURE);
    break;
  }


  /* user selection on fragment tags (not on event tag) */
  if( (depth>0) && (USER_FRAG_SELECT_FUNC != NULL) && (USER_FRAG_SELECT_FUNC(tag)==0) )return;


  /* update depth, tagstack, numstack, etc. */
  depth++;
  if( (depth>(sizeof(tagstack)/sizeof(int))) || (depth>(sizeof(numstack)/sizeof(int))) ) {
    printf("?error...tagstack/numstack overflow\n");
    exit(EXIT_FAILURE);
  }
  tagstack[depth-1]=tag;
  numstack[depth-1]=num;
  is_a_container=is_container(type);
  myname=(char*)get_matchname();
  noexpand=is_a_container&&(max_depth>=0)&&(depth>max_depth);


  /* verbose header */
  if(verbose!=0) {
    xml+=sprintf(xml,"\n"); indent(0);
    if(fragment_type==BANK) {
      xml+=sprintf(xml,"<!-- header words: %d, %#x -->\n",buf[0],buf[1]);
    } else {
      xml+=sprintf(xml,"<!-- header word: %#x -->\n",buf[0]);
    }
  }


  /* xml opening fragment */
  indent(0);


  /* format and content */
  if((fragment_type==BANK)&&(depth==1)) {
    xml+=sprintf(xml,"<%s format=\"evio\" count=\"%d\"",event_tag,nbuf);
    if(brief==0)xml+=sprintf(xml," content=\"%s\"",get_typename(type));
  } else if(myname!=NULL) {
    xml+=sprintf(xml,"<%s",myname);
    if(brief==0)xml+=sprintf(xml," content=\"%s\"",get_typename(type));
  } else if((fragment_type==BANK)&&(depth==2)) {
    xml+=sprintf(xml,"<%s",bank2_tag);
    if(brief==0)xml+=sprintf(xml," content=\"%s\"",get_typename(type));
  } else if(is_a_container||no_typename) {
    xml+=sprintf(xml,"<%s",fragment_name[fragment_type]);
    if(brief==0)xml+=sprintf(xml," content=\"%s\"",get_typename(type));
  } else {
    xml+=sprintf(xml,"<%s",get_typename(type));
  }

  /* data_type, tag, and num */
  if(brief==0)xml+=sprintf(xml," data_type=\"0x%x\"",type);
  if(brief==0)xml+=sprintf(xml," tag=\"%d\"",tag);
  if((brief==0)&&(fragment_type==BANK))xml+=sprintf(xml," num=\"%d\"",(int)num);

  /* length, ndata for verbose */
  if(verbose!=0) {
    xml+=sprintf(xml," length=\"%d\" ndata=\"%d\"",length,
                 get_ndata(type, (length - fragment_offset[fragment_type]), padding));
  }


  /* noexpand option */
  if(noexpand)xml+=sprintf(xml," opt=\"noexpand\"");
  xml+=sprintf(xml,">\n");
  

  /* fragment data */
  dump_data(&buf[fragment_offset[fragment_type]], type,
	    length-fragment_offset[fragment_type], padding, noexpand);


  /* xml closing fragment */
  indent(0);
  if((fragment_type==BANK)&&(depth==1)) {
    xml+=sprintf(xml,"</%s>\n\n",event_tag);
    xml+=sprintf(xml,"<!-- end buffer %d -->\n\n",nbuf);
  } else if(myname!=NULL) {
    xml+=sprintf(xml,"</%s>\n",myname);
  } else if((fragment_type==BANK)&&(depth==2)) {
    xml+=sprintf(xml,"</%s>\n",bank2_tag);
  } else if(is_a_container||no_typename) {
    xml+=sprintf(xml,"</%s>\n",fragment_name[fragment_type]);
  } else {
    xml+=sprintf(xml,"</%s>\n",get_typename(type));
  }


  /* decrement stack depth */
  depth--;


  return;
}



/**
 * This routine puts evio data into an xml string representing an evio event.
 *
 * @param data     pointer to data to put in xml string
 * @param type     type of evio data (ie. short, bank, etc)
 * @param length   length of data in 32 bit words
 * @param padding  number of bytes to be ignored at end of data
 * @param noexpand if true, just print data as ints, don't expand as detailed xml
 */
static void dump_data(unsigned int *data, int type, int length, int padding, int noexpand) {

  int i,j,len;
  int p=0;
  short *s;
  char *c, *start;
  unsigned char *uc;
  char format[132];
  int fLen,fTag,dLen,dTag;


  nindent+=indent_size;


  /* dump data if no expansion, even if this is a container */
  if (noexpand) {
    sprintf(format,"%%#%d%s ",w32,(xtod==0)?"x":"d");
    for(i=0; i<length; i+=n32) {
      indent(0);
      for(j=i; j<min((i+n32),length); j++) {
        xml+=sprintf(xml,format,data[j]);
      }
      xml+=sprintf(xml,"\n");
    }
    nindent-=indent_size;
    return;
  }


  /* dump the data or call dump_fragment */
  switch (type) {

  /* unknown */
  case 0x0:
  /* unsigned 32 bit int */
  case 0x1:
    if(!no_data) {
      sprintf(format,"%%#%d%s ",w32,(xtod==0)?"x":"d");
      for(i=0; i<length; i+=n32) {
        indent(0);
        for(j=i; j<min((i+n32),length); j++) {
          xml+=sprintf(xml,format,data[j]);
        }
        xml+=sprintf(xml,"\n");
      }
    }
    break;

  /* 32 bit float */
  case 0x2:
    if(!no_data) {
      sprintf(format,"%%#%d.%df ",w32,p32);
      for(i=0; i<length; i+=n32) {
        indent(0);
        for(j=i; j<min(i+n32,length); j++) {
          xml+=sprintf(xml,format,*(float*)&data[j]);
        }
        xml+=sprintf(xml,"\n");
      }
    }
    break;

  /* string */ // TODO: need to print array of strings
  case 0x3:
    if(!no_data) {
      start=(char*)&data[0];
      c=start;
      while((c[0]!=0x4)&&((c-start)<length*4)) {
        len=strlen(c);
        indent(0);
        sprintf(format,"<![CDATA[%%.%ds]]>\n",len);
        xml+=sprintf(xml,format,c);
        c+=len+1;
      }
    }
    break;

  /* 16 bit int */
  case 0x4:
    if(!no_data) {
      int numShorts = 2*length;
      if (padding == 2) numShorts--;
      
      sprintf(format,"%%%dhd ",w16);
      s=(short*)&data[0];
      for(i=0; i<numShorts; i+=n16) {
        indent(0);
        for(j=i; j<min(i+n16,numShorts); j++) {
          xml+=sprintf(xml,format,s[j]);
        }
        xml+=sprintf(xml,"\n");
      }
    }
    break;

  /* unsigned 16 bit int */
  case 0x5:
    if(!no_data) {
      int numShorts = 2*length;
      if (padding == 2) numShorts--;
      
      sprintf(format,"%%#%d%s ",w16,(xtod==0)?"hx":"d");
      s=(short*)&data[0];
      for(i=0; i<numShorts; i+=n16) {
        indent(0);
        for(j=i; j<min(i+n16,numShorts); j++) {
          xml+=sprintf(xml,format,s[j]);
        }
        xml+=sprintf(xml,"\n");
      }
    }
    break;

  /* 8 bit int */
  case 0x6:
    if(!no_data) {
      int numBytes = 4*length;
      if (padding >=1 && padding <= 3) numBytes -= padding;

      sprintf(format,"   %%%dd ",w8);
      c=(char*)&data[0];
      for(i=0; i<numBytes; i+=n8) {
        indent(0);
        for(j=i; j<min(i+n8,numBytes); j++) {
          xml+=sprintf(xml,format,c[j]);
        }
        xml+=sprintf(xml,"\n");
      }
    }
    break;

  /* unsigned 8 bit int */
  case 0x7:
    if(!no_data) {
      int numBytes = 4*length;
      if (padding >=1 && padding <= 3) numBytes -= padding;

      sprintf(format,"   %%#%d%s ",w8,(xtod==0)?"x":"d");
      uc=(unsigned char*)&data[0];
      for(i=0; i<numBytes; i+=n8) {
        indent(0);
        for(j=i; j<min(i+n8,numBytes); j++) {
          xml+=sprintf(xml,format,uc[j]);
        }
        xml+=sprintf(xml,"\n");
      }
    }
    break;

  /* 64 bit double */
  case 0x8:
    if(!no_data) {
      sprintf(format,"%%#%d.%de ",w64,p64);
      for(i=0; i<length/2; i+=n64) {
        indent(0);
        for(j=i; j<min(i+n64,length/2); j++) {
          xml+=sprintf(xml,format,*(double*)&data[2*j]);
        }
        xml+=sprintf(xml,"\n");
      }
    }
    break;

  /* 64 bit int */
  case 0x9:
    if(!no_data) {
      sprintf(format,"%%%dlld ",w64);
      for(i=0; i<length/2; i+=n64) {
        indent(0);
        for(j=i; j<min(i+n64,length/2); j++) {
          xml+=sprintf(xml,format,*(long long*)&data[2*j]);
        }
        xml+=sprintf(xml,"\n");
      }
    }
    break;

  /* unsigned 64 bit int */
  case 0xa:
    if(!no_data) {
      sprintf(format,"%%#%dll%s ",w64,(xtod==0)?"x":"d");
      for(i=0; i<length/2; i+=n64) {
        indent(0);
        for(j=i; j<min(i+n64,length/2); j++) {
          xml+=sprintf(xml,format,*(long long*)&data[2*j]);
        }
        xml+=sprintf(xml,"\n");
      }
    }
    break;

  /* 32 bit int */
  case 0xb:
    if(!no_data) {
      sprintf(format,"%%#%dd ",w32);
      for(i=0; i<length; i+=n32) {
        indent(0);
        for(j=i; j<min((i+n32),length); j++) {
          xml+=sprintf(xml,format,data[j]);
        }
        xml+=sprintf(xml,"\n");
      }
    }
    break;

  /* composite */
  case 0xf:
    if(!no_data) {
      fLen=data[0]&0xffff;
      fTag=(data[0]>>20)&0xfff;
      c=(char*)&data[1];
      indent(4);
      xml+=sprintf(xml,"<formatString tag=\"%d\">\n",fTag);
      indent(11);
      xml+=sprintf(xml,"%s\n",c);
      indent(4);
      xml+=sprintf(xml,"</formatString>\n");
      

      dLen=(data[fLen+1])&0xffff;
      dTag=(data[fLen+1]>>20)&0xfff;
      indent(4);
      xml+=sprintf(xml,"<data tag=\"%d\">\n",dTag);
      sprintf(format,"%%#%d%s ",w32,(xtod==0)?"x":"d");
      for(i=0; i<dLen; i+=n32) {
        indent(7);
        for(j=i; j<min((i+n32),dLen); j++) {
          xml+=sprintf(xml,format,data[fLen+2+j]);
        }
        xml+=sprintf(xml,"\n");
      }
      indent(4);
      xml+=sprintf(xml,"</data>\n");
    }
    break;

  /* bank */
  case 0xe:
  case 0x10:
    while(p<length) {
      dump_fragment(&data[p],BANK);
      p+=data[p]+1;
    }
    break;

  /* segment */
  case 0xd:
  case 0x20:
    while(p<length) {
      dump_fragment(&data[p],SEGMENT);
      p+=(data[p]&0xffff)+1;
    }
    break;

  /* tagsegment */
  case 0xc:
    while(p<length) {
      dump_fragment(&data[p],TAGSEGMENT);
      p+=(data[p]&0xffff)+1;
    }
    break;

  default:
    if(!no_data) {
      sprintf(format,"%%#%d%s ",w32,(xtod==0)?"x":"d");
      for(i=0; i<length; i+=n32) {
        indent(0);
        for(j=i; j<min(i+n32,length); j++) {
          xml+=sprintf(xml,format,(unsigned int)data[j]);
        }
        xml+=sprintf(xml,"\n");
      }
    }
    break;
  }


  /* decrease indent */
  nindent-=indent_size;
  return;
}


/*---------------------------------------------------------------- */

/**
 * Get the number of items given the data type, data length, and padding.
 *
 * @param type    numerical value of data type
 * @param length  length of data in 32 bit words
 * @param padding number of bytes used to pad data at the end:
 *                 0 or 2 for short types, 0-3 for byte types
 */
static int get_ndata(int type, int length, int padding) {

    switch (type) {

        case 0x0:
        case 0x1:
        case 0x2:
            return(length);
            break;

        case 0x3:
            //??? // TODO: strings arrays?
            return(1);
            break;

        /* 16 bit ints */
        case 0x4:
        case 0x5:
            if (padding == 2) {
                return(2*length - 1);
            }
            return(2*length);
            break;

        /* 8 bit ints */
        case 0x6:
        case 0x7:
            if (padding >= 0 && padding <= 3) {
                return(4*length - padding);
            }
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
        case 0x10:
        case 0x20:
        case 0x40:
        default:
            return(length);
            break;
    }
}


/*---------------------------------------------------------------- */


static void indent(int extra) {

  int i;

  for(i=0; i<nindent+extra; i++)xml+=sprintf(xml," ");
  return;
}


/*---------------------------------------------------------------- */


static const char *get_char_att(const char **atts, const int natt, const char *name) {

  int i;

  for(i=0; i<natt; i+=2) {
    if(strcasecmp(atts[i],name)==0)return((const char*)atts[i+1]);
  }

  return(NULL);
}


/*---------------------------------------------------------------- */


static const char *get_matchname() {

  int i,j,ntd,nnd,nt,nn,num;
  int tagmatch,nummatch;


  /* search dictionary for tag/num match */
  for(i=0; i<ndict; i++) {
    
    tagmatch=1;
    ntd=dict[i].ntag;
    if(ntd>0) {
      nt=min(ntd,depth);
      for(j=0; j<nt; j++) {
        if(dict[i].tag[ntd-j-1]!=tagstack[depth-j-1]) {
          tagmatch=0;
          break;
        }
      }
    }

    nummatch=1;
    nnd=dict[i].nnum;
    if(nnd>0) {
      nn=min(nnd,depth);
      for(j=0; j<nn; j++) {
        num=numstack[depth-j-1];
        if((num>=0)&&(dict[i].num[nnd-j-1]!=num)) {
          nummatch=0;
          break;
        }
      }
    }

    /* tag and num match, done */
    if((tagmatch==1)&&(nummatch==1))return(dict[i].name);

  }  /* try next dictionary element */

  return(NULL);
}


/*---------------------------------------------------------------- */


void evio_xmldump_done(char *string, int len) {

  sprintf(string," ");
  return;
}


/*---------------------------------------------------------------- */
/*--- Set functions ---------------------------------------------- */
/*---------------------------------------------------------------- */


int set_event_tag(char *tag) {

  event_tag=tag;
  return(0);
}


/*---------------------------------------------------------------- */


int set_bank2_tag(char *tag) {

  bank2_tag=tag;
  return(0);

}


/*---------------------------------------------------------------- */


int set_n8(int val) {

  n8=val;
  return(0);

}


/*---------------------------------------------------------------- */


int set_n16(int val) {

  n16=val;
  return(0);

}


/*---------------------------------------------------------------- */


int set_n32(int val) {

  n32=val;
  return(0);

}


/*---------------------------------------------------------------- */


int set_n64(int val) {

  n64=val;
  return(0);

}


/*---------------------------------------------------------------- */

int set_w8(int val) {

  w8=val;
  return(0);

}


/*---------------------------------------------------------------- */


int set_w16(int val) {

  w16=val;
  return(0);

}


/*---------------------------------------------------------------- */


int set_w32(int val) {

  w32=val;
  return(0);

}


/*---------------------------------------------------------------- */


int set_p32(int val) {

  p32=val;
  return(0);

}


/*---------------------------------------------------------------- */


int set_w64(int val) {

  w64=val;
  return(0);

}


/*---------------------------------------------------------------- */


int set_p64(int val) {

  p64=val;
  return(0);

}


/*---------------------------------------------------------------- */


int set_xtod(int val) {

  xtod=val;
  return(0);

}


/*---------------------------------------------------------------- */


int set_indent_size(int val) {

  indent_size=val;
  return(0);

}


/*---------------------------------------------------------------- */


int set_max_depth(int val) {

  max_depth=val;
  return(0);

}


/*---------------------------------------------------------------- */


int set_no_typename(int val) {

  no_typename=val;
  return(0);

}


/*---------------------------------------------------------------- */


int set_verbose(int val) {

  verbose=val;
  return(0);

}


/*---------------------------------------------------------------- */


int set_brief(int val) {

  brief=val;
  return(0);

}


/*---------------------------------------------------------------- */

int set_no_data(int val) {

  no_data=val;
  return(0);

}


/*---------------------------------------------------------------- */


