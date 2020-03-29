#include "socks5.h"
#include "connect.h"
#include "hex.h"
#include "network.h"
#include <netdb.h>

s5addr::s5addr(const char *p_, size_t len_) : chars_ref(p_, len_) {
    if (len > 2) {
        switch(p[1]) {
        case 1:
            if (len > 7)
                len = 7;
            break;
        case 3:
            if (len > (p[1] & 0xff) + 4U)
                len = (p[1] & 0xff) + 4U;
            break;
        case 4:
            if (len > 19)
                len = 19;
            break;
        default:
            break; } }
}

std::ostream &operator<<(std::ostream &out, s5addr addr) {
    char sep = ':';
    int port = 0;
    switch (addr[0]) {
    case 1:
        out << (addr[1] & 0xff) << "." << (addr[2] & 0xff) << "."
            << (addr[3] & 0xff) << "." << (addr[4] & 0xff);
        port = 5;
        break;
    case 3:
        out << addr.substr(2, addr[1] & 0xff);
        port = (addr[1] & 0xff) + 2;
        break;
    case 4:
        sep = '!';
        for (int i = 0; i < 16; i += 2) {
            if (i) out << ':';
            out << hex(((addr[i+1] & 0xff) << 8) + (addr[i+2] & 0xff)); }
        port = 17;
        break;
    default:
        out << "<invalid address type " << (addr[0] & 0xff) << ">";
        return out;
    }
    out << sep << (((addr[port] & 0xff) << 8) + (addr[port+1] & 0xff));
    return out;
}

bool s5addr::valid() const {
    if (len < 2) return false;
    switch(p[0]) {
    case 1: return len == 7;
    case 3: return len == (p[1] & 0xff) + 4U;
    case 4: return len == 19;
    default: return false; }
}

int errno2socks5error(int err) {
    switch (err) {
    case ENETUNREACH:
        return 3;
    case ETIMEDOUT:
        return 4;
    case ECONNREFUSED:
        return 5;
    case EAFNOSUPPORT:
        return 8;
    default:
        return 1;
    }
}

const char *socks5error(int code) {
    static char tmp[32];
    switch (code) {
    case 0: return "success";
    case 1: return "general server failer";
    case 2: return "connection not allowed";
    case 3: return "network unreachable";
    case 4: return "host unreachable";
    case 5: return "connection refused";
    case 6: return "TTL expired";
    case 7: return "command not supported";
    case 8: return "address type not supported";
    default:
        sprintf(tmp, "code x%02x", code & 0xff);
        return tmp; }
}

int get_address(s5addr addr, sockaddr_any *sa) {
    switch (addr[0]) {
    case 1: {
        sa->sa_in.sin_family = AF_INET;
        memcpy(&sa->sa_in.sin_addr, addr.begin() + 1, 4);
        memcpy(&sa->sa_in.sin_port, addr.begin() + 5, 2);
        return sizeof sa->sa_in; }
    case 3: {
        struct addrinfo *info = nullptr;
        char address[256];
        int rv = -1;
        int alen = addr[1] & 0xff;
        memcpy(address, addr.begin() + 2, alen);
        address[alen] = 0;
        if (getaddrinfo(address, nullptr, 0, &info))
            return -1;
        for (struct addrinfo *i = info; i; i = i->ai_next) {
            if (i->ai_family == AF_INET) {
                memcpy(&sa->sa_in, i->ai_addr, sizeof sa->sa_in);
                memcpy(&sa->sa_in.sin_port, addr.begin() + 2 + alen, 2);
                rv = sizeof sa->sa_in;
                break;
            } else if (i->ai_family == AF_INET6) {
                memcpy(&sa->sa_in6, i->ai_addr, sizeof sa->sa_in6);
                memcpy(&sa->sa_in6.sin6_port, addr.begin() + 2 + alen, 2);
                rv = sizeof sa->sa_in6;
                break; } }
        freeaddrinfo(info);
        return rv; }
    case 4: {
        memset(sa, 0, sizeof sa->sa_in6);
        sa->sa_in6.sin6_family = AF_INET;
        memcpy(&sa->sa_in6.sin6_addr, addr.begin() + 1, 16);
        memcpy(&sa->sa_in6.sin6_port, addr.begin() + 17, 2);
        return sizeof sa->sa_in6; } } 
    return -1;
}

int connect_to(s5addr addr, std::function<void(int, iomanage &&)> fn) {
    sockaddr_any address;
    if (!addr) return -1;
    int len = get_address(addr, &address);
    if (len <= 0) return -1;
    return connect_to(&address, len, 0, fn);
}
