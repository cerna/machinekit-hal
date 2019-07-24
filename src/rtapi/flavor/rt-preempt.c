/********************************************************************
* Description:  rt-preempt.c
*
*               This file, 'rt-preempt.c', implements the unique
*               functions for the RT_PREEMPT thread system.
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

#include "rtapi_flavor.h"
#include "rt-preempt.h"

// if this exists, and contents is '1', it's RT_PREEMPT
#define PREEMPT_RT_SYSFS "/sys/kernel/realtime"

#ifdef RTAPI

// Access the rtpreempt_stats_t thread status object
#define FTS(ts) ((rtpreempt_stats_t *)&(ts->flavor))

// Access the rtpreempt_exception_t thread exception detail object
#define FED(detail) ((rtpreempt_exception_t)detail.flavor)

void rtpreempt_exception_handler_hook(int type,
                                      rtapi_exception_detail_t *detail,
                                      int level)
{
    rtapi_threadstatus_t *ts = &global_data->thread_status[detail->task_id];
    switch ((rtpreempt_exception_id_t)type)
    {
        // Timing violations
    case RTP_DEADLINE_MISSED:
        rtapi_print_msg(level,
                        "%d: Unexpected realtime delay on RT thread %d ",
                        type, detail->task_id);
        rtpreempt_print_thread_stats(detail->task_id);
        break;

    default:
        rtapi_print_msg(level,
                        "%d: unspecified exception detail=%p ts=%p",
                        type, detail, ts);
    }
}

void rtpreempt_print_thread_stats(int task_id)
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

static int kernel_is_rtpreempt()
{
    FILE *fd;
    int retval = 0;

    if ((fd = fopen(PREEMPT_RT_SYSFS, "r")) != NULL)
    {
        int flag;
        retval = ((fscanf(fd, "%d", &flag) == 1) && (flag));
        fclose(fd);
    }
    return retval;
}

#ifdef RTAPI
static flavor_descriptor_t flavor_rt_prempt_descriptor = {
    .name = "rt-preempt",
    .flavor_id = 3,
    .flavor_magic = 1,
    .flags = FLAVOR_DOES_IO + FLAVOR_IS_RT,
    .exception_handler_hook = rtpreempt_exception_handler_hook,
    .module_init_hook = posix_module_init_hook,
    .module_exit_hook = NULL,
    .task_update_stats_hook = NULL,
    .task_print_thread_stats_hook = rtpreempt_print_thread_stats,
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
static flavor_descriptor_t flavor_rt_preempt_descriptor = {
    .name = "rt-preempt",
    .flavor_id = 3,
    .flavor_magic = 1,
    .module_init_hook = posix_module_init_hook};
#endif

static void lib_init(void)
{
    // Only register the Preempt_RT flavour if and only if the current kernel is patched
    if (kernel_is_rtpreempt)
    {
        register_flavor(flavor_rt_preempt_descriptor);
    }
}
static void lib_fini(void)
{
    unregister_flavor(flavor_rt_preempt_descriptor);
}

FLAVOR_STAMP("rt-preempt", 3, 1, 1)