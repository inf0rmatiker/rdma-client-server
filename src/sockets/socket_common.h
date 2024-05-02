#ifndef SOCKET_COMMON_H
#define SOCKET_COMMON_H

#include <stdbool.h>

#define MAX_MSG_SIZE 255
#define MIN_PORT 1024
#define MAX_PORT 49151

bool is_valid_port(int);

#endif /* SOCKET_COMMON_H */