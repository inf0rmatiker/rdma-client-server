#include "socket_common.h"

bool is_valid_port(int port) {
        return port >= 1024 && port <= 49151;
}