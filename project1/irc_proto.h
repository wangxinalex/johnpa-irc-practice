#ifndef _IRC_PROTO_H_
#define _IRC_PROTO_H_


#include "arraylist.h"

void handle_line(Arraylist clientList, int srcIndex, char *servername, char *line);



#endif /* _IRC_PROTO_H_ */
