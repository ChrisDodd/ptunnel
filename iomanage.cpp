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
#include <errno.h>
#include <fcntl.h>
#include "iomanage.h"
#include <iostream>
#include "log.h"
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>

volatile sig_atomic_t	iomanage::attention = 0;
fd_set			iomanage::active, iomanage::output_wait;
int			iomanage::active_count;
std::vector<iomanage*>	iomanage::fds;
std::priority_queue<iomanage::timeout>	iomanage::timeout::wait;
size_t			iomanage::timeout::ignore_count;

static void set_attention(int sig) { iomanage::attention = 1; }

void iomanage::set_async(bool async) {
    if (async) {
	static bool first_time = true;
	if (first_time) {
	    signal(SIGIO, set_attention);
	    first_time = false; }
	fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_ASYNC | O_NONBLOCK);
	fcntl(fd, F_SETOWN, getpid());
	attention = 1;
    } else
	fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) & ~(O_NONBLOCK | O_ASYNC));
}

iomanage::iomanage(int fd, bool async) : fd(fd), info(0), useCR(false),
    throttled(false), records(0), dropped(0)
{
    fcntl(fd, F_SETFD, fcntl(fd, F_GETFD) | FD_CLOEXEC);
    if (async) set_async();
    if ((size_t)fd >= fds.size()) fds.resize(fd + 1);
    if (fds[fd]) ERROR(fd << " already in use, will leak!");
    fds[fd] = this;
    TRACE("start on " << fd);
    if (!FD_ISSET(fd, &active) && !FD_ISSET(fd, &output_wait)) active_count++;
    FD_SET(fd, &active);
}

iomanage::iomanage(iomanage &&a)
: onClose(std::move(a.onClose)), fd(a.fd), info(a.info), ibuf(std::move(a.ibuf)),
  obuf(std::move(a.obuf)), useCR(a.useCR), throttled(a.throttled), records(a.records),
  dropped(a.dropped)
{
    if (fd >= 0) fds[fd] = this;
    a.onClose = std::function<void(struct iomanage *)>();
    a.fd = -1;
    a.info = 0;
}

iomanage::~iomanage() {
    assert(!info);
    if (fd >= 0) {
        fds[fd] = nullptr;
        if (FD_ISSET(fd, &active) || FD_ISSET(fd, &output_wait)) {
            --active_count;
            FD_CLR(fd, &active);
            FD_CLR(fd, &output_wait); } }
}

void iomanage::set_ready_fn(std::function<void(iomanage *)> fn) { assert(0); }
void iomanage::set_cmd_fn(std::function<int(iomanage *, chars_ref)> fn) { assert(0); }

struct iomanage_generic : iomanage {
    std::function<void(struct iomanage *)> ready_fn;
    iomanage_generic(int fd, std::function<void(struct iomanage *)> fn)
	: iomanage(fd), ready_fn(fn)  {}
    void ready() { ready_fn(this); }
    void set_ready_fn(std::function<void(struct iomanage *)> fn) {
	ready_fn = fn; }
};

iomanage *iomanage::start(std::function<void(iomanage *)> fn, int fd, bool async) {
    return new iomanage_generic(fd, fn);
}

void iomanage::close() {
    TRACE("Closing connection " << fd);
    if (onClose) onClose(this);
    ::close(fd);
    delete this;
}

void iomanage::ignore(bool on) {
    if (on) {
	if (FD_ISSET(fd, &active) && !FD_ISSET(fd, &output_wait)) active_count--;
	FD_CLR(fd, &active);
    } else {
	if (!FD_ISSET(fd, &active) && !FD_ISSET(fd, &output_wait)) active_count++;
	FD_SET(fd, &active); }
}

void iomanage::wait_for_output(bool on) {
    if (on) {
        if (!FD_ISSET(fd, &active) && !FD_ISSET(fd, &output_wait)) active_count++;
        FD_SET(fd, &output_wait);
    } else {
        if (!FD_ISSET(fd, &active) && FD_ISSET(fd, &output_wait)) active_count--;
        FD_CLR(fd, &output_wait); }
}

int iomanage::read() {
    size_t off = ibuf.size();
    ibuf.resize(off + 1024);
    int rv = ::read(fd, &ibuf[off], 1024);
    if (rv <= 0) {
	ibuf.resize(off);
	if (rv < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
	    TRACE("no data reading from " << fd);
	    return 0; }
	if (rv < 0)
	    WARN("error " << strerror(errno) << " reading from " << fd);
	else
	    INFO("eof reading from " << fd);
	return -1;
    } else
	ibuf.resize(off + rv);
    return 0;
}

int iomanage::write() {
    if (obuf.size()) {
	int rv = ::write(fd, obuf.data(), obuf.size());
	if (rv <= 0) {
	    if (rv < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
	        if (!throttled) {
		    TRACE("buffer full writing to " << fd << " after " <<
		          records << " records, throttling");
		    records = 0;
		    throttled = true; }
                wait_for_output(true);
		return 0; }
	    if (rv < 0)
		WARN("error " << strerror(errno) << " writing to " << fd);
	    else
		INFO("eof writing to " << fd);
            wait_for_output(false);
	    return -1;
	} else {
	    if (throttled) {
		TRACE("unthrottling " << fd << " after " << dropped <<
		      " dropped records");
		dropped = 0;
		throttled = false; }
	    if (rv == (int)obuf.size()) {
                wait_for_output(false);
		obuf.clear();
	    } else {
                wait_for_output(true);
		obuf.erase(0, rv); } }
    } else {
        wait_for_output(false);
    }
    return 0;
}

void iomanage::run(bool nonblock) {
    struct timeval zero = { 0, 0 };
    while (active_count > 0 || timeout::wait.size() > timeout::ignore_count) {
	timeval wait;
	while (!timeout::wait.empty() &&
	       (wait = timeout::wait.top().when - now()) <= 0)
	{
	    std::function<void()> fn = std::move(timeout::wait.top().what);
	    timeout::wait.pop();
	    fn(); 
	    if (active_count <= 0 && timeout::wait.empty())
		return; }
	fd_set	read_fds = active, write_fds = output_wait;
	if (LOG_TRACE3) {
	    std::cerr << "select(" << fds.size() << ", ";
	    for (unsigned i = 0; i < fds.size(); i++)
		std::cerr << (FD_ISSET(i, &read_fds) ? '1' : '0');
	    std::cerr << ", ";
	    for (unsigned i = 0; i < fds.size(); i++)
		std::cerr << (FD_ISSET(i, &write_fds) ? '1' : '0');
	    if (!timeout::wait.empty())
		std::cerr << ", " << (wait.tv_sec + wait.tv_usec/1000000.0);
	    std::cerr << ") = " << std::flush; }
	attention = 0;
	int cnt = select(fds.size(), &read_fds, &write_fds, 0,
			 nonblock ? &zero : timeout::wait.empty() ? 0 : &wait);
	if (LOG_TRACE3) {
	    if (cnt < 0) std::cerr << strerror(errno);
	    else std::cerr << cnt;
	    std::cerr << " (";
	    for (unsigned i = 0; i < fds.size(); i++)
		std::cerr << (FD_ISSET(i, &read_fds) ? '1' : '0');
	    std::cerr << ", ";
	    for (unsigned i = 0; i < fds.size(); i++)
		std::cerr << (FD_ISSET(i, &write_fds) ? '1' : '0');
	    std::cerr << ")" << std::endl; }
	if (cnt <= 0) {
	    if (cnt < 0 && errno != EAGAIN && errno != EINTR)
		perror("select");
	    if (!nonblock) continue;
	    return; }
	for (int fd = 0; (size_t)fd < fds.size(); fd++) {
	    if (FD_ISSET(fd, &write_fds) && fds[fd])
		fds[fd]->write();
	    if (FD_ISSET(fd, &read_fds) && fds[fd])
		fds[fd]->ready(); } }
}

void iomanage::closeAll() {
    for (int i = 0; (size_t)i < fds.size(); i++)
	if (fds[i]) fds[i]->close();
}
