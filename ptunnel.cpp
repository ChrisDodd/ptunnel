#include "connect.h"
#include "log.h"
#include "proxy.h"

int verbose = 0;

int main(int ac, char **av) {
    int ok = true;
    const char *key = nullptr;
    for (int i = 1; i < ac;) {
        char *arg = av[i++];
        if (*arg == '-' || *arg == '+') {
            int flag = *arg == '+';
            while (*++arg) switch(*arg) {
            case '5':
                if (!listen_on(av[i++], socks5_client)) {
                    std::cerr << "Invalid listen spec " << av[i-1] << std::endl;
                    exit(1); }
                break;
            case 'L':
                if (!listen_on(av[i++], [key](int fd) { return tunnel_peer(fd, key); })) {
                    std::cerr << "Invalid listen spec " << av[i-1] << std::endl;
                    exit(1); }
                break;
            case 'C':
                if (connect_peer(av[i++], key) < 0) {
                    std::cerr << "Invalid onnect spec " << av[i-1] << std::endl;
                    exit(1); }
                break;
            case 'X':
                key = av[i++];
                break;
            case 'v':
                ++verbose;
                break;
            default:
                std::cerr << "Unknown argument " << (flag ? '+' : '-') << *arg << std::endl;
                ok = false;
            }
        } else {
            std::cerr << "Unknown argument '" << arg << "'" << std::endl;
        }
    }
    if (!ok) {
        std::cerr << "usage: " << av[0] << std::endl;
        exit(1);
    }
    iomanage::run();
}
