#include "message.h"
#include "common.h"
#include "debug.h"
#include "arraylist.h"
#include <string.h>
#include <ctype.h>

/* prepareMessage: adds deep copy of null terminated message + "\r\n" onto receiver's out queue */
int prepareMessage(client_t *receiver, char *message){
  char *toSend = malloc (sizeof(char) * (strlen(message)+2 + 1) );

  if (!toSend){
    DPRINTF(DEBUG_ERRS,"Failed to create copy of the message to client %d\n",receiver->sock);
    return -1;
  }

  sprintf(toSend,"%s\r\n",message);

  if (arraylist_add(receiver->outbuf,toSend) < 0){
    DPRINTF(DEBUG_ERRS,"Failed to add a message onto outbuf of client %d\n",receiver->sock);
    return -1;
  }

  return 0;
}
/* sendNumericReply: creates message with numeric reply code and add it onto receiver's out queue */
int sendNumericReply(client_t *receiver, char *servername, int replyCode, char **texts, int n_texts){
  char buf[MAX_CONTENT_LENGTH + 2]; /* large enough to hold message */
  int i;
  int numWritten;

  numWritten = snprintf(buf,sizeof buf, ":%s %d", servername, replyCode);
  if (numWritten > MAX_CONTENT_LENGTH){
    return -1; /* not enought to fit even necessary part */
  }

  for (i = 0; i < n_texts - 1; i++){
    numWritten += snprintf(buf + numWritten, sizeof buf  - numWritten, " %s", texts[i]);

    if ( numWritten > MAX_CONTENT_LENGTH){
      DPRINTF(DEBUG_ERRS, "sendNumericReply: Message Too Long. Truncating message %d for client %d\n", replyCode, receiver->sock);
      /* just send it. This is okey since errorcode should have been in the queue already. */
      buf[MAX_CONTENT_LENGTH] = '\0';
      return prepareMessage(receiver,buf);
    }
  }

  if (strchr(texts[n_texts-1],' ')){
    numWritten += snprintf(buf + numWritten, sizeof buf - numWritten, " :%s", texts[i]);
  }
  else{
    numWritten += snprintf(buf + numWritten, sizeof buf - numWritten, " %s", texts[i]);
  }
  if ( numWritten > MAX_CONTENT_LENGTH){
    DPRINTF(DEBUG_ERRS, "sendNumericReply: Message Too Long. Truncating message %d for client %d\n", replyCode, receiver->sock);
    /* just send it. This is okey since errorcode should have been in the queue already. */
    buf[MAX_CONTENT_LENGTH] = '\0';
    return prepareMessage(receiver,buf);
  }
  DPRINTF(DEBUG_COMMANDS,"sendNumericReply: ready to send message '%s' to client %d\n", buf, receiver->sock);
  return prepareMessage(receiver,buf);
}


int sendMOTD(client_t *receiver, char *servername){
  char buf[MAX_MSG_LEN + 1];

  snprintf(buf,MAX_MSG_LEN+1,"- %s Message of the day - ",servername);
  if (sendNumericReply(receiver, servername, RPL_MOTDSTART, (char **)&buf, 1) < 0){
    DPRINTF(DEBUG_ERRS,"cmd_nick: failed to add RPL_MOTDSTART message to client\n");
    return -1;
  }
  snprintf(buf,MAX_MSG_LEN+1,"- %s",servername);
  if (!sendNumericReply(receiver, servername, RPL_MOTD, (char **)&buf, 1)){
    DPRINTF(DEBUG_ERRS,"cmd_nick: failed to add RPL_MOTD message to client\n");
    return -1;
  }
  snprintf(buf,MAX_MSG_LEN+1,"End of /MOTD command");
  if (!sendNumericReply(receiver, servername, RPL_ENDOFMOTD, (char **)&buf, 1)){
    DPRINTF(DEBUG_ERRS,"cmd_nick: failed to add RPL_ENDOFMOTD message to client\n");
    return -1;
  }
  return 0;
}

Boolean isValidNick(char *nick){
  int i;

  if (strlen(nick) > MAX_USERNAME)
    return FALSE;

  if (!isalpha((int)nick[0]))
    return FALSE;

  for (i = 0; i < strlen(nick); i++){
    if (!isalnum((int)nick[i]) && !(nick[i] >= '-' && nick[i] <= '^') && nick[i] != '`' && nick[i] != '{' && nick[i] != '}'){
      return FALSE;
    }
  }

  return TRUE;
}

Boolean isValidChanname(char *channame){
    int i;
    if (channame[0] != '#' && channame[0] != '&'){
        return FALSE;
    }

    if (strlen(channame) > MAX_CHANNAME)
        return FALSE;

    for (int i; i < strlen(channame) ; i++){
        /* parser will prevent SPACE, NUL, CR, LF, and comma. Thus check for bell only */
        if (channame[0] == 0x7 )
            return FALSE;

    }
    return TRUE;
}

int sendChannelBroadcast(client_t *sender, channel_t *channel, Boolean senderreceive, char *message){
  int i;
  for (i = 0; i < arraylist_size(channel->userlist); i++){
    if (senderreceive || (arraylist_get(channel->userlist,i) != sender) ){
      prepareMessage(arraylist_get(channel->userlist,i), message); /* ignore return value */
    }
  }
  return 0;
}
