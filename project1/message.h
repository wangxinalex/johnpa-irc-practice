#ifndef MESSAGE_H
#define MESSAGE_H

#include "common.h"

/** MESSAGE_H
 *
 *  Collection of utility functions for generating messages and putting then onto out queue
 *
 **/

 #define MAX_CONTENT_LENGTH (MAX_MSG_LEN-2)  /*size of messsage without CRLF */

 /* prepareMessage: adds deep copy of null terminated buf + "\r\n" onto receiver's out queue */
int prepareMessage(client_t *receiver, char *message);
/* sendNumericReply: creates message with numeric reply code and add it onto receiver's out queue */
int sendNumericReply(client_t *receiver, char *servername, int replyCode, char **texts, int n_texts);
/* sendMOTD: send RPL_MOTDSTART, RPL_MOTD, RPL_MOTDEND */
int sendMOTD(client_t *receiver, char *servername);
/* isValidNick: returns TRUE if valid, FALSE if not valid */
Boolean isValidNick(char *nick);
/* sendChannelBroadcast: send message to Channel.
 *                       sendereceive parameter specifies whether sender should receive the message too */
int sendChannelBroadcast(client_t *sender, channel_t *channame, Boolean senderreceive, char *message);


int sendMessage(Arraylist clientList, client_t *sender, char *destination, char *message);
int sendUser(Arraylist clientList, client_t *sender, char *channame, char *message);



#endif
