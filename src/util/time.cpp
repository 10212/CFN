/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (C) 2014 Named Data Networking Project
 * See COPYING for copyright and distribution information.
 */

#include "common.hpp"

#include "time.hpp"
#include <time.h>
#include <stdexcept>
#include <sys/time.h>

namespace ndn {
namespace time {

Point
now()
{
// based on suggestion from https://svn.boost.org/trac/boost/ticket/3504
#if defined(_POSIX_TIMERS) && ( _POSIX_TIMERS > 0 ) && defined(_POSIX_MONOTONIC_CLOCK)

  struct timespec t;
  int res = clock_gettime(CLOCK_MONOTONIC, &t);

  if (res == -1) {
    throw std::runtime_error("clock_gettime");
  }

  return Point(time::seconds(t.tv_sec) + time::nanoseconds(t.tv_nsec));

#else

  // fallback to wall clock time

  struct timeval tv;
  int res = gettimeofday(&tv, 0);

  if (res == -1) {
    throw std::runtime_error("gettimeofday");
  }

  return Point(time::seconds(tv.tv_sec) + time::microseconds(tv.tv_usec));

#endif
}

} // namespace time
} // namespace ndn
