/********************************************************************
* Description:  posix-nort.h
*               POSIX non real-time flavor descriptors
*
*
* Copyright (C) 2019       John Morris <john AT zultron DOT com>
                2019       Jakub Fišer <jakub DOT fiser AT erythio net>
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public
* License as published by the Free Software Foundation; either
* version 2.1 of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public
* License along with this library; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
********************************************************************/

#include "rtapi_flavor.h"

static void __attribute__((constructor)) lib_init(void);
static void __attribute__((destructor)) lib_fini(void);

typedef struct {

    int wait_errors; // RT deadline missed

    // filled in by rtapi_thread_update_stats() RTAPI method
    long utime_sec;      // user CPU time used
    long utime_usec;

    long stime_sec;      // system CPU time used
    long stime_usec;

    long ru_minflt;        // page reclaims (soft page faults)
    long ru_majflt;        // page faults (hard page faults)
    long ru_nsignals;      // signals received
    long ru_nivcsw;        // involuntary context switches

    long startup_ru_minflt; // page fault counts at end of
    long startup_ru_majflt; // initalisation
    long startup_ru_nivcsw; //

} posix_stats_t;
// Check the stats struct size
ASSERT_SIZE_WITHIN(posix_stats_t, MAX_FLAVOR_THREADSTATUS_SIZE);

extern flavor_descriptor_t flavor_posix_nonrt_descriptor;