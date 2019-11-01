// These accessors make hook access slightly more convenient, and most important
// make mocking possible for unit testing.
//
// They must be in a separate file from functions calling them for mock calls to
// work.
#include "rtapi_flavor.h"

// NOTES, IMPORTANT: redo so the checking of function implementation (not NULL pointer)
// happens on initialization, not here

/* ========== START Wrapped MACROs for accessor function implementations ========== */
#define WRAPPER_HELPER(wrapped) wrapped

#define RETURN_HELPER(retval) return retval;

#define EXECUTE_BASED_ON_STATE(checked_state, if_retval_operation, else_retval_operation)    \
    do                                                                                       \
    {                                                                                        \
        hal_u32_t temp_state = rtapi_load_u32(&(global_flavor_access_structure_ptr->state)); \
        if (!(~temp_state & checked_state))                                                  \
        {                                                                                    \
            if_retval_operation                                                              \
        }                                                                                    \
        else                                                                                 \
        {                                                                                    \
            else_retval_operation                                                            \
        }                                                                                    \
    } while (false);

#define EXECUTE_AND_TEST_FOR_STATE_CHANGE_AFTER_EBOS(new_state, int_retval_operation)                                                                                                                                                                               \
    do                                                                                                                                                                                                                                                              \
    {                                                                                                                                                                                                                                                               \
        int retval = int_retval_operation;                                                                                                                                                                                                                          \
        hal_u32_t new_temp_state = rtapi_load_u32(&(global_flavor_access_structure_ptr->state));                                                                                                                                                                    \
        if (!(~new_temp_state & new_state))                                                                                                                                                                                                                         \
        {                                                                                                                                                                                                                                                           \
            return retval;                                                                                                                                                                                                                                          \
        }                                                                                                                                                                                                                                                           \
        else                                                                                                                                                                                                                                                        \
        {                                                                                                                                                                                                                                                           \
            if (!retval)                                                                                                                                                                                                                                            \
            {                                                                                                                                                                                                                                                       \
                rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI: EXECUTE_AND_TEST_FOR_STATE_CHANGE_AFTER_EBOS: Function '%s' didn't change state from '%d' to '%d', but returned with value '%d'. This is an error\n", #int_retval_operation, temp_state, new_state, retval); \
                return -ENOEXEC;                                                                                                                                                                                                                                    \
            }                                                                                                                                                                                                                                                       \
            else                                                                                                                                                                                                                                                    \
            {                                                                                                                                                                                                                                                       \
                rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI: EXECUTE_AND_TEST_FOR_STATE_CHANGE_AFTER_EBOS: Function '%s' didn't change state from '%d' to '%d', returned with value '%d'\n", #int_retval_operation, temp_state, new_state, retval);                       \
                return retval;                                                                                                                                                                                                                                      \
            }                                                                                                                                                                                                                                                       \
        }                                                                                                                                                                                                                                                           \
        return retval;                                                                                                                                                                                                                                              \
    } while (false);

#define SIGNAL_ERROR_AFTER_EBOS_AND_RETURN(failed_function, error_retval, return_operation, needed_state)                                                                                                                                                                 \
    do                                                                                                                                                                                                                                                                    \
    {                                                                                                                                                                                                                                                                     \
        rtapi_print_msg(RTAPI_MSG_WARN, "RTAPI: FLAVOUR MODULE function call '%s' signaled error (%d)->%s when in state %d, because the required state for the function call is %d\n", #failed_function, error_retval, strerror(error_retval), temp_state, needed_state); \
        RETURN_HELPER(return_operation)                                                                                                                                                                                                                                   \
    } while (false);

#define VOID_FUNCTION_RETURN_HELPER(function, return_operation) \
    do                                                          \
    {                                                           \
        function;                                               \
        RETURN_HELPER(return_operation)                         \
    } while (false);
/* ========== END Wrapped MACROs for accessor function implementations ========== */

/* ========== START Public MACROs for accessor function implementations ========== */
#define OPERATION_PERMITED_IN_CURRENT_STATE_ON_INT_FUNCTION(checked_state, function) EXECUTE_BASED_ON_STATE(checked_state, RETURN_HELPER(function), SIGNAL_ERROR_AFTER_EBOS_AND_RETURN(function, EPERM, -EPERM, checked_state))

#define OPERATION_PERMITED_IN_CURRENT_STATE_ON_VOID_FUNCTION(checked_state, function) EXECUTE_BASED_ON_STATE(checked_state, VOID_FUNCTION_RETURN_HELPER(function, WRAPPER_HELPER(0)), SIGNAL_ERROR_AFTER_EBOS_AND_RETURN(function, EPERM, -EPERM, checked_state))

#define OPERATION_PERMITED_IN_CURRENT_STATE_ON_INT_FUNCTION_WITH_STATE_TEST(checked_state, new_state, function) EXECUTE_BASED_ON_STATE(checked_state, EXECUTE_AND_TEST_FOR_STATE_CHANGE_AFTER_EBOS(new_state, function), SIGNAL_ERROR_AFTER_EBOS_AND_RETURN(function, EPERM, -EPERM, checked_state))

#define STRING_GETTER_PERMITTED_IN_CURRENT_STATE(checked_state, function, error_string) EXECUTE_BASED_ON_STATE(checked_state, RETURN_HELPER(function), SIGNAL_ERROR_AFTER_EBOS_AND_RETURN(function, EPERM, error_string, checked_state))
/* ========== END Public MACROs for accessor function implementations ========== */

// Based on (so far non existing) value of state variable allow only some
// subset of functions:
// Allow nothing when FLAVOUR module is not registered
// Allow only arming when FLAVOUR module is registered and not armed
// Allow everything when FLAVOUR module registered and armed

/* ========== START Accessor function implementations between RTAPI and FLAVOUR MODULE========== */
int flavor_exception_handler_hook(int type, rtapi_exception_detail_t *detail, int level)
{
    OPERATION_PERMITED_IN_CURRENT_STATE_ON_INT_FUNCTION(FLAVOR_STATE_ARMED, global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor->exception_handler_hook(type, detail, level))
}

int flavor_module_init_hook(void)
{
    OPERATION_PERMITED_IN_CURRENT_STATE_ON_INT_FUNCTION_WITH_STATE_TEST(FLAVOR_STATE_INSTALLED, WRAPPER_HELPER(temp_state) | FLAVOR_STATE_ARM, global_flavor_access_structure_ptr->flavor_module_hot_metadata_descriptor->module_init_hook())
}

int flavor_module_exit_hook(void)
{
    OPERATION_PERMITED_IN_CURRENT_STATE_ON_INT_FUNCTION_WITH_STATE_TEST(FLAVOR_STATE_ARMED, WRAPPER_HELPER(temp_state) & ~FLAVOR_STATE_ARM, global_flavor_access_structure_ptr->flavor_module_hot_metadata_descriptor->module_exit_hook())
}

int flavor_task_update_stats_hook(void)
{
    OPERATION_PERMITED_IN_CURRENT_STATE_ON_VOID_FUNCTION(FLAVOR_STATE_ARMED, global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor->task_update_stats_hook())
}

int flavor_task_print_thread_stats_hook(int task_id)
{
    OPERATION_PERMITED_IN_CURRENT_STATE_ON_VOID_FUNCTION(FLAVOR_STATE_ARMED, global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor->task_print_thread_stats_hook(task_id))
}

int flavor_task_new_hook(int task_id, task_data *task)
{
    OPERATION_PERMITED_IN_CURRENT_STATE_ON_INT_FUNCTION(FLAVOR_STATE_ARMED, global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor->task_new_hook(task_id, task))
}

int flavor_task_delete_hook(int task_id)
{
    OPERATION_PERMITED_IN_CURRENT_STATE_ON_INT_FUNCTION(FLAVOR_STATE_ARMED, global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor->task_delete_hook(task_id))
}

int flavor_task_start_hook(int task_id)
{
    OPERATION_PERMITED_IN_CURRENT_STATE_ON_INT_FUNCTION(FLAVOR_STATE_ARMED, global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor->task_start_hook(task_id))
}

int flavor_task_stop_hook(int task_id)
{
    OPERATION_PERMITED_IN_CURRENT_STATE_ON_INT_FUNCTION(FLAVOR_STATE_ARMED, global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor->task_stop_hook(task_id))
}

int flavor_task_pause_hook(int task_id)
{
    OPERATION_PERMITED_IN_CURRENT_STATE_ON_INT_FUNCTION(FLAVOR_STATE_ARMED, global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor->task_pause_hook(task_id))
}

int flavor_task_wait_hook(const int flags)
{
    OPERATION_PERMITED_IN_CURRENT_STATE_ON_INT_FUNCTION(FLAVOR_STATE_ARMED, global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor->task_wait_hook(flags))
}

int flavor_task_resume_hook(int task_id)
{
    OPERATION_PERMITED_IN_CURRENT_STATE_ON_INT_FUNCTION(FLAVOR_STATE_ARMED, global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor->task_resume_hook(task_id))
}

int flavor_task_delay_hook(long int nsec)
{
    OPERATION_PERMITED_IN_CURRENT_STATE_ON_VOID_FUNCTION(FLAVOR_STATE_ARMED, global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor->task_wait_hook(nsec))
}

long long int flavor_get_time_hook(void)
{
    OPERATION_PERMITED_IN_CURRENT_STATE_ON_INT_FUNCTION(FLAVOR_STATE_ARMED, global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor->get_time_hook())
}

long long int flavor_get_clocks_hook(void)
{
    OPERATION_PERMITED_IN_CURRENT_STATE_ON_INT_FUNCTION(FLAVOR_STATE_ARMED, global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor->get_clocks_hook())
}

int flavor_task_self_hook(void)
{
    OPERATION_PERMITED_IN_CURRENT_STATE_ON_INT_FUNCTION(FLAVOR_STATE_ARMED, global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor->task_self_hook())
}

long long flavor_task_pll_get_reference_hook(void)
{
    /*OPERATION_PERMITED_IN_CURRENT_STATE_ON_INT_FUNCTION(FLAVOR_STATE_ARMED, global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor->task_wait_hook(flags))
    if (global_flavor_access_structure_ptr->flavor_module_hot_metadata_descriptor && global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor)
    {
        if (global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor->task_pll_get_reference_hook)
        {
            return global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor->task_pll_get_reference_hook();
        }
        return -ENOSYS;
    }
    return -EPERM;*/

    if (global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor->task_pll_get_reference_hook)
        return global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor->task_pll_get_reference_hook();
    else
        return 0;
}

int flavor_task_pll_set_correction_hook(long value)
{
    /*OPERATION_PERMITED_IN_CURRENT_STATE_ON_INT_FUNCTION(FLAVOR_STATE_ARMED, global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor->task_wait_hook(flags))
    if (global_flavor_access_structure_ptr->flavor_module_hot_metadata_descriptor && global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor)
    {
        if (global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor->task_start_hook)
        {
            return global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor->task_pll_set_correction_hook(value);
        }
        return -ENOSYS;
    }
    return -EPERM;*/

    if (global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor->task_pll_set_correction_hook)
        return global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor->task_pll_set_correction_hook(value);
    else
        return 0;
}

const char *const flavor_get_installed_name(void)
{
    STRING_GETTER_PERMITTED_IN_CURRENT_STATE(FLAVOR_STATE_INSTALLED, global_flavor_access_structure_ptr->flavor_module_cold_metadata_descriptor->name, NULL)
}

int flavor_get_installed_id(void)
{
    OPERATION_PERMITED_IN_CURRENT_STATE_ON_INT_FUNCTION(FLAVOR_STATE_INSTALLED, global_flavor_access_structure_ptr->flavor_module_cold_metadata_descriptor->id)
}

int flavor_verify_installed_feature(int feature)
{
    OPERATION_PERMITED_IN_CURRENT_STATE_ON_INT_FUNCTION(FLAVOR_STATE_INSTALLED, WRAPPER_HELPER(global_flavor_access_structure_ptr->flavor_module_cold_metadata_descriptor->flags) & WRAPPER_HELPER(feature))
}

/*const char *flavor_name(void)
{
    // Reiplement in the rtapi_flavor.c
    SET_FLAVOR_DESCRIPTOR_DEFAULT();
    return f->name;
}

int flavor_id(void)
{
    // Reiplement in the rtapi_flavor.c
    SET_FLAVOR_DESCRIPTOR_DEFAULT();
    return f->flavor_id;
}

int flavor_feature(int feature)
{
    // Reiplement in the rtapi_flavor.c
    SET_FLAVOR_DESCRIPTOR_DEFAULT();
    return (f->flags & feature);
}*/

/* ========== START Accessor function implementations between RTAPI and FLAVOUR MODULE========== */

#ifdef RTAPI
EXPORT_SYMBOL(flavor_get_installed_name);
EXPORT_SYMBOL(flavor_get_installed_id);
EXPORT_SYMBOL(flavor_verify_installed_feature);
#endif
