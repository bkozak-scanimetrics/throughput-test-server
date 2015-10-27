#ifndef _SERVER_STR_STUFF_H_
#define _SERVER_STR_STUFF_H_

/*******************************************************************************
*                                   INCLUDES                                   *
*******************************************************************************/
#include <netinet/in.h>
#include <sys/socket.h>

/*******************************************************************************
*                              FUNCTION PROTOTYPES                             *
*******************************************************************************/
int readSockLine(int sockd, char* buffer, int bufSize);
void stripInPlace(char* str, int len);
char* getStrAddrIPv6(struct sockaddr_in6* clientInfo);
int strToPort(in_port_t* port,const char* str);
#endif //_SERVER_STR_STUFF_H_
