#include "irc_proto.h"
#include "debug.h"

#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include "common.h"
#include "arraylist.h"

#include "message.h"

#define MAX_COMMAND 16

/* You'll want to define the CMD_ARGS to match up with how you
* keep track of clients.  Probably add a few args...
* The command handler functions will look like
* void cmd_nick(CMD_ARGS)
* e.g., void cmd_nick(your_client_thingy *c, char *prefix, ...)
* or however you set it up.
*/

#define CMD_ARGS Arraylist clientList, int srcIndex, Arraylist channelList, char *servername, char *prefix, char **params, int n_params

typedef void (*cmd_handler_t)(CMD_ARGS);
#define COMMAND(cmd_name) void cmd_name(CMD_ARGS)


struct dispatch {
  char cmd[MAX_COMMAND];
  int needreg; /* Must the user be registered to issue this cmd? */
  int minparams; /* send NEEDMOREPARAMS if < this many params */
  cmd_handler_t handler;
};


#define NELMS(array) (sizeof(array) / sizeof(array[0]))

/* Define the command handlers here.  This is just a quick macro
* to make it easy to set things up */
COMMAND(cmd_nick);
COMMAND(cmd_user);
COMMAND(cmd_quit);
COMMAND(cmd_join);
COMMAND(cmd_part);
COMMAND(cmd_list);
COMMAND(cmd_privmsg);
COMMAND(cmd_who);
/* Fill in the blanks */

/* Dispatch table.  "reg" means "user must be registered in order
* to call this function".  "#param" is the # of parameters that
* the command requires.  It may take more optional parameters.
*/
struct dispatch cmds[] = {
  /* cmd,    reg  #parm  function */
  { "NICK",    0, 0, cmd_nick },
  { "USER",    0, 4, cmd_user },
  { "QUIT",    1, 0, cmd_quit },
  { "JOIN",    1, 1, cmd_join },
  { "PART",    1, 1, cmd_part },
  { "LIST",    1, 0, cmd_list },
  { "PRIVMSG", 1, 0, cmd_privmsg },
  { "WHO",     1, 0, cmd_who },
  /* Fill in the blanks... */
};

/* Handle a command line.  NOTE:  You will probably want to
* modify the way this function is called to pass in a client
* pointer or a table pointer or something of that nature
* so you know who to dispatch on...
* Mostly, this is here to do the parsing and dispatching
* for you
*
* This function takes a single line of text.  You MUST have
* ensured that it's a complete line (i.e., don't just pass
* it the result of calling read()).
* Strip the trailing newline off before calling this function.
*/
void handle_line_temp(Arraylist clientList, int srcIndex, char *servername, char *line){
  int lineLen = strlen(line);
  char *copy = malloc( sizeof(char) * (lineLen + 2 + 1));

  memcpy(copy,line,lineLen);
  strcpy(copy+lineLen,"\r\n");

  client_t *client = arraylist_get(clientList, srcIndex);
  arraylist_add(client->outbuf,copy);
}
void handle_line(Arraylist clientList, int srcIndex, Arraylist channelList, char *servername, char *line)
{
  char *prefix = NULL, *command, *pstart, *params[MAX_MSG_TOKENS];
  int n_params = 0;
  char *trailing = NULL;

  client_t *sender = arraylist_get(clientList,srcIndex);

  DPRINTF(DEBUG_INPUT, "Handling line: %s\n", line);
  command = line;
  if (*line == ':') {
    prefix = ++line;
    command = strchr(prefix, ' ');
  }
  if (!command || *command == '\0') {
    /* Send an unknown command error! */
    //some_of_your_code_better_go_here();
    params[0] = "No Command Specified";
    sendNumericReply(sender, servername, ERR_UNKNOWNCOMMAND, params, 1);
    return;
  }

  while (*command == ' ') {
    *command++ = 0;
  }
  if (*command == '\0') {
    //and_more_of_your_code_should_go_here();
    /* Send an unknown command error! */
    params[0] = "No Command Specified";
    sendNumericReply(sender, servername, ERR_UNKNOWNCOMMAND, params, 1);
    return;
  }
  pstart = strchr(command, ' ');
  if (pstart) {
    while (*pstart == ' ') {
      *pstart++ = '\0';
    }
    if (*pstart == ':') {
      trailing = pstart;
    } else {
      trailing = strstr(pstart, " :");
    }
    if (trailing) {
      while (*trailing == ' ')
      *trailing++ = 0;
      if (*trailing == ':')
      *trailing++ = 0;
    }

    do {
      if (*pstart != '\0') {
        params[n_params++] = pstart;
      } else {
        break;
      }
      pstart = strchr(pstart, ' ');
      if (pstart) {
        while (*pstart == ' ') {
          *pstart++ = '\0';
        }
      }
    } while (pstart != NULL && n_params < MAX_MSG_TOKENS);
  }

  if (trailing && n_params < MAX_MSG_TOKENS) {
    params[n_params++] = trailing;
  }

  DPRINTF(DEBUG_INPUT, "Prefix:  %s\nCommand: %s\nParams (%d):\n",
  prefix ? prefix : "<none>", command, n_params);
  int i;
  for (i = 0; i < n_params; i++) {
    DPRINTF(DEBUG_INPUT, "   %s\n", params[i]);
  }
  DPRINTF(DEBUG_INPUT, "\n");

  for (i = 0; i < NELMS(cmds); i++) {
    if (!strcasecmp(cmds[i].cmd, command)) {
      if (cmds[i].needreg && !(sender->registered) ) {
        params[0] = "You have not registered";
        sendNumericReply(sender, servername, ERR_NOTREGISTERED, params, 1);
        return;
      } else if (n_params < cmds[i].minparams) {
        params[0] = command;
        params[1] = "Not enough parameters";
        sendNumericReply(sender, servername, ERR_NEEDMOREPARAMS, params, 2);
        return;
      } else {
        (*cmds[i].handler)(clientList, srcIndex, channelList, servername, prefix, params, n_params);
      }
      break;
    }
  }
  if (i == NELMS(cmds)) {
    /* ERROR - unknown command! */
    //yet_again_you_should_put_code_here();
    params[0] = command;
    params[1] = "Unknown Command";
    sendNumericReply(sender, servername, ERR_UNKNOWNCOMMAND, params, 2);
  }
}



/* Command handlers */
/* MODIFY to take the arguments you specified above! */
void cmd_nick(CMD_ARGS)
{
  int i;
  client_t *sender = CLIENT_GET(clientList,srcIndex);
  char *messageArgs[MAX_MSG_TOKENS];

  /* ERR_NONICKNAMEGIVEN */
  if (n_params == 0){
    messageArgs[0] = "No nickname given";
    sendNumericReply(sender, servername, ERR_NONICKNAMEGIVEN, messageArgs, 1);
    return;
  }

  /* check validity of specified nick */
  if (!isValidNick(params[0])){
    messageArgs[0] = params[0];
    messageArgs[1] = "Erroneus nickname";
    sendNumericReply(sender, servername, ERR_ERRONEOUSNICKNAME, messageArgs, 2);
    return;
  }

  /* look for duplicate nick names */
  int numClient = arraylist_size(clientList);
  for (i=0;i<numClient;i++){
    client_t *other = CLIENT_GET(clientList,i);
    if (strcasecmp(params[0],other->nick)){
      messageArgs[0] = params[0];
      messageArgs[1] = "Nickname is already in use";
      sendNumericReply(sender, servername, ERR_NICKNAMEINUSE, messageArgs, 2);
      return;
    }
  }


  /* registered and in a channel. i.e. Nick change situation*/
  if (sender->registered && (arraylist_size(sender->chanlist) != 0)){
    char buf[MAX_CONTENT_LENGTH+2];
    if ( snprintf(buf,sizeof buf,":%s!%s@%s NICK %s",sender->nick,sender->user,sender->servername,params[0]) > MAX_CONTENT_LENGTH){
      DPRINTF(DEBUG_ERRS, "cmd_nick: nick change broadcast message too large\n");
      return;
    }
    for (i=0;i<arraylist_size(sender->chanlist);i++){
      sendChannelBroadcast(sender, CHANNEL_GET(sender->chanlist,i) , FALSE, buf);
    }
  }

  /* add nick */
  strcpy(sender->nick,params[0]);

  /* now registered case */
  if ((sender->registered == FALSE) && (strlen(sender->user) != 0)){
    sender->registered = TRUE;
    sendMOTD(sender,servername);
  }
}

void cmd_user(CMD_ARGS)
{
  char *messageArgs[MAX_MSG_TOKENS];
  /* Arraylist clientList, int srcIndex, char *servername, char *prefix, char **params, int n_params */
  client_t *sender = CLIENT_GET(clientList,srcIndex);
  if (sender->user[0] != '\0'){
    messageArgs[0] = "You may not register";
    sendNumericReply(sender, servername, ERR_ALREADYREGISTRED, messageArgs, 1);
    return;
  }

  strncpy(sender->user, params[0], MAX_USERNAME - 1);
  sender->user[MAX_USERNAME-1] = '\0';
  strncpy(sender->realname, params[3], MAX_REALNAME - 1);

  if (!sender->registered && sender->nick[0] != '\0'){
    sender->registered = TRUE;
    sendMOTD(sender,servername);
  }
}




void cmd_quit(CMD_ARGS)
{
  int i;
  client_t *sender = CLIENT_GET(clientList,srcIndex);

  DPRINTF(DEBUG_CLIENTS,"client %d entered cmd_quit\n",sender->sock);

  if (sender->registered && arraylist_size(sender->chanlist) != 0){
    char buf[MAX_CONTENT_LENGTH+1];
    snprintf(buf,sizeof buf,":%s!%s@%s QUIT %s",sender->nick,sender->user,sender->servername,params[0]);
    for (i=0;i<arraylist_size(sender->chanlist);i++){
      sendChannelBroadcast(sender, CHANNEL_GET(sender->chanlist,i) , FALSE, buf);
    }
  }
  remove_client(clientList,srcIndex);
}

void cmd_join(CMD_ARGS)
{
  int i=0;
  client_t *sender = CLIENT_GET(clientList,srcIndex);
  int numTokens;
  char *lastStr;
  char **tokens = splitByDelimStr(params[0],",",&numTokens,&lastStr);
  char *messageArgs[MAX_MSG_TOKENS];

  /* only one channel allowed for now */
  /*for (i = 0; i < numTokens; i++){*/
    int chanIndex = findChannelIndexByChanname(channelList,tokens[i]);

    /* check if the user is already in that channel */
    if (arraylist_index_of(sender->chanlist,CHANNEL_GET(channelList,chanIndex)) >= 0){
        freeTokens(&tokens,numTokens);
        return;
        /* continue; // if more than one channel allowed */
    }

    if (chanIndex == -1){
      /* no existing channel with that name */
      /* verify validity of channame */
      if (!isValidChanname(tokens[i])){
        messageArgs[0] = tokens[i];
        messageArgs[1] = "No such channel";
        sendNumericReply(sender, servername, ERR_NOSUCHCHANNEL, messageArgs, 2);
        return;
      }
      /* create channel */
      channel_t *newChannel = channel_alloc_init(tokens[i]);
      if (newChannel){
        chanIndex = arraylist_add(channelList,newChannel);
      }

      if (!newChannel || chanIndex < 0){
        messageArgs[0] = tokens[i];
        messageArgs[1] = "Cannot join channel (+l)";
        sendNumericReply(sender, servername, ERR_TOOMANYCHANNELS, messageArgs, 2);
        return;
      }

    }

    /* add user to channel */
    arraylist_add(CHANNEL_GET(channelList,chanIndex)->userlist,sender);

    if (arraylist_size(sender->chanlist) != 0){
        /* remove user from previous channel */
        /* only applies one channel allowed condition */

    }


 /* }*/


  freeTokens(&tokens,numTokens);
}
void cmd_part(CMD_ARGS)
{
}
void cmd_list(CMD_ARGS)
{
}


void cmd_privmsg(CMD_ARGS)
{
}
void cmd_who(CMD_ARGS)
{

}
/* And so on */

