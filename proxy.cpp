#include "proxy.h"
#include "connect.h"
#include "dynvec.h"
#include "encrypt.h"
#include <errno.h>
#include <fcntl.h>
#include "iomanage.h"
#include <iostream>
#include "log.h"
#include <map>
#include <memory>
#include "socks5.h"
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

void hd(const char *p, size_t len) {
    static char digits[] = "0123456789abcdef";
    while (len > 0) {
        std::cerr << "   ";
        size_t i;
        for (i = 0; i < 16 && i < len; ++i)
            std::cerr << ' ' << digits[(p[i] >> 4) & 0xf] << digits[p[i] & 0xf];
        for (; i < 16; ++i)
            std::cerr << "   ";
        std::cerr << "  ";
        for (i = 0; i < 16 && i < len; ++i) {
            char ch = p[i] & 0x7f;
            if (ch < ' ' || ch == 127) ch = '.';
            std::cerr << ch; }
        std::cerr << std::endl;
        p += i;
        len -= i; }
}

struct listen_info : iomanage {
    std::vector<network>	        allow;
    std::function<iomanage *(int)>      accept;
    listen_info(int fd, const std::vector<network> &a, std::function<iomanage *(int)> acc)
	: iomanage(fd), allow(a), accept(acc) { }
    void ready();
    bool allow_connection(in_addr from) {
	for (auto &n : allow) if (n.has(from)) return true;
	return allow.empty(); }
};

void listen_info::ready()
{
    sockaddr_in	addr;
    socklen_t addrlen = sizeof(addr);
    int nfd;
    while ((nfd = ::accept(fd, (sockaddr *)&addr, &addrlen)) >= 0) {
	if (allow_connection(addr.sin_addr)) {
            accept(nfd);
	    INFO("Accept new connection from " << endpoint(addr) <<
		 " as " << nfd);
	} else {
	    ::close(nfd);
	    WARN("Reject connection from " << endpoint(addr));
	}
    }
    if (errno == EAGAIN || errno == EWOULDBLOCK)
	return;
    perror("accept");
    close();
}

iomanage *listen_on(endpoint e, const std::vector<network> &allow,
                    std::function<iomanage *(int)> accept)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return 0;
    int set = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &set, sizeof(set));
    struct sockaddr_in	addr;
    addr.sin_family = AF_INET;
    addr.sin_addr = e.addr;
    addr.sin_port = e.port;
    if (bind(fd, (sockaddr *)&addr, sizeof(addr)) < 0 || listen(fd, 1) < 0) {
	close(fd);
	return 0; }
    INFO("listening on " << e << allow << " as " << fd);
    return new listen_info(fd, allow, accept);
}

iomanage *listen_on(const char *spec, std::function<iomanage *(int)> accept) {
    endpoint                    e;
    std::vector<network>        allow;
    if (!(spec = spec >> e))
        return nullptr;
    while (isspace(*spec)) ++spec;
    if (*spec == '/' || *spec == '|' || *spec == '[' || *spec == '(') do {
        network tmp;
        if (!(spec = ++spec >> tmp))
            return nullptr;
        allow.push_back(tmp);
        while (isspace(*spec)) ++spec;
    } while(*spec == ',');
    while (isspace(*spec)) ++spec;
    if (*spec == ')' || *spec == ']') ++spec;
    while (isspace(*spec)) ++spec;
    if (*spec) return nullptr;
    return listen_on(e, allow, accept);
}

struct peer_info;

struct client_info : iomanage {
    enum { START, REQUEST, CONNECTING, NEED_RENUMBER, CONNECTED } state = START;
    int                 channel = -1;
    peer_info           *peer = nullptr;
    client_info(int fd) : iomanage(fd, true) {}
    client_info(client_info &&a) = default;
    client_info(iomanage &&a) : iomanage(std::move(a)) {}
    ~client_info();
    void ready();
};

struct peer_info : iomanage {
    dynvec<client_info *>               clients;
    int                                 num_clients = 0;
    static std::set<peer_info *>        all_peers;
    enum control_commands { NEW_CONNECTION, CONNECTION_ESTABLISHED, RENUMBER_CONNECTION,
                            CONNECTION_FAILED };

    peer_info(int fd) : iomanage(fd, true) { all_peers.insert(this); }
    peer_info(peer_info &&a)
    : iomanage(std::move(a)), clients(std::move(a.clients)), num_clients(a.num_clients) {
        all_peers.insert(this); }
    peer_info(iomanage &&a) : iomanage(std::move(a)) {
        all_peers.insert(this); }
    ~peer_info();
    static peer_info *connect_peer(client_info *, s5addr addr);
    void ready();
    void send_s5reply(client_info *client);
};
std::set<peer_info *> peer_info::all_peers;

client_info::~client_info() {
    if (peer) {
        if (peer->clients[channel] == this)
            peer->clients[channel] = nullptr;
        else
            ERROR("incorrect client " << fd << " channel " << channel << " for peer " << peer->fd);
    }
}

peer_info::~peer_info() {
    for (auto cl : clients)
        if (cl) cl->close();
    all_peers.erase(this);
}

void client_info::ready() {
    int err = read();
    while (!err && ibuf.size() > 0) {
        switch (state) {
        case START:
            if (ibuf.size() <= 2 && ibuf.size() < (ibuf[1] & 0xff) + 2U)
                return;
            if (ibuf[0] != 0x5) {
                WARN("Invalid SOCKS version " << (ibuf[0] & 0xff));
                err = -1;
            } else if (memchr(&ibuf[2], 0, ibuf[1] & 0xff) == 0) {
                INFO("SOCKS5 request requiring authentication, rejecting");
                obuf.append("\x5\xff", 2);
                write();
                err = -1;
            } else {
                TRACE(fd << ": SOCKS5 connection started");
                obuf.append("\x5", 2);
                write();
                state = REQUEST;
            }
            ibuf.erase(0, (ibuf[1] & 0xff) + 2);
            break;
        case REQUEST: {
            if (ibuf.size() < 6) return;
            if (ibuf[0] != 0x5) {
                WARN("Invalid SOCKS version " << (ibuf[0] & 0xff));
                err = -1;
                break;
            } else if (ibuf[1] != 0x1) {
                WARN("Unsupported SOCKS request " << (ibuf[1] & 0xff));
                err = -1;
                break;
            }
            size_t addrlen = 0;
            switch (ibuf[3]) {
            case 1: addrlen = 6; break;                         // IPv4
            case 4: addrlen = 18; break;                        // IPv6
            case 3: addrlen = (ibuf[4] & 0xff) + 3; break;      // domainname
            default:
                WARN("Unsupported SOCKS address type " << (ibuf[3] & 0xff));
                obuf.append("\x5\x8\x0\x0", 4);
                write();
                err = -1; }
            if (ibuf.size() < addrlen+4) return;
            s5addr addr(&ibuf[3], addrlen+1);
            TRACE(fd << ": SOCKS5 connect request to " << addr);
            if (!(peer = peer_info::connect_peer(this, addr))) {
                WARN("Failed to connect to " << addr);
                obuf.append("\x5\x1\x0\x0", 4);
                write();
                err = -1;
            }
            ibuf.erase(0, addrlen+4);
            break; }
        case CONNECTING:
        case NEED_RENUMBER:
            // can't do anything until the connection completes
            TRACE(fd << ": data on connection in progress, waiting");
            return;
        case CONNECTED: {
            size_t len = ibuf.size() > 255 ? 255 : ibuf.size();
            TRACE2(fd << ": copying " << len << " bytes to channel " << channel <<
                   " on peer " << peer->fd);
            if (LOG_TRACE3) hd(&ibuf[0], len);
            if (channel >= 0) {
                peer->obuf += (char)channel;
                peer->obuf += (char)len; }
            peer->obuf += chars_ref(&ibuf[0], len);
            peer->write();
            ibuf.erase(0, len); }
        }
    }
    if (err < 0) close();
}

iomanage *socks5_client(int fd) {
    return new client_info(fd);
}

iomanage *tunnel_peer(int fd, const char *key) {
    auto *rv = new peer_info(fd);
    if (key) {
        auto *tmp = rv;
        TRACE("encrypting " << tmp->fd);
        rv = new encrypt_io<peer_info>(key, std::move(*tmp));
        delete tmp;
    }
    return rv;
}

void peer_info::send_s5reply(client_info *client) {
    sockaddr_any addr;
    socklen_t len = sizeof(addr);
    if (getsockname(client->fd, &addr.sa, &len) < 0)
        ERROR("getsockname failed " << strerror(errno));
    switch (addr.sa.sa_family) {
    case AF_INET:
        obuf += (char)client->channel;
        obuf.append("\xa\x5\0\0\1", 5);
        obuf.append((char *)&addr.sa_in.sin_addr, 4);
        obuf.append((char *)&addr.sa_in.sin_port, 2);
        TRACE(fd << ": sending SOCKS5 ipv4 reply " << s5addr(&obuf[obuf.size()-7], 7));
        break;
    case AF_INET6:
        obuf += (char)client->channel;
        obuf.append("\x16\x5\0\0\4", 5);
        obuf.append((char *)&addr.sa_in6.sin6_addr, 16);
        obuf.append((char *)&addr.sa_in6.sin6_port, 2);
        TRACE(fd << ": sending SOCKS5 ipv6 reply " << s5addr(&obuf[obuf.size()-19], 19));
        break;
    default:
        ERROR("Unknown address family " << addr.sa.sa_family);
        break; } 
}

void peer_info::ready() {
    if (read() < 0) {
        close();
        return; }
    while (ibuf.size() > 1 && ibuf.size() >= (ibuf[1] & 0xff) + 2U) {
        int len = ibuf[1] & 0xff;
        if (int channel = ibuf[0] & 0xff) {
            if (auto client = clients[channel]) {
                if (ibuf[1]) {
                    if (client->state == client_info::CONNECTING) {
                        client->state = client_info::CONNECTED;
                        client->ignore(false); }
                    client->obuf += chars_ref(&ibuf[2], ibuf[1] & 0xff);
                    TRACE2(fd << ": copying " << (ibuf[1] & 0xff) << " bytes from channel " <<
                           channel << " to " << client->fd);
                    if (LOG_TRACE3) hd(&ibuf[2], ibuf[1] & 0xff);
                    client->write();
                } else {
                    TRACE(fd << ": EOF on channel " << channel << ", closing " << client->fd);
                    client->close();
                    clients[channel] = nullptr;
                    --num_clients; }
            } else {
                if (ibuf[1]) {
                    obuf += (char)channel;
                    obuf += (char)0;
                    write(); } }
        } else {
            switch(ibuf[2]) {
            case NEW_CONNECTION: {
                int channel = ibuf[3] & 0xff, req_channel = channel;
                while (clients[channel]) ++channel;
                int err = -1;
                if (channel > 255) {
                    ERROR("Ran out of channels!");
                    err = -1;
                } else {
                    s5addr addr(&ibuf[4], (ibuf[1] & 0xff) - 2);
                    TRACE(fd << ": connect req on channel " << req_channel << " to " << addr);
                    err = connect_to(addr, [=](int err, iomanage &&io) {
                        // FIXME -- if the peer connection goes away before the client connection
                        // completes, 'this' will dangle and the code will crash.  Should figure
                        // out a way of recovering from this (reestablish the peer connection?)
                        // or at least avoid the crash.
                        if (err) {
                            obuf.append("\x0\x3", 2);
                            obuf += (char)CONNECTION_FAILED;
                            obuf += (char)req_channel;
                            obuf += err < 0 ? (char)-err : (char)errno2socks5error(err);
                            INFO("connection to " << addr << " failed: " <<
                                 (err < 0 ? socks5error(-err) : strerror(err)));
                        } else {
                            auto client = clients[channel] = new client_info(std::move(io));
                            client->channel = channel;
                            client->peer = this;
                            num_clients++;
                            TRACE(fd << ": connection " << client->fd <<
                                  " established for channel " << req_channel);
                            if (channel == req_channel) {
                                obuf.append("\x0\x2", 2);
                                obuf += (char)CONNECTION_ESTABLISHED;
                                obuf += (char)channel;
                                client->state = client_info::CONNECTED;
                                send_s5reply(client);
                            } else {
                                TRACE(fd << ": renumber to channel " << channel);
                                obuf.append("\x0\x3", 2);
                                obuf += (char)RENUMBER_CONNECTION;
                                obuf += (char)req_channel;
                                obuf += (char)channel;
                                client->state = client_info::NEED_RENUMBER;
                                client->ignore(true); } }
                        write(); }); }
                if (err < 0) {
                    INFO("connect failed locally");
                    obuf.append("\x0\x3", 2);
                    obuf += (char)CONNECTION_FAILED;
                    obuf += (char)req_channel;
                    obuf += (char)1;
                    write(); }
                break; }
            case CONNECTION_ESTABLISHED: {
                int channel = ibuf[3] & 0xff;
                if (auto client = clients[channel]) {
                    TRACE(fd << ": connection for " << client->fd << " established on "
                          "channel " << channel);
                    if (client->state == client_info::NEED_RENUMBER)
                        send_s5reply(client);
                    client->state = client_info::CONNECTED;
                    client->ignore(false);
                } else {
                    ERROR("Invalid channel " << channel); }
                break; }
            case CONNECTION_FAILED: {
                int channel = ibuf[3] & 0xff;
                if (auto client = clients[channel]) {
                    TRACE(fd << ": connection for " << client->fd << " failed on "
                          "channel " << channel);
                    client->obuf += (char)5;
                    client->obuf += ibuf[4];
                    client->obuf += (char)0;
                    client->obuf += (char)0;
                    client->write();
                    client->close();
                    clients[channel] = nullptr;
                    --num_clients;
                } else {
                    ERROR("Invalid channel " << channel); }
                break; }
            case RENUMBER_CONNECTION: {
                int channel = ibuf[3] & 0xff;
                if (!clients[channel]) {
                    ERROR("Invalid channel " << channel);
                    break; }
                auto *client = clients[channel];
                clients[channel] = nullptr;
                int newchannel = ibuf[4] & 0xff;
                TRACE(fd << ": renumber request " << channel << " -> " << newchannel);
                if (clients[newchannel]) {
                    WARN("Channel " << newchannel << " already in use for renumber, trying again");
                    while (clients[++channel]);
                    clients[channel] = client;
                    client->channel = channel;
                    timer(millisec((1 + random()%30) * 100), [this, channel, newchannel]() {
                        obuf.append("\x0\x3", 2);
                        obuf += (char)RENUMBER_CONNECTION;
                        obuf += (char)newchannel;
                        obuf += (char)channel;
                        write(); });
                    break; }
                clients[channel = newchannel] = client;
                client->channel = channel;
                obuf.append("\x0\x2", 2);
                obuf += (char)CONNECTION_ESTABLISHED;
                obuf += (char)channel;
                if (client->state == client_info::NEED_RENUMBER)
                    send_s5reply(client);
                write();
                client->state = client_info::CONNECTED;
                client->ignore(false);
                break; }
            default:
                ERROR("Invalid peer command " << (ibuf[2] & 0xff)); }
        }
        ibuf.erase(0, len+2);
    }
    if (ibuf.size() > 2)
        TRACE(fd << ": partial read " << ibuf.size() << " of " <<
              ((ibuf[1] & 0xff) + 2) << " bytes");
}

peer_info *peer_info::connect_peer(client_info *client, s5addr addr) {
    if (!addr) return nullptr;
    peer_info *rv = nullptr;
    for (auto *p : all_peers)
        if (!rv || p->num_clients < rv->num_clients)
            rv = p;
    if (!rv) {
        WARN("No peer to connect to " << addr);
        return rv; }
    unsigned channel = 0;
    while (rv->clients[++channel]);
    if (channel > 255) {
        ERROR("Ran out of channels!");
        return nullptr; }
    INFO(rv->fd << ":requesting connection to " << addr << " on channel " << channel);
    rv->obuf += (char)0;
    rv->obuf += (char)(addr.len + 2);
    rv->obuf += (char)NEW_CONNECTION;;
    rv->obuf += (char)channel;
    rv->obuf += addr;
    rv->write();
    rv->clients[channel] = client;
    rv->num_clients++;
    client->state = client_info::CONNECTING;
    client->ignore(true);
    client->channel = channel;
    return rv;
}

int connect_peer(const char *addr, const char *key) {
    return connect_to(addr, [key](int err, iomanage &&io) {
        if (err) {
            ERROR("could not connect to peer: " << (err > 0 ? strerror(err) : socks5error(-err)));
        } else {
            auto *peer = new peer_info(std::move(io));
            if (key) {
                TRACE("encrypting " << peer->fd);
                new encrypt_io<peer_info>(key, std::move(*peer));
                delete peer;
            }
        }
    });
}
