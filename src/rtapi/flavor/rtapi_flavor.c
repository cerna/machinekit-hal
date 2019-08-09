#include <stdlib.h> // getenv
#include <dlfcn.h>  // dlopen
#include <stdbool.h>
#include <dirent.h> // dir
#include "rtapi_flavor.h"
#include "rtapi_lib_find.h"
#ifdef ULAPI
#include "ulapi.h"
#endif

struct flavor_library
{
    flavor_cold_metadata compile_time_metadata;
    const char *library_path; //Path to the dynamic library implementing the flavor API
    bool library_used;
};
typedef struct flavor_library flavor_library
#define MAX_NUMBER_OF_FLAVORS 10

    // Help for unit test mocking
    int flavor_mocking = 0; // Signal from tests
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

// Global access structure for storing pointers to control structures of currently installed
// FLAVOUR module
// TODO: Protect and export only getter function with const pointer
flavor_access_structure global_flavor_access_structure = {0};
flavor_access_structure_ptr global_flavor_access_structure_ptr = &global_flavor_access_structure;
// Local solib handle of currently open flavor
static void *flavor_handle = NULL;

// Simpler than linked list and so-so good enough
// TODO: ReDO to real linked list
static flavor_library known_libraries[MAX_NUMBER_OF_FLAVORS] = {0};
static int free_index_known_libraries = 0;

static bool flavor_library_factory(const char *path, const char *name, unsigned int id, unsigned int weight, unsigned int magic, unsigned int flags)
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
            // ReDO: Check for name and ID and then maybe path, and what about magic
            if (strcmp(known_libraries[i].library_path, path) != 0)
            {
                rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI: FLAVOUR API library finder: Two libraries with same name name, but different paths were found on the system. First library '%s' on %s, second library '%s' on %s.", known_libraries[i].library_name, known_libraries[i].library_path, name, path);
                return false;
            }
            rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI: FLAVOUR API library finder: Two libraries with same name '%s' were found on the system in %s.", name, path);
            return false;
        }

        flavor_library *temp = &known_libraries[free_index_known_libraries];
        if (temp->library_used)
        {
            rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI: FLAVOUR API library finder: There was an error by trying to access once created library");
        }
        strncpy(&(temp->compile_time_metadata.name), name, MAX_FLAVOR_NAME_LEN + 1) char *path_alloc = strdup(path);
        if (path_alloc == NULL)
        {
            int error = errno;
            rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI: FLAVOUR API library finder: There was an error when trying to malloc: (%d)->%s", error, strerror(error));
            *temp = {0};
            return false;
        }
        free_index_known_libraries++;
        temp->library_path = (const char *)path_alloc;
        temp->compile_time_metadata.weight = weight;
        temp->compile_time_metadata.id = id;
        temp->compile_time_metadata.magic = magic;
        temp->compile_time_metadata.flags = flags;
        temp->library_used = true;

        return true;
    }
}

static void flavor_library_free(flavor_library *to_free)
{
    free(to_free->library_path);
    *to_free = {0};
    to_free->library_used = false;
}

static bool free_known_libraries(void)
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

/* ========== START FLAVOUR module registration and unregistration functions ========== */
// Point of contact with flavour API library
void register_flavor(flavor_cold_metadata_ptr descriptor_to_register)
{
    // TODO: Set local state flag
    if (global_flavor_access_structure_ptr->flavor_module_cold_metadata_descriptor == NULL)
    {
        global_flavor_access_structure_ptr->flavor_module_cold_metadata_descriptor = descriptor_to_register;
    }
}
// Point of contact with flavour API library
void unregister_flavor(flavor_cold_metadata_ptr descriptor_to_unregister)
{
    // TODO: Set localstate flag
    if (global_flavor_access_structure_ptr->flavor_module_cold_metadata_descriptor == descriptor_to_unregister)
    {
        global_flavor_access_structure_ptr->flavor_module_cold_metadata_descriptor = NULL;
    }
}
/* ========== START FLAVOUR module registration and unregistration functions ========== */

/* ========== START FLAVOUR module arming and yielding functions ========== */
// Point of contact with flavour API library
void arm_flavor(flavor_hot_metadata_ptr descriptor_to_arm)
{
    // TODO: Set local state flag
    if (global_flavor_access_structure_ptr->flavor_module_hot_metadata_descriptor == NULL)
    {
        global_flavor_access_structure_ptr->flavor_module_hot_metadata_descriptor = descriptor_to_arm;
    }
}
// Point of contact with flavour API library
void yield_flavor(flavor_hot_metadata_ptr descriptor_to_yield)
{
    // TODO: Set localstate flag
    if (global_flavor_access_structure_ptr->flavor_module_hot_metadata_descriptor == descriptor_to_yield)
    {
        global_flavor_access_structure_ptr->flavor_module_hot_metadata_descriptor = NULL;
    }
}
/* ========== START FLAVOUR module arming and yielding functions ========== */

// In the end, the solib is alway dlopened on the filepath, so in every case this function
// will be called and the call can be successful only when there is no other library dlopened
// We are assuming that the input *solib_path is already tested as being Machinekit flavour
// API library
static bool install_flavor_solib(const char *solib_path)
{
    if (flavor_handle == NULL)
    {
        flavor_handle = dlopen(solib_path, RTLD_NOW | RTLD_LOCAL);
        if (flavor_handle == NULL)
        {
            char *error;
            error = dlerror();
            rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI: Error occured when trying to load flavor: %s->%s", solib_path, error);
            return false;
        }
        return true;
    }
    return false;
}

// The only point in addition to install_flavor_solib, where is happening the direct
// manipulation with .so library file, all access should go directly though this functionality
static bool uninstall_flavor_solib(void)
{
    if (flavor_handle != NULL)
    {
        int retval;
        retval = dlclose(flavor_handle);
        if (retval)
        {
            char *error;
            error = dlerror();
            rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI: There was an error when unloading the FLAVOR LIBRARY: %s", error);
            // We are not returning false, because we are setting the flavor_handle to NULL manually
            // (the library will unload itself on shutdown in the worst case scenario)
        }
        flavor_handle = NULL;
        return true;
    }
    return false;
}

static bool execute_checked_uninstall_of_flavor(void)
{
    // We are not checking the flavor_descriptor because we are using this function both for unistall of
    // correctly installed flavour library (FD populated) and for unload of incorrectly installed flavour
    // library (flavor_handle populated but FD unpopulated)
    if (uninstall_flavor_solib())
    {
        // FH was populated and now is unpopulated
        // Flavor solib should run it's descructor code and call unregister flavor,
        // if the solib was not dlopened multiple times, which we definitely do not want
        if (flavor_descriptor != NULL)
        {
            //Somewhere error happened (incorrect flavour library), signal and so
            rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI: FLAVOR library loader: Library '%' did not correctly unloaded flavor_descriptor.\n", flavor_descriptor->name);
            flavor_descriptor = NULL;
        }
        return true;
    }
    // Nothing is actually installed
    // and the flavor_descriptor is still DEFINITELLY null, it not, it's my programming fuck up
    return false;
}
// +++better check the logic, it's kind of fishy+++
static bool execute_checked_install_of_flavor(const char *path)
{
    if (flavor_descriptor == NULL)
    {
        if (install_flavor_solib(path))
        {
            if (flavor_descriptor == NULL)
            {
                // Hopefully should also check situation when developer mix the constructor
                // and destructor
                execute_checked_uninstall_of_flavor();
                return false;
            }
            // Flavor was successfully installed and FD was registered
            return true;
        }
        //Something installed but should not be installed or loading error
        //Can library in constructor dlopen itself and hold that way reference
        return false;
    }
    rtspi_print_msg(RTAPI_MSG_ERR, "RTAPI: FLAVOUR library loader: There is already flavour API library '%s' installed. You cannot install more than one library at a time.\n", flavor_descriptor->name);
    return false;
}

static bool install_flavor_by_name(const char *name)
{
    for (int i = 0; i < free_index_known_libraries; i++)
    {
        if (strcasecmp(known_libraries[i].library_name, name) == 0)
        {
            return execute_checked_install_of_flavor(known_libraries[i].library_path);
        }
    }
    return false;
}

static bool LAMBDA_is_machinekit_flavor_solib_v1(const char *const path, size_t size_of_input, void *input, flavor_module_v1_found_callback flavor_find)
{
    return is_machinekit_flavor_solib_v1(path, size_of_input, input, flavor_library_factory, NULL);
}

static bool LAMBDA_file_find(const char *const path)
{
    //Předělat na scan_file_for_elf_sections?
    //Nebo poskládat test_file_for_module_data tak, aby používal scan_file_for_elf_sections
    return test_file_for_module_data(path, "machinekit-flavor", LAMBDA_is_machinekit_flavor_solib_v1, NULL);
}

static int search_directory_for_flavor_modules(const char *const path)
{
    return for_each_node(path, NULL, LAMBDA_file_find, NULL);
}

static int search_mountpoint_for_flavor_modules(const char *const path)
{
    return for_each_node(path, search_mountpoint_for_flavor_modules, LAMBDA_file_find, NULL);
}

static bool install_flavor_by_path(const char *const path)
{
    LAMBDA_file_find(path);
    for (int i = 0; i < free_index_known_libraries; i++)
    {
        if (strcmp(known_libraries[i].library_path, path) == 0)
        {
            return execute_checked_install_of_flavor(known_libraries[i].library_path);
        }
    }
    return false;
}

static bool install_flavor_by_id(unsigned int id)
{
    for (int i = 0; i < free_index_known_libraries; i++)
    {
        if (known_libraries[i].library_id == id)
        {
            return execute_checked_install_of_flavor(known_libraries[i].library_path);
        }
    }
    return false;
}

static void bubble_sort_known_libraries(void)
{
    flavor_library temporary = {0};
    for (int i = 0; i < free_index_known_libraries - 1; i++)
    {
        for (int ii = i; ii < free_index_known_libraries - i - 1; ii++)
        {
            if (known_libraries[ii + 1].library_weight < known_libraries[ii].library_weight)
            {
                temporary = known_libraries[ii + 1];
                known_libraries[ii + 1] = known_libraries[ii];
                known_libraries[ii] = temporary;
            }
        }
    }
}

static bool install_default_flavor(void)
{
    int i = 0;

    bubble_sort_known_libraries();
    for (; i < free_index_known_libraries; i++)
    {
        rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI: FLAVOUR library loader: Trying to install flavour library module '%s'.\n", known_libraries[i].library_name);
        if (execute_checked_install_of_flavor(known_libraries[i].library_path))
        {
            rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI: FLAVOUR library loader: Flavour library module '%s' installed.\n", known_libraries[i].library_name);
            return true;
        }
    }
    rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI: FLAVOUR library loader: Not one flavour module could be installed (Tested %d modules). Have you installed at leas one? Maybe try to set the path directly.\n", i);
    return false;
}

static int discover_default_flavor_module(void)
{
    return get_paths_of_library_module("FLAVOR_LIB_DIR", search_directory_for_flavor_modules, NULL);
}
/*//To delete
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
}*/

//Probably also to delete
/*
flavor_descriptor_ptr flavor_byname(const char *flavorname)
{
    flavor_descriptor_ptr *i;
    for (i = flavor_list; *i != NULL; i++)
    {
        if (!strcasecmp(flavorname, (*i)->name))
            break;
    }
    return *i;
}*/
/*
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
}*/

/*
 * Here are implemented the nonstatic "public" function by which higher parts of rtapi.so program
 * communicate with rtapi_flavor
 * These functions are also exported by the EXPORT_SYMBOL MACRO
*/

int get_names_of_known_flavor_modules(void)
{
}
// TO REWORK!!!
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
