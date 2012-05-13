
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
 *  numTokenPtr: number of tokens generated.
 *  lastTokenTerminatedPtr: whether last token was terminated by delimeter or not
 *
 *  Return:
 *    array of deep-copied strings. Array and strings should be freed after use,
 *    although recommended to call freeTokens(&array,numTokens);
 *    NULL on error. value of *numTokens and *lastTokenTerminated are undefined.
 **/
char **splitByDelimStr(const char *buf, const char *delimStr, int *numTokenPtr, int *lastTokenTerminatedPtr){

  char **retArr;
  int i;
  const char *tmp, *nextToken;
  size_t tokenSize;

  int numToken;
  int lastTokenTerminated = 0;

  if (!buf || !delimStr){
    /* handle error gracefully */
    DPRINTF(DEBUG_ERRS,"splitByCRLF: NULL input detected\n");
    return NULL;
  }

  /* count tokens */
  numToken = 0; /* number of Tokens = number of delimeters + (last Token was delimeter at end? 0: 1) */

  nextToken = buf;
  do{
      if (nextToken[0] == '\0' ){
        lastTokenTerminated = ( nextToken != 0);
        break;
      }
      numToken++;
      tmp = strpbrk(nextToken, delimStr);
      if (!tmp){
        lastTokenTerminated = 0;
        break;
      }
      nextToken = tmp;
      while (strchr(delimStr, *(nextToken++)))
          ;
  } while (1);
  if (numToken == 0){
    return NULL;
  }
  /* Allocate token array */
  retArr = malloc( sizeof(char *) *  (numToken));

  if (!retArr){
    DPRINTF(DEBUG_ERRS,"splitByDelimStr: retArr malloc failed\n");
    return NULL;
  }

  for (i=0,nextToken=buf; i < numToken-1; i++){
    char *tokenEnd = strpbrk(nextToken,delimStr); /* guarantee to succeed */
    tokenSize = tokenEnd - nextToken;
    retArr[i] = malloc (sizeof(char) * (tokenSize + 1) );
    if (!retArr[i]){
        DPRINTF(DEBUG_ERRS,"splitByDelimStr: retArr[%d] malloc failed\n",i);
        freeTokens(&retArr,i);
        return NULL;
    }
    memcpy(retArr[i],nextToken,tokenSize);
    retArr[i][tokenSize] = '\0';
    nextToken = tokenEnd+1;
    while (strchr(delimStr, *(nextToken++)))
        ;
  }
  /* handle last token */
  tokenSize =  (lastTokenTerminated) ? strlen(nextToken) : strpbrk(nextToken,delimStr) - nextToken;

  retArr[numToken-1] = malloc(sizeof(char) * tokenSize);
  memcpy(retArr[numToken-1],nextToken,tokenSize);
  retArr[numToken-1][tokenSize] = '\0';

  if (numTokenPtr)
      *numTokenPtr = numToken;
  if (lastTokenTerminatedPtr)
      *lastTokenTerminatedPtr = lastTokenTerminated;
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

int addClientToList(Arraylist list, char *servername, int sockfd, struct sockaddr_storage *remoteaddr){
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

client_t *client_alloc_init(char *servername, int sockfd, struct sockaddr_storage *remoteaddr){
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
    /* drop the client?? */
    /*
        arraylist_free(newClient->outbuf);
        arraylist_free(newClient->chanlist);
        free(newClient);
        return NULL;
    */
  }
  newClient->hopcount = 0;
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

