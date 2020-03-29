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
#ifndef _hex_h_
#define _hex_h_

#include <iostream>
#include <iomanip>

class hex {
    intmax_t	val;
    int		width;
    char	fill;
public:
    hex(intmax_t v, int w=0, char f=' ') : val(v), width(w), fill(f) {}
    hex(void *v, int w=0, char f=' ') : val((intmax_t)v), width(w), fill(f) {}
    friend std::ostream &operator<<(std::ostream &os, const hex &h);
};

inline std::ostream &operator<<(std::ostream &os, const hex &h) {
    auto save = os.flags();
    os << std::hex << std::setw(h.width) << std::setfill(h.fill) << h.val;
    os.flags(save);
    return os; }

#endif /* _hex_h_ */
