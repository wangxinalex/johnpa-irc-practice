#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <assert.h>
#include <fcntl.h>

#include "debug.h"
#include "common.h"

#define MAX_CLIENTS 512
#define BACKLOG MAX_CLIENTS

#define MAX_MESSAGE 512
typedef struct{
    int sockfd;
    unsigned readbuf_size;
    char readbuf[MAX_MESSAGE + 1];
} client;

void usage()
{
    fprintf(stderr, "minid <port number to listen to>\n");
    exit(EXIT_FAILURE);
}

// get sockaddr, IPv4 or IPv6:
/*void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
*/

/* calls getaddrinfo(), socket(), bind(), listen()
   on error, prints relevant error messages using fprintf or perror,
            and return -1*/
int setupListenSocket(char *protocol){
  int listenfd;

  /* setsockopt */
  int yes = 1;

  /* addrinfo vars */
  struct addrinfo hints;
  struct addrinfo *res;
  int status;

  /* getaddrinfo */
  memset(&hints, 0 ,sizeof(hints));
  hints.ai_family = AF_INET; /* use AF_INET6 for IPv6 */
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  if ((status = getaddrinfo(NULL,protocol, &hints, &res)) != 0){
    DPRINTF(DEBUG_SOCKETS,"getaddinfo error: %s\n",gai_strerror(status));
    return -1;
  }

  /* listen */
  listenfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  if (listenfd < 0){
    perror("socket");
    return -1;
  }

  /* for non-blocking sockets */
  if (fcntl(listenfd, F_SETFL, O_NONBLOCK) < 0){
    perror("fcntl");
    return -1;
  }

  /* reuse sockets. */
  if (setsockopt(listenfd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int)) < 0){
    perror("setsockopt");
    return -1;
  }

  /* bind */
  if (bind(listenfd, res->ai_addr, res->ai_addrlen) < 0){
    perror("bind");
    return -1;
  }

  /* done with addrinfo */
  freeaddrinfo(res);

  if (listen(listenfd, BACKLOG) < 0){
    perror("listen");
    return -1;
  }

  return listenfd;
}

int listIndexByFD(client **list, int numClient, int fd){
  int i;
  for (i = 0; i < numClient; i++){
    if (list[i]->sockfd == fd)
      return i;
  }
  return -1;
}

/* returns index on success, -1 on error */
int listAddClient(client **list, int *numClient, int fd){
  if ( (*numClient) == MAX_CLIENTS)
    return -1;
  list[*numClient] = malloc( sizeof(client) );
  if (list[*numClient] == NULL){
    return -1;
  }

  list[*numClient]->sockfd = fd;
  list[*numClient]->readbuf_size = 0;

  (*numClient)++;

  return (*numClient)-1;
}

int handleLine(client **list, int numClient, int srcIndex, char *line){
  DPRINTF(DEBUG_INPUT,"handleLine for client %d: %s\n",list[srcIndex]->sockfd,line);
  if (send(list[srcIndex]->sockfd,line,strlen(line),0) < 0){
    perror("send");
    return -1;
  }
  if (send(list[srcIndex]->sockfd,"\n",strlen("\n"),0) < 0){
    perror("send");
  }
  return 0;
}

int main( int argc, char *argv[] )
{
  int i,j;
  int listenfd;

  /* select vars */
  fd_set master_set;
  fd_set read_set;
  fd_set write_set;
  int fdmax;

  /* clinet arr */
  client *list[MAX_CLIENTS];
  int numClient = 0;

  if (set_debug("all")){
    fprintf(stderr,"Failed to set debug level\n");
    return EXIT_FAILURE;
  }

  if (argc < 2)
    usage();

  /* delegate all function calling to setupListenSocket.
     It should print out relevant err messages. */
  listenfd = setupListenSocket(argv[1]);
  if (listenfd < 0){
    return EXIT_FAILURE;
  }

  /* prepare for select */
  fdmax = listenfd;
  FD_ZERO(&master_set);
  FD_SET(listenfd,&master_set);
  FD_ZERO(&write_set); /* initially no data to write */

  /* main loop!! */
  for (;;){
    int retval;
    read_set = master_set;

    /* wait until any sockets become available */
    retval = select(fdmax+1,&read_set,&write_set,NULL,NULL);

    if (retval <= 0)
      continue; /* handle errors gracefully and wait again if timed out (it shouldn't)*/

    /* at least one socket ready*/
    for (i=0;i <= fdmax; i++){
      if (FD_ISSET(i, &read_set)) {
        /*i is ready to read */
        if (i == listenfd){
          /* Incoming Connection */
          struct sockaddr_storage remoteaddr;
          socklen_t addrlen = sizeof(remoteaddr);
          int newfd = accept(listenfd, (struct sockaddr *)&remoteaddr, &addrlen);
          if (newfd == -1) {
            perror("accept");
            continue;
          }

          FD_SET(newfd,&master_set);
          if (newfd > fdmax){
            fdmax = newfd;
          }
          DPRINTF(DEBUG_SOCKETS,"select: new connection on socket %d\n",newfd);

          if ( listAddClient(list,&numClient,newfd) < 0){
            /* "Sorry we cannot accept your request now. Please try again later" situation */
            /* just close the connection myself. HAHA */
            close(newfd);
            FD_CLR(newfd,&master_set);
            continue;
          }
        }
        else{
          /* Client data ready to be read */
          int listIndex = listIndexByFD(list,numClient,i);
          char *tempPtr;

          /* for split function */
          char** tokenArr;
          int numTokens;


          if (listIndex < 0){
            /* this shouldn't happen */
            DPRINTF(DEBUG_SOCKETS,"recv: server was not able to locate fd %d in the client list. Adding one for now.\n",i);
            listIndex = listAddClient(list,&numClient,i);
            if (listIndex < 0){
              /*we couldn't add*/
              close(i);
              FD_CLR(i,&master_set);
            }
          }

          int nbytes = recv(i, list[listIndex]->readbuf + list[listIndex]->readbuf_size,
                                MAX_MESSAGE - list[listIndex]->readbuf_size, 0);

          /* case 1: error */
          if (nbytes <= 0){
            if (nbytes == 0){
              DPRINTF(DEBUG_SOCKETS,"recv: client %d hungup\n",i);
            }
            else{
              perror("recv");
            }
            close(i);
            FD_CLR(i, &master_set);
            continue;
          }

          /* NULL terminate to use strstr */
          list[listIndex]->readbuf_size += nbytes;
          list[listIndex]->readbuf[list[listIndex]->readbuf_size] = '\0';
          tempPtr = strstr(list[listIndex]->readbuf,"\n");
          if (!tempPtr){
            if (list[listIndex]->readbuf_size == MAX_MESSAGE){
              /* Message too long. Dump the content */
              DPRINTF(DEBUG_INPUT,"recv: message longer than MAX_MESSAGE detected. The message will be discarded\n");
              list[listIndex]->readbuf_size = 0;
            }
            continue;
          }
          tokenArr = splitByDelimStr(list[listIndex]->readbuf,"\n",&numTokens,&tempPtr);
          if (!tokenArr){
            DPRINTF(DEBUG_INPUT,"splitByDelimStr: failed to split inputToken\n");
            list[listIndex]->readbuf_size = 0;
            continue;
          }

          if (numTokens != 0){
            list[listIndex]->readbuf_size = strlen(tempPtr);
            memmove(list[listIndex]->readbuf,tempPtr,list[listIndex]->readbuf_size);
          }

          for (j=0;j<numTokens;j++){
            if (handleLine(list,numClient,listIndex,tokenArr[j]) < 0){
              break; /* can't handle more lines */
            }
          }

        }
      }
    }
  }

  return EXIT_SUCCESS;
}

