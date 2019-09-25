/********************************************************************
* Description:  rtapi_cmdline_args.c
*               This file, 'rtapi_cmdline_args.c', implements functions
*               used for exporting and manipulation of command line arguments
*               passed to each application as a 'int argc, char** argv',
*               including the process name and information shown when using
*               the 'ps' command
*
* Copyright (C) 2019        Jakub Fišer <jakub DOT fiser AT erythio DOT net>
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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <errno.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <features.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>

#include <linux/version.h>

#include "rtapi_cmdline_args.h"
#include "syslog_async.h"

#if (__GLIBC__ >= 2 && __GLIBC_MINOR__ >= 30)
#define GETTID() gettid()
#else
#include <sys/syscall.h>
#ifdef SYS_gettid
#define GETTID() (int)syscall(SYS_gettid)
#else
#error "SYS_gettid unavailable on this system"
#endif
#endif

extern char **environ;

static int current_state_argc = -1;
// This memory is directly owned by this block of code and should be properly dealt with
static char **current_state_argv = NULL;
// Usable area for command line arguments (and so for data shown in 'ps')
static void *data_first_stake = NULL;
static void *data_last_stake = NULL;
// This number represents the whole space which can be used for command line argument data
// as shown in /proc/$(pid)/cmdline file
static size_t size_of_area_for_data = 0;
// This counter holds a value of currently used chars
static size_t used_data_counter = 0;
static int envc = -1;
// This memory is directly owned by this block of code and should be properly dealt with
static char **new_environ = NULL;
// This memory is directly owned by this block of code and should be properly dealt with
static char *new_environ_space = NULL;
static bool init_done = false;
static bool exit_done = false;
// This mutex protects the access to cmdline argument space between data_first_stake and data_last_stake,
// used_data_counter, current_state_argc and current_state_argv[]
static pthread_mutex_t cmdline_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_t main_thread = {0};

/* This function works like this: We malloc new char pointer array, which we will 
 * use as a argv in subsequent operations (like changing the title and so on)
 * Then we will create new memory space for the ENVIRONMENT and shift data there.
 * In the end we resize the ARGV memory space to encompass the original ENVIRONMENT
 * space and tell kernel about all this
*/
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 18, 0)
int cmdline_args_init(int argc, char **argv)
{
    int fd_stat = -1;
    int retval = -1;
    int read_characters = -1;
    char *temporary_character = NULL;
    char read_buffer[2048];
    size_t string_lenght = 0;
    struct prctl_mm_map prctl_map = {0};

    unsigned long int old_start_code = 0;
    unsigned long int old_end_code = 0;
    unsigned long int old_start_data = 0;
    unsigned long int old_end_data = 0;
    unsigned long int old_start_brk = 0;
    unsigned long int old_brk = 0;
    unsigned long int old_start_stack = 0;
    unsigned long int old_arg_start = 0;
    unsigned long int old_arg_end = 0;
    unsigned long int old_env_start = 0;
    unsigned long int old_env_end = 0;

    errno = 0;

    // Test if this is first run
    if (init_done)
    {
        // Nothing to do, the function can be called only once
        syslog_async(LOG_ERR, "RTAPI_CMDLINE_INIT already done\n");
        errno = ENOENT;
        goto return_value;
    }
    // We are signaling that no one else should be able to go over this line
    // On error the 'init_done' will be set back to false
    init_done = true;
    // Test if we are called from the main thread
    if (getpid() != GETTID())
    {
        syslog_async(LOG_ERR, "RTAPI_CMDLINE_INIT has to be run from the main thread. RUN from tid: %d, pid: %d\n", GETTID(), getpid());
        errno = EACCES;
        goto error_end;
    }
    main_thread = pthread_self();

    fd_stat = open("/proc/self/stat", O_RDONLY);
    if (fd_stat < 0)
    {
        syslog_async(LOG_ERR, "RTAPI_CMDLINE_INIT could not open /proc/%d/stat\n", getpid());
        goto error_end;
    }
    read_characters = read(fd_stat, &read_buffer, sizeof(read_buffer));
    retval = close(fd_stat);
    if (retval)
    {
        syslog_async(LOG_ERR, "RTAPI_CMDLINE_INIT could not close /proc/%d/stat, fd: %d\n", getpid(), fd_stat);
        goto error_end;
    }
    if (read_characters < 1)
    {
        syslog_async(LOG_ERR, "RTAPI_CMDLINE_INIT could not read /proc/%d/stat\n", getpid());
        errno = EINVAL;
        goto error_end;
    }

    temporary_character = read_buffer;
    // We are skipping the first 25 values in /proc/self/stat file
    for (int i = 0; i < 25; i++)
    {
        if (!temporary_character)
        {
            goto error_process_stat;
        }
        // We are skipping the absolute first char in read, but we don't care
        // about it
        temporary_character = strchr(temporary_character + 1, ' ');
    }
    if (!temporary_character)
    {
        goto error_process_stat;
    }

    retval = sscanf(temporary_character, "%lu %lu %lu", &old_start_code, &old_end_code, &old_start_stack);
    if (retval != 3)
    {
        goto error_process_stat;
    }

    // We are skipping the additional 19 values in /proc/self/stat
    for (int i = 0; i < 19; i++)
    {
        if (!temporary_character)
        {
            goto error_process_stat;
        }
        temporary_character = strchr(temporary_character + 1, ' ');
    }
    if (!temporary_character)
    {
        goto error_process_stat;
    }

    retval = sscanf(temporary_character, "%lu %lu %lu %lu %lu %lu %lu", &old_start_data, &old_end_data, &old_start_brk, &old_arg_start, &old_arg_end, &old_env_start, &old_env_end);
    if (retval != 7)
    {
        goto error_process_stat;
    }

    // We need to compute number of strings in the environ memory space
    for (envc = 0; environ[envc] != NULL; envc++)
    {
    }

    data_first_stake = (void *)old_arg_start;
    used_data_counter = (size_t)(((char *)old_arg_end - (char *)old_arg_start) + 1);
    data_last_stake = (void *)old_env_end;
    size_of_area_for_data = (size_t)(((char *)data_last_stake - (char *)data_first_stake) + 1);

    new_environ = (char **)malloc(sizeof(char *) * (envc + 1));
    if (!new_environ)
    {
        syslog_async(LOG_ERR, "RTAPI_CMDLINE_INIT could not malloc memory for environment vector\n");
        goto error_end;
    }
    new_environ_space = (char *)malloc((size_t)(((char *)old_env_end - (char *)old_env_start) + 1));
    if (!new_environ_space)
    {
        syslog_async(LOG_ERR, "RTAPI_CMDLINE_INIT could not malloc memory for new environment space\n");
        goto error_environ_space_malloc;
    }

    current_state_argc = argc;
    // We are doing this so subsequent calls to realloc when changing this pointer
    // array are completelly legal
    current_state_argv = (char **)malloc((current_state_argc + 1) * sizeof(char *));
    if (!current_state_argv)
    {
        syslog_async(LOG_ERR, "RTAPI_CMDLINE_INIT could not malloc memory for new argument vector\n");
        goto error_argv_malloc;
    }
    for (int i = 0; i < argc; i++)
    {
        current_state_argv[i] = argv[i];
    }
    // By definition last member of argv should be NULL
    current_state_argv[current_state_argc] = NULL;

    temporary_character = new_environ_space;
    memcpy(new_environ_space, (char *)old_env_start, (size_t)(((char *)old_env_end - (char *)old_env_start) + 1));
    for (int i = 0; i < envc; i++)
    {
        string_lenght = strlen(environ[i]) + 1;
        new_environ[i] = temporary_character;
        temporary_character += string_lenght;
    }
    new_environ[envc] = NULL;
    environ = new_environ;

    // We are getting the current brk line as a last thing
    old_brk = (unsigned long int)sbrk(0);

    prctl_map = (struct prctl_mm_map){
        .start_code = old_start_code,
        .end_code = old_end_code,
        .start_stack = old_start_stack,
        .start_data = old_start_data,
        .end_data = old_end_data,
        .start_brk = old_start_brk,
        .brk = old_brk,
        .arg_start = old_arg_start,
        .arg_end = old_env_end,
        .env_start = (unsigned long int)new_environ_space,
        .env_end = (unsigned long int)(new_environ_space + (size_t)((char *)old_env_end - (char *)old_env_start)),
        .auxv = NULL,
        .auxv_size = 0,
        .exe_fd = -1,
    };

    if (prctl(PR_SET_MM, PR_SET_MM_MAP, (long int)&prctl_map, sizeof(prctl_map), 0) < 0)
    {
        syslog_async(LOG_ERR, "RTAPI_CMDLINE_INIT could not PR_SET_MM_MAP\n");
        goto error_pr_set_mm_map;
    }

    memset((char *)old_env_start, '\0', (size_t)(((char *)data_last_stake - (char *)old_env_start) + 1));

    // We have reached this point and all should be good, signal initialization done
    goto return_value;

error_process_stat:
    syslog_async(LOG_ERR, "RTAPI_CMDLINE_INIT could not process /proc/%d/stat\n", getpid());
    errno = EINVAL;
    goto error_end;
error_pr_set_mm_map:
    free(current_state_argv);
error_argv_malloc:
    free(new_environ_space);
error_environ_space_malloc:
    free(new_environ);
error_end:
    init_done = false;
return_value:
    return -errno;
}
#else
// This is only for kernel versions before 3.18
// Can be deleted after Machinekit no longer supports the 3.8
// The process needs to have a CAP_SYS_RESOURCE capability
int cmdline_args_init(int argc, char **argv)
{
    char *temporary_character = NULL;
    char *delimiter = NULL;
    size_t string_lenght = 0;

    errno = 0;

    // Test if this is first run
    if (init_done)
    {
        // Nothing to do, the function can be called only once
        syslog_async(LOG_ERR, "RTAPI_CMDLINE_INIT already done\n");
        errno = ENOENT;
        goto return_value;
    }
    // We are signaling that no one else should be able to go over this line
    // On error the 'init_done' will be set back to false
    init_done = true;
    // Test if we are called from the main thread
    if (getpid() != GETTID())
    {
        syslog_async(LOG_ERR, "RTAPI_CMDLINE_INIT has to be run from the main thread. RUN from tid: %d, pid: %d\n", GETTID(), getpid());
        errno = EACCES;
        goto error_end;
    }
    main_thread = pthread_self();

    // Now we compute the lenght in continuous memory which strings pointed to by
    // argv take (Logic taken from Postgres open-source code: https://github.com/postgres/postgres/blob/master/src/backend/utils/misc/ps_status.c)
    for (int i = 0; i < argc; i++)
    {
        if (i == 0 || delimiter + 1 == argv[i])
        {
            delimiter = argv[i] + strlen(argv[i]);
        }
    }

    // Now we shoul be at delimiter between argv memory space and environ space
    if (delimiter == NULL || (*delimiter != '\0' && *(delimiter + 1) == '\0'))
    {
        // Should not happen
        syslog_async(LOG_ERR, "RTAPI_CMDLINE_INIT discovered NULL delimiter between argv memory and environ memory\n");
        errno = EFAULT;
        goto error_end;
    }
    // Now we prepare stakes for marking out the new memory
    data_first_stake = (void *)argv[0];
    used_data_counter = (size_t)((delimiter + 1) - (char *)data_first_stake);

    for (envc = 0; environ[envc] != NULL; envc++)
    {
        if (delimiter + 1 == environ[envc])
        {
            delimiter = environ[envc] + strlen(environ[envc]);
        }
    }

    data_last_stake = (void *)delimiter;
    size_of_area_for_data = (size_t)(((char *)data_last_stake - (char *)data_first_stake) + 1);

    new_environ = (char **)malloc(sizeof(char *) * (envc + 1));
    if (!new_environ)
    {
        syslog_async(LOG_ERR, "RTAPI_CMDLINE_INIT could not malloc memory for environment vector\n");
        goto error_end;
    }
    new_environ_space = (char *)malloc((size_t)(size_of_area_for_data - used_data_counter));
    if (!new_environ_space)
    {
        syslog_async(LOG_ERR, "RTAPI_CMDLINE_INIT could not malloc memory for new environment space\n");
        goto error_environ_space_malloc;
    }

    current_state_argc = argc;
    // We are doing this so subsequent calls to realloc when changing this pointer
    // array are completelly legal
    current_state_argv = (char **)malloc((current_state_argc + 1) * sizeof(char *));
    if (!current_state_argv)
    {
        syslog_async(LOG_ERR, "RTAPI_CMDLINE_INIT could not malloc memory for new argument vector\n");
        goto error_argv_malloc;
    }
    for (int i = 0; i < argc; i++)
    {
        current_state_argv[i] = argv[i];
    }
    // By definition last member of argv should be NULL
    current_state_argv[current_state_argc] = NULL;

    temporary_character = new_environ_space;
    memcpy((void *)new_environ_space, (char *)data_first_stake + used_data_counter, (size_t)(size_of_area_for_data - used_data_counter));
    for (int i = 0; i < envc; i++)
    {
        string_lenght = strlen(environ[i]) + 1;
        new_environ[i] = temporary_character;
        temporary_character += string_lenght;
    }
    new_environ[envc] = NULL;
    environ = new_environ;

    // Now we will tell kernel about the new memory spaces
    // This is actually not that great and should hapve checks like in systemD code:
    // https://github.com/systemd/systemd/blob/master/src/basic/process-util.c
    // But given that it is only used for pre 3.18 kernel, well...
    // For msgd process, the best way it would be to check the  CAP_SYS_RESOURCE capability,
    // but that would require cap_get_proc() and -lcap shared library and as such
    // I see it as not worthy
    if (prctl(PR_SET_MM, PR_SET_MM_ENV_START, (unsigned long int)new_environ_space, 0, 0) < 0)
    {
        syslog_async(LOG_ERR, "RTAPI_CMDLINE_INIT could not PR_SET_MM_ENV_START\n");
        goto error_pr_set_mm;
    }
    if (prctl(PR_SET_MM, PR_SET_MM_ENV_END, (unsigned long int)(new_environ_space + (size_t)(size_of_area_for_data - used_data_counter)), 0, 0) < 0)
    {
        syslog_async(LOG_ERR, "RTAPI_CMDLINE_INIT could not PR_SET_MM_ENV_END\n");
        goto error_pr_set_mm;
    }
    if (prctl(PR_SET_MM, PR_SET_MM_ARG_START, (unsigned long int)data_first_stake, 0, 0) < 0)
    {
        syslog_async(LOG_ERR, "RTAPI_CMDLINE_INIT could not PR_SET_MM_ARG_START\n");
        goto error_pr_set_mm;
    }
    if (prctl(PR_SET_MM, PR_SET_MM_ARG_END, (unsigned long int)data_last_stake, 0, 0) < 0)
    {
        syslog_async(LOG_ERR, "RTAPI_CMDLINE_INIT could not PR_SET_MM_ARG_END\n");
        goto error_pr_set_mm;
    }

    memset((char *)data_first_stake + used_data_counter, '\0', (size_t)(size_of_area_for_data - used_data_counter));

    // We have reached this point and all should be good, signal initialization done
    goto return_value;

error_pr_set_mm:
    free(current_state_argv);
error_argv_malloc:
    free(new_environ_space);
error_environ_space_malloc:
    free(new_environ);
error_end:
    init_done = false;
return_value:
    return -errno;
}
#endif

void cmdline_args_exit(void)
{
    if (init_done)
    {
        free(current_state_argv);
        free(new_environ);
        free(new_environ_space);
    }

    exit_done = true;
}

const char *const get_process_name(void)
{
    if (!init_done || exit_done)
    {
        syslog_async(LOG_ERR, "RTAPI_CMDLINE_ARGS GET_PROCESS_NAME has been called before initialization or after exit\n");
        return NULL;
    }

    return (const char *const)current_state_argv[0];
}

bool set_process_name(const char *const new_name)
{
    size_t new_name_lenght = 100;
    size_t delta = 0;
    size_t previous_used_data_counter = 0;
    char *temporary_space = NULL;
    char *delimiter = NULL;
    bool retval = false;

    // Test if we are called from the main thread
    if (getpid() != GETTID())
    {
        syslog_async(LOG_ERR, "RTAPI_CMDLINE_ARGS SET_PROCESS_NAME has to be run from the main thread. RUN from tid: %d, pid: %d\n", GETTID(), getpid());
        goto end;
    }

    // Test if we are called after initialization and before exit
    if (!init_done)
    {
        syslog_async(LOG_ERR, "RTAPI_CMDLINE_ARGS SET_PROCESS_NAME has to be called after initialization (call to cmdline_args_init) of process %s with PID %d\n", get_process_name(), getpid());
        goto end;
    }
    if (exit_done)
    {
        syslog_async(LOG_ERR, "RTAPI_CMDLINE_ARGS SET_PROCESS_NAME has to be called before exit (call to cmdline_args_exit) of process %s with PID %d\n", get_process_name(), getpid());
        goto end;
    }

    new_name_lenght = strlen(new_name) + 1;
    if (new_name_lenght > 16)
    {
        syslog_async(LOG_ERR, "RTAPI_CMDLINE_ARGS SET_PROCESS_NAME has to be passed name shorter than 16 characters, passed name %s has %d characters\n", new_name, new_name_lenght);
        goto end;
    }

    pthread_mutex_lock(&cmdline_mutex);

    // There probably is absolutely no chance of this constraint being broken, so the check is only for a good feeling
    if ((used_data_counter - (size_t)(strlen(get_process_name()) + 1) + new_name_lenght) > size_of_area_for_data)
    {
        syslog_async(LOG_ERR, "RTAPI_CMDLINE_ARGS SET_PROCESS_NAME has breached the maximum size allocated for cmdline arguments of %ld with wanting to write %ld\n", size_of_area_for_data, (size_t)(used_data_counter - (size_t)(strlen(get_process_name()) + 1) + new_name_lenght));
        goto mutex_relase;
    }

    temporary_space = (char *)malloc(used_data_counter);
    if (!temporary_space)
    {
        int error = errno;
        syslog_async(LOG_ERR, "RTAPI_CMDLINE_ARGS SET_PROCESS_NAME encountered and error (%d)-> %s when trying to allocate %ld chars for temporary_space\n", error, strerror(error), used_data_counter);
        goto mutex_relase;
    }

    memcpy(temporary_space, (char *)data_first_stake, used_data_counter);

    delimiter = (char *)data_first_stake;
    strncpy(delimiter, new_name, new_name_lenght);
    delimiter += new_name_lenght;

    for (int i = 1; i < current_state_argc; i++)
    {
        delta = strlen(temporary_space + (current_state_argv[i] - (char *)data_first_stake)) + 1;
        memcpy(delimiter, (char *)(temporary_space + (current_state_argv[i] - (char *)data_first_stake)), delta);
        current_state_argv[i] = delimiter;
        delimiter += delta;
    }

    previous_used_data_counter = used_data_counter;
    // We are not adding the 1 for first char as in other cases because the delimiter is actually pointing one char
    // behind the end
    used_data_counter = (size_t)((delimiter - (char *)data_first_stake));
    if (previous_used_data_counter > used_data_counter)
    {
        memset(delimiter, '\0', previous_used_data_counter - used_data_counter);
    }

    if (pthread_setname_np(main_thread, get_process_name()))
    {
        syslog_async(LOG_ERR, "RTAPI_CMDLINE_ARGS SET_PROCESS_NAME cannot change name of process %d from %s to %s\n", getpid(), get_process_name(), new_name);
        // We leave it here without an error
    }

    retval = true;

mutex_relase:
    pthread_mutex_unlock(&cmdline_mutex);
    free(temporary_space);
end:
    return retval;
}