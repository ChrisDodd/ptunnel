#ifndef _socks5_h_
#define _socks5_h_

#include "chars_ref.h"
#include <functional>

struct s5addr : chars_ref {
    s5addr() = default;
    s5addr(const char *p, size_t len);
    friend std::ostream &operator<<(std::ostream &, s5addr);
    bool valid() const;
    explicit operator bool() const { return valid(); }
};

union sockaddr_any;
struct iomanage;
int get_address(s5addr addr, sockaddr_any *sa);
int connect_to(s5addr addr, std::function<void(int, iomanage &&)> fn);
int errno2socks5error(int err);
const char *socks5error(int err);

#endif /* _socks5_h_ */
