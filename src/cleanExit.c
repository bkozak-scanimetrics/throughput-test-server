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
* Handles resource clean-up on abrupt exit (from signals)                     *
******************************************************************************/

/******************************************************************************
*                                  INCLUDES                                   *
******************************************************************************/
#include "cleanExit.h"

#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <stdbool.h>
/******************************************************************************
*                                    DATA                                     *
******************************************************************************/
static int* fdList;
static int  fdListSize;
static int  fdListMem;
/******************************************************************************
*                             FUNCTION PROTOTYPES                             *
******************************************************************************/
static void sigHandler(int signum);
/******************************************************************************
*                            FUNCTION DEFINITIONS                             *
******************************************************************************/
/**
* Signal handler for handling exit
**/
static void sigHandler(int signum){
	for(int i = 0; i < fdListSize; i++){
		close(fdList[i]);
	}
	free(fdList);

	exit(-1);
}
/**
* Add to list of file descriptors to clean up on exit
**/
void cleanExit_add_fd(int fd){
	if(fdListSize == fdListMem){
		int* tmp = realloc(fdList,fdListMem+8);

		fdListMem += 8;
		fdList = tmp;
	}

	fdList[fdListSize] = fd;
	fdListSize += 1;
}
/**
* Start handling the given signal so that we can clean up resources
**/
void cleanExit_add_signal(int signum){
	signal(signum,sigHandler);
}
/**
* Forget all resources we want to handle. Signal handlers are left in place.
**/
void cleanExit_stop(void){
	sigset_t set;

	sigfillset(&set);

	sigprocmask(SIG_BLOCK,&set,NULL);

	free(fdList);

	fdListSize = 0;
	fdListMem = 0;
	fdList = NULL;

	sigprocmask(SIG_UNBLOCK,&set,NULL);
}
