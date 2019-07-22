#include <dlfcn.h>

#include "rtapi_flavor.h"

// Prototype for plugin flavor descriptor updater
typedef void (*xenomai2_do_prototype)(void);

// really in nucleus/heap.h but we rather get away with minimum include files
#ifndef XNHEAP_DEV_NAME
#define XNHEAP_DEV_NAME "/dev/rtheap"
#endif
#define XENO_GID_SYSFS "/sys/module/xeno_nucleus/parameters/xenomai_gid"

// These exist on Xenomai but not on RTAI
#define PROC_IPIPE_XENOMAI "/proc/ipipe/Xenomai"
#define XENO_GID_SYSFS "/sys/module/xeno_nucleus/parameters/xenomai_gid"

static void *mod_handle = NULL;
static xenomai2_do_prototype do_load_constructor = NULL;
static xenomai2_do_prototype do_load_destructor = NULL;

static int kernel_is_xenomai()
{
    struct stat sb;

    return ((stat(XNHEAP_DEV_NAME, &sb) == 0) &&
            (stat(PROC_IPIPE_XENOMAI, &sb) == 0) &&
            (stat(XENO_GID_SYSFS, &sb) == 0));
}

static int xenomai_flavor_check(void);

static int xenomai_can_run_flavor()
{
    if (!kernel_is_xenomai())
        return 0;

    if (!xenomai_flavor_check())
        return 0;

    return 1;
}

static int xenomai_gid()
{
    FILE *fd;
    int gid = -1;

    if ((fd = fopen(XENO_GID_SYSFS, "r")) != NULL)
    {
        if (fscanf(fd, "%d", &gid) != 1)
        {
            fclose(fd);
            return -EBADF; // garbage in sysfs device
        }
        else
        {
            fclose(fd);
            return gid;
        }
    }
    return -ENOENT; // sysfs device cant be opened
}

static int user_in_xenomai_group()
{
    int numgroups, i;
    gid_t *grouplist;
    int gid = xenomai_gid();

    if (gid < 0)
        return gid;

    numgroups = getgroups(0, NULL);
    grouplist = (gid_t *)calloc(numgroups, sizeof(gid_t));
    if (grouplist == NULL)
        return -ENOMEM;
    if (getgroups(numgroups, grouplist) > 0)
    {
        for (i = 0; i < numgroups; i++)
        {
            if (grouplist[i] == (unsigned)gid)
            {
                free(grouplist);
                return 1;
            }
        }
    }
    else
    {
        free(grouplist);
        return errno;
    }
    return 0;
}

static int xenomai_flavor_check(void)
{
    // catch installation error: user not in xenomai group
    int retval = user_in_xenomai_group();

    switch (retval)
    {
    case 1: // yes
        break;
    case 0:
        fprintf(stderr, "this user is not member of group xenomai\n");
        fprintf(stderr, "please 'sudo adduser <username>  xenomai',"
                        " logout and login again\n");
        exit(EXIT_FAILURE);

    default:
        fprintf(stderr, "cannot determine if this user "
                        "is a member of group xenomai: %s\n",
                strerror(-retval));
        exit(EXIT_FAILURE);
    }
    return retval;
}

static void lib_init(void)
{
    // Only register the XENOMAI flavour if and only if the current system is capable of running it
    if (xenomai_can_run_flavor && mod_handle == NULL)
    {
        // Load the xenomai_loader.so module that does the real work
        // Both libraries (xenomai2.so and xenomai2loader.so) are tightly coupled and for
        // security reasons cannot be in two different places
        mod_handle = dlopen("./xenomai2loader.so", RTLD_GLOBAL | RTLD_NOW);
        if (!mod_handle)
        {
            rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI: XENOMAI2 flavour module: Unable to load xenomai2loader.so: %s\n", dlerror());
            return;
        }
        // Start the actual constructor (but only if there is destructor too)
        xenomai2_do_prototype do_load_constructor = dlsym(mod_handle, "do_load");
        xenomai2_do_prototype do_load_destructor = dlsym(mod_handle, "do_unload");
        if (do_load_contructor == NULL || do_load_destructor == NULL)
        {
            rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI: XENOMAI2 flavour module: Unable to load the constructor function do_load from xenomai2loader.so: %s\n", dlerror());
            return;
        }
        do_load_constructor();
        return;
    }
    // This should not happen
    rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI: XENOMAI2 flavour module: The library xenomai2.so dlopened twice");
}
static void lib_fini(void)
{
    if (mod_handle != NULL && do_load_destructor != NULL)
    {
        do_load_destructor();
        return;
    }
    // We are not reporting RTAPI_MSG_ERR here as the situation can happen when machinekit-flavor-xenomai2
    // is installed on the machine, but the kernel running is not XENOMAI 2
    // This will then be called during testing for runnable flavour
    rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI: XENOMAI2 flavour module: Cannot call the destructor, as the library xenomai2loader.so is not loaded.");
}

FLAVOR_STAMP("xenomai2", 2, 1)