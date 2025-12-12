#ifndef NET_H
#define NET_H

#include "common.h"

int tcp_listen(const char *port);
int tcp_connect(const char *host, const char *port);
FILE *fdopen_r(int fd);
FILE *fdopen_w(int fd);

int send_line(FILE *out, const char *line);
int recv_line(FILE *in, char *buf, int cap);

#endif
