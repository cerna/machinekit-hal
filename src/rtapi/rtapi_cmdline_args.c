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
static void *data_first_stake = NULL;
static void *data_last_stake = NULL;
static size_t data_usable_area = 0;
static int envc = -1;
static char **new_environ = NULL;
static char *new_environ_space = NULL;
bool init_done = false;

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

    // Test if this is first run
    if (init_done)
    {
        // Nothing to do, the function can be called only once
        goto error_init_already_done;
    }
    // We are signaling that no one else should be able to go over this line
    // On error the 'init_done' will be set back to false
    init_done = true;
    // Test if we are called from the main thread
    if (getpid() != GETTID())
    {
        goto error_not_called_from_main_thread;
    }

    fd_stat = open("/proc/self/stat", O_RDONLY);
    if (fd_stat < 0)
    {
        goto error_open_stat;
    }
    read_characters = read(fd_stat, &read_buffer, sizeof(read_buffer));
    retval = close(fd_stat);
    if (retval)
    {
        goto error_close_stat;
    }
    if (read_characters < 1)
    {
        goto error_read_stat;
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
    data_usable_area = (char *)old_env_end - (char *)old_arg_start;
    data_last_stake = (void *)old_env_end;

    new_environ = (char **)malloc(sizeof(char *) * (envc + 1));
    if (!new_environ)
    {
        goto error_malloc_environ;
    }
    new_environ_space = (char *)malloc((size_t)((char *)old_env_end - (char *)old_env_start));
    if (!new_environ_space)
    {
        goto error_environ_space;
    }

    current_state_argc = argc;
    // We are doing this so subsequent calls to realloc when changing this pointer
    // array are completelly legal
    current_state_argv = (char **)malloc((current_state_argc + 1) * sizeof(char *));
    if (!current_state_argv)
    {
        goto error_argv_malloc;
    }
    for (int i = 0; i < argc; i++)
    {
        current_state_argv[i] = argv[i];
    }
    // By definition last member of argv should be NULL
    current_state_argv[current_state_argc] = NULL;

    temporary_character = new_environ_space;
    memcpy((void *)new_environ_space, (void *)old_env_start, (size_t)((char *)old_env_end - (char *)old_env_start));
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

    if (prctl(PR_SET_MM, PR_SET_MM_MAP, (long)&prctl_map, sizeof(prctl_map), 0) < 0)
    {
        goto error_pr_set_mm_map;
    }

    memset((char *)old_env_start, '\0', (size_t)((char *)data_last_stake - (char *)old_env_start));

    // We have reached this point and all should be good, signal initialization done
    return 0;

// There are error return codes at a one place
error_pr_set_mm_map:
    syslog_async(LOG_ERR, "RTAPI_CMDLINE_INIT could not PR_SET_MM_MAP\n");
    free(new_environ_space);
    free(new_environ);
    free(current_state_argv);
    init_done = false;
    return -errno;
error_argv_malloc:
    free(new_environ_space);
error_environ_space:
    free(new_environ);
error_malloc_environ:
    syslog_async(LOG_ERR, "RTAPI_CMDLINE_INIT could not malloc memory for new environment\n");
    init_done = false;
    return -errno;
error_process_stat:
    syslog_async(LOG_ERR, "RTAPI_CMDLINE_INIT could not process /proc/%d/stat\n", getpid());
    init_done = false;
    return -EINVAL;
error_open_stat:
    syslog_async(LOG_ERR, "RTAPI_CMDLINE_INIT could not open /proc/%d/stat\n", getpid());
    init_done = false;
    return -errno;
error_read_stat:
    syslog_async(LOG_ERR, "RTAPI_CMDLINE_INIT could not read /proc/%d/stat\n", getpid());
    init_done = false;
    return -EINVAL;
error_close_stat:
    syslog_async(LOG_ERR, "RTAPI_CMDLINE_INIT could not close /proc/%d/stat, fd: %d\n", getpid(), fd_stat);
    init_done = false;
    return -errno;
error_not_called_from_main_thread:
    syslog_async(LOG_ERR, "RTAPI_CMDLINE_INIT has to be run from the main thread. RUN from tid: %d, pid: %d\n", GETTID(), getpid());
    init_done = false;
    return -EACCES;
error_init_already_done:
    syslog_async(LOG_ERR, "RTAPI_CMDLINE_INIT already done\n");
    init_done = false;
    return -ENOENT;
}
#else
// This is only for kernel versions before 3.18
// Can be deleted after Machinekit no longer supports the 3.8
// The process needs to have a CAP_SYS_RESOURCE capability
int cmdline_args_init(int argc, char **argv)
{
    char *temporary_character = NULL;
    char *delimiter = NULL;
    void *data_middle_stake = NULL;
    size_t string_lenght = 0;

    // Test if this is first run
    if (init_done)
    {
        // Nothing to do, the function can be called only once
        goto error_init_already_done;
    }
    // We are signaling that no one else should be able to go over this line
    // On error the 'init_done' will be set back to false
    init_done = true;
    // Test if we are called from the main thread
    if (getpid() != GETTID())
    {
        goto error_not_called_from_main_thread;
    }

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
    if (delimiter == NULL)
    {
        // Should not happen
        goto error_delimiter_null;
    }
    data_middle_stake = (void *)delimiter;

    for (envc = 0; environ[envc] != NULL; envc++)
    {
        if (delimiter + 1 == environ[envc])
            delimiter = environ[envc] + strlen(environ[envc]);
    }

    // Now we prepare stakes for marking out the new memory
    data_first_stake = argv[0];
    data_usable_area = delimiter - (char *)data_first_stake - 1; //Do I have to substract 1
    data_last_stake = (void *)((char *)data_first_stake + data_usable_area);
    printf("envc: %d\n", envc);
    new_environ = (char **)malloc(sizeof(char *) * (envc + 1));
    if (!new_environ)
    {
        goto error_malloc_environ;
    }
    printf("malloc: %ld\n", (size_t)((char *)data_last_stake - (char *)data_middle_stake));
    new_environ_space = (char *)malloc((size_t)((char *)data_last_stake - (char *)data_middle_stake));
    if (!new_environ_space)
    {
        goto error_environ_space;
    }
    printf("malloc: %ld\n", (size_t)((char *)data_last_stake - (char *)data_middle_stake));

    current_state_argc = argc;
    // We are doing this so subsequent calls to realloc when changing this pointer
    // array are completelly legal
    current_state_argv = (char **)malloc((current_state_argc + 1) * sizeof(char *));
    if (!current_state_argv)
    {
        printf("fu: %d\n", 0);

        goto error_argv_malloc;
    }
    for (int i = 0; i < argc; i++)
    {
        current_state_argv[i] = argv[i];
    }
    // By definition last member of argv should be NULL
    current_state_argv[current_state_argc] = NULL;

    temporary_character = new_environ_space;
    memcpy((void *)new_environ_space, data_middle_stake, (size_t)((char *)data_last_stake - (char *)data_middle_stake));
    printf("MEMCOPY: %ld\n", (size_t)((char *)data_last_stake - (char *)data_middle_stake));
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
    if (prctl(PR_SET_MM, PR_SET_MM_ENV_END, (unsigned long int)(new_environ_space + (size_t)((char *)data_last_stake - (char *)data_middle_stake)), 0, 0) < 0)
    {
    }
    if (prctl(PR_SET_MM, PR_SET_MM_ARG_START, (unsigned long int)new_environ_space, 0, 0) < 0)
    {
    }
    if (prctl(PR_SET_MM, PR_SET_MM_ARG_START, (unsigned long int)data_first_stake, 0, 0) < 0)
    {
    }
    if (prctl(PR_SET_MM, PR_SET_MM_ARG_END, (unsigned long int)data_last_stake, 0, 0) < 0)
    {
    }

    memset(data_middle_stake, '\0', (size_t)((char *)data_last_stake - (char *)data_middle_stake));
    // We have reached this point and all should be good, signal initialization done
    return 0;

// There are error return codes at a one place
error_delimiter_null:
    syslog_async(LOG_ERR, "RTAPI_CMDLINE_INIT discovered NULL delimiter between argv memory and environ memory\n");
    init_done = false;
    return -EFAULT;
error_argv_malloc:
    free(new_environ_space);
error_environ_space:
    free(new_environ);
error_malloc_environ:
    syslog_async(LOG_ERR, "RTAPI_CMDLINE_INIT could not malloc memory\n");
    init_done = false;
    return -errno;
error_not_called_from_main_thread:
    syslog_async(LOG_ERR, "RTAPI_CMDLINE_INIT has to be run from the main thread. RUN from tid: %d, pid: %d\n", GETTID(), getpid());
    init_done = false;
    return -EACCES;
error_init_already_done:
    syslog_async(LOG_ERR, "RTAPI_CMDLINE_INIT already done\n");
    init_done = false;
    return -ENOENT;
}
#endif

void cmdline_args_exit(void)
{
    if (current_state_argv)
    {
        free(current_state_argv);
    }
    if (new_environ)
    {
        free(new_environ);
    }
    if (new_environ_space)
    {
        free(new_environ_space);
    }
}