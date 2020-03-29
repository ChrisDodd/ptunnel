#include "iomanage.h"
#include "network.h"

struct addrinfo *get_addrinfo(const char *address);
int connect_to(void *sock_addr, size_t sockaddr_len, int proto,
               std::function<void(int, iomanage &&)> fn);
int connect_to(const char *address, std::function<void(int, iomanage &&)> fn);
int listen_socket(const char *address, int backlog);
