#ifndef common_h
#define common_h

#include <sys/socket.h>
#include <sys/types.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdarg.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <stdbool.h>
#include <limits.h>
#include <pthread.h>
#include <netinet/in.h>

#define SERVER_PORT 8989
#define MAX_LINE 4096
#define BUFFERSIZE 65536
#define SOCKET_ERROR (-1)
#define TIMEOUT_DELAY_SEC 20

typedef struct sockaddr SA;
typedef struct sockaddr_in SA_IN;

void err_n_die(const char *fmt, ...);
char *bin2hex(const unsigned char *input, size_t len);
int check(int exp, const char *msg);

#endif /* common_h */
