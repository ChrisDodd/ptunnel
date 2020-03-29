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
#include <ctype.h>
#include "connect.h"
#include <errno.h>
#include <netdb.h>
#include "network.h"
#include <stdlib.h>

const char *operator>>(const char *p, in_addr &addr)
{
    uint32_t	a = 0;
    int		shift = 24;
    if (!p || !isdigit(*p)) {
	errno = EINVAL;
	return 0; }
    errno = 0;
    while (shift >= 0) {
	unsigned long c = strtoul(p, (char **)&p, 10);
	if (errno) return 0;
	if (*p != '.') {
	    if ((c >> shift) >> 8) { errno = ERANGE; return 0; }
	    a += c;
	    break; }
	if (c > 255) { errno = ERANGE; return 0; }
	a += c << shift;
	p++;
	shift -= 8; }
    addr.s_addr = htonl(a);
    return p;
}

endpoint::endpoint(const char *p, const char **rest)
{
    addr.s_addr = 0;
    port = 0;
    if (!p || !*p) {
	errno = EINVAL;
	return; }
    errno = 0;
    if (isalpha(*p)) {
	struct addrinfo *info = get_addrinfo(p);
	if (!info || !((sockaddr_in *)info->ai_addr)->sin_port)
	    errno = EINVAL;
	else {
	    addr = ((sockaddr_in *)info->ai_addr)->sin_addr;
	    port = ((sockaddr_in *)info->ai_addr)->sin_port;
            if (rest) *rest = ""; }
	freeaddrinfo(info);
	return; }
    if (!(p = p >> addr)) return;
    if (*p == ':') port = htons(strtol(p+1, (char **)&p, 10));
    if (errno || (*p && !rest)) {
	addr.s_addr = 0;
	port = 0;
    } else if (rest)
	*rest = p;
}

network::network(const char *p, const char **rest)
{
    uint32_t	net = 0;
    addr.s_addr = ~0;
    masksz = -1;
    if (!p || !*p) {
	errno = EINVAL;
	return; }
    errno = 0;
    if (!(p = p >> addr)) return;
    if (*p == '/') {
	masksz = strtol(p+1, (char **)&p, 10);
	if (errno || (*p && !rest)) {
	    masksz = -1;
	    addr.s_addr = ~0;
	    return; }
    } else if (errno || (*p && !rest)) {
	addr.s_addr = ~0;
	return;
    } else switch(net >> 29) {
	case 0: case 1: case 2: case 3:
	    masksz = 8;
	    break;
	case 4: case 5:
	    masksz = 16;
	    break;
	case 6:
	    masksz = 24;
	    break;
	default:
	    addr.s_addr = ~0;
	    return; }
    addr.s_addr &= mask();
    if (rest) *rest = p;
}

