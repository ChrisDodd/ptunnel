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
#ifndef _iomanage_h_
#define _iomanage_h_

#include <assert.h>
#include "chars_ref.h"
#include <functional>
#include <queue>
#include <set>
#include <signal.h>
#include <stdlib.h>
#include <string>
#include <sys/select.h>
#include "timeval.h"
#include <vector>

#ifndef O_ASYNC
#define O_ASYNC FASYNC
#endif

struct iomanage {
    std::function<void(struct iomanage *)> onClose;
    int		fd;
    void 	*info;
    std::string	ibuf, obuf;
    bool	useCR, throttled;
    unsigned long	records, dropped;
    class timeout {
	friend struct iomanage;
	timeval			when;
	std::function<void()>	what;
	bool			ignore;
	static size_t		ignore_count;
	static std::priority_queue<timeout>	wait;
	timeout(timeval w, std::function<void()> f, bool ign = false) :
	    when(w), what(f), ignore(ign) {
	    if (ignore) ignore_count++; }
    public:
	~timeout() { if (ignore) ignore_count--; }
	bool operator < (const timeout &a) const { return a.when < when; }
    };
    friend class timeout;
protected:
    iomanage(int fd, bool async = true);
    iomanage(const iomanage &a) = delete;
    iomanage(iomanage &&a);
public:
    void set_async(bool async = true);
    virtual ~iomanage();

    static volatile sig_atomic_t	attention;
    static fd_set			active, output_wait;
    static int				active_count;
    static std::vector<iomanage *>	fds;

    static iomanage *start(std::function<void(iomanage *)> fn, int fd,
			   bool async = true);
    static void timer(timeval when, std::function<void()> what,
		      bool ign = false) {
	timeout::wait.push(timeout(when+now(), what, ign)); }
    static void timerAt(timeval when, std::function<void()> what,
			bool ign = false) {
	timeout::wait.push(timeout(when, what, ign)); }
    virtual void ready() = 0;
    virtual void set_ready_fn(std::function<void(iomanage *)> fn);
    virtual void set_cmd_fn(std::function<int(iomanage *, chars_ref)> fn);
    virtual void set_debug(int level) {}
    void ignore(bool on = true);
    void wait_for_output(bool on = true);
    virtual void close();
    virtual int read();
    virtual int write();
    void obuf_append(chars_ref l) {
	if (useCR) {
	    while (auto nl = l.find('\n')) {
		obuf += l.before(nl);
		obuf += "\r\n";
		l = l.after(nl)+1; } }
	obuf += l; }
    void obuf_append_line(chars_ref l) {
	obuf_append(l);
	if (useCR) obuf += '\r';
	obuf += '\n'; }
    void lineout(chars_ref l, unsigned prio = 0) {
	if (write() < 0) close();
	else if (obuf.size() <= (prio << 10)) {
	    obuf_append_line(l);
	    records++;
	    if (write() < 0) close();
	} else {
	    dropped++; } }
    static void run(bool nonblock = false);
    static void closeAll();
};

#endif /* _iomanage_h_ */
