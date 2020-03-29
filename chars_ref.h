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
#ifndef _chars_ref_h_
#define _chars_ref_h_

#include <assert.h>
#include <iostream>
#include <stdlib.h>
#include <string>
#include <string.h>

struct chars_ref {
    const char	*p;
    size_t	len;
    chars_ref() : p(0), len(0) {}
    chars_ref(const char *str, size_t l) : p(l ? str : 0), len(l) {}
    chars_ref(const char *str) : p(str), len(str ? strlen(str) : 0) {}
    chars_ref(const std::string &str) : p(str.data()), len(str.size()) {}
    void clear() { p = 0; len = 0; }
    chars_ref(const chars_ref &a) : p(a.p), len(a.len) {}
    chars_ref &operator=(const chars_ref &a) {
	p = a.p; len = a.len; return *this; }
    explicit operator bool const () { return p != 0; }

    bool operator==(const chars_ref &a) const {
	return p ? (a.p && len == a.len && (!len || !strncmp(p, a.p, len)))
		 : !a.p; }
    bool operator==(const std::string &a) const {
	return p ? (len == a.size() && (!len || !strncmp(p, a.data(), len)))
		 : false; }
    bool operator==(const char *a) const {
	return p ? (a && (!len || !strncmp(p, a, len)) && !a[len]) : !a; }
    template <class T> bool operator!=(T a) const { return !(*this == a); }

    int compare(const chars_ref &a) const {
	if (!p) return a.p ? -1 : 0;
	if (!a.p) return 1;
	int rv = strncmp(p, a.p, std::min(len, a.len));
	if (!rv && len != a.len) rv = len < a.len ? -1 : 1;
	return rv; }
    int compare(const std::string &a) const {
	if (!p) return -1;
	int rv = strncmp(p, a.data(), std::min(len, a.size()));
	if (!rv && len != a.size()) rv = len < a.size() ? -1 : 1;
	return rv; }
    int compare(const char *a) const {
	if (!p) return -1;
	int rv = strncmp(p, a, len);
	if (!rv && a[len]) rv = -1;
	return rv; }
    template <class T> bool operator<(T a) const { return compare(a) < 0; }
    template <class T> bool operator<=(T a) const { return compare(a) <= 0; }
    template <class T> bool operator>(T a) const { return compare(a) > 0; }
    template <class T> bool operator>=(T a) const { return compare(a) >= 0; }

    operator std::string() const { return std::string(p, len); }
    std::string string() const { return std::string(p, len); }
    chars_ref &operator+=(size_t i) {
	if (len < i) { p = 0; len = 0; }
	else { p += i; len -= i; }
	return *this; }
    chars_ref &operator++() { p++; if (len) len--; else p = 0; return *this; }
    chars_ref operator++(int) { chars_ref rv(*this); ++*this; return rv; }
    char operator[](size_t i) const { return i < len ? p[i] : 0; }
    char operator*() const { return len ? *p : 0; }
    chars_ref operator+(size_t i) const {
	chars_ref rv(*this); rv += i; return rv; }
    chars_ref trunc(size_t len) const {
        return chars_ref(p, std::min(len, this->len)); }
    chars_ref substr(size_t off, size_t len) const {
        if (off > this->len) return chars_ref();
        return chars_ref(p+off, std::min(len, this->len - off)); }
    chars_ref &trim(const char *white = " \t\r\n") {
	while (len > 0 && strchr(white, *p)) { p++; len--; }
	while (len > 0 && strchr(white, p[len-1])) {len--; }
	return *this; }
    bool trimCR() {
	bool rv = false;
	while (len > 0 && p[len-1] == '\r') { rv = true; len--; }
	return rv; }
    chars_ref trim(const char *white = " \t\r\n") const {
	chars_ref rv(*this);
	rv.trim(white);
	return rv; }
    const char *begin() const { return p; }
    const char *end() const { return p + len; }
    const char *find(char ch) const {
	return p ? (const char *)memchr(p, ch, len) : p; }
    const char *find(const char *set) const {
	if (!p) return 0;
	size_t off = strcspn(p, set);
	return off >= len ? 0 : p + off; }
    chars_ref before(const char *s) const {
	return (size_t)(s-p) <= len ? chars_ref(p, s-p) : chars_ref(); }
    chars_ref after(const char *s) const {
	return (size_t)(s-p) <= len ? chars_ref(s, p+len-s) : chars_ref(); }
};

template <class T> inline
    bool operator==(T a, const chars_ref &b) { return b == a; }
template <class T> inline
    bool operator!=(T a, const chars_ref &b) { return b != a; }
template <class T> inline
    bool operator>=(T a, const chars_ref &b) { return b <= a; }
template <class T> inline
    bool operator>(T a, const chars_ref &b) { return b < a; }
template <class T> inline
    bool operator<=(T a, const chars_ref &b) { return b >= a; }
template <class T> inline
    bool operator<(T a, const chars_ref &b) { return b > a; }

inline std::ostream &operator<<(std::ostream &os, const chars_ref &a) {
    return a.len ? os.write(a.p, a.len) : os;  }
inline std::string &operator+=(std::string &s, const chars_ref &a) {
    return a.len ? s.append(a.p, a.len) : s; }
inline std::string operator+(const std::string &s, const chars_ref &a) {
    std::string rv(s); rv += a; return rv; }

template<class T> inline chars_ref operator>>(chars_ref p, T &a) {
    if (const char *e = p.begin() >> a)
	p += e - p.begin();
    else
	p.clear();
    return p;
}

chars_ref operator>>(chars_ref p, int &a);

#endif /* _chars_ref_h_ */
