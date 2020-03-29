#ifndef _log_h_
#define _log_h_
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
#include <iostream>
#include <vector>
extern int verbose;

#define LOG_ERROR	(verbose >= 0)
#define LOG_WARN	(verbose >= 1)
#define LOG_INFO	(verbose >= 2)
#define LOG_TRACE	(verbose >= 3)
#define LOG_TRACE2	(verbose >= 4)
#define LOG_TRACE3	(verbose >= 5)

#ifdef __cplusplus
extern "C" {
#endif
const char *log_timestamp();
#ifdef __cplusplus
}
#endif

#define ERROR(X)	(LOG_ERROR ? (std::cerr << log_timestamp() << X << std::endl) : std::cerr)
#define WARN(X)		(LOG_WARN ? (std::cerr << log_timestamp() << X << std::endl) : std::cerr)
#define INFO(X)		(LOG_INFO ? (std::cerr << log_timestamp() << X << std::endl) : std::cerr)
#define TRACE(X)	(LOG_TRACE ? (std::cerr << log_timestamp() << X << std::endl) : std::cerr)
#define TRACE2(X)	(LOG_TRACE2 ? (std::cerr << log_timestamp() << X << std::endl) : std::cerr)
#define TRACE3(X)	(LOG_TRACE3 ? (std::cerr << log_timestamp() << X << std::endl) : std::cerr)

template<class T> std::ostream &operator<<(std::ostream &out, const std::vector<T> &v) {
    const char *sep = " ";
    out << '[';
    for (auto &el : v) {
        out << sep << el;
        sep = ", "; }
    return out << (sep+1) << ']';
}

#endif /* _log_h_ */
