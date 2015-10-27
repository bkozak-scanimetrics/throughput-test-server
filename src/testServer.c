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
/******************************************************************************
* This source file implements a simple TCP server for testing openwsn         *
*                                                                             *
* The intention is to read packets through this server in order to measure    *
* the throughput of end devices.                                              *
******************************************************************************/


/******************************************************************************
*                                   INCLUDES                                  *
******************************************************************************/
#include "testServer.h"
#include "serverStrStuff.h"
#include "cleanExit.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>
#include <stdbool.h>
#include <time.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <assert.h>
/******************************************************************************
*                                   DEFINES                                   *
******************************************************************************/
#define PRINT_PROGRESS_PERSTAR 64
#define PRINT_PROGRESS_PERLINE 79

#define THROUGHPUT_BUF_SIZE 2048

#define UDP_REPLY_MIN_SIZE 4
/******************************************************************************
*                                     DATA                                    *
******************************************************************************/
static const char* HELP="Program for running a simple ipv6 server\n"
"\n"
"Usage:\n"
"%s %s\n"
"Options:\n"
"-e,--echo        Run an echo server which echos lines of text back to the\n"
"                 client. Server only accepts one connection and stops when\n"
"                 the keyword \"exit\" is read on its own line.\n"
"-t,--throughput  Run a throughput server which measures throughput of some\n"
"                 client which is streaming data. Closes the connection when\n"
"                 a zero byte is read from the client.\n"
"-d,--udp         Run server as a udp server. By default we run as tcp\n"
"                 server. The echo server will not run in udp mode.\n"
"-s,--tcp         Run server as a tcp server. This is the default. The echo\n"
"                 server will always run as a tcp server\n"
"-p,--pingpong    Run UDP throughput server in ping-pong mode. Causes the\n"
"                 test server to send reply packets to confirm every packet\n"
"                 recieved by the server. Does nothing if not in UDP\n"
"                 throughput server mode. By default, replies are not sent.\n"
"--port pnum      Run the server on the given port. By default, the OS will\n"
"                 select an open port.\n";

static const char* USAGE="[-h] [-e | -t] [-s | -d] [-p] [--port pnum]";

static const char* ARG_ERR="Try -h or --help to get help text";
/******************************************************************************
*                              FUNCTION PROTOTYPES                            *
******************************************************************************/
static int listenAllIPv6(u_short* port,bool tcp);
static int waitForConnectIPv6(int list_s,struct sockaddr_in6* clientInfo);
static int echoServer(int conn_s);
static int throughputServerTCP(int conn_s);
static int throughputServerUDP(int sockfd,bool pingpong);
static double calcThroughput(uint32_t n,struct timespec t0,struct timespec t1);
static bool hasSequence(uint8_t* buf,int len,const uint8_t* seq,int seqLen);
static void printProgress(bool done, unsigned byteCount,unsigned change);
static uint32_t extract_packet_number(uint8_t* buf, int len,int* err);
static int construct_reply(uint8_t* pkt,int len,uint8_t* reply);
/******************************************************************************
*                             FUNCTION DEFINITIONS                            *
******************************************************************************/
/**
* Prints out progress using stars to represent data packets
*
* Args:
* done - set true once all input has been recieved
* byteCount - the number of bytes recieved so far
* change - the number of bytes which were just read in
*
* Returns:
* void
**/
static void printProgress(bool done, unsigned byteCount,unsigned change){
	unsigned lineMult = 1+PRINT_PROGRESS_PERSTAR*PRINT_PROGRESS_PERLINE;

	if(done){
		putchar('\n');
		return;
	}

	if(!change){
		return;
	}

	for(unsigned n = (byteCount+1-change); n <= byteCount; n++){
		if(!(n%lineMult)){
			putchar('\n');
		}
		if(!(n%PRINT_PROGRESS_PERSTAR)){
			putchar('*');
		}
	}
	fflush(stdout);
}
/**
* Returns throughput in kib/s
*
* Calculates the throughput represented by the given byte count and start and
* end times.
*
* Args:
* bc - the byte count
* t0 - the time at which byte transfer started
* t1 - the time at which byte transfer stopped
*
* Returns:
* The throughput in kib/s as a double
**/
static double calcThroughput(uint32_t n,struct timespec t0,struct timespec t1){
	double kib = ((double)n)/(1024.0/8.0);
	double secondsPassed = (double)(t1.tv_sec - t0.tv_sec);
	double nanosPassed = (double)(t1.tv_nsec - t0.tv_nsec);


	double throughput;
	if( !secondsPassed && !nanosPassed){
		throughput = 0.0;
	}
	else{
		throughput = kib/(secondsPassed+(0.000000001*nanosPassed));
	}

	return throughput;
}
/**
* Extracts the packet number from a packet buffer
*
* Args:
* buf - buffer containing the packet
* len - length of the packet buffer
* err - pointer to integer; if not NULL, will be set non-zero on failure to
* 	extract packet number (in which case 0xFFFFFFFF will be returned).
*
* Returns:
* 	The number of the packet found in the buffer. On error will return
* 	0xFFFFFFFF (but can also return this legitimatley, so *err needs to be
* 	checked).
**/
static uint32_t extract_packet_number(uint8_t* buf, int len,int* err){
	uint32_t result = 0;

	if(len < 4) {
		if(err) {
			*err = 1;
		}
		return 0xFFFFFFFF;
	}

	result |= buf[0] << 0 ;
	result |= buf[1] << 8 ;
	result |= buf[2] << 16;
	result |= buf[3] << 24;

	return result;
}
/**
* Search for a sequence of bytes within a byte buffer
*
* Args:
* buf - buffer to search in
* len - length of buffer we are searching in
* seq - sequence of bytes we are searching for.
* seqLen - length of sequence of bytes to search for.
*
* Returns:
* True if the sub sequence is found within the buffer and false otherwise
**/
static bool hasSequence(uint8_t* buf,int len,const uint8_t* seq,int seqLen){
	for(int i = len-seqLen; i>=0; i--){

		for(int n = 0; n < seqLen; n++){
			if(buf[i+n] != seq[n]){
				break;
			}
			else if(n == (seqLen-1)){
				return true;
			}
		}
	}

	return false;
}
/**
* Constructs reply for the given packet
*
* Fills the reply buffer with the reply to be sent in response to the given
* packet. The size of the reply packet in bytes is returned (zero returned on
* error).
*
* Args:
* pkt - the packet to reply to
* len - length of the given packet
* reply - space in which to construct the reply packet. Must be at least
* 	UDP_REPLY_MIN_SIZE bytes long.
*
* Returns:
* zero on error and the size of the reply packet on success.
**/
static int construct_reply(uint8_t* pkt,int len,uint8_t* reply){
	int err = 0;
	uint32_t seqno = extract_packet_number(pkt,len,&err);

	if(err) {
		return 0;
	}

	reply[0] = (seqno >> 0 )&0xFF;
	reply[1] = (seqno >> 8 )&0xFF;
	reply[2] = (seqno >> 16)&0xFF;
	reply[3] = (seqno >> 24)&0xFF;

	return 4;
}
/**
* Create an IPV6 listening socket which listens on all interfaces
*
* Will call exit on fatal error.
*
* Args:
* port - the port number to listen on (set zero to have the OS choose).
* tcp - set true to open tcp connection and false for udp connection
*
* Returns:
* The listening socket
**/
static int listenAllIPv6(u_short* port, bool tcp){
	int list_s;
	int ret;

	struct sockaddr_in6 servaddr;

	if(tcp){
		list_s = socket(AF_INET6, SOCK_STREAM, 0);
	}
	else{
		list_s = socket(AF_INET6, SOCK_DGRAM, 0);
	}

	if ( list_s < 0 ) {
	 	 fprintf(stderr, "Error creating listening socket.\n");
	 	 exit(EXIT_FAILURE);
	}

	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin6_family = AF_INET6;
	servaddr.sin6_addr = in6addr_any;
	servaddr.sin6_port = htons(*port);

	ret = bind(list_s,(struct sockaddr *)&servaddr,sizeof(servaddr));

	if ( ret < 0 ) {
	 	 perror(NULL);
	 	 fprintf(stderr, "Error calling bind()\n");
	 	 exit(-1);
	 }

	 if (tcp){
	 	 if (listen(list_s, LISTENQ) < 0 ) {
	 	 	 perror(NULL);
	 	 	 fprintf(stderr, "Error calling listen()\n");
	 	 	 exit(EXIT_FAILURE);
	 	 }
	 }

	 struct sockaddr_in6 realAddr;
	 socklen_t len = sizeof(realAddr);

	 if (getsockname(list_s, (struct sockaddr *)&realAddr, &len) == -1){
	 	  perror("getsockname");
	 }
	 else{
	 	 *port = ntohs(realAddr.sin6_port);
	 }

	 return list_s;
}

/**
* Waits for a connection on an IPV6 socket
*
* Will call exit if certain errors occur.
*
* Args:
* list_s - the listening socket to listen for a connection on
* clientInfo - pointer to a struct sockaddr_in6 which will be filled with
* 	information about the connected client if it is not NULL (ignored if
* 	NULL).
*
* Returns:
* The connection socket
**/
static int waitForConnectIPv6(int list_s,struct sockaddr_in6* clientInfo){

	int conn_s;

	if(clientInfo){

		memset(clientInfo, 0, sizeof(*clientInfo));
		socklen_t ciLen = sizeof(*clientInfo);
		conn_s = accept(list_s, clientInfo, &ciLen);
	}
	else{
		conn_s = accept(list_s, NULL, NULL);
	}

	if ( conn_s < 0 ) {
		fprintf(stderr, "ECHOSERV: Error calling accept()\n");
		exit(EXIT_FAILURE);
	}

	return conn_s;
}

/**
* Implements an echo server
*
* Implements an echo server using the given socket. Will print helpful info to
* stdout. May call exit on fatal error.
*
* Will return when the keyword 'exit' is read from the socket by itself on a
* line or if the client closes its end of the connection.
*
* Args:
* conn_s - the socket to communicate on
*
* Returns:
* Zero - always.
**/
static int echoServer(int conn_s){

	char buffer[MAX_LINE+1];
	int bytesRead;

	 while ( 1 ) {

	 	 //Retrieve an input line from the connected socket
	 	 //then simply write it back to the same socket.
	 	 bytesRead =  readSockLine(conn_s, buffer, MAX_LINE+1);
	 	 if (bytesRead < 0){
	 	 	perror(NULL);
	 	 	fprintf(stderr, "Error reading from socket!\n");
	 	 	exit(-1);
	 	 }

	 	 if(bytesRead == 0){
	 	 	 printf("Connection closed by client\n");
	 	 	 break;
	 	 }

	 	 //echo what we recieved.
	 	 if (write(conn_s, buffer, bytesRead) < 0){
	 	 	 perror(NULL);
	 	 	 fprintf(stderr, "Error reading from socket!\n");
	 	 	 exit(-1);
	 	 }

	 	 stripInPlace(buffer,bytesRead+1);
	 	 printf("Echo Server: %s\n",buffer);


	 	 if(!strncmp(buffer,"exit",MAX_LINE)){
	 	 	 break;
	 	 }
	 }

	 return 0;
}
/**
* Measures throughput from a given socket.
*
* Results are printed to stdout.
*
* Args:
* sockfd - the socket to read from
*
* Returns:
* zero
**/
static int throughputServerTCP(int sockfd){

	uint8_t byte;
	uint32_t bytesRead = 0;

	struct timespec t0;
	struct timespec t1;

	int rc = read(sockfd,&byte,1);

	//take first time measurement just after the first byte arrives
	if (clock_gettime(CLOCK_MONOTONIC,&t0)){
		perror("Error reading monotonic clock!");
		exit(-1);
	}

	while ( 1 ) {

		if(rc == 1){
			bytesRead += 1;
			printProgress(false,bytesRead,1);
		}
		else if(rc < 0){
			perror("Error reading from socket!\n");
	 	 	exit(-1);
		}
		else if (rc == 0){
			break;
		}

		rc = read(sockfd,&byte,1);
	}

	if (clock_gettime(CLOCK_MONOTONIC,&t1)){
		perror("Error reading monotonic clock!");
		exit(-1);
	}

	printProgress(true,bytesRead,0);


	double throughput = calcThroughput(bytesRead,t0,t1);

	printf("Recieved %u bytes in total\n",bytesRead);
	printf("Throughput was ~ %lf kib/s\n",throughput);

	return 0;
}
/**
* Implements a UDP throughput measurement server
*
* Connects with the first client who sends packets to this server. Outputs
* results to stdout.
*
* Args:
* sockfd - the socket to operate on
* pingpong - if set true, send reply packets to sender
*
* Returns:
* Zero
**/
static int throughputServerUDP(int sockfd, bool pingpong){

	const uint8_t stopSeq[] = {0xFF,0xFF,0xFF,0xFF};

	struct sockaddr_in6 clientAddr;
	socklen_t addrlen = sizeof(clientAddr);

	uint8_t buffer[THROUGHPUT_BUF_SIZE];
	uint8_t reply[UDP_REPLY_MIN_SIZE];

	struct timespec t0;
	struct timespec t1;

	uint32_t bytesRead = 0;

	int rc = recvfrom(
			sockfd,buffer,THROUGHPUT_BUF_SIZE,MSG_TRUNC,
			&clientAddr,&addrlen
		);

	if(rc < 0){
		perror("Error reading from socket!\n");
		exit(-1);
	}

	bytesRead += rc;

	//take first time measurement just after the first byte arrives
	if (clock_gettime(CLOCK_MONOTONIC,&t0)){
		perror("Error reading monotonic clock!");
		exit(-1);
	}

	char* addrStr = getStrAddrIPv6(&clientAddr);
	printf("Incoming connection from: %s\n",addrStr);
	free(addrStr);

	if(connect(sockfd,&clientAddr,sizeof(clientAddr))){
		perror("Error connecting socket!\n");
		exit(-1);
	}

	while ( 1 ) {
		printProgress(false,bytesRead,rc);

		if(pingpong && rc) {
			int reply_len = construct_reply(buffer,rc,reply);

			if(!reply_len){
				fprintf(stderr,"Malformed packet!\n");
			} else if(write(sockfd,reply,reply_len) != reply_len) {
				perror("Error writing to socket\n");
				exit(-1);
			}
		}
		if(hasSequence(buffer,rc,stopSeq,sizeof(stopSeq))){
			break;
		}

		rc = read(sockfd,&buffer,THROUGHPUT_BUF_SIZE);

		if(rc < 0){
			perror("Error reading from socket!\n");
	 	 	exit(-1);
		} else if (rc == 0){
			break;
		}

		bytesRead += rc;
	}

	if (clock_gettime(CLOCK_MONOTONIC,&t1)){
		perror("Error reading monotonic clock!");
		exit(-1);
	}

	printProgress(true,bytesRead,0);

	double throughput = calcThroughput(bytesRead,t0,t1);

	printf("Recieved %u bytes in total\n",bytesRead);
	printf("Throughput was ~ %lf kib/s\n",throughput);

	return 0;
}
/**
* Parses command line options
*
* Args:
* argc - argument count
* argv - program arguments
*
* Returns:
* A structure containing all of the program options
**/
struct serverOpts getServerOpts(int argc, char** argv){
	enum serverMode mode = DEFAULT_SERVER_MODE;
	in_port_t port = DEFAULT_PORT_NUM;
	bool tcp = true;
	bool pingpong = false;

	bool gotMode = false;
	bool gotPort = false;
	bool gotTransport = false;

	int lopt_ind = 0;
	int c;

	const char* shopts = "hetsdp";
	struct option lopts[] = {
		{"help",0,NULL,'h'},
		{"echo",0,NULL,'e'},
		{"througput",0,NULL,'t'},
		{"tcp",0,NULL,'s'},
		{"udp",0,NULL,'d'},
		{"pingpong",0,NULL,'p'},
		{"port",1,NULL,'r'},
		{NULL, 0, NULL, 0}
	};

	while( (c = getopt_long(argc, argv,shopts,lopts,&lopt_ind)) != -1 ){

		switch(c){
		case 'h':
			printf(HELP,argv[0],USAGE);
			exit(0);
		break;
		case 'e':
		case 't':
			if(gotMode){
				fprintf(
					stderr,
					"Server mode was set multiple times!\n"
				);
				exit(-1);
			}
			gotMode = true;
			mode = (c=='e') ? ECHO_SERVER : THROUGHPUT_SERVER;
			break;
		case 's':
		case 'd':
			if(gotTransport){
				fprintf(
					stderr,
					"Transport type was set multiple "
					"times!\n"
				);
				exit(-1);
			}
			gotTransport = true;
			tcp = (c=='s');
			break;
		case 'r':
			if(gotPort){
				fprintf(
					stderr,"Port was set multiple times!\n"
				);
				exit(-1);
			}
			gotPort = true;
			 if(!!strToPort(&port,optarg)){
			 	fprintf(
			 		stderr,
					"\"%s\" is not a valid port number!\n",
					optarg
				);
				exit(-1);
			 }
			break;
		case 'p':
			pingpong = true;
			break;
		case '?':
			printf("%s %s\n",argv[0],USAGE);
			printf("%s\n",ARG_ERR);
			exit(-1);
			break;
		default:
			printf("Got unexpected char '%c' from getopt!\n",c);
			exit(-1);
		}
	}

	if(optind != argc){
		fprintf(
			stderr,"Got unexpected argument \"%s\"\n",argv[optind]
		);
		printf("%s %s\n",argv[0],USAGE);
		printf("%s\n",ARG_ERR);
		exit(-1);
	}

	struct serverOpts ret = {mode,port,tcp,pingpong};
	return ret;
}

/**
* Program entry point
*
* Args:
* argc - program argument count
* argv - program arguments
*
* Returns:
* Program exit code.
**/
int main(int argc, char** argv){

	struct serverOpts opts = getServerOpts(argc,argv);

	 u_short port = opts.port;

	 if(opts.mode == ECHO_SERVER){
	 	 int list_s = listenAllIPv6(&port,true);

	 	 printf("Creating echo server on port %d\n",port);

	 	 struct sockaddr_in6 clientInfo;
	 	 int conn_s = waitForConnectIPv6(list_s,&clientInfo);

	 	 char* addrStr = getStrAddrIPv6(&clientInfo);
	 	 printf("Incoming connection from: %s\n",addrStr);
	 	 free(addrStr);

	 	 cleanExit_add_fd(list_s);
	 	 cleanExit_add_fd(conn_s);
	 	 cleanExit_add_signal(SIGINT);

	 	 echoServer(conn_s);

	 	 cleanExit_stop();

	 	 /*  Close the connected socket  */
	 	 if ( close(conn_s) < 0 ) {
	 	 	 fprintf(stderr, "ECHOSERV: Error calling close()\n");
	 	 	 exit(EXIT_FAILURE);
	 	 }

	 	 /*  Close the listening socket  */
	 	 if ( close(list_s) < 0 ) {
	 	 	 fprintf(stderr, "ECHOSERV: Error calling close()\n");
	 	 	 exit(EXIT_FAILURE);
	 	 }
	 }
	 else if(opts.mode == THROUGHPUT_SERVER && opts.tcp){
	 	 int list_s = listenAllIPv6(&port,true);
	 	 printf("Creating throughput server on port %d\n",port);

	 	 struct sockaddr_in6 clientInfo;
	 	 int conn_s = waitForConnectIPv6(list_s,&clientInfo);

	 	 char* addrStr = getStrAddrIPv6(&clientInfo);
	 	 printf("Incoming connection from: %s\n",addrStr);
	 	 free(addrStr);

	 	 cleanExit_add_fd(list_s);
	 	 cleanExit_add_fd(conn_s);
	 	 cleanExit_add_signal(SIGINT);

	 	 throughputServerTCP(conn_s);

	 	 cleanExit_stop();

	 	 /*  Close the connected socket  */
	 	 if ( close(conn_s) < 0 ) {
	 	 	 fprintf(stderr, "ECHOSERV: Error calling close()\n");
	 	 	 exit(EXIT_FAILURE);
	 	 }

	 	 /*  Close the listening socket  */
	 	 if ( close(list_s) < 0 ) {
	 	 	 fprintf(stderr, "ECHOSERV: Error calling close()\n");
	 	 	 exit(EXIT_FAILURE);
	 	 }
	 }
	 else if(opts.mode == THROUGHPUT_SERVER && !opts.tcp){
	 	  int list_s = listenAllIPv6(&port,false);
	 	  printf("Creating UDP throughput server on port %d\n",port);

	 	 cleanExit_add_fd(list_s);
	 	 cleanExit_add_signal(SIGINT);

	 	  throughputServerUDP(list_s,opts.pingpong);

	 	  cleanExit_stop();

	 	  //wait a bit just in case some packet retries are still
	 	  //arriving
	 	  sleep(1);

	 	  if ( close(list_s) < 0 ) {
	 	 	 fprintf(stderr, "ECHOSERV: Error calling close()\n");
	 	 	 exit(EXIT_FAILURE);
	 	 }
	 }


	 return 0;
}
