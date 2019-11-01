#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdlib.h> // getenv
#include <dlfcn.h>  // dlopen
#include <stdbool.h>
#include <dirent.h> // dir
#include <getopt.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <alloca.h>
#include <inttypes.h>
#include <limits.h>

#include "rtapi_flavor.h"
#include "rtapi_lib_find.h"
#include "rtapi_cmdline_args.h"
#include "config.h"
#ifdef ULAPI
#include "ulapi.h"
#endif

#define MEMBER_SIZEOF(structure, member) sizeof(((structure *)0)->member)

extern char *optarg;
extern int optind;
extern int opterr;
extern int optopt;

struct flavor_library
{
    flavor_cold_metadata compile_time_metadata;
    const char *const library_path; //Path to the dynamic library implementing the flavor API
    // Simple linked list implementation
    struct flavor_library *next;
};
typedef struct flavor_library flavor_library;

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

// Global access structure for storing pointers to control structures of currently installed
// FLAVOUR module
flavor_access_structure global_flavor_access_structure = {0};
flavor_access_structure_ptr global_flavor_access_structure_ptr = &global_flavor_access_structure;
// Local solib handle of currently open flavor
static void *flavor_handle = NULL;

/* ========== START FLAVOUR module VERY SIMPLE flavour library linked list ========== */

// This linked list implementation is extremely simple and as such very susceptible to errors, the only method of access
// is itineration over all elements starting from the head (known_libraries_head)

static flavor_library *known_libraries_head = NULL;

static bool priority_insert_on_weight(flavor_library *new_node)
{
    flavor_library *new_library = NULL;

    if (!new_node)
    {
        rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI: FLAVOUR API library linked list adder: The flavor library module passed as an argument was NULL\n");
        return false;
    }
    new_library = malloc(sizeof(flavor_library));
    if (!new_library)
    {
        int error = -errno;
        rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI: FLAVOUR API library linked list adder: Cannot malloc space for the new flavor library module named %s of path %s, error (%d)->%s\n", new_node->compile_time_metadata.name, new_node->library_path, error, strerror(-error));
        return false;
    }
    memcpy(new_library, new_node, sizeof(flavor_library));
    if (known_libraries_head)
    {
        flavor_library *current = known_libraries_head;
        flavor_library *previous = NULL;
        while (current)
        {
            if (new_library->compile_time_metadata.weight > current->compile_time_metadata.weight)
            {
                break;
            }
            previous = current;
            current = current->next;
        }
        if (previous)
        {
            previous->next = new_library;
            new_library->next = current;
        }
        else
        {
            flavor_library *temporary_head = known_libraries_head;
            known_libraries_head = new_library;
            new_library->next = temporary_head;
        }
    }
    else
    {
        known_libraries_head = new_library;
    }

    return true;
}

static void flavor_library_free(flavor_library *to_free)
{
    free((char *)to_free->library_path);
    free(to_free);
}

// This has to be called as a last thing, pretty much before unloading of FLAVOUR MODULE, because the objects allocated in this memory
// are used in the all other operations of this code
static void delete_whole_list(void)
{
    flavor_library *local_head = known_libraries_head;
    known_libraries_head = NULL;
    for (; local_head; local_head = local_head->next)
    {
        flavor_library_free(local_head);
    }
}

/* ========== END FLAVOUR module VERY SIMPLE flavour library linked list ========== */

static inline bool check_function_pointer_validity(void *function_ptr)
{
    // There are not many ways how to test that the pointer is a valid function pointer
    // for function with specific signature
    // Pretty much the only way is to test if the pointer point to function by way of dladdr1 (which
    // will also take care of checking if the pointer is in the loaded shared library memory space),
    // but then it is implemented by way of ELF hackery and needs visible symbols (so no static modifier)
    // Because of this we will only test for not NULL
    //
    // Pointers to better checking implementations and redoing welcome
    return function_ptr ? true : false;
}
#define WRAPPER_HELPER(string) string
#define CHECK_FOR_FUNCTION(structure, function_name, ...)                                               \
    do                                                                                                  \
    {                                                                                                   \
        if (!check_function_pointer_validity(WRAPPER_HELPER(structure)->WRAPPER_HELPER(function_name))) \
        {                                                                                               \
            rtapi_print_msg(RTAPI_MSG_ERR, __VA_ARGS__);                                                \
            retval = false;                                                                             \
        }                                                                                               \
    } while (false);

static bool is_flavor_runtime_business_valid(const char *const flavor_name, flavor_runtime_business_ptr runtime_business)
{
#define FRBV_CHECK(structure, function_name) CHECK_FOR_FUNCTION(structure, function_name, "RTAPI: FLAVOUR API library finder: Flavor '%s' exports NULL %s function (RUNTIME BUSINESS). This is extremely bad.\n", flavor_name, #function_name)

    bool retval = true;
    FRBV_CHECK(runtime_business, task_delete_hook)
    FRBV_CHECK(runtime_business, task_start_hook)
    FRBV_CHECK(runtime_business, task_stop_hook)

    FRBV_CHECK(runtime_business, task_delay_hook)
    FRBV_CHECK(runtime_business, get_time_hook)
    FRBV_CHECK(runtime_business, get_clocks_hook)
    FRBV_CHECK(runtime_business, task_self_hook)

    FRBV_CHECK(runtime_business, task_update_stats_hook)
    FRBV_CHECK(runtime_business, task_print_thread_stats_hook)

    FRBV_CHECK(runtime_business, task_pause_hook)
    FRBV_CHECK(runtime_business, task_wait_hook)
    FRBV_CHECK(runtime_business, task_resume_hook)
    return retval;
}

static bool is_flavor_hot_metadata_valid(const char *const flavor_name, flavor_hot_metadata_ptr hot_metadata)
{
#define FHMV_CHECK(structure, function_name) CHECK_FOR_FUNCTION(structure, function_name, "RTAPI: FLAVOUR API library finder: Flavor '%s' exports NULL %s function (HOT METADATA). This is extremely bad.\n", flavor_name, #function_name)

    bool retval = true;
    FHMV_CHECK(hot_metadata, module_init_hook)
    FHMV_CHECK(hot_metadata, module_exit_hook)
    return retval;
}

void signal_if_excessive_number_of_flavor_libraries_found(void)
{
    static unsigned int runs = 0;

    runs++;
    if (runs > LIMIT_FLAVOR_LIBRARIES_FOUND)
    {
        rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI: FLAVOUR API library finder: The flavor library module number %d found, this is a little bit excessive\n", runs);
    }
    else if (runs == LIMIT_FLAVOR_LIBRARIES_FOUND)
    {
        rtapi_print_msg(RTAPI_MSG_INFO, "RTAPI: FLAVOUR API library finder: There seems to be an excessive number (%d) of found flavor library modules\n", LIMIT_FLAVOR_LIBRARIES_FOUND);
    }
}

static bool flavor_library_factory(const char *const path, char *name, unsigned int id, unsigned int weight, unsigned int magic, unsigned int flags, void *cloobj)
{
    signal_if_excessive_number_of_flavor_libraries_found();

    // We are assuming that this function will be called multiple times for the same
    // library given possible existence of multiple symlinks, which by search function
    // will all be treated as a match
    // But the name HAS(!) TO BE unique even by case-insensitive match and is generally
    // shorter to test than path
    for (flavor_library *index = known_libraries_head; index; index = index->next)
    {
        if (strcasecmp(index->compile_time_metadata.name, name) == 0 || index->compile_time_metadata.id == id)
        {
            if (strcmp(index->library_path, path) != 0)
            {
                rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI: FLAVOUR API library finder: Two libraries with same name name, but different paths were found on the system. First library '%s' on %s, second library '%s' on %s.", index->compile_time_metadata.name, index->library_path, name, path);
                return false;
            }
            rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI: FLAVOUR API library finder: Two libraries with same name '%s' or id '%d' were found on the system in %s.", name, id, path);
            return false;
        }
    }

    // Path is mallocated once even if it would be prettier to have it in an array in flavor_library the same way
    // as flavor_cold_metadata.name, but given the PATH_MAX is pretty big, it would cause unnecessary large copying (about 2 order of magnitude)
    char *path_alloc = strdup(path);
    if (!path_alloc)
    {
        int error = -errno;
        rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI: FLAVOUR API library finder: There was an error when trying to malloc: (%d)->%s", error, strerror(-error));
        return false;
    }
    flavor_library library = {
        .compile_time_metadata = {
            .id = id,
            .weight = weight,
            .magic = magic,
            .flags = flags},
        .library_path = path_alloc,
        .next = NULL};
    strncpy((char *)library.compile_time_metadata.name, name, MEMBER_SIZEOF(flavor_cold_metadata, name));

    return priority_insert_on_weight(&library);
}

/* ========== START FLAVOUR module registration and unregistration functions ========== */
// Point of contact with flavour API library
// This function is called from FLAVOUR LIBRARY MODULE
void register_flavor(flavor_hot_metadata_ptr descriptor_to_register)
{
    hal_u32_t temp_state = rtapi_load_u32(&(global_flavor_access_structure_ptr->state));
    if (!(~temp_state & FLAVOR_STATE_INITIALIZED))
    {
        global_flavor_access_structure_ptr->flavor_module_hot_metadata_descriptor = descriptor_to_register;
        // WE are not changing the global state here, because this decision is for rtapi_flavor RTAPI to rule
    }
}

// Point of contact with flavour API library
// This function is called from FLAVOUR LIBRARY MODULE
void unregister_flavor(flavor_hot_metadata_ptr descriptor_to_unregister)
{
    hal_u32_t temp_state = rtapi_load_u32(&(global_flavor_access_structure_ptr->state));
    if (!(~temp_state & FLAVOR_STATE_INSTALLED) && global_flavor_access_structure_ptr->flavor_module_hot_metadata_descriptor == descriptor_to_unregister)
    {
        global_flavor_access_structure_ptr->flavor_module_hot_metadata_descriptor = NULL;
        // WE are not changing the global state here, because this decision is for rtapi_flavor RTAPI to rule
    }
}
/* ========== END FLAVOUR module registration and unregistration functions ========== */

/* ========== START FLAVOUR module arming and yielding functions ========== */
// Point of contact with flavour API library
// This function is called from FLAVOUR LIBRARY MODULE
int arm_flavor(flavor_runtime_business_ptr descriptor_to_arm)
{
    hal_u32_t temp_state = rtapi_load_u32(&(global_flavor_access_structure_ptr->state));
    if (!(~temp_state & FLAVOR_STATE_INSTALLED))
    {
        if (is_flavor_runtime_business_valid(global_flavor_access_structure_ptr->flavor_module_cold_metadata_descriptor->name, descriptor_to_arm))
        {
            global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor = descriptor_to_arm;
            // WE are changing the global state here as this decision is wholly in competence of FLAVOUR LIBRARY MODULE
            // and by calling this function, it signalling the rtapi_flavor RTAPI library to do the change
            rtapi_store_u32(&(global_flavor_access_structure_ptr->state), (temp_state | FLAVOR_STATE_ARM));
            return 0;
        }
        return -EINVAL;
    }
    return -EPERM;
}

// Point of contact with flavour API library
// This function is called from FLAVOUR LIBRARY MODULE
int yield_flavor(flavor_runtime_business_ptr descriptor_to_yield)
{
    hal_u32_t temp_state = rtapi_load_u32(&(global_flavor_access_structure_ptr->state));
    if ((~temp_state & FLAVOR_STATE_ARMED) && global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor == descriptor_to_yield)
    {
        global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor = NULL;
        // WE are changing the global state here as this decision is wholly in competence of FLAVOUR LIBRARY MODULE
        // and by calling this function, it signalling the rtapi_flavor RTAPI library to do the change
        rtapi_store_u32(&(global_flavor_access_structure_ptr->state), (temp_state & ~FLAVOR_STATE_ARM));
        return 0;
    }
    return -EPERM;
}
/* ========== END FLAVOUR module arming and yielding functions ========== */

// In the end, the solib is alway dlopened on the filepath, so in every case this function
// will be called and the call can be successful only when there is no other library dlopened
// We are assuming that the input *solib_path is already tested as being Machinekit flavour
// API library
static bool install_flavor_solib(const char *solib_path)
{
    if (!flavor_handle)
    {
        flavor_handle = dlopen(solib_path, RTLD_NOW | RTLD_LOCAL);
        if (!flavor_handle)
        {
            char *error = dlerror();
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
    if (flavor_handle)
    {
        int retval = -1;
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
    // WE are allowing the call only in STATEs INITIALIZED, NOT EXITED and NOT ARMED, i.e. we don't care
    // if the state is INSTALLED or NOT INSTALLED, because at the end of this function call the state will be NOT INSTALLED
    hal_u32_t temp_state = rtapi_load_u32(&(global_flavor_access_structure_ptr->state));
    if (!(temp_state & ~(FLAVOR_STATE_INSTALLED)))
    {
        rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI: FLAVOR library loader: Call to EXECUTE_CHECKED_UNINSTALL_OF_FLAVOR is not allowed in state '%d'.\n", temp_state);
        return false;
    }

    if (uninstall_flavor_solib())
    {
        // FH was populated and now is unpopulated
        // Flavor solib should run it's descructor code and call unregister flavor,
        // if the solib was not dlopened multiple times, which we definitely do not want
        if (global_flavor_access_structure_ptr->flavor_module_hot_metadata_descriptor)
        {
            //Somewhere an error happened (incorrect flavour library), signal and so
            rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI: FLAVOR library loader: Library '%s' did not correctly unloaded flavor_descriptor.\n", global_flavor_access_structure_ptr->flavor_module_cold_metadata_descriptor->name);
            global_flavor_access_structure_ptr->flavor_module_hot_metadata_descriptor = NULL;
        }
        temp_state = rtapi_load_u32(&(global_flavor_access_structure_ptr->state));
        rtapi_store_u32(&(global_flavor_access_structure_ptr->state), (temp_state & ~FLAVOR_STATE_INSTALL));
        return true;
    }
    rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI: FLAVOR library loader: Call to EXECUTE_CHECKED_UNINSTALL_OF_FLAVOR was performed when no FLAVOUR MODULE LIBRARY is loaded. This signals fault in higher logic. The current status is '%d'.\n", temp_state);
    return false;
}

static bool execute_checked_install_of_flavor(flavor_library *library_to_install)
{
    hal_u32_t temp_state = rtapi_load_u32(&(global_flavor_access_structure_ptr->state));
    if (!(~temp_state & FLAVOR_STATE_INITIALIZED))
    {
        rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI: FLAVOR library loader: Call to EXECUTE_CHECKED_INSTALL_OF_FLAVOR is not allowed in state '%d'.\n", temp_state);
        if (temp_state & FLAVOR_STATE_INSTALL)
        {
            rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI: FLAVOUR library loader: There is already flavour API library '%s' installed. You cannot install more than one library at a time.\n", global_flavor_access_structure_ptr->flavor_module_cold_metadata_descriptor->name);
        }
        return false;
    }

    if (install_flavor_solib(library_to_install->library_path))
    {
        if (!(global_flavor_access_structure_ptr->flavor_module_hot_metadata_descriptor))
        {
            // Hopefully should also check situation when developer mix the constructor
            // and destructor
            if (!execute_checked_uninstall_of_flavor())
            {
                rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI: FLAVOR library loader: EXECUTE_CHECKED_INSTALL_OF_FLAVOR, failed automatic install of FLAVOR module could not uninstall flavor module.\n");
            }
            return false;
        }
        // Now we need to verify the installed FLAVOUR module
        if (!is_flavor_hot_metadata_valid(library_to_install->compile_time_metadata.name, global_flavor_access_structure_ptr->flavor_module_hot_metadata_descriptor))
        {
            rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI: FLAVOR library loader: EXECUTE_CHECKED_INSTALL_OF_FLAVOR, flavour shared library defined in '%d' failed HOT METADATA validation check.\n", library_to_install->library_path);
            if (!execute_checked_uninstall_of_flavor())
            {
                rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI: FLAVOR library loader: EXECUTE_CHECKED_INSTALL_OF_FLAVOR, failed validation of FLAVOR module could not uninstall flavor module.\n");
            }
            return false;
        }
        global_flavor_access_structure_ptr->flavor_module_cold_metadata_descriptor = &(library_to_install->compile_time_metadata);
        // Flavor was successfully installed and FD was registered
        temp_state = rtapi_load_u32(&(global_flavor_access_structure_ptr->state));
        rtapi_store_u32(&(global_flavor_access_structure_ptr->state), (temp_state | FLAVOR_STATE_INSTALL));
        return true;
    }
    //Something installed but should not be installed or loading error
    rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI: FLAVOUR library loader: There is an error trying to load shared library in NOT INSTALLED state.\n");
    return false;
}

static bool LAMBDA_is_machinekit_flavor_solib_v1(const char *const path, size_t size_of_input, void *input, void *cloobj)
{
    return is_machinekit_flavor_solib_v1(path, size_of_input, input, flavor_library_factory, cloobj);
}

static bool LAMBDA_file_find(const char *const path, void *cloobj)
{
    //Předělat na scan_file_for_elf_sections?
    //Nebo poskládat test_file_for_module_data tak, aby používal scan_file_for_elf_sections
    return test_file_for_module_data(path, "machinekit-flavor", LAMBDA_is_machinekit_flavor_solib_v1, cloobj);
}

static int search_directory_for_flavor_modules(const char *const path, void *cloobj)
{
    return for_each_node(path, NULL, LAMBDA_file_find, cloobj);
}

static int search_mountpoint_for_flavor_modules(const char *const path, void *cloobj)
{
    return for_each_node(path, search_mountpoint_for_flavor_modules, LAMBDA_file_find, cloobj);
}

static int discover_default_flavor_modules(void)
{
    if (known_libraries_head)
    {
        return 0; // So far we want this function to run only once, change in the future possible
    }
    return get_paths_of_library_module("FLAVOR_LIB_DIR", search_directory_for_flavor_modules, NULL);
}

static bool install_flavor_by_path(const char *const path)
{
    if (LAMBDA_file_find(path, NULL))
    {
        rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI: FLAVOR library loader: INSTALL_FLAVOR_BYT_PATH could not find flavor module at specified path '%s'.\n", path);
    }

    for (flavor_library *index = known_libraries_head; index; index = index->next)
    {
        if (strcmp(index->library_path, path) == 0)
        {
            return execute_checked_install_of_flavor(index);
        }
    }
    return false;
}

static bool install_flavor_by_id(unsigned int id)
{
    int found = 0;

    found = discover_default_flavor_modules();
    rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI: FLAVOUR library loader: INSTALL_FLAVOR_BY_ID function found %d flavor library modules.\n", found);

    for (flavor_library *index = known_libraries_head; index; index = index->next)
    {
        if (index->compile_time_metadata.id == id)
        {
            return execute_checked_install_of_flavor(index);
        }
    }
    rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI: FLAVOUR library loader: Library with specified ID '%d' could not be installed. Library HAS TO BE in standard location for this function to find.\n", id);
    return false;
}

static bool install_default_flavor(void)
{
    unsigned int counter = 0;
    int found = 0;

    found = discover_default_flavor_modules();
    rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI: FLAVOUR library loader: INSTALL_DEFAULT_FLAVOR function found %d flavor library modules.\n", found);

    for (flavor_library *index = known_libraries_head; index; index = index->next)
    {
        counter++;
        rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI: FLAVOUR library loader: Trying to install flavour library module '%s'.\n", index->compile_time_metadata.name);
        if (execute_checked_install_of_flavor(index))
        {
            rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI: FLAVOUR library loader: Flavour library module '%s' installed.\n", index->compile_time_metadata.name);
            return true;
        }
    }
    rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI: FLAVOUR library loader: Not one flavour module could be installed (Tested %d modules). Have you installed at least one? Maybe try to set the path directly.\n", counter);
    return false;
}

static bool install_flavor_by_name(const char *name)
{
    int found = 0;

    found = discover_default_flavor_modules();
    rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI: FLAVOUR library loader: INSTALL_FLAVOR_BY_NAME function found %d flavor library modules.\n", found);

    for (flavor_library *index = known_libraries_head; index; index = index->next)
    {
        if (strcasecmp(index->compile_time_metadata.name, name) == 0)
        {
            return execute_checked_install_of_flavor(index);
        }
    }
    rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI: FLAVOUR library loader: Library with specified name '%s' could not be installed. Library HAS TO BE in standard location for this function to find.\n", name);
    return false;
}

/* ========== START FLAVOUR module user input functions ========== */
union flavor_input_data {
    char flavor_name[MAX_FLAVOR_NAME_LEN + 1];
    char flavor_path[PATH_MAX];
    unsigned int flavor_id;
};
typedef union flavor_input_data flavor_input_data;
enum flavor_input_data_state
{
    FLAVOR_NOT_SET,
    FLAVOR_ID_SET,
    FLAVOR_PATH_SET,
    FLAVOR_NAME_SET
};
typedef enum flavor_input_data_state flavor_input_data_state;
struct flavor_input_data_processed
{
    flavor_input_data_state state;
    flavor_input_data data;
};
typedef struct flavor_input_data_processed flavor_input_data_processed;

static int parse_flavor_data_from_string(char *input_string, flavor_input_data_processed *output_tuple)
{
    char *delimiter = NULL;
    int retval = -1;
    // Flavor ID value 0 is invalid
    uintmax_t flavor_id = 0;

    if (!output_tuple)
    {
        rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI: PARSE_DATA_FROM_STRING was passed NULL pointer as an output_tuple\n");
        retval = -EINVAL;
        goto end;
    }
    // This function basically "takes over" the output-tuple object, so signal the NOT-SET state
    output_tuple->state = FLAVOR_NOT_SET;

    // We are discharging errno before we try to convert input into unsigned integer
    errno = 0;
    flavor_id = strtoumax(input_string, &delimiter, 10);
    if (errno == ERANGE)
    {
        // User passed negative value, that's an error
        rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI PARSE_DATA_FROM_STRING was passed numeric value as a flavor_id, which is outside the allowed range\n");
        retval = -ERANGE;
        goto end;
    }

    if (*delimiter == '\0')
    {
        // There is a possibility that the user wanted flavour NAME or file with path looking like '-number', however
        // this is little bit far fetched, ugly and solvable by changing 'goto end' to 'goto file-test'
        if (*input_string == '-')
        {
            // User passed negative value, that's an error
            rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI PARSE_DATA_FROM_STRING was passed negative value as a flavor_id, value: %s\n", input_string);
            retval = -EINVAL;
            goto end;
        }
        // What user passed is a valid unsigned long integer, not name starting with number, so we are done
        if (flavor_id > UINT_MAX)
        {
            // User passed value too high, that's an error
            rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI PARSE_DATA_FROM_STRING was passed value too high as a flavor_id, value: %s, limit: %u\n", input_string, UINT_MAX);
            retval = -EINVAL;
            goto end;
        }
        if (flavor_id == 0)
        {
            // User passed value 0, that's an error
            rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI PARSE_DATA_FROM_STRING was passed 0 as a value as a flavor_id, value 0 is not valid\n");
            retval = -EINVAL;
            goto end;
        }
        output_tuple->data.flavor_id = (unsigned int)flavor_id;
        output_tuple->state = FLAVOR_ID_SET;
        rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI PARSE_DATA_FROM_STRING found a flavor_ID number %u\n", (unsigned int)flavor_id);
        goto success;
    }

    // Test if given string is not an address to a file, in which case we will presume
    // that user wanted to pass a PATH
    if (!(faccessat(AT_FDCWD, input_string, F_OK | R_OK, AT_EACCESS)))
    {
        if (realpath(input_string, output_tuple->data.flavor_path))
        {
            // No error occured, we now should have absolute path copied into output_tuple
            rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI PARSE_DATA_FROM_STRING found a flavor PATH  %s\n", output_tuple->data.flavor_path);
            output_tuple->state = FLAVOR_PATH_SET;
            goto success;
        }
        int error = errno;
        rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI PARSE_DATA_FROM_STRING cout not get REALPATH on file: %s, error (%d)->%s\n", input_string, error, strerror(error));
        retval = -EINVAL;
        goto end;
    }

    // What the user passed is a string name and we will take it as such
    if (strlen(input_string) > MAX_FLAVOR_NAME_LEN)
    {
        // User passed value too long, that's an error
        rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI PARSE_DATA_FROM_STRING was passed too long value for flavor name '%s', passed string lenght: %ld, maximum lenght: %d\n", input_string, strlen(input_string), MAX_FLAVOR_NAME_LEN);
        retval = -EINVAL;
        goto end;
    }
    // Zero signals that the value is not used, flavor_id cannot be 0
    strncpy(output_tuple->data.flavor_name, input_string, sizeof output_tuple->data.flavor_name);
    output_tuple->state = FLAVOR_NAME_SET;
    rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI PARSE_DATA_FROM_STRING found a flavor name  %s\n", output_tuple->data.flavor_name);

success:
    retval = 0;
end:
    return retval;
}

static int extract_flavor_from_cmdline_arguments(int argc, char *const *argv, flavor_input_data_processed *output_tuple)
{
    struct option long_flavor_options[] = {
        {"flavor", required_argument, 0, 'f'},
        {0}};
    char *short_flavor_options = "f:";
    char *input = NULL;
    int old_opterr = opterr;
    int old_optind = optind;
    int character_retval = -1;
    int option_index = 0;
    int retval = -1;

    if (!output_tuple)
    {
        rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI: EXTRACT_FLAVOR_FROM_CMDLINE_ARGUMENTS was passed NULL pointer as an output_tuple\n");
        retval = -EINVAL;
        goto end;
    }

    // We are not presuming (actually, we are presuming the exact opposite) that this operation will be the first
    // iteration
    optind = 1;
    // For this operation only, we are setting the error output to silent
    opterr = 0;

    while ((character_retval = getopt_long(argc, argv, short_flavor_options, long_flavor_options, &option_index)) != -1)
    {
        switch (character_retval)
        {
        case 'f':
            if (!input)
            {
                // This is little bit on the edge of 'good code' as we are deleting the first '=' if present because
                // if the user passed argument as a '-f=ident', then optarg will include it and we don't want it
                // But we are nowhere else specifying that flavour names cannot start with '='
                // Based on discussion this should be removed
                input = strdupa((*optarg == '=' ? (optarg + 1) : optarg));
            }
            else
            {
                // The same option exists more than once in the cmdline arguments, that's an error
                //rtapi_msg_prinf(RTAPI_ERR,"RTAPI EXTRACT_FLAVOR_FROM_CMDLINE_ARGUMENT was passed --flavor=optarg or -f optarg more than once with arguments '%s' and '%s'\n",input, optarg);
                retval = -EINVAL;
                goto cleanup;
            }
            break;
        case '?':
        case ':':
        default:
            break;
        }
    }
    if (!input)
    {
        rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI: EXTRACT_FLAVOR_FROM_CMDLINE_ARGUMENTS did not discover any --flavor option\n");
        retval = -ENODATA;
        goto cleanup;
    }

    retval = parse_flavor_data_from_string(input, output_tuple);

cleanup:
    opterr = old_opterr;
    optind = old_optind;
end:
    return retval;
}

static int extract_flavor_from_cmdline_arguments_wrapper(int *argc, char **argv, void *cloobj)
{
    flavor_input_data_processed *fidp = (flavor_input_data_processed *)cloobj;
    return extract_flavor_from_cmdline_arguments(*argc, (char *const *)argv, fidp);
}

static int extract_flavor_from_environmet_variables(flavor_input_data_processed *output_tuple)
{
    char *input = NULL;
    int retval = -1;

    if (!output_tuple)
    {
        rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI: EXTRACT_FLAVOR_FROM_ENVIRONMENT_VARIABLES was passed NULL pointer as an output_tuple\n");
        retval = -EINVAL;
        goto end;
    }

    input = getenv("FLAVOR");
    if (!input)
    {
        // Nothing found in environment
        rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI: EXTRACT_FLAVOR_FROM_ENVIRONMENT_VARIABLES did not discover any FLAVOR variable\n");
        retval = -ENODATA;
        goto end;
    }

    retval = parse_flavor_data_from_string(input, output_tuple);

end:
    return retval;
}
/* ========== END FLAVOUR module user input functions ========== */

/*
 * Here are implemented the nonstatic "public" function by which higher parts of rtapi.so program
 * communicate with rtapi_flavor
 * These functions are also exported by the EXPORT_SYMBOL MACRO
*/

int get_names_of_known_flavor_modules(char **output_string_map)
{
    int string_counter = 0;
    size_t string_lenght = 0;
    size_t delta = 0;
    char *delimiter = NULL;

    if (*output_string_map)
    {
        // The output_string_map pointer is not set to NULL
        rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI: GET_NAME_OF_KNOWN_MODULES was passed NULL pointer as an output_string_map\n");
        goto end;
    }

    for (flavor_library *index; index; index = index->next)
    {
        string_lenght += strlen(index->compile_time_metadata.name) + 1;
    }

    *output_string_map = malloc(string_lenght);
    if (!(*output_string_map))
    {
        // An error occured when mallocing new string map
        int error = errno;
        rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI: GET_NAME_OF_KNOWN_MODULES encountered an error when mallocing %ld chars for output_string_map with error (%d)->%s\n", string_lenght, error, strerror(error));
        goto end;
    }

    delimiter = *output_string_map;
    for (flavor_library *index; index; index = index->next)
    {
        delta = strlen(index->compile_time_metadata.name) + 1;
        strncpy(delimiter, index->compile_time_metadata.name, delta);
        delimiter += delta;
        string_counter++;
    }

end:
    return string_counter;
}

int flavor_module_startup(void)
{
    flavor_input_data_processed flavor_information_cmdline = {0};
    flavor_input_data_processed flavor_information_environment = {0};
    flavor_input_data_processed *valid_data = NULL;
    int retval_cmdline = -1;
    int retval_environment = -1;
    int retval = -1;
    hal_u32_t temp_state = 0;

    // This operation only makes a sense in the INITIALIZED state (and nothing else)
    temp_state = rtapi_load_u32(&(global_flavor_access_structure_ptr->state));
    if (~temp_state & FLAVOR_STATE_INITIALIZED)
    {
        retval = -EPERM;
        goto end;
    }

    retval_cmdline = execute_on_cmdline_copy(extract_flavor_from_cmdline_arguments_wrapper, (void *)&flavor_information_cmdline);
    retval_environment = extract_flavor_from_environmet_variables(&flavor_information_environment);

    if (retval_cmdline != -ENODATA && retval_environment != -ENODATA)
    {
        // At least one has to be -ENODATA (i.e. variable/argument not present), as we consider passing command both
        // in Environment variable and Commandline argument as a potential security rist
        rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI: FLAVOR_MODULE_STARTUP cannot have have defined both --flavor in commandline arguments [(%d)->%s] and FLAVOR in environment variables [(%d)->%s]\n", retval_cmdline, strerror(-retval_cmdline), retval_environment, strerror(-retval_environment));
        retval = -EPERM;
        goto end;
    }
    if (retval_cmdline != 0 && retval_cmdline != -ENODATA)
    {
        // An error when processing the command line arguments occured
        retval = retval_cmdline;
        rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI: FLAVOR_MODULE_STARTUP discovered an error in --flavor in commandline arguments (%d)->%s\n", retval_cmdline, strerror(-retval_cmdline));
        goto end;
    }
    if (retval_environment != 0 && retval_environment != -ENODATA)
    {
        // An error when processing the environment variables occured
        rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI: FLAVOR_MODULE_STARTUP discovered an error in FLAVOR in environment variables (%d)->%s\n", retval_environment, strerror(-retval_environment));
        retval = retval_environment;
        goto end;
    }

    // At this point only one or none retval_* should be 0, so store the address of valid structure into
    // a pointer for easy access later on
    if (!retval_cmdline)
    {
        valid_data = &flavor_information_cmdline;
    }
    if (!retval_environment)
    {
        valid_data = &flavor_information_environment;
    }

    // The pointer is not NULL, which means that user expressed which flavour module he wants to load
    if (valid_data)
    {
        switch (valid_data->state)
        {
        case FLAVOR_PATH_SET:
            retval = install_flavor_by_path(valid_data->data.flavor_path);
            goto end;
        case FLAVOR_ID_SET:
            retval = install_flavor_by_id(valid_data->data.flavor_id);
            goto end;
        case FLAVOR_NAME_SET:
            retval = install_flavor_by_name(valid_data->data.flavor_name);
            goto end;
        default:
            // This absolutelly should not happen as it means FLAVOR_NOT_SET
            rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI: FLAVOR_MODULE_STARTUP encountered an unknown error\n");
            retval = -ENOTRECOVERABLE;
            goto end;
        }
    }
    // The user did not specify which flavour module he wants to load, the program will automatically load the best
    // module available
    else
    {
        retval = install_default_flavor();
    }

end:
    return retval;
}

int flavor_module_shutdown(void)
{
    int retval = -1;
    hal_u32_t temp_state = 0;
    hal_u32_t exit_state = 0;

    // This operation only makes a sense in the INSTALLED state (and nothing else) -> REALLY
    // NEBO dodělat ještě if a zavolat yielding funkci pokud je status ARMED
    temp_state = rtapi_load_u32(&(global_flavor_access_structure_ptr->state));
    if (~temp_state & FLAVOR_STATE_INSTALLED)
    {
        retval = -EPERM;
        goto end;
    }
    // Dodělat implementaci

    // Reload the current state and save new one
    temp_state = rtapi_load_u32(&(global_flavor_access_structure_ptr->state));
    exit_state = temp_state | FLAVOR_STATE_EXIT;
    if (exit_state != FLAVOR_STATE_EXITED)
    {
        // Something went wrong
    }
    rtapi_store_u32(&(global_flavor_access_structure_ptr->state), exit_state);
end:
    return retval;
}

int flavor_is_installed(void)
{
    hal_u32_t temp_state = rtapi_load_u32(&(global_flavor_access_structure_ptr->state));
    if (temp_state & FLAVOR_STATE_INSTALL)
    {
        // Follow the norm that OK returns 0?
        return 0;
    }
    return -ENXIO;
}

int flavor_is_armed(void)
{
    hal_u32_t temp_state = rtapi_load_u32(&(global_flavor_access_structure_ptr->state));
    if (temp_state & FLAVOR_STATE_ARM)
    {
        // Follow the norm that OK returns 0?
        return 0;
    }
    return -ENXIO;
}

#ifdef RTAPI
EXPORT_SYMBOL(get_names_of_known_flavor_modules);
//EXPORT_SYMBOL(flavor_module_startup);
EXPORT_SYMBOL(flavor_is_installed);
EXPORT_SYMBOL(flavor_is_armed);
#endif