#ifndef UTIL_H
#define UTIL_H

#include "common.h"

void die(const char *m);
int str_starts(const char *s, const char *p);
int parse_int(const char *s, int *out);
void trim_crlf(char *s);

#endif
