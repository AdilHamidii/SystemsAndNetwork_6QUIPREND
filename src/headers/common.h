#ifndef COMMON_H
#define COMMON_H

#define _POSIX_C_SOURCE 200112L


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <time.h>
#include <stdarg.h>


#include <unistd.h>
#include <fcntl.h>
#include <signal.h>


#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>




#include <pthread.h>



#define MAX_PLAYERS 10
#define MIN_PLAYERS 2

#define DECK_SIZE 104
#define ROWS 4
#define ROW_MAX 5
#define HAND_SIZE 10

#define LINE_MAX 2048
#define PLAYER_NAME_MAX 32

#define LOG_PATH "logs/partie.log"

#endif
