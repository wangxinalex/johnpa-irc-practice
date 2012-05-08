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
#include <errno.h>
#include <signal.h>

#include "debug.h"
#include "rtlib.h"
#include "rtgrading.h"
#include "common.h"
#include "irc_proto.h"
#include "arraylist.h"
#include "sircd.h"

u_long curr_nodeID;
rt_config_file_t   curr_node_config_file;  /* The config_file  for this node */
rt_config_entry_t *curr_node_config_entry; /* The config_entry for this node */

void init_node(char *nodeID, char *config_file);
void irc_server();
const Boolean clientEquals(const Object obj1, const Object obj2);
int handle_incoming_conn(Arraylist list, char *servername, int listenfd);

void
usage() {
  fprintf(stderr, "sircd [-h] [-D debug_lvl] <nodeID> <config file>\n");
  exit(-1);
}

/* calls getaddrinfo(), socket(), bind(), listen()
  on error, prints relevant error messages using fprintf or perror,
            and return -1*/
int setupListenSocket(unsigned long protocol){
  int listenfd;

  /* setsockopt */
  int yes = 1;

  /* addrinfo vars */
  struct addrinfo hints;
  struct addrinfo *res;
  int status;

  char temp[MAX_CONFIG_FILE_LINE_LEN + 1];

  snprintf(temp,MAX_CONFIG_FILE_LINE_LEN,"%lu",protocol);
  /* getaddrinfo */
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET; /* use AF_INET6 for IPv6 */
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  if ((status = getaddrinfo(NULL, temp, &hints, &res)) != 0){
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

  if (listen(listenfd, MAX_CLIENTS) < 0){
    perror("listen");
    return -1;
  }

  return listenfd;
}

/* Handle incoming connection */
/* accept(), create new client and add to list */
/* returns index of new client on the list */
int handle_incoming_conn(Arraylist list, char *servername, int listenfd){
  /* Incoming Connection */
  struct sockaddr_storage remoteaddr;
  socklen_t addrlen = sizeof(remoteaddr);
  int newfd = accept(listenfd, (struct sockaddr *)&remoteaddr, &addrlen);

  if (newfd == -1) {
    DEBUG_PERROR("accept");
    return -1;
  }

  DPRINTF(DEBUG_SOCKETS,"handle_incoming_conn: new connection on socket %d\n",newfd);

  return addClientToList(list, servername, newfd, &remoteaddr);
}


fd_set master_set;
fd_set write_set;


int main( int argc, char *argv[] )
{
  /* for parsing args */
  extern char *optarg;
  extern int optind;
  int ch;

  /* vars */
  int i,j;
  int listenfd;

  /* select vars */
  fd_set read_set;
  int fdmax;

  /* client arr */
  Arraylist clientList;

  /* servername */
  char servername[MAX_SERVERNAME+1];

  while ((ch = getopt(argc, argv, "hD:")) != -1)
  switch (ch) {
  case 'D':
    if (set_debug(optarg)) {
      exit(0);
    }
    break;
  case 'h':
  default: /* FALLTHROUGH */
    usage();
  }
  argc -= optind;
  argv += optind;

  if (argc < 2) {
    usage();
  }
  signal(SIGPIPE, SIG_IGN);
  init_node(argv[0], argv[1]);

  printf( "I am node %lu and I listen on port %d for new users\n", curr_nodeID, curr_node_config_entry->irc_port );

  /* Start your engines here! */
  if (gethostname(servername,MAX_SERVERNAME) < 0){
    perror("gethostname");
    return EXIT_FAILURE;
  }
  servername[MAX_SERVERNAME] = '\0';

  /* delegate all function calling to setupListenSocket.
        It should print out relevant err messages. */
  listenfd = setupListenSocket(curr_node_config_entry->irc_port);
  if (listenfd < 0){
    return EXIT_FAILURE;
  }

  /* initialize client array */
  clientList = arraylist_create(clientEquals);
  /* prepare for select */
  fdmax = listenfd;
  FD_ZERO(&master_set);
  FD_ZERO(&write_set);
  FD_SET(listenfd,&master_set);

  /* FD_ZERO(&write_set);  initially no data to write */

  /* main loop!! */
  for (;;){
    int retval;
    read_set = master_set;

    /* wait until any sockets become available */
    retval = select(fdmax+1,&read_set,&write_set,NULL,NULL);

    if (retval <= 0){
      if (retval < 0)
      DEBUG_PERROR("select");
      continue; /* handle errors gracefully and wait again if timed out (it shouldn't)*/
    }
    /* at least one socket ready*/
    for (i = 0; i <= fdmax; i++){
      if (FD_ISSET(i, &write_set)) {
        int listIndex = findClientIndexBySockFD(clientList,i);
        client_t *thisClient = (client_t *) CLIENT_GET(clientList,listIndex);
        /*client data ready to be written */
        int nbytes;
        /* iterate through item to be sent, until it will block */
        Arraylist outbuf = thisClient->outbuf;

        while (!arraylist_is_empty(outbuf))
        {
          /* write */
          char *dataToSend = (char *) (arraylist_get(outbuf,0)) + thisClient->outbuf_offset;
          size_t sizeToSend = strlen(dataToSend);
          nbytes = send(thisClient->sock, dataToSend, sizeToSend, 0);
          if (nbytes <0){
            /* error */
            if (errno == EPIPE || errno == ECONNRESET){
              /* connection lost or closed by the peer */

              DPRINTF(DEBUG_SOCKETS,"send: client %d hungup\n",i);
              handle_line(clientList,listIndex,servername,"QUIT :Connection closed");
              continue;
            }
          }
          else if (nbytes == sizeToSend){
            /* current line completely sent */
            char *toRemove = (char *) (arraylist_get(outbuf,0));
            arraylist_removeIndex(outbuf,0);
            free(toRemove);
          }
          else{
            /* partial send */
            thisClient->outbuf_offset += nbytes;
            break;
          }
        }
        if (arraylist_is_empty(outbuf))
          FD_CLR(i, &write_set);
      }
      if (FD_ISSET(i, &read_set)) {
        if (i == listenfd){
          /* incoming connection */
          int newindex = handle_incoming_conn(clientList,servername,listenfd);
          if (newindex < 0){
            continue;
          }
          client_t *newClient = CLIENT_GET(clientList,newindex);
          int newfd = newClient->sock;
          FD_SET(newfd,&master_set);
          if (newfd > fdmax){
            fdmax = newfd;
          }
        }
        else{
          /* client data ready */

          int listIndex = findClientIndexBySockFD(clientList,i);
          char *tempPtr;

          /* for split function */
          char** tokenArr;
          int numTokens;

          if (listIndex < 0){
            close(i);
            FD_CLR(i,&master_set);
            continue;
          }

          int nbytes = recv(i, CLIENT_GET(clientList,listIndex)->inbuf + CLIENT_GET(clientList,listIndex)->inbuf_size,
          MAX_MSG_LEN - CLIENT_GET(clientList,listIndex)->inbuf_size, 0);

          /* recv failed. Either client left or error */
          if (nbytes <= 0){
            if (nbytes == 0){
              DPRINTF(DEBUG_SOCKETS,"recv: client %d hungup\n",i);
              handle_line(clientList,listIndex,servername,"QUIT :Connection closed");
            }
            else if (errno == ECONNRESET || errno == EPIPE){
              DPRINTF(DEBUG_SOCKETS,"recv: client %d connection reset \n",i);
              handle_line(clientList,listIndex,servername,"QUIT :Connection closed");
            }
            else{
              perror("recv");
            }
            continue;
          }

          /* NULL terminate to use strstr */
          CLIENT_GET(clientList,listIndex)->inbuf_size += nbytes;
          CLIENT_GET(clientList,listIndex)->inbuf[CLIENT_GET(clientList,listIndex)->inbuf_size] = '\0';
          tempPtr = strpbrk(CLIENT_GET(clientList,listIndex)->inbuf,"\r\n");
          if (!tempPtr){
            if (CLIENT_GET(clientList,listIndex)->inbuf_size == MAX_MSG_LEN){
              /* Message too long. Dump the content */
              DPRINTF(DEBUG_INPUT,"recv: message longer than MAX_MESSAGE detected. The message will be discarded\n");
              CLIENT_GET(clientList,listIndex)->inbuf_size = 0;
            }
            continue;
          }
          tokenArr = splitByDelimStr(CLIENT_GET(clientList,listIndex)->inbuf,"\r\n",&numTokens,&tempPtr);
          if (!tokenArr){
            DPRINTF(DEBUG_INPUT,"splitByDelimStr: failed to split inputToken\n");
            CLIENT_GET(clientList,listIndex)->inbuf_size = 0;
            continue;
          }

          if (numTokens != 0){
            CLIENT_GET(clientList,listIndex)->inbuf_size = strlen(tempPtr);
            memmove(CLIENT_GET(clientList,listIndex)->inbuf,tempPtr,CLIENT_GET(clientList,listIndex)->inbuf_size);
          }

          for (j=0;j<numTokens;j++){
            handle_line(clientList,listIndex,servername,tokenArr[j]);
          }

          if (arraylist_size(CLIENT_GET(clientList,listIndex)->outbuf) > 0){
            FD_SET(i,&write_set); /* one or more data to write */
          }
          freeTokens(&tokenArr,numTokens);
        }
      }
    }
  }

  return 0;
}

/*
* void init_node( int argc, char *argv[] )
*
* Takes care of initializing a node for an IRC server
* from the given command line arguments
*/
void
init_node(char *nodeID, char *config_file)
{
  int i;

  curr_nodeID = atol(nodeID);
  rt_parse_config_file("sircd", &curr_node_config_file, config_file );

  /* Get config file for this node */
  for( i = 0; i < curr_node_config_file.size; ++i )
  if( curr_node_config_file.entries[i].nodeID == curr_nodeID )
  curr_node_config_entry = &curr_node_config_file.entries[i];

  /* Check to see if nodeID is valid */
  if( !curr_node_config_entry )
  {
    printf( "Invalid NodeID\n" );
    exit(1);
  }
}

const Boolean clientEquals(const Object obj1, const Object obj2){
  client_t *c1 = (client_t *)obj1;
  client_t *c2 = (client_t *)obj2;
  /* simple comparison of sockfds */
  return (c1->sock == c2->sock) ? TRUE : FALSE;
}

