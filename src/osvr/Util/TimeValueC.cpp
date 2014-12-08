/** @file
    @brief Implementation

    @date 2014

    @author
    Ryan Pavlik
    <ryan@sensics.com>
    <http://sensics.com>
*/

// Copyright 2014 Sensics, Inc.
//
// All rights reserved.
//
// (Final version intended to be licensed under
// the Apache License, Version 2.0)

// Internal Includes
#include <osvr/Util/TimeValueC.h>

// Library/third-party includes
#include <vrpn_Shared.h>

// Standard includes
#if defined(OSVR_HAVE_STRUCT_TIMEVAL_IN_SYS_TIME_H)
#include <sys/time.h>
typedef time_t tv_seconds_type;
typedef suseconds_t tv_microseconds_type;
#elif defined(OSVR_HAVE_STRUCT_TIMEVAL_IN_WINSOCK2_H)
//#include <winsock2.h>
typedef long tv_seconds_type;
typedef long tv_microseconds_type;
#endif

#define OSVR_USEC_PER_SEC 1000000;

void osvrTimeValueNormalize(struct OSVR_TimeValue *tv) {
    if (!tv) {
        return;
    }
    const OSVR_TimeValue_Microseconds rem =
        tv->microseconds / OSVR_USEC_PER_SEC;
    tv->seconds += rem;
    tv->microseconds -= rem * OSVR_USEC_PER_SEC;
    /* By here, abs(microseconds) < OSVR_USEC_PER_SEC:
       now let's get signs the same. */
    if (tv->seconds > 0 && tv->microseconds < 0) {
        tv->seconds--;
        tv->microseconds += OSVR_USEC_PER_SEC;
    } else if (tv->seconds < 0 && tv->microseconds > 0) {
        tv->seconds++;
        tv->microseconds -= OSVR_USEC_PER_SEC;
    }
}

void osvrTimeValueSum(struct OSVR_TimeValue *tvA,
                      const struct OSVR_TimeValue *tvB) {
    if (!tvA || !tvB) {
        return;
    }
    tvA->seconds += tvB->seconds;
    tvA->microseconds += tvB->microseconds;
    osvrTimeValueNormalize(tvA);
}

void osvrTimeValueDifference(struct OSVR_TimeValue *tvA,
                             const struct OSVR_TimeValue *tvB) {
    if (!tvA || !tvB) {
        return;
    }
    tvA->seconds -= tvB->seconds;
    tvA->microseconds -= tvB->microseconds;
    osvrTimeValueNormalize(tvA);
}

#ifdef OSVR_HAVE_STRUCT_TIMEVAL

void osvrTimeValueGetNow(OSVR_INOUT_PTR struct OSVR_TimeValue *dest) {
    timeval tv;
    vrpn_gettimeofday(&tv, nullptr);
    osvrStructTimevalToTimeValue(dest, &tv);
}

void osvrTimeValueToStructTimeval(timeval *dest,
                                  const struct OSVR_TimeValue *src) {
    if (!dest || !src) {
        return;
    }
    dest->tv_sec = tv_seconds_type(src->seconds);
    dest->tv_usec = tv_microseconds_type(src->microseconds);
}

void osvrStructTimevalToTimeValue(struct OSVR_TimeValue *dest,
                                  const struct timeval *src) {
    if (!dest || !src) {
        return;
    }
    dest->seconds = src->tv_sec;
    dest->microseconds = src->tv_usec;
    osvrTimeValueNormalize(dest);
}

#endif
