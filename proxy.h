#ifndef _proxy_h_
#define _proxy_h_

#include "chars_ref.h"
#include <functional>
#include <iostream>
#include "network.h"
#include <signal.h>
#include <vector>

struct iomanage;

iomanage *listen_on(endpoint e, const std::vector<network> &allow,
                    std::function<iomanage *(int)> accept);
iomanage *listen_on(const char *spec, std::function<iomanage *(int)> accept);

iomanage *socks5_client(int fd);
iomanage *tunnel_peer(int fd, const char *key);
int connect_peer(const char *spec, const char *key);

#endif /* _proxy_h_ */
