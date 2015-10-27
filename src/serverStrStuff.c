/*
 * Copyright (c) 2015, Scanimetrics - http://www.scanimetrics.com
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*******************************************************************************
* Does some string related stuff for the test server                           *
*******************************************************************************/


/*******************************************************************************
*                                   INCLUDES                                   *
*******************************************************************************/
#include "serverStrStuff.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
/*******************************************************************************
*                             FUNCTION DEFINITIONS                             *
*******************************************************************************/
/**
* Read a line of text from a socket
*
* Reads from a socket until a newline is encountered or bufSize-1 bytes are
* read or until a read from the socket returns zero bytes. The trailing new
* line (if it was encountered) will be included in the output.
*
* A terminating NULL byte will be added to the buffer unless the buffer size
* is exactly zero (in which case no data will be written to the buffer).
*
*
* Args:
* sockd - open socket to read from
* buffer - buffer to write data into
* bufSize - the size of the buffer given
*
*
* Returns:
* The number of bytes read (can be zero if the socket was closed) or a negative
* number on error.
**/
int readSockLine(int sockd, char* buffer, int bufSize){
	int numRead = 0;

	int rc;
	char c;

	while(1){
		if(numRead >= (bufSize-1)){
			break;
		}

		rc = read(sockd, &c, 1);
		if( rc == 1){
			buffer[numRead] = c;
			numRead += 1;

			if(c == '\n'){
				break;
			}
		}
		else if(rc < 0){
			return -1;
		}
		else if(rc == 0){
			break;
		}
	}

	if(bufSize != 0){
		buffer[numRead] = '\0';
	}

    return numRead;
}

/**
* Strips leading and trailing whitespace from a string in place.
*
* This function assumes that the give string is terminated by a NULL byte. If
* this is not the case, the function may not work as expected.
*
* Args:
* str - the string to modify
* len - the total length of the string (including trailing NULL byte).
*
* Returns:
* Nothing
**/
void stripInPlace(char* str, int len){

	//find first non-space char position
	int firstBlack = 0;
	for(int i = 0; i < len; i++){

		if(isspace(str[i])){
			firstBlack += 1;
		}
		else{
			break;
		}
	}

	//shift the string down to overwrite the spaces
	//a NULL char is assumed to be at the last
	//position
	for(int n = firstBlack; n < len; n++){
		str[n-firstBlack] = str[n];
	}

	//overwrite trailing whitespace with nulls
	for(int z = len-1-firstBlack; z>=0; z--){

		if(isspace(str[z]) || !str[z]){
			str[z] = '\0';
		}
		else{
			break;
		}
	}
}

/**
* Returns a string representing an IPV6 address
*
* The string is allocated on the heap and should be freed after using.
*
* Args:
* clientInfo - structure containing information on the connected client
*
* Returns:
* A string representing the client's IPV6 address.
**/
char* getStrAddrIPv6(struct sockaddr_in6* clientInfo){

	char* buf = calloc(INET6_ADDRSTRLEN,sizeof(char));

	inet_ntop(AF_INET6, &(clientInfo->sin6_addr),buf,INET6_ADDRSTRLEN);

	return buf;
}
/**
* Converts the given string to a valid port number
*
* Returns -2 on system error (in which case errno will be set), -1 if the given
* string does not represent a valid port number and zero otherwise.
*
* Args:
* port - pointer to in_port_t which will be loaded with the converted port num
* str - string to convert
*
* Returns:
* Zero on success and non-zero on error.
**/
int strToPort(in_port_t* port,const char* str){

	int len = strlen(str)+1;

	char* tmpStr = malloc(len);
	if(!tmpStr){
		return -2;
	}
	memcpy(tmpStr,str,len);
	stripInPlace(tmpStr,len);

	char* endptr = NULL;
	int retVal = 0;

	long tmp = strtol(tmpStr,&endptr,10);
	if(*endptr){
		retVal = -1;
	}
	else if( (tmp < 0) || (tmp> ((1<<16) -1) ) ){
		retVal = -1;
	}
	else{
		*port = tmp;
	}

	free(tmpStr);

	return retVal;
}
