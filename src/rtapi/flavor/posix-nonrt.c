/********************************************************************
* Description:  posix-nonrt.c
*
*               This file, 'posix-nonrt.c', implements the unique
*               functions for the POSIX non real-time thread system.
*
* Copyright (C) 2012, 2013 Michael Büsch <m AT bues DOT CH>,
*                          John Morris <john AT zultron DOT com>,
*                          Michael Haberler <license AT mah DOT priv DOT at>
*               2019       Jakub Fišer <jakub DOT fiser AT erythio DOT net
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
*
********************************************************************/

#include "posix-nonrt.h"
#include "rtapi_flavor.h"

#ifdef RTAPI

// Access the posix_stats_t thread status object
#define FTS(ts) ((posix_stats_t *)&(ts->flavor))

static void posix_print_thread_stats(int task_id)
{
    rtapi_threadstatus_t *ts =
        &global_data->thread_status[task_id];

    rtapi_print("    wait_errors=%d\t",
                FTS(ts)->wait_errors);
    rtapi_print("usercpu=%lduS\t",
                FTS(ts)->utime_sec * 1000000 +
                    FTS(ts)->utime_usec);
    rtapi_print("syscpu=%lduS\t",
                FTS(ts)->stime_sec * 1000000 +
                    FTS(ts)->stime_usec);
    rtapi_print("nsigs=%ld\n",
                FTS(ts)->ru_nsignals);
    rtapi_print("    ivcsw=%ld\t",
                FTS(ts)->ru_nivcsw -
                    FTS(ts)->startup_ru_nivcsw);
    rtapi_print("    minflt=%ld\t",
                FTS(ts)->ru_minflt -
                    FTS(ts)->startup_ru_minflt);
    rtapi_print("    majflt=%ld\n",
                FTS(ts)->ru_majflt -
                    FTS(ts)->startup_ru_majflt);
    rtapi_print("\n");
}

#endif
#ifdef RTAPI
static flavor_descriptor_t flavor_posix_nonrt_descriptor = {
    .name = "posix-nonrt",
    .flavor_id = 2,
    .flavor_magic = 1,
    .flags = 0,
    .exception_handler_hook = NULL,
    .module_init_hook = posix_module_init_hook,
    .module_exit_hook = NULL,
    .task_update_stats_hook = NULL,
    .task_print_thread_stats_hook = posix_print_thread_stats,
    .task_new_hook = posix_task_new_hook,
    .task_delete_hook = posix_task_delete_hook,
    .task_start_hook = posix_task_start_hook,
    .task_stop_hook = posix_task_stop_hook,
    .task_pause_hook = NULL,
    .task_wait_hook = posix_wait_hook,
    .task_resume_hook = NULL,
    .task_delay_hook = posix_task_delay_hook,
    .get_time_hook = NULL,
    .get_clocks_hook = NULL,
    .task_self_hook = posix_task_self_hook,
    .task_pll_get_reference_hook = posix_task_pll_get_reference_hook,
    .task_pll_set_correction_hook = posix_task_pll_set_correction_hook};
#else
static flavor_descriptor_t flavor_posix_nonrt_descriptor = {
    .name = "posix-nonrt",
    .flavor_id = 2,
    .flavor_magic = 1,
    .flags = 0,
    .module_init_hook = posix_module_init_hook};
#endif

static void lib_init(void)
{
    // POSIX non real-time flavour should always be able to run
    // So no checks against current system are made
    register_flavor(flavor_posix_nonrt_descriptor);
}
static void lib_fini(void)
{
    unregister_flavor(flavor_posix_nonrt_descriptor);
}

FLAVOR_STAMP("posix-nonrt", 2, 0, 1)