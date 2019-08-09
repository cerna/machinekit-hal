#ifndef RTAPI_FLAVOR_H
#define RTAPI_FLAVOR_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <assert.h>
#include "rtapi_common.h"

// Flavor features:  flavor_cold_metadata.flags bits for configuring flavor
// - Whether iopl() needs to be called
#define FLAVOR_DOES_IO RTAPI_BIT(0)
// - Whether flavor has hard real-time latency
#define FLAVOR_IS_RT RTAPI_BIT(1)
// - Whether flavor has hard real-time latency
#define FLAVOR_TIME_NO_CLOCK_MONOTONIC RTAPI_BIT(2)
// - Whether flavor runs outside RTAPI threads
#define FLAVOR_NOT_RTAPI RTAPI_BIT(3)

#define MAX_FLAVOR_NAME_LEN 20

// The exception code puts structs in shm in an opaque blob; this is used to
// check the allocated storage is large enough
// https://stackoverflow.com/questions/807244/
#define ASSERT_SIZE_WITHIN(type, size) \
    typedef char assertion_failed_##type##_[2 * !!(sizeof(type) <= size) - 1]

    // Hook type definitions for the FLAVOUR module API structs
    typedef void (*rtapi_exception_handler_hook_t)(int type, rtapi_exception_detail_t *detail, int level);
    typedef int (*rtapi_module_init_hook_t)(void);
    typedef void (*rtapi_module_exit_hook_t)(void);
    typedef int (*rtapi_task_update_stats_hook_t)(void);
    typedef void (*rtapi_print_thread_stats_hook_t)(int task_id);
    typedef int (*rtapi_task_new_hook_t)(int task_id, task_data *task);
    typedef int (*rtapi_task_delete_hook_t)(int task_id);
    typedef int (*rtapi_task_start_hook_t)(int task_id);
    typedef int (*rtapi_task_stop_hook_t)(int task_id);
    typedef int (*rtapi_task_pause_hook_t)(int task_id);
    typedef int (*rtapi_task_wait_hook_t)(const int flags);
    typedef int (*rtapi_task_resume_hook_t)(int task_id);
    typedef void (*rtapi_delay_hook_t)(long int nsec);
    typedef long long int (*rtapi_get_time_hook_t)(void);
    typedef long long int (*rtapi_get_clocks_hook_t)(void);
    typedef int (*rtapi_task_self_hook_t)(void);
    typedef long long (*rtapi_task_pll_get_reference_hook_t)(void);
    typedef int (*rtapi_task_pll_set_correction_hook_t)(long value);

    /* ========== START compile-time metadata FLAVOUR module struct ========== */
    struct flavor_cold_metadata
    {
        // Name represents unique identifier of the flavour API shared library,
        // there cannot be two on the same system with the same name at the same moment
        const char name[MAX_FLAVOR_NAME_LEN + 1];
        // Flavour ID represents unique identifier of the flavour API shared library,
        // there cannot be two on the same system with the same name at the same moment
        const unsigned int id;
        // Flavour magic represents version of the flavour API implementation and is unique
        // only in context of given flavour
        // Use is intended for volatile flavours where changes in auxiliary API happen often
        // and to synchronize what parts of Machinekit-HAL expect of flavour with given "name/flavor_id"
        // and what this exact shared library flavour API implementation can support
        const unsigned int magic;
        // Flavour specific flags defined at the start of this document
        const unsigned long flags;
        // Flavour weight represents the ordering in which multiple found FLAVOUR modules
        // on system will be tried in automatic mode to register
        const unsigned int weight;
        // Flavour API version number is intended as a synchronization mechanism for decouplinh
        // or RTAPI.so shared library and FLAVOUR module shared libraries
        // It's intend is to future-proof and now should be always 1 (look at function is_machinekit_flavor_solib_v1)
        const unsigned int api_version;
    };
    typedef struct flavor_cold_metadata flavor_cold_metadata;
    typedef flavor_cold_metadata *flavor_cold_metadata_ptr;
    /* ========== END compile-time metadata FLAVOUR module struct ========== */

    /* ========== START run-time metadata FLAVOUR module struct ========== */
    struct flavor_hot_metadata
    {
        const rtapi_module_init_hook_t module_init_hook;
        const rtapi_module_exit_hook_t module_exit_hook;
    };
    typedef struct flavor_hot_metadata flavor_hot_metadata;
    typedef flavor_hot_metadata *flavor_hot_metadata_ptr;

    /* The global flavour run-time metadata struct
     * States:  not-NULL: means that the FLAVOUR module was successfully
     *                    registered
     *          NULL:     state should only happed in initialization and shut down
     *                    phase of Machinekit 
    */
    extern flavor_hot_metadata_ptr flavor_module_metadata_descriptor;
    /* ========== END run-time metadata FLAVOUR module struct ========== */

    /* ========== START run-time business logic FLAVOUR module struct ========== */
    struct flavor_runtime_business
    {
        const rtapi_exception_handler_hook_t exception_handler_hook;
        const rtapi_task_update_stats_hook_t task_update_stats_hook;
        const rtapi_print_thread_stats_hook_t task_print_thread_stats_hook;
        const rtapi_task_new_hook_t task_new_hook;
        const rtapi_task_delete_hook_t task_delete_hook;
        const rtapi_task_start_hook_t task_start_hook;
        const rtapi_task_stop_hook_t task_stop_hook;
        const rtapi_task_pause_hook_t task_pause_hook;
        const rtapi_task_wait_hook_t task_wait_hook;
        const rtapi_task_resume_hook_t task_resume_hook;
        const rtapi_delay_hook_t task_delay_hook;
        const rtapi_get_time_hook_t get_time_hook;
        const rtapi_get_clocks_hook_t get_clocks_hook;
        const rtapi_task_self_hook_t task_self_hook;
        const rtapi_task_pll_get_reference_hook_t task_pll_get_reference_hook;
        const rtapi_task_pll_set_correction_hook_t task_pll_set_correction_hook;
    };
    typedef struct flavor_runtime_business flavor_runtime_business;
    typedef flavor_runtime_business *flavor_runtime_business_ptr;

    /* The global flavour run-time business logic struct
     * States:  not-NULL: means that the FLAVOUR module was successfully
     *                    initialized and RTAPI flavor_ operations are now possible
     *          NULL:     signals that the flavor_module_init_hook(void) function
     *                    was not run yet, ran with error, or the flavor_module_exit_hook 
     *                    was already run 
    */
    extern flavor_runtime_business_ptr flavor_module_business_descriptor;
    /* ========== END run-time business logic FLAVOUR module struct ========== */

    /* ========== START FLAVOUR module constructor and destructor functions ========== */
    // Main point function by which new flavor module can register itself
    extern void register_flavor(flavor_cold_metadata_ptr descriptor_to_register);

    // Main point function by which registered flavor module can unregister itself
    extern void unregister_flavor(flavor_cold_metadata_ptr descriptor_to_unregister);
    /* ========== END FLAVOUR module constructor and destructor functions ========== */

    /* ========== START FLAVOUR module hot initialization and shutdown functions ========== */
    // Main point function by which ready registered FLAVOUR module becomes operational
    extern void arm_flavor(flavor_runtime_business_ptr descriptor_to_arm);

    // Main point function by which operational (and registered) FLAVOUR module can
    // change state to hot ready
    extern void yield_flavor(flavor_runtime_business_ptr descriptor_to_yield);
    /* ========== END FLAVOUR module hot initialization and shutdown functions ========== */

    /*// Wrappers around flavor_descriptor
    typedef const char *(flavor_names_t)(flavor_descriptor_ptr **fd);
    extern flavor_names_t flavor_names;
    typedef flavor_descriptor_ptr(flavor_byname_t)(const char *flavorname);
    extern flavor_byname_t flavor_byname;
    extern flavor_descriptor_ptr flavor_byid(rtapi_flavor_id_t flavor_id);
    typedef flavor_descriptor_ptr(flavor_default_t)(void);
    extern flavor_default_t flavor_default;
    typedef int(flavor_is_configured_t)(void);
    extern flavor_is_configured_t flavor_is_configured;
    typedef void(flavor_install_t)(flavor_descriptor_ptr flavor_id);
    extern flavor_install_t flavor_install;*/

    // Wrappers for functions in the flavor_descriptor_t
    extern int flavor_exception_handler_hook(
        flavor_descriptor_ptr f, int type, rtapi_exception_detail_t *detail,
        int level);
    extern int flavor_module_init_hook(flavor_descriptor_ptr f);
    extern void flavor_module_exit_hook(flavor_descriptor_ptr f);
    extern int flavor_task_update_stats_hook(flavor_descriptor_ptr f);
    extern void flavor_task_print_thread_stats_hook(
        flavor_descriptor_ptr f, int task_id);
    extern int flavor_task_new_hook(
        flavor_descriptor_ptr f, task_data *task, int task_id);
    extern int flavor_task_delete_hook(
        flavor_descriptor_ptr f, task_data *task, int task_id);
    extern int flavor_task_start_hook(
        flavor_descriptor_ptr f, task_data *task, int task_id);
    extern int flavor_task_stop_hook(
        flavor_descriptor_ptr f, task_data *task, int task_id);
    extern int flavor_task_pause_hook(
        flavor_descriptor_ptr f, task_data *task, int task_id);
    extern int flavor_task_wait_hook(flavor_descriptor_ptr f, const int flags);
    extern int flavor_task_resume_hook(
        flavor_descriptor_ptr f, task_data *task, int task_id);
    extern void flavor_task_delay_hook(flavor_descriptor_ptr f, long int nsec);
    extern long long int flavor_get_time_hook(flavor_descriptor_ptr f);
    extern long long int flavor_get_clocks_hook(flavor_descriptor_ptr f);
    extern int flavor_task_self_hook(flavor_descriptor_ptr f);
    extern long long flavor_task_pll_get_reference_hook(
        flavor_descriptor_ptr f);
    extern int flavor_task_pll_set_correction_hook(
        flavor_descriptor_ptr f, long value);
    /*
    // Accessors for flavor_descriptor
    typedef const char *(flavor_name_t)(flavor_descriptor_ptr f);
    //Do I need this or do I need to
    //extern flavor_name_t flavor_name;
    //extern int flavor_id(flavor_descriptor_ptr f);
    typedef int(flavor_feature_t)(flavor_descriptor_ptr f, int feature);
    extern flavor_feature_t flavor_feature;
*/
    // Help for unit test mocking
    extern int flavor_mocking;
    extern int flavor_mocking_err;

/* Stamping MACRO for the tagging of Machinekit flavour shared library
 *
 * Idea is to create consistent array in ELF file section "machinekit-flavor"
 * which can then be read by the libELF powered tool from RTAPI shared library module
 * 
 * machinekit_flavor_name =         Name of flavour library, size of the name is limited to MAX_FLAVOR_NAME_LEN                                       
 *                                  VALUE: const char*
 * 
 * machinekit_flavor_id =           ID of flavour library, cannot be 0
 *                                  VALUE: unsigned int
 * 
 * machinekit_flavor_weight =       Ordering in which rtapi.so will try to load known flavour
 *                                  libraries, higher number means the library will be tried
 *                                  sooner, has sense only for automatic (default) loading
 *                                  VALUE: unsigned integer
 * 
 * machinekit_flavor_magic =        Special magic number of the flavour, definition defined 
 *                                  by flavour itself
 *                                  Used for diferentianing of FLAVOUR modules versions when 
 *                                  implementing same API version
 *                                  VALUE: unsigned integer
 * 
 * machinekit_flavor_flags =        Combination of flags defining the properties of flavor
 *                                  VALUE: unsigned integer
 * 
 * machinekit_flavor_api_version =  RESERVED number for future changes in rtapi,
 *                                  for now only 1 has to be used
 *                                  VALUE: unsigned integer
 * 
 * USE as FLAVOR_STAMP(evl-core, 2, 1)
 * will create memory array {unsigned int = API version, unsigned int = weight, null terminated char array = name of flavour}
 */
#define FLAVOR_NAME_DEFINE(flavour_name_define)                                                   \
    const char flavor_name[] __attribute__((section("machinekit-flavor"))) = flavour_name_define; \
    static_assert((sizeof(flavor_name) / sizeof(char)) <= (MAX_FLAVOR_NAME_LEN + 1) * sizeof(char), "Flavor name is too long.");
#define FLAVOR_ID_DEFINE(flavour_id_define) const unsigned int flavor_id __attribute__((section("machinekit-flavor"))) = flavour_id_define;
#define FLAVOR_MAGIC_DEFINE(flavour_magic_define) const unsigned int flavor_magic __attribute__((section("machinekit-flavor"))) = flavour_magic_define;
#define FLAVOR_FLAGS_DEFINE(flavour_magic_define) const unsigned int flavor_flags __attribute__((section("machinekit-flavor"))) = flavour_flags_define;
#define FLAVOR_WEIGHT_DEFINE(flavour_weight_define) const unsigned int flavor_weight __attribute__((section("machinekit-flavor"))) = flavour_weight_define;
#define FLAVOR_API_VERSION_DEFINE(flavour_api_version_define) const unsigned int flavor_api_version __attribute__((section("machinekit-flavor"))) = flavour_api_version_define;
#define FLAVOR_STAMP(machinekit_flavor_name, machinekit_flavor_id, machinekit_flavor_weight, machinekit_flavor_magic, machinekit_flavor_flags, machinekit_flavor_api_version) \
    FLAVOR_API_VERSION_DEFINE(machinekit_flavor_api_version)                                                                                                                  \
    FLAVOR_WEIGHT_DEFINE(machinekit_flavor_weight)                                                                                                                            \
    FLAVOR_MAGIC_DEFINE(machinekit_flavor_magic)                                                                                                                              \
    FLAVOR_FLAGS_DEFINE(machinekit_flavor_flags)                                                                                                                              \
    FLAVOR_ID_DEFINE(machinekit_flavor_id)                                                                                                                                    \
    FLAVOR_NAME_DEFINE(machinekit_flavor_name)

#ifdef __cplusplus
}
#endif

#endif
