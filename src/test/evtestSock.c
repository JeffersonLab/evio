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
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/errno.h>
#include <netinet/tcp.h> /* TCP_NODELAY def */
#include "evio.h"

#if defined sun || defined linux || defined VXWORKS
#  define socklen_t int
#endif

#define LISTENQ 10
#define SA struct sockaddr


#define MIN(a,b) (a<b)? a : b

/* prototypes */
static int  tcp_listen(unsigned short port, int size);
static int  Accept(int fd, struct sockaddr *sa, socklen_t *salenptr);
static int32_t *makeEvent();
static int32_t *makeEvent2();

char *dictionary =
"<xmlDict>\n"
"  <xmldumpDictEntry name=\"Tag1-Num1\"   tag=\"1\"   num=\"1\"/>\n"
"  <xmldumpDictEntry name=\"Tag2-Num2\"   tag=\"2\"   num=\"2\"/>\n"
"  <xmldumpDictEntry name=\"Tag3-Num3\"   tag=\"3\"   num=\"3\"/>\n"
"  <xmldumpDictEntry name=\"Tag4-Num4\"   tag=\"4\"   num=\"4\"/>\n"
"  <xmldumpDictEntry name=\"Tag5-Num5\"   tag=\"5\"   num=\"5\"/>\n"
"  <xmldumpDictEntry name=\"Tag6-Num6\"   tag=\"6\"   num=\"6\"/>\n"
"  <xmldumpDictEntry name=\"Tag7-Num7\"   tag=\"7\"   num=\"7\"/>\n"
"  <xmldumpDictEntry name=\"Tag8-Num8\"   tag=\"8\"   num=\"8\"/>\n"
"</xmlDict>\n";


static int     noDelay    = 1;
static int     serverPort = 22333;
static int     bufferSize = 8192;
static int     receiveBufferSize = 4*8192;
static char    *host = "localhost";
static int     recvFd;


static void *receiverThread(void *arg) {
    int listenfd=0, handle, status, nevents, nwords, *ip;
    int buffer[2048], i;
    int *dict, dictLen;
    struct sockaddr_in cliaddr;
    socklen_t          len;

    len = sizeof(cliaddr);
    
printf("Receiver thread: listen on server socket\n");

    /* open a listening socket */
    listenfd = tcp_listen(serverPort, bufferSize);
    if (listenfd < 0) {
        printf("Error starting listening socket\n");
        exit(1);
    }

    /* wait for connection to client */
printf("Receiver thread: accepting\n");
    recvFd = Accept(listenfd, (SA *) &cliaddr, &len);
    if (recvFd < 0) {
        printf("Error receiving client TCP connection\n");
        exit(1);
    }
    
printf("Receiver thread: got client ... \n");
    status = evOpenSocket(recvFd, "r", &handle);
printf ("    Opened socket, status = %#x\n", status);

     status = evGetDictionary(handle, &dict, &dictLen);
     printf ("    get dictionary, status = %#x\n\n", status);

     if (dictionary != NULL) {
         printf("DICTIONARY =\n%s\n", dict);
         free(dict);
     }

     nevents = 0;
    
     while ((status = evRead(handle, buffer, 2048)) == S_SUCCESS) {
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
     }
    
    status = evClose(handle);
printf ("    Closed socket, status = %#x\n\n", status);


printf ("    Will reopen socket for reading\n");
    status = evOpenSocket(recvFd, "r", &handle);
printf ("    Opened socket, status = %#x\n", status);

    nevents = 0;
    
    while ((status = evRead(handle, buffer, 2048)) == S_SUCCESS) {
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
    }
    
printf("\n    Last read, status = %x\n", status);
    if (status == EOF) {
printf("    Last read, reached EOF!\n");
    }
    
    evClose(handle);
}



static int createSendFd() {

    struct sockaddr_in sin;
    int   sendFd, on=1, off=0;
    struct hostent  *hp;


    bzero(&sin, sizeof(sin));

    sendFd = socket(AF_INET, SOCK_STREAM, 0);
    if (sendFd < 0) {
        printf("cannot open socket\n");
        exit(1);
    }

    hp = gethostbyname(host);
    if (hp == 0 && (sin.sin_addr.s_addr = inet_addr(host)) == -1) {
        fprintf(stderr, "%s: unknown host\n", host);
        exit(1);
    }

    if (hp != 0) {
        bcopy((const void *)hp->h_addr, (void *) &sin.sin_addr, hp->h_length);
    }

    sin.sin_port = htons(serverPort);
    sin.sin_family = AF_INET;

    if (setsockopt(sendFd, IPPROTO_TCP, TCP_NODELAY,(const char *)  &on, sizeof(on)) < 0) {
        printf("setsockopt TCP_NODELAY failed\n");
        exit(1);
    }

    if (connect(sendFd, (SA *)&sin, sizeof(sin)) < 0) {
        perror("connect");
        printf("connect failed: host %s port %d\n", inet_ntoa(sin.sin_addr),
               ntohs(sin.sin_port));

        exit(1);
    }

    return (sendFd);
}

/**
 * Routine to write a specified number of bytes to a TCP socket.
 *
 * @param fd   socket's file descriptor
 * @param vptr pointer to buffer from which bytes are supplied
 * @param n    number of bytes to write
 *
 * @return number of bytes written if successful
 * @return -1 if error and errno is set
 */
static int tcpWrite(int fd, const void *vptr, int n)
{
    int         nleft;
    int         nwritten;
    const char  *ptr;

    ptr = (char *) vptr;
    nleft = n;
  
    while (nleft > 0) {
        if ( (nwritten = write(fd, (char*)ptr, nleft)) <= 0) {
            if (errno == EINTR) {
                nwritten = 0;       /* and call write() again */
            }
            else {
                return(nwritten);   /* error */
            }
        }

        nleft -= nwritten;
        ptr   += nwritten;
    }
    return(n);
}


int main()
{
    int i, handle, status, sendFd, maxEvBlk=2;
    int *ip, *pBuf;
    pthread_t tid;

    /* Try sending extra big block headers ... */
    uint32_t data[] = {
        0x0000000f,
        0x00000001,
        0x0000000A,
        0x00000001,
        0x00000000,
        0x00000004,
        0x00000000,
        0xc0da0100,
        0x00000001,
        0x00000002,

        0x00000004,
        0x00010101,
        0x00000001,
        0x00000002,
        0x00000003,
        
        0x0000000f,
        0x00000001,
        0x0000000A,
        0x00000001,
        0x00000000,
        0x00000004,
        0x00000000,
        0xc0da0100,
        0x00000001,
        0x00000002,

        0x00000004,
        0x00010101,
        0x00000001,
        0x00000002,
        0x00000003,
        
        0x00000009,
        0x00000002,
        0x00000009,
        0x00000000,
        0x00000000,
        0x00000204,
        0x00000000,
        0xc0da0100,
        0x00000003,
    };

    printf("Try running Receiver thread\n");
      
    /* run receiver thread */
    pthread_create(&tid, NULL, receiverThread, (void *) NULL);

    /* give it a chance to start */
    sleep(2);

    /* Create sending socket */
    sendFd = createSendFd();

    printf("Sending socket fd = %d\n\n", sendFd);

    /* write data by hand over network */
    tcpWrite(sendFd, data, 156);
    
    status = evClose(handle);
    printf ("    \"Closed\" buffer, status = %#x\n\n", status);

    free(pBuf);

    /* Don't exit the program before the receiver thread can do its stuff. */
    sleep(6);
}


int main2()
{
    int i, handle, status, sendFd, maxEvBlk=2;
    int *ip, *pBuf;
    pthread_t tid;


    printf("Try running Receiver thread\n");
      
    /* run receiver thread */
    pthread_create(&tid, NULL, receiverThread, (void *) NULL);

    /* give it a chance to start */
    sleep(2);

    /* Create sending socket */
    sendFd = createSendFd();

    printf("Sending socket fd = %d\n\n", sendFd);

    

    printf("\nEvent I/O tests to socket (%d)\n", sendFd);
    status = evOpenSocket(sendFd, "w", &handle);
    printf ("    Opened socket, status = %#x\n", status);


    status = evWriteDictionary(handle, dictionary);
    printf ("    Write dictionary to socket, status = %#x\n\n", status);

    pBuf = ip = makeEvent();
    
    printf ("    Will write ** SINGLE ** event to buffer, status = %#x\n",status);
    status = evWrite(handle, ip);

    status = evClose(handle);
    printf ("    \"Closed\" buffer, status = %#x\n\n", status);



    ip = pBuf;
    
    status = evOpenSocket(sendFd, "w", &handle);
    printf ("    Opened socket for multiple writes, status = %#x\n", status);

    status = evIoctl(handle, "N", (void *) (&maxEvBlk));
    printf ("    Changed max events/block to %d, status = %#x\n", maxEvBlk, status);
    
    printf ("    Will write 3 events to buffer\n");
    evWrite(handle,ip);
    evWrite(handle,ip);
    evWrite(handle,ip);

    status = evClose(handle);
    printf ("    Closed send socket, status %#x\n\n", status);

    free(pBuf);

    /* Don't exit the program before the receiver thread can do its stuff. */
    sleep(10);
}


static int32_t *makeEvent2()
{
    int32_t *bank, *segment;
    short *word;


    bank = (int *) calloc(1, 11*sizeof(int32_t));
    bank[0] = 10;                    /* event length = 10 */
    bank[1] = 1 << 16 | 0x20 << 8;   /* tag = 1, bank 1 contains segments */

    segment = &(bank[2]);
    segment[0] = 2 << 24 | 0xb << 16 | 2; /* tag = 2, seg 1 has 2 - 32 bit ints, len = 2 */
    segment[1] = 0x1;
    segment[2] = 0x2;
        
    segment += 3;
        
    segment[0] = 3 << 24 | 2 << 22 | 4 << 16 | 2; /* tag = 3, 2 bytes padding, seg 2 has 3 shorts, len = 2 */
    word = (short *) &(segment[1]);
    word[0] = 0x3;
    word[1] = 0x4;
    word[2] = 0x5;

    segment += 3;

    /* HI HO - 2 strings */
    segment[0] =    4 << 24 | 0x3 << 16 | 2; /* tag = 4, seg 3 has 2 strings, len = 2 */
    segment[1] = 0x48 << 24 | 0 << 16   | 0x49 << 8 | 0x48 ;   /* H \0 I H */
    segment[2] =   4  << 24 | 4 << 16   | 0 << 8    | 0x4F ;   /* \4 \4 \0 O */

    return(bank);
}

static int32_t *makeEvent()
{
    int32_t *bank;

    bank = (int *) calloc(1, 5*sizeof(int32_t));
    bank[0] = 4;                    /* event length = 4 */
    bank[1] = 1 << 16 | 0x1 << 8;   /* tag = 1, bank 1 contains ints */
    bank[2] = 1;
    bank[3] = 2;
    bank[4] = 3;

    return(bank);
}



/*****************************************************/
static int tcp_listen(unsigned short port, int size) {

    int                 listenfd, err, rcvBufSize;
    const int           debug=1, on=1;
    struct sockaddr_in  servaddr;

    err = listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (err < 0) {
        if (debug) fprintf(stderr, "tcp_listen: socket error\n");
        return err;
    }

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family      = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port        = htons(port);

    if (noDelay) {
        err = setsockopt(listenfd, IPPROTO_TCP, TCP_NODELAY, (const void *) &on, sizeof(on));
        if (err < 0) {
            close(listenfd);
            if (debug) fprintf(stderr, "tcp_listen: setsockopt error\n");
            return err;
        }
    }

    /* default TCP buffer = 4x data buffer size */
    err = setsockopt(listenfd, SOL_SOCKET, SO_RCVBUF, (const void *) &receiveBufferSize, sizeof(receiveBufferSize));
    if (err < 0) {
        close(listenfd);
        if (debug) fprintf(stderr, "tcp_listen: setsockopt error\n");
        return err;
    }

    err = setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void *) &on, sizeof(on));
    if (err < 0) {
        close(listenfd);
        if (debug) fprintf(stderr, "tcp_listen: setsockopt error\n");
        return err;
    }

    err = setsockopt(listenfd, SOL_SOCKET, SO_KEEPALIVE, (const void *) &on, sizeof(on));
    if (err < 0) {
        close(listenfd);
        if (debug) fprintf(stderr, "tcp_listen: setsockopt error\n");
        return err;
    }

    err = bind(listenfd, (SA *) &servaddr, sizeof(servaddr));
    if (err < 0) {
        close(listenfd);
        if (debug) fprintf(stderr, "tcp_listen: bind error\n");
        return err;
    }

    err = listen(listenfd, LISTENQ);
    if (err < 0) {
        close(listenfd);
        if (debug) fprintf(stderr, "tcp_listen: listen error\n");
        return err;
    }

    return(listenfd);
}


/************************************************************/
static int Accept(int fd, struct sockaddr *sa, socklen_t *salenptr) {
    int  n;

again:
        if ((n = accept(fd, sa, salenptr)) < 0) {
#ifdef EPROTO
        if (errno == EPROTO || errno == ECONNABORTED) {
#else
        if (errno == ECONNABORTED) {
#endif
            goto again;
        }
        else {
            fprintf(stderr, "Accept: error, errno = %d\n", errno);
        }
        }
        return(n);
        }

