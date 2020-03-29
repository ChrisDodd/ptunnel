#include <arpa/inet.h>
#include "connect.h"
#include <fcntl.h>
#include "iomanage.h"
#include "log.h"
#include <netdb.h>
#include "network.h"
#include "socks5.h"
#include <stdlib.h>
#include <unistd.h>

std::ostream &operator<<(std::ostream &out, const struct sockaddr &sa) {
    static char buffer[256];
    if (sa.sa_family == AF_INET) {
        struct sockaddr_in *addr = (struct sockaddr_in *)&sa;
        unsigned char *u = (unsigned char *)&addr->sin_addr;
        out << (u[0] & 0xff) << '.' << (u[1] & 0xff) << '.' << (u[2] & 0xff) << '.'
            << (u[3] & 0xff) << ':' << ntohs(addr->sin_port);
    } else if (sa.sa_family == AF_INET6) {
        struct sockaddr_in6 *addr = (struct sockaddr_in6 *)&sa;
        unsigned char *u = (unsigned char *)&addr->sin6_addr;
        char *p = buffer;
        for (int i = 0; i < 8; ++i) {
            if (i) *p++ = ':';
            if (u[2*i])
                p += sprintf(p, "%x%02x", u[2*i], u[2*i + 1]);
            else
                p += sprintf(p, "%x", u[2*i + 1]); }
        sprintf(p, "!%d", ntohs(addr->sin6_port));
        if ((p = strstr(buffer, ":0:0:"))) {
            char *e = p+4;
            while (e[1] == '0' && e[2] == ':') e += 2;
            memmove(p+1, e, strlen(e) + 1); }
        out << buffer;
    } else {
        out << "<af " << sa.sa_family << ">";
    }
    return out;
}

struct addrinfo *get_addrinfo(const char *address)
{
    struct addrinfo *info = 0;
    const char *port = strrchr(address, ':');
    char addr_buf[64];
    snprintf(addr_buf, sizeof(addr_buf), "%.*s", (int)(port ? port-address : 99), address);
    // '*' should be INADDR_ANY not localhost...
    if (!strcmp(addr_buf, "*"))
        strcpy(addr_buf, "0.0.0.0");
    if (port) ++port;
    if (getaddrinfo(addr_buf, port, 0, &info)) 
        return 0;
    if (!port)
	for (struct addrinfo *i = info; i; i = i->ai_next)
	    if (i->ai_family == AF_INET || i->ai_family == AF_INET6)
		((struct sockaddr_in *)i->ai_addr)->sin_port = 0;
    return info;
}

struct connect_info : iomanage {
    std::function<void(int, iomanage &&)>  fn;
    connect_info(int fd, std::function<void(int, iomanage &&)> fn) : iomanage(fd), fn(fn) {
        wait_for_output(true);
        ignore(true); }
    void ready() override { ERROR(fd << " readable when waiting for connection"); }
    int write() override {
        int error;
        socklen_t error_len = sizeof(int);
        if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &error_len) < 0) error = errno;
        wait_for_output(false);
        ignore(false);
        fn(error, std::move(*this));
        if (fd >= 0)
            close();
        else
            delete this;
        return error ? -1 : 0; }
};

struct socks5_connect_info : iomanage {
    std::function<void(int, iomanage &&)>       fn;
    std::string                                 addr;
    enum { START, CONNECTING, CONNECTED }       state = START;
    void start() {
        obuf.append("\x5\x1\x0", 3);
        write(); }
    socks5_connect_info(int fd, std::string a, std::function<void(int, iomanage &&)> fn)
    : iomanage(fd), fn(fn), addr(a) { start(); }
    socks5_connect_info(iomanage &&io , std::string a, std::function<void(int, iomanage &&)> fn)
    : iomanage(std::move(io)), fn(fn), addr(a) { start(); }
    void ready() override {
        int err = read();
        while (!err && ibuf.size() >= 2 && state != CONNECTED) {
            if (ibuf[0] != 0x5) {
                WARN("socks server invalid version " << (ibuf[0] & 0xff));
                err = -11;
                break; }
            switch (state) {
            case START:
                if (ibuf[1] != 0) {
                    WARN("socks server wants method " << (ibuf[1] & 0xff));
                    err = -11;
                } else {
                    const char *a = addr.c_str();
                    const char *port = strchr(a, ':');
                    char buf[128] = "\x5\x1\x0\x0";
                    uint16_t port_num = 0;
                    snprintf(buf+5, sizeof(buf)-6, "%.*s", (int)(port ? port-a : addr.size()), a);
                    size_t len = 5 + strlen(buf+5);
                    if (port)
                        port_num = atoi(port+1);
                    struct in_addr tmp;
                    if (inet_aton(buf+4, &tmp) == 1) {
                        buf[3] = 0x1;
                        memcpy(buf+4, &tmp, 4);
                        len = 8;
                    } else {
                        buf[3] = 0x3;
                        buf[4] = len - 5;
                    }
                    buf[len++] = port_num >> 8;
                    buf[len++] = port_num & 0xff;
                    TRACE(fd << ": sendin SOCKS5 connreq " << s5addr(buf+3, len-3));
                    obuf.append(buf, len);
                    write();
                    ibuf.erase(0, 2);
                    state = CONNECTING; }
                break;
            case CONNECTING:
                if (ibuf[1] != 0) {
                    ERROR("socks server error: " << socks5error(ibuf[1]));
                    err = -(ibuf[1] & 0xff);
                } else {
                    TRACE(fd << ": got reply " << s5addr(&ibuf[3], ibuf.size() - 3));
                    switch(ibuf[3]) {
                    case 1: ibuf.erase(0, 10); break;
                    case 3: ibuf.erase(0, (ibuf[4] & 0xff) + 7); break;
                    case 4: ibuf.erase(0, 22); break;
                    default:
                        err = -1; }
                    if (!err) state = CONNECTED; }
                break;
            case CONNECTED:
                break; } }
        if (err || state == CONNECTED) {
            fn(err, std::move(*this));
            if (fd >= 0)
                close();
            else
                delete this; }
    }
};

int connect_to(void *sock_addr, size_t sockaddr_len, int proto,
               std::function<void(int, iomanage &&)> fn)
{
    struct sockaddr *sa = (struct sockaddr *)sock_addr;
    int fd = socket(sa->sa_family, SOCK_STREAM, proto);
    if (fd >= 0) {
	fcntl(fd, F_SETFD, FD_CLOEXEC);
        auto *ci = new connect_info(fd, fn);
        INFO("connect to " << *sa);
        errno = 0;
	if (connect(fd, sa, sockaddr_len) == 0 || errno != EINPROGRESS) {
            fn(errno, std::move(*ci));
            if (ci->fd >= 0)
                ci->close();
            else
                delete ci; }
        return 0;
    } else {
        return -1; }
}

int connect_to(const char *address, std::function<void(int, iomanage &&)> fn)
{
    if (const char *proxy = strchr(address, '@')) {
        std::string addr(address, proxy - address);
        return connect_to(proxy+1, [fn, addr](int err, iomanage &&io) {
            if (err)
                fn(err, std::move(io));
            else
                new socks5_connect_info(std::move(io), addr, fn);
        }); }
    struct addrinfo *info = get_addrinfo(address);
    if (!info || !((struct sockaddr_in *)info->ai_addr)->sin_port) {
	errno = EINVAL;
	return -1;
    }
    int rv = connect_to(info->ai_addr, info->ai_addrlen, info->ai_protocol, fn);
    freeaddrinfo(info);
    return rv;
}

int listen_socket(const char *address, int backlog)
{
    struct addrinfo *info = get_addrinfo(address);
    int rv = -1;
    if (info && ((struct sockaddr_in *)info->ai_addr)->sin_port)
	rv = socket(info->ai_family, SOCK_STREAM, info->ai_protocol);
    if (rv >= 0) {
	fcntl(rv, F_SETFD, FD_CLOEXEC);
	const int one = 1;
	setsockopt(rv, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(1));
        INFO("listen on " << info->ai_addr);
	if (bind(rv, info->ai_addr, info->ai_addrlen) < 0 ||
	    listen(rv, backlog) < 0)
	{
	    close(rv);
	    rv = -1;
	}
    }
    if (info) freeaddrinfo(info);
    return rv;
}

