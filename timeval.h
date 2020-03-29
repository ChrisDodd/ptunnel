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
#ifndef _timeval_h_
#define _timeval_h_

extern "C" {
#include <ctype.h>
#include <sys/time.h>
}

#include <iostream>
#include <iomanip>

inline timeval &operator += (timeval &a, const timeval &b) {
    a.tv_sec += b.tv_sec;
    a.tv_usec += b.tv_usec;
    if (a.tv_usec >= 1000000) {
	a.tv_usec -= 1000000;
	a.tv_sec++; }
    return a; }
inline timeval &operator += (timeval &a, time_t b) {
    a.tv_sec += b;
    return a; }
inline timeval &operator -= (timeval &a, const timeval &b) {
    a.tv_sec -= b.tv_sec;
    a.tv_usec -= b.tv_usec;
    if (a.tv_usec < 0) {
	a.tv_usec += 1000000;
	a.tv_sec--; }
    return a; }
inline timeval &operator -= (timeval &a, time_t b) {
    a.tv_sec -= b;
    return a; }
inline timeval operator + (const timeval &a, const timeval &b) {
    timeval rv(a); rv += b; return rv; }
inline timeval operator + (const timeval &a, time_t b) {
    timeval rv(a); rv += b; return rv; }
inline timeval operator + (time_t a, const timeval &b) {
    timeval rv(b); rv += a; return rv; }
inline timeval operator - (const timeval &a, const timeval &b) {
    timeval rv(a); rv -= b; return rv; }
inline timeval operator - (const timeval &a, time_t b) {
    timeval rv(a); rv -= b; return rv; }
inline timeval operator - (time_t a, const timeval &b) {
    timeval rv = { a, 0 }; rv -= b; return rv; }
inline bool operator < (const timeval &a, const timeval &b) {
    return a.tv_sec == b.tv_sec ? a.tv_usec < b.tv_usec : a.tv_sec < b.tv_sec; }
inline bool operator > (const timeval &a, const timeval &b) { return b < a; }
inline bool operator <= (const timeval &a, const timeval &b) { return !(b<a); }
inline bool operator >= (const timeval &a, const timeval &b) { return !(a<b); }
inline bool operator < (const timeval &a, time_t b) { return a.tv_sec < b; }
inline bool operator <= (const timeval &a, time_t b) {
    return a.tv_sec == b ? a.tv_usec == 0 : a.tv_sec < b; }
inline bool operator > (time_t a, const timeval &b) { return a > b.tv_sec; }
inline bool operator >= (const timeval &a, time_t b) { return a.tv_sec >= b; }
inline bool operator > (const timeval &a, time_t b) {
    return a.tv_sec == b ? a.tv_usec > 0 : a.tv_sec > b; }
inline bool operator <= (time_t a, const timeval &b) { return a <= b.tv_sec; }

inline timeval now() { timeval rv; gettimeofday(&rv, 0); return rv; }
inline timeval days(long s) { timeval rv = { 24*3600*s, 0 }; return rv; }
inline timeval hours(long s) { timeval rv = { 3600*s, 0 }; return rv; }
inline timeval minutes(long s) { timeval rv = { 60*s, 0 }; return rv; }
inline timeval seconds(int s) { timeval rv = { s, 0 }; return rv; }
inline timeval seconds(long s) { timeval rv = { s, 0 }; return rv; }
inline timeval seconds(double s) {
    timeval rv = { (time_t)s, 0 };
    rv.tv_usec = (s-rv.tv_sec) * 1000000;
    return rv; }
inline timeval millisec(long s) { timeval rv = { s/1000, (int)((s%1000)*1000) };
				  return rv; }
inline timeval microsec(long s) { timeval rv = { s/1000000, (int)(s%1000000) };
				  return rv; }
inline double seconds(const timeval &t) { return t.tv_sec + t.tv_usec/1e6; }

inline std::ostream &operator<<(std::ostream &os, timeval t) {
    char ofill = os.fill('0');
    if (t.tv_sec < 1000000) {
	if (t.tv_sec >= 60) {
	    if (t.tv_sec >= 3600) {
		os << t.tv_sec/3600 << ':';
		t.tv_sec %= 3600;
		os << std::setw(2); }
	    os << t.tv_sec/60 << ':';
	    t.tv_sec %= 60;
	    os << std::setw(2); }
	os << t.tv_sec;
    } else {
	char buffer[32];
	time_t tmp = t.tv_sec;
	strftime(buffer, sizeof(buffer), "%F %T", localtime(&tmp));
	os << buffer; }
    if (t.tv_usec)
	os << '.' << std::setw(6) << t.tv_usec;
    os.fill(ofill);
    return os;
}



#if 0
inline std::istream &operator>>(std::istream &is, timeval &t) {
    char buffer[32], *p;
    struct tm tm;
    is.get(buffer, 32, ",\n");
    if (!(p = strptime(buffer, "%F %T", &tm)))
	is.setstate(ios_base::failbit);
    else {
	t.tv_sec = mktime(&tm);
	if (*p == '.') {
	    char *us = p+1;
	    t.tv_usec = strtol(us, &p, 10);
	    while (p-us < 6) {
		t.tv_usec *= 10;
		us--; } }
	while (isspace(*p)) p++;
	if (*p)
	    is.setstate(ios_base::failbit); }
    return is;
}
#endif

inline const char *operator>>(const char *p, timeval &rv) {
    struct tm	tm;
    timeval	t;
    if (!p) return 0;
    while (isspace(*p)) p++;
    const char *s = p;
    t.tv_sec = strtoull(p, (char **)&p, 10);
    if (p == s) return 0;
    if (*p == '-' && t.tv_sec >= 1970 && t.tv_sec < 3000) {
	if (!(p = strptime(s, "%F %T", &tm)))
	    return 0;
	t.tv_sec = mktime(&tm);
    } else if (*p == ':' && t.tv_sec < 60 && isdigit(p[1])) {
	t.tv_sec *= 60;
	t.tv_sec += strtoull(p+1, (char **)&p, 10);
	if (*p == ':' && isdigit(p[1])) {
	    t.tv_sec *= 60;
	    t.tv_sec += strtoull(p+1, (char **)&p, 10); } }
    if (*p == '.') {
	const char *us = p+1;
	t.tv_usec = strtol(us, (char **)&p, 10);
	while (p-us < 6) {
	    t.tv_usec *= 10;
	    us--; }
    } else t.tv_usec = 0;
    rv = t;
    return p;
}

inline char *operator>>(char *p, timeval &t) {
    return const_cast<char *>(const_cast<const char *>(p) >> t);
}

#endif /* _timeval_h_ */
