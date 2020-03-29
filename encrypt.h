#ifndef _encrypt_h_
#define _encrypt_h_

#include <fcntl.h>
#include "hex.h"
#include "iomanage.h"
#include "log.h"
#include <sys/types.h>
#include <sys/stat.h>
#include "tea.h"
#include <unistd.h>

template<class IO> class encrypt_io : public IO {
    uint64_t    k0, k1;

    int         in_keybytes = -8;
    uint64_t    in_cipher, in_key;
    int read() override {
        size_t off = this->ibuf.size();
        int rv = IO::read();
        if (in_keybytes < 0) {
            // reading IV
            while (in_keybytes < 0 && off < this->ibuf.size()) {
                in_cipher = (in_cipher << 8) + (this->ibuf[off++] & 0xff);
                in_keybytes++; }
            this->ibuf.erase(0, off);
            TRACE3(this->fd << ": read IV " << hex(in_cipher, 16, '0'));
            off = 0; }
        while (off < this->ibuf.size()) {
            if (in_keybytes == 0) {
                in_key = tea_encode(in_cipher, k0, k1);
                TRACE3(this->fd << ": input key bytes: " << hex(in_key, 16, '0'));
                in_keybytes = 8; }
            in_cipher = (in_cipher << 8) + (this->ibuf[off] & 0xff);
            this->ibuf[off++] ^= in_key & 0xff;
            in_key >>= 8;
            --in_keybytes; }
        return rv;
    }

    int         out_keybytes = -8;
    uint64_t    out_cipher = 0, out_key;
    size_t      out_off = 0;
    void sendIV() {
        int rand = open("/dev/urandom", O_RDONLY);
        if (::read(rand, &out_cipher, 8) != 8);  // should not happen
        close(rand);
        out_keybytes = 0;
        out_off = 0;
        for (int i = 56; i >= 0; i -= 8)
            this->obuf += (char)((out_cipher >> i) & 0xff);
        IO::write();
        out_off = this->obuf.size();
        TRACE3(this->fd << ": write IV " << hex(out_cipher, 16, '0'));
    }
    int write() override {
        while (out_off < this->obuf.size()) {
            if (out_keybytes == 0) {
                out_key = tea_encode(out_cipher, k0, k1);
                TRACE3(this->fd << ": output key bytes: " << hex(out_key, 16, '0'));
                out_keybytes = 8; }
            this->obuf[out_off] ^= out_key & 0xff;
            out_cipher = (out_cipher << 8) + (this->obuf[out_off++] & 0xff);
            out_key >>= 8;
            --out_keybytes; }
        int rv = IO::write();
        out_off = this->obuf.size();
        return rv;
    }

 public:
    encrypt_io() = delete;
    encrypt_io(encrypt_io &&) = default;
    encrypt_io(uint64_t k0, uint64_t k1, IO &&on) : IO(std::move(on)), k0(k0), k1(k1) {
        sendIV();
    }
    encrypt_io(const char *key, IO &&on)
    : IO(std::move(on)), k0(0x5555555555555555), k1(0x5555555555555555) {
        while (*key) {
            auto tmp = k1 >> 55;
            k1 = (k1 << 9) | (k0 >> 55);
            k0 = (k0 << 9) | tmp;
            k0 ^= *key++; }
        sendIV();
    }
};

#endif /* _encrypt_h_ */
