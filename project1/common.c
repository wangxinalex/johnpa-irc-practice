
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include "common.h"
#include "debug.h"

void freeTokens(char ***ptrToTokenArr, int numTokens){
  int i;
  for (i=0;i<numTokens;i++){
    free((*ptrToTokenArr)[i]);
  }
  free(*ptrToTokenArr);
  *ptrToTokenArr = NULL;
}
/** Function splitByDelimStr
 *
 *  This function splits a string into tokens deliminated by delimStr.
 *
 *  Arguments
 *  buf: string to split
 *  delimStr: delimiter for split
 *
 *  Modifies
 *  numTokens: Number of Tokens terminated by delimStr
 *  ptrToTrailing: Points to start of unterminated string in string buf  OR   position of NULL terminator
 *
 *  Return:
 *    array of deep-copied strings. Array and strings should be freed after use,
 *    although recommended to call freeTokens(&array,numTokens);
 *
 **/
char **splitByDelimStr(const char *buf, const char *delimStr, int *numTokens, char **ptrToTrailing){

  char **retArr;
  int i;
  char *tempPtr;

  if (!buf || !delimStr || !numTokens || !ptrToTrailing){
    /* handle error gracefully */
    DPRINTF(DEBUG_ERRS,"splitByCRLF: NULL input detected\n");
    if (numTokens)
      *numTokens = 0;
    if (ptrToTrailing)
      *ptrToTrailing = NULL;
    return NULL;
  }

  /* count tokens */
  (*numTokens) = 0;
  tempPtr = strpbrk(buf,delimStr);
  while ( tempPtr ){
    (*numTokens)++;
    /* move tempPtr until no delimiter found */
    do{
      tempPtr++;
    }while (tempPtr == strpbrk(tempPtr, delimStr));
    tempPtr = strpbrk(tempPtr, delimStr);
  }

  /* no tokens found */
  if ( (*numTokens) == 0){
    *ptrToTrailing = (char *)buf;
    return NULL;
  }

  /* Allocate token array */
  retArr = malloc( sizeof(char *) *  (*numTokens));

  if (!retArr){
    DPRINTF(DEBUG_ERRS,"splitByDelimStr: retArr malloc failed\n");
    *numTokens = 0;
    *ptrToTrailing = (char *) buf;
    return NULL;
  }

  for (i=0,tempPtr=(char *)buf; i < (*numTokens); i++){
    char *tokenStart = tempPtr;
    char *tokenEnd = strpbrk(tempPtr,delimStr);
    retArr[i] = malloc (sizeof(char) * ((tokenEnd - tokenStart) + 1) );
    if (!retArr[i]){
      DPRINTF(DEBUG_ERRS,"splitByDelimStr: retArr[%d] malloc failed\n",i);
      freeTokens(&retArr,i-1);
      *numTokens = 0;
      *ptrToTrailing = (char *)buf;
      return NULL;
    }
    memcpy(retArr[i],tokenStart,(tokenEnd-tokenStart));
    retArr[i][(tokenEnd-tokenStart)] = '\0';
    tempPtr = tokenEnd + 1;
    while (tempPtr == strpbrk(tempPtr, delimStr)){
      tempPtr++;
    }
  }
  *ptrToTrailing = tempPtr;

  return retArr;
}


int findClientIndexBySockFD(Arraylist list, int sockfd){
  int i;
  for (i = 0; i < arraylist_size(list); i++){
    client_t *tempClient = (client_t *)arraylist_get(list,i);
    if (tempClient->sock == sockfd)
    return i;
  }
  return -1;
}
int findClientIndexByNick(Arraylist clientList, char *nick){
    int i;
    for (i = 0; i < arraylist_size(clientList); i++){
        if (strcmp(CLIENT_GET(clientList,i)->nick,nick) == 0){
            return i;
        }
    }
    return -1;
}
int findChannelIndexByChanname(Arraylist channelList, char *channame){
    int i;
    for (i = 0; i < arraylist_size(channelList); i++){
        if (strcmp(CHANNEL_GET(channelList,i)->name,channame) == 0){
            return i;
        }
    }
    return -1;

}

static const Boolean strComparatorForArraylist(const Object obj1, const Object obj2){
  return (0 == strcmp((char *)obj1, (char *)obj2) ) ? TRUE : FALSE;
}
static const Boolean clientComparator(const Object obj1, const Object obj2){

    client_t *client1 = (client_t *)obj1;
    client_t *client2 = (client_t *)obj2;

    return (client1->sock == client2->sock);

}
static const Boolean channelComparator(const Object obj1, const Object obj2){

    channel_t *client1 = (channel_t *)obj1;
    channel_t *client2 = (channel_t *)obj2;

    return (strcmp(client1->name,client2->name) == 0);

}


int addClientToList(Arraylist list, char *servername, int sockfd, void *remoteaddr){
    client_t *newClient = client_alloc_init(servername,sockfd,remoteaddr);
    if (!newClient)
        return -1;
    int index = arraylist_add(list,newClient);
  if (index < 0){
    /* "Sorry we cannot accept your request now. Please try again later" situation */
    /* just close the connection myself. HAHA */
    DPRINTF(DEBUG_SOCKETS,"addClientToList: failed to add client %d to the client list\n",sockfd);
    arraylist_free(newClient->outbuf);
    free(newClient);
    close(sockfd);
  }

  return index;
}

void freeOutbuf(client_t *client){
  Arraylist outbuf = client->outbuf;
  int i;
  for (i = 0; i < arraylist_size(outbuf); i++){
    free(arraylist_get(outbuf,i));
  }
  arraylist_free(outbuf);
}

client_t *client_alloc_init(char *servername, int sockfd, void *remoteaddr){
  client_t *newClient;
  int index;
  newClient = malloc(sizeof(client_t));
  if (!newClient){
    DPRINTF(DEBUG_ERRS,"client_alloc_init: failed to create client entry for socket %d\n",sockfd);
    close(sockfd);
    return NULL;
  }
  /* initialize client entry */
  newClient->sock = sockfd;
  memcpy(&newClient->cliaddr, remoteaddr, sizeof(struct sockaddr_storage));
  newClient->inbuf_size = 0;
  newClient->registered = FALSE;
  newClient->outbuf = arraylist_create();
  newClient->outbuf_offset = 0;
  INIT_STRING(newClient->hostname);
  strcpy(newClient->servername,servername);
  INIT_STRING(newClient->nick);
  INIT_STRING(newClient->user);
  INIT_STRING(newClient->realname);
  INIT_STRING(newClient->inbuf);
  newClient->chanlist = arraylist_create();
  if (  (index = getnameinfo((struct sockaddr *)&newClient->cliaddr,sizeof(struct sockaddr_storage),newClient->hostname,MAX_HOSTNAME,NULL,0,0)) < 0){
    DPRINTF(DEBUG_SOCKETS,"getnameinfo: %s and hostname: %s\n",gai_strerror(index),newClient->hostname);
  }

  return newClient;

}

channel_t *channel_alloc_init(char *channame){
    channel_t *newChannel;
    newChannel = malloc(sizeof(channel_t));
    if (!newChannel){
        DPRINTF(DEBUG_ERRS,"channel_alloc_init: failed to create channel entry");
        return NULL;
    }
    strncpy(newChannel->name,channame,MAX_CHANNAME);
    newChannel->userlist = arraylist_create();
    INIT_STRING(newChannel->topic);
    INIT_STRING(newChannel->key);
    return newChannel;
}

extern fd_set master_set;
extern fd_set write_set;

/* remove client from our lists
 * NOTE: does not perform any IRC messaging thingys*/
void remove_client(Arraylist clientList, int srcIndex){
    int i;
    client_t *client = CLIENT_GET(clientList,srcIndex);

    for (i=0;i<arraylist_size(client->chanlist);i++){
        arraylist_remove(CHANNEL_GET(client->chanlist,i)->userlist,client); //remove users from all channel;
    }
    arraylist_remove(clientList,client);

    freeOutbuf(client);
    arraylist_free(client->chanlist);

    close(client->sock);
    FD_CLR(client->sock, &master_set);
    FD_CLR(client->sock, &write_set);

    free(client);
}

