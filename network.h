/*****************************************************************************
 * Copyright 2014 SRI International                                          *
 *                                                                           *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may   *
 * not use this file except in compliance with the License.                  *
 * You may obtain a copy of the License at                                   *
 *                                                                           *
 * http://www.apache.org/licenses/LICENSE-2.0                                *
 *                                                                           *
 * Unless required by applicable law or agreed to in writing, software       *
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT *
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.          *
 * See the License for the specific language governing permissions and       *
 * limitations under the License.                                            *
 *****************************************************************************/
#ifndef _network_h_
#define _network_h_

#include <stdint.h>

extern "C" {
#define _BSD_SOURCE     1
#define __FAVOR_BSD     1
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <netinet/if_ether.h>
}

#include <iostream>
#include "chars_ref.h"

inline bool operator==(in_addr a, in_addr b) { return a.s_addr == b.s_addr; }
inline bool operator!=(in_addr a, in_addr b) { return a.s_addr != b.s_addr; }
inline bool operator<(in_addr a, in_addr b) { return a.s_addr < b.s_addr; }
inline std::ostream &operator<<(std::ostream &os, in_addr a) {
    unsigned char *p = (unsigned char *)&a.s_addr;
    os << (int)p[0] << '.' << (int)p[1] << '.' << (int)p[2] << '.' << (int)p[3];
    return os; }

union sockaddr_any {
    struct sockaddr     sa;
    struct sockaddr_in  sa_in;
    struct sockaddr_in6 sa_in6;
};
std::ostream &operator<<(std::ostream &out, const struct sockaddr &sa);
inline std::ostream &operator<<(std::ostream &out, const sockaddr_any &sa) {
    return operator<<(out, sa.sa); }
inline std::ostream &operator<<(std::ostream &out, const struct sockaddr_in &sa) {
    return operator<<(out, *reinterpret_cast<const struct sockaddr *>(&sa)); }
inline std::ostream &operator<<(std::ostream &out, const struct sockaddr_in6 &sa) {
    return operator<<(out, *reinterpret_cast<const struct sockaddr *>(&sa)); }

struct endpoint {
    in_addr     addr;
    uint16_t    port; /* in network byte order */
    endpoint() { addr.s_addr = INADDR_ANY; port = 0; }
    endpoint(in_addr a) { addr = a; port = 0; }
    endpoint(const sockaddr_in &a) {
	addr = a.sin_addr; port = a.sin_port; }
    endpoint(const char *spec, const char **rest = 0);
    endpoint &operator=(const endpoint &a) {
        addr = a.addr; port = a.port; return *this; }
    bool operator==(endpoint a) const {
        return addr == a.addr && port == a.port; }
    bool operator<(endpoint a) const {
        return addr == a.addr ? port < a.port : addr < a.addr; }
    explicit operator bool() { return addr.s_addr != 0 || port != 0; }
    bool operator !() { return addr.s_addr == 0 && port == 0; }
};

inline std::ostream &operator<<(std::ostream &os, endpoint a) {
    os << a.addr << ':' << ntohs(a.port);
    return os; }

struct network {
    in_addr     addr;
    short	masksz;
    network() : masksz(-1) { addr.s_addr = ~0; }
    network(in_addr a, short msk) : addr(a), masksz(msk) {}
    network(const char *spec, const char **rest = 0);
    uint32_t mask() const { return masksz>0 ? htonl(~0U << (32-masksz)) : 0U; }
    explicit operator bool() const { return masksz >= 0; }
    bool operator !() const { return masksz < 0; }
    bool has(in_addr a) const { return (a.s_addr & mask()) == addr.s_addr; }
};

inline std::ostream &operator<<(std::ostream &os, network n) {
    os << n.addr << '/' << n.masksz;
    return os; }

inline endpoint parse_endpoint(const char *spec, const char **rest = 0)
    { return endpoint(spec, rest); }
inline endpoint parse_endpoint(const chars_ref &spec) {
    const char *rest;
    endpoint rv(spec.begin(), &rest);
    if (rest != spec.end()) return endpoint();
    return rv; }
inline network parse_network(const char *spec, const char **rest = 0)
    { return network(spec, rest); }
inline network parse_network(const chars_ref &spec) {
    const char *rest;
    network rv(spec.begin(), &rest);
    if (rest != spec.end()) return network();
    return rv; }

inline const char *operator>>(const char *p, endpoint &ep) {
    if (p && !(ep = parse_endpoint(p, &p))) p = 0;
    return p; }
inline const char *operator>>(const char *p, network &net) {
    if (p && !(net = parse_network(p, &p))) p = 0;
    return p; }
const char *operator>>(const char *p, in_addr &a);

#endif /* _network_h_ */
