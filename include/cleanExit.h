#ifndef _CLEAN_EXIT_H_
#define _CLEAN_EXIT_H_

/******************************************************************************
*                             FUNCTION PROTOTYPES                             *
******************************************************************************/
void cleanExit_add_fd(int fd);
void cleanExit_add_signal(int signum);
void cleanExit_stop(void);

#endif // _CLEAN_EXIT_H_
