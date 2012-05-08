#ifndef _COMMON_H_
#define _COMMON_H_

#include <sys/types.h>
#include <netinet/in.h>
#include "arraylist.h"


/*
  constants
*/
#undef TRUE
#define TRUE 1

#undef FALSE
#define FALSE 0


#define MAX_CLIENTS 512
#define MAX_MSG_TOKENS 10
#define MAX_MSG_LEN 512
#define MAX_USERNAME 32
#define MAX_HOSTNAME 512
#define MAX_SERVERNAME 512
#define MAX_REALNAME 512
#define MAX_CHANNAME 512

#define CLIENT_GET(LIST,INDEX) ((client_t *)arraylist_get((LIST),(INDEX)))
#define CHANNEL_GET(LIST,INDEX) ((channel_t *)arraylist_get((LIST),(INDEX)))

#define INIT_STRING(STRING) (strcpy(STRING,""))

typedef enum {
    ERR_INVALID = 1,
    ERR_NOSUCHNICK = 401,
    ERR_NOSUCHCHANNEL = 403,
    ERR_NORECIPIENT = 411,
    ERR_NOTEXTTOSEND = 412,
    ERR_UNKNOWNCOMMAND = 421,
    ERR_ERRONEOUSNICKNAME = 432,
    ERR_NICKNAMEINUSE = 433,
    ERR_NONICKNAMEGIVEN = 431,
    ERR_NOTONCHANNEL = 442,
    ERR_NOLOGIN = 444,
    ERR_NOTREGISTERED = 451,
    ERR_NEEDMOREPARAMS = 461,
    ERR_ALREADYREGISTRED = 462
} err_t;

typedef enum {
    RPL_NONE = 300,
    RPL_USERHOST = 302,
    RPL_LISTSTART = 321,
    RPL_LIST = 322,
    RPL_LISTEND = 323,
    RPL_WHOREPLY = 352,
    RPL_ENDOFWHO = 315,
    RPL_NAMREPLY = 353,
    RPL_ENDOFNAMES = 366,
    RPL_MOTDSTART = 375,
    RPL_MOTD = 372,
    RPL_ENDOFMOTD = 376
} rpl_t;



typedef struct {
    int sock;
    struct sockaddr_storage cliaddr; /*modified to handle both IPv4 and IPv6. */
    unsigned inbuf_size;
    int registered;
    unsigned outbuf_offset;
    Arraylist outbuf; /* array of char* lines to be send over */
    char hostname[MAX_HOSTNAME+1];
    char servername[MAX_SERVERNAME+1];
    char user[MAX_USERNAME+1];
    char nick[MAX_USERNAME+1];
    char realname[MAX_REALNAME+1];
    char inbuf[MAX_MSG_LEN+1];
    Arraylist chanlist;
} client_t;


typedef struct {
    char name[MAX_CHANNAME+1];
    char topic[MAX_MSG_LEN+1];
    char key[MAX_CHANNAME+1];
    Arraylist userlist;
} channel_t;

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
char **splitByDelimStr(const char *buf, const char *delimStr, int *numTokens, char **ptrToTrailing);

/** Function freeTokens
 *  frees TokenArray returned by splitByDelimStr
 */
void freeTokens(char ***ptrToTokenArr, int numTokens);

int findClientIndexBySockFD(Arraylist list, int sockfd);
int findClientIndexByNick(Arraylist clientList, char *nickname);
int findChannelIndexByChanname(Arraylist chanList, char *channame);
int addClientToList(Arraylist list, char *servername, int sockfd, void *remoteaddr);

client_t *client_alloc_init(char *servername, int sockfd, void *remoteaddr);
channel_t *channel_alloc_init(client_t *creator,char *channame);

void remove_client(Arraylist clientList, int srcIndex);

void freeOutbuf(client_t *client);

#endif
