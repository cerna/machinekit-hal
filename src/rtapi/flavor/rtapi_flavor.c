#include <stdlib.h> // getenv
#include <dlfcn.h>  // dlopen
#include <stdbool.h>
#include <dirent.h> // dir
#include "rtapi_flavor.h"
#ifdef ULAPI
#include "ulapi.h"
#endif

static struct flavor_library
{
    const char *library_name;
    const char *library_path;    //Path to the dynamic library implementing the flavor API
    unsigned int library_weight; //Weight is used also as an array index
    bool library_used;
}
#define flavor_library struct flavor_library
#define MAX_NUMBER_OF_FLAVORS 10

// Help for unit test mocking
int flavor_mocking = 0;     // Signal from tests
int flavor_mocking_err = 0; // Pass error to tests
// - Mock exit(status), returning NULL and passing error out of band
#define EXIT_NULL(status)                \
    do                                   \
    {                                    \
        if (flavor_mocking)              \
        {                                \
            flavor_mocking_err = status; \
            return NULL;                 \
        }                                \
        else                             \
            exit(status);                \
    } while (0)
// - Mock exit(status), returning (nothing) and passing error out of band
#define EXIT_NORV(status)                \
    do                                   \
    {                                    \
        if (flavor_mocking)              \
        {                                \
            flavor_mocking_err = status; \
            return;                      \
        }                                \
        else                             \
            exit(status);                \
    } while (0)

// Global flavor descriptor gets set after a flavor is chosen
// TODO: Protect flavor_descriptor and export only getter function with const pointer
flavor_descriptor_ptr flavor_descriptor = NULL;
// Local solib handle of currently open flavor
static void *flavor_handle = NULL;

// Simpler than linked list and so-so good enough
// TODO: ReDO to real linked list
static flavor_library known_libraries[MAX_NUMBER_OF_FLAVORS] = {0};
static int free_index_known_libraries = 0;

static bool flavor_library_factory(const char *name, const char *path, unsigned int weight)
{
    // We are assuming that this function will be called multiple times fot the same
    // library given possible existence of multiple symlinks, which by search function
    // will all be treated as a match
    // But the name HAS(!) TO BE unique even by case-insensitive match and is generally
    // shorter to test than path
    for (int i = 0; i < free_index_known_libraries; i++)
    {
        if (strcasecmp(known_libraries[i].library_name, name) == 0)
        {
            if (strcmp(known_libraries[i].library_path, path) != 0)
            {
                rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI: FLAVOUR API library finder: Two libraries with same name name, but different paths were found on the system. First library '%s' on %s, second library '%s' on %s.", known_libraries[i].library_name, known_libraries[i].library_path, name, path);
                return false;
            }
            rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI: FLAVOUR API library finder: Two libraries with same name '%s' were found on the system in %s.", name, path);
            return false;
        }

        char *name_alloc = strdup(name);
        if (name_alloc == NULL)
        {
            int error = errno;
            rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI: FLAVOUR API library finder: There was an error when trying to malloc: (%d)->%s", error, strerror(error));
            return false;
        }
        char *path_alloc = strdup(path);
        if (path_alloc == NULL)
        {
            int error = errno;
            rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI: FLAVOUR API library finder: There was an error when trying to malloc: (%d)->%s", error, strerror(error));
            free(name_alloc);
            return false;
        }
        flavor_library *temp = &known_libraries[free_index_known_libraries];
        free_index_known_libraries++;
        temp->library_name = (const char *)name_alloc;
        temp->library_path = (const char *)path_alloc;
        temp->library_weight = weight;
        temp->library_used = true;

        return true;
    }
}

static void flavor_library_free(flavor_library *to_free)
{
    free(to_free->library_name);
    free(to_free->library_path);
    to_free->library_used = false;
}

static bool free_known_libraries()
{
    if (free_index_known_libraries == 0)
    {
        return false;
    }
    for (int i = 0; i < free_index_known_libraries; i++)
    {
        flavor_library_free(&known_libraries[i]);
    }
    return true;
}

// Point of contact with flavour API library
void register_flavor(flavor_descriptor_ptr descriptor_to_register)
{
    if (flavor_descriptor == NULL)
    {
        flavor_descriptor = descriptor_to_register;
    }
}
// Point of contact with flavour API library
void unregister_flavor(flavor_descriptor_ptr descriptor_to_unregister)
{
    if (flavor_descriptor == descriptor_to_unregister)
    {
        flavor_descriptor = NULL;
    }
}

static bool install_flavor_solib(const char *solib_path)
{
    flavor_handle = dlopen(solib_path, RTLD_NOW);
    if (flavor_handle == NULL)
    {
        char *error;
        error = dlerror();
        rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI: Error occured when trying to load flavor: %s->%s", solib_path, error);
        return false;
    }
    return true;
}

static bool close_failed_library_install(void)
{
    int retval;
    if (flavor_handle != NULL && flavor_descriptor == NULL)
    {
        retval = dlclose(flavor_handle);
        if (retval)
        {
            char *error;
            error = dlerror();
            rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI: There was an error when unloading the FLAVOR LIBRARY: %s", error);
            // We will swallow the fact that the library was not unloaded without an error
        }
        flavor_handle = NULL;
        return true;
    }
    return false;
}

bool uninstall_flavor(void)
{
    int retval;
    if (flavor_descriptor != NULL)
    {
        if (flavor_handle != NULL)
        {
            retval = dlclose(flavor_handle);
            if (retval)
            {
                char *error;
                error = dlerror();
                rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI: There was an error when unloading the FLAVOR LIBRARY: %s", error);
                // We are not returning false, because if the flavor_descriptor is NULL, then all is good
            }
            // Flavor solib should run it's descructor code and call unregister flavor,
            // if the solib was not dlopened multiple times, which we definitely do not want
            if (flavor_descriptor != NULL)
            {
                rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI: FLAVOR library %s refused to unload", flavor_descriptor->name);
                return false;
            }
            return true;
        }
        // Really should not happen, but if for some reason the no library is loaded and the global
        // flavor_descriptor is not NULL, then set him to NULL
        flavor_descriptor = NULL;
        return true;
    }
    return false;
}

/*static bool find_flavor_solibs(const char *directory_path)
{
    int retval;
    DIR *directory;
    struct dirent *entry;
    directory = opendir(directory_path);
    if (directory == NULL)
    {
        rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI: Cannot open directory '%s' for lookup of available FLAVOR LIBRARIES", directory_path);
        return false;
    }

    retval = closedir(directory);
    if (retval)
    {
        char *error = errno();
        rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI: Cannot close directory '%s' for lookup of available FLAVOR LIBRARIES, error: %s", directory_path, error);
    }
    return true;
}*/

const char **known_flavors()
{
}

bool install_flavor_by_name(const char *flavor_name)
{
}

bool install_default_flavor(void)
{
}

//To delete
const char *flavor_names(flavor_descriptor_ptr **fd)
{
    const char *name;
    do
    {
        if (*fd == NULL)
            // Init to beginning of list
            *fd = flavor_list;
        else
            // Go to next in list
            (*fd)++;
    } while (**fd != NULL && !flavor_can_run_flavor(**fd));

    if (**fd == NULL)
        // End of list; no name
        name = NULL;
    else
        // Not end; return name
        name = (**fd)->name;
    return name;
}

//Probably also to delete
flavor_descriptor_ptr flavor_byname(const char *flavorname)
{
    flavor_descriptor_ptr *i;
    for (i = flavor_list; *i != NULL; i++)
    {
        if (!strcasecmp(flavorname, (*i)->name))
            break;
    }
    return *i;
}

//Probably also to delete
flavor_descriptor_ptr flavor_byid(rtapi_flavor_id_t flavor_id)
{
    flavor_descriptor_ptr *i;
    for (i = flavor_list; *i != NULL; i++)
    {
        if ((*i)->flavor_id == flavor_id)
            break;
    }
    return *i;
}

//Part of flavor loading, completely rework
flavor_descriptor_ptr flavor_default(void)
{
    const char *fname = getenv("FLAVOR");
    flavor_descriptor_ptr *flavor_handle = NULL;
    flavor_descriptor_ptr flavor = NULL;

    if (fname && fname[0])
    {
        // $FLAVOR set in environment:  verify it or fail
        flavor = flavor_byname(fname);
        if (flavor == NULL)
        {
            rtapi_print_msg(RTAPI_MSG_ERR, "FATAL:  No such flavor '%s';"
                                           " valid flavors are\n",
                            fname);
            for (flavor_handle = NULL; (fname = flavor_names(&flavor_handle));)
                rtapi_print_msg(RTAPI_MSG_ERR, "FATAL:      %s\n",
                                (*flavor_handle)->name);
            EXIT_NULL(100);
        }
        if (!flavor_can_run_flavor(flavor))
        {
            rtapi_print_msg(RTAPI_MSG_ERR, "FATAL:  Flavor '%s' from"
                                           " environment cannot run\n",
                            fname);
            EXIT_NULL(101);
        }
        else
        {
            rtapi_print_msg(RTAPI_MSG_INFO,
                            "INFO:  Picked flavor '%s' id %d (from environment)\n",
                            flavor->name, flavor->flavor_id);
            return flavor;
        }
    }
    else
    {
        // Find best flavor automatically
        flavor = NULL;
        for (flavor_handle = flavor_list;
             *flavor_handle != NULL;
             flavor_handle++)
        {
            // Best is highest ID that can run
            if ((!flavor || (*flavor_handle)->flavor_id > flavor->flavor_id) && flavor_can_run_flavor(*flavor_handle))
                flavor = (*flavor_handle);
        }
        if (!flavor)
        {
            // This should never happen:  POSIX can always run
            rtapi_print_msg(RTAPI_MSG_ERR,
                            "ERROR:  Unable to find runnable flavor\n");
            EXIT_NULL(102);
        }
        else
        {
            rtapi_print_msg(RTAPI_MSG_INFO, "INFO:  Picked default flavor '%s'"
                                            " automatically\n",
                            flavor->name);
            return flavor;
        }
    }
}

// Flavour will install itself automatically as plugin module
void flavor_install(flavor_descriptor_ptr flavor)
{
    if (flavor_descriptor != NULL)
    {
        rtapi_print_msg(RTAPI_MSG_ERR, "FATAL:  Flavor '%s' already"
                                       " configured\n",
                        flavor_descriptor->name);
        EXIT_NORV(103);
    }
    if (!flavor_can_run_flavor(flavor))
    {
        rtapi_print_msg(RTAPI_MSG_ERR, "FATAL:  Flavor '%s' cannot run\n",
                        flavor->name);
        EXIT_NORV(104);
    }
    flavor_descriptor = flavor;
    rtapi_print_msg(RTAPI_MSG_DBG, "Installed flavor '%s'\n", flavor->name);
}

int flavor_is_configured(void)
{
    return flavor_descriptor != NULL;
}

#ifdef RTAPI
EXPORT_SYMBOL(flavor_names);
EXPORT_SYMBOL(flavor_is_configured);
EXPORT_SYMBOL(flavor_byname);
EXPORT_SYMBOL(flavor_default);
//EXPORT_SYMBOL(flavor_install);
EXPORT_SYMBOL(uninstall_flavor);
#endif
