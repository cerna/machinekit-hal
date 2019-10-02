// These accessors make hook access slightly more convenient, and most important
// make mocking possible for unit testing.
//
// They must be in a separate file from functions calling them for mock calls to
// work.
#include "rtapi_flavor.h"

// NOTES, IMPORTANT: redo so the checking of function implementation (not NULL pointer)
// happens on initialization, not here

#define OPERATION_PERMITTED_IN_CURRENT_STATE()
// Based on (so far non existing) value of state variable allow only some
// subset of functions:
// Allow nothing when FLAVOUR module is not registered
// Allow only arming when FLAVOUR module is registered and not armed
// Allow everything when FLAVOUR module registered and armed

int flavor_exception_handler_hook(int type, rtapi_exception_detail_t *detail, int level)
{
    if (global_flavor_access_structure_ptr->flavor_module_hot_metadata_descriptor && global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor)
    {
        if (global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor->exception_handler_hook)
        {
            return global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor->exception_handler_hook(type, detail, level);
        }
        return -ENOSYS;
    }
    return -EPERM;
    /*
    SET_FLAVOR_DESCRIPTOR_DEFAULT();
    if (f->exception_handler_hook)
    {
        f->exception_handler_hook(type, detail, level);
        return 0;
    }
    else
        return -ENOSYS; // Unimplemented*/
}
int flavor_module_init_hook(void)
{
    if (global_flavor_access_structure_ptr->flavor_module_hot_metadata_descriptor)
    {
        if (global_flavor_access_structure_ptr->flavor_module_hot_metadata_descriptor->module_init_hook)
        {
            return global_flavor_access_structure_ptr->flavor_module_hot_metadata_descriptor->module_init_hook();
        }
        return -ENOSYS;
    }
    return -EPERM;
    /*SET_FLAVOR_DESCRIPTOR_DEFAULT();
    if (f->module_init_hook)
        return f->module_init_hook();
    else
        return 0;*/
}
int flavor_module_exit_hook(void)
{
    if (global_flavor_access_structure_ptr->flavor_module_hot_metadata_descriptor && global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor)
    {
        if (global_flavor_access_structure_ptr->flavor_module_hot_metadata_descriptor->module_exit_hook)
        {
            return global_flavor_access_structure_ptr->flavor_module_hot_metadata_descriptor->module_exit_hook();
        }
        return -ENOSYS;
    }
    return -EPERM;
    /*SET_FLAVOR_DESCRIPTOR_DEFAULT();
    if (f->module_exit_hook)
        f->module_exit_hook();*/
}
int flavor_task_update_stats_hook(void)
{
    if (global_flavor_access_structure_ptr->flavor_module_hot_metadata_descriptor && global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor)
    {
        if (global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor->task_update_stats_hook)
        {
            return global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor->task_update_stats_hook();
        }
        return -ENOSYS;
    }
    return -EPERM;
    /*SET_FLAVOR_DESCRIPTOR_DEFAULT();
    if (f->task_update_stats_hook)
        return f->task_update_stats_hook();
    else
        return 0;*/
}
void flavor_task_print_thread_stats_hook(int task_id)
{
    if (global_flavor_access_structure_ptr->flavor_module_hot_metadata_descriptor && global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor)
    {
        if (global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor->task_print_thread_stats_hook)
        {
            global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor->task_print_thread_stats_hook(task_id);
        }
    }
    /*SET_FLAVOR_DESCRIPTOR_DEFAULT();
    if (f->task_print_thread_stats_hook)
        f->task_print_thread_stats_hook(task_id);*/
}
int flavor_task_new_hook(int task_id, task_data *task)
{
    if (global_flavor_access_structure_ptr->flavor_module_hot_metadata_descriptor && global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor)
    {
        if (global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor->task_new_hook)
        {
            return global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor->task_new_hook(task_id, task);
        }
        return -ENOSYS;
    }
    return -EPERM;
    /*SET_FLAVOR_DESCRIPTOR_DEFAULT();
    if (flavor_descriptor->task_new_hook)
        return f->task_new_hook(task, task_id);
    else
        return -ENOSYS; // Unimplemented*/
}
int flavor_task_delete_hook(int task_id)
{
    if (global_flavor_access_structure_ptr->flavor_module_hot_metadata_descriptor && global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor)
    {
        if (global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor->task_delete_hook)
        {
            return global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor->task_delete_hook(task_id);
        }
        return -ENOSYS;
    }
    return -EPERM;
    /*SET_FLAVOR_DESCRIPTOR_DEFAULT();
    if (f->task_delete_hook)
        return f->task_delete_hook(task, task_id);
    else
        return 0;*/
}
int flavor_task_start_hook(int task_id)
{
    if (global_flavor_access_structure_ptr->flavor_module_hot_metadata_descriptor && global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor)
    {
        if (global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor->task_start_hook)
        {
            return global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor->task_start_hook(task_id);
        }
        return -ENOSYS;
    }
    return -EPERM;
    /*SET_FLAVOR_DESCRIPTOR_DEFAULT();
    return f->task_start_hook(task, task_id);*/
}
int flavor_task_stop_hook(int task_id)
{
    if (global_flavor_access_structure_ptr->flavor_module_hot_metadata_descriptor && global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor)
    {
        if (global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor->task_stop_hook)
        {
            return global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor->task_stop_hook(task_id);
        }
        return -ENOSYS;
    }
    return -EPERM;
    /*SET_FLAVOR_DESCRIPTOR_DEFAULT();
    return f->task_stop_hook(task, task_id);*/
}
int flavor_task_pause_hook(int task_id)
{
    if (global_flavor_access_structure_ptr->flavor_module_hot_metadata_descriptor && global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor)
    {
        if (global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor->task_pause_hook)
        {
            return global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor->task_pause_hook(task_id);
        }
        return -ENOSYS;
    }
    return -EPERM;
    /*SET_FLAVOR_DESCRIPTOR_DEFAULT();
    if (f->task_pause_hook)
        return f->task_pause_hook(task, task_id);
    else
        return 0;*/
}
int flavor_task_wait_hook(const int flags)
{
    if (global_flavor_access_structure_ptr->flavor_module_hot_metadata_descriptor && global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor)
    {
        if (global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor->task_wait_hook)
        {
            return global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor->task_wait_hook(flags);
        }
        return -ENOSYS;
    }
    return -EPERM;
    /*
    SET_FLAVOR_DESCRIPTOR_DEFAULT();
    if (f->task_wait_hook)
        return f->task_wait_hook(flags);
    else
        return 0;*/
}
int flavor_task_resume_hook(int task_id)
{
    if (global_flavor_access_structure_ptr->flavor_module_hot_metadata_descriptor && global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor)
    {
        if (global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor->task_resume_hook)
        {
            return global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor->task_resume_hook(task_id);
        }
        return -ENOSYS;
    }
    return -EPERM;
    /*SET_FLAVOR_DESCRIPTOR_DEFAULT();
    if (f->task_resume_hook)
        return f->task_resume_hook(task, task_id);
    else
        return -ENOSYS; // Unimplemented*/
}
void flavor_task_delay_hook(long int nsec)
{
    if (global_flavor_access_structure_ptr->flavor_module_hot_metadata_descriptor && global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor)
    {
        if (global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor->task_wait_hook)
        {
            return global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor->task_wait_hook(nsec);
        }
        return -ENOSYS;
    }
    return -EPERM;
    /*SET_FLAVOR_DESCRIPTOR_DEFAULT();
    if (f->task_delay_hook)
        f->task_delay_hook(nsec);*/
}
long long int flavor_get_time_hook(void)
{
    if (global_flavor_access_structure_ptr->flavor_module_hot_metadata_descriptor && global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor)
    {
        if (global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor->get_time_hook)
        {
            return global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor->get_time_hook();
        }
        return -ENOSYS;
    }
    return -EPERM;
    /*
    SET_FLAVOR_DESCRIPTOR_DEFAULT();
    if (f->get_time_hook)
        return f->get_time_hook();
    else
        return -ENOSYS; // Unimplemented*/
}
long long int flavor_get_clocks_hook(void)
{
    if (global_flavor_access_structure_ptr->flavor_module_hot_metadata_descriptor && global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor)
    {
        if (global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor->get_clocks_hook)
        {
            return global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor->get_clocks_hook();
        }
        return -ENOSYS;
    }
    return -EPERM;
    /*
    SET_FLAVOR_DESCRIPTOR_DEFAULT();
    if (f->get_clocks_hook)
        return f->get_clocks_hook();
    else
        return -ENOSYS; // Unimplemented*/
}
int flavor_task_self_hook(void)
{
    if (global_flavor_access_structure_ptr->flavor_module_hot_metadata_descriptor && global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor)
    {
        if (global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor->task_self_hook)
        {
            return global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor->task_self_hook();
        }
        return -ENOSYS;
    }
    return -EPERM;
    /*
    SET_FLAVOR_DESCRIPTOR_DEFAULT();
    if (flavor_descriptor->task_self_hook)
        return f->task_self_hook();
    else
        return -ENOSYS;*/
}
long long flavor_task_pll_get_reference_hook(void)
{
    if (global_flavor_access_structure_ptr->flavor_module_hot_metadata_descriptor && global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor)
    {
        if (global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor->task_pll_get_reference_hook)
        {
            return global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor->task_pll_get_reference_hook();
        }
        return -ENOSYS;
    }
    return -EPERM;
    /*
    SET_FLAVOR_DESCRIPTOR_DEFAULT();
    if (flavor_descriptor->task_pll_get_reference_hook)
        return f->task_pll_get_reference_hook();
    else
        return 0;*/
}
int flavor_task_pll_set_correction_hook(long value)
{
    if (global_flavor_access_structure_ptr->flavor_module_hot_metadata_descriptor && global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor)
    {
        if (global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor->task_start_hook)
        {
            return global_flavor_access_structure_ptr->flavor_module_business_logic_descriptor->task_pll_set_correction_hook(value);
        }
        return -ENOSYS;
    }
    return -EPERM;
    /*
    SET_FLAVOR_DESCRIPTOR_DEFAULT();
    if (flavor_descriptor->task_pll_set_correction_hook)
        return f->task_pll_set_correction_hook(value);
    else
        return 0;*/
}

const char *get_installed_flavor_name(void) { return "PERFECT"; }

unsigned int get_installed_flavor_id(void) { return 1000; }

int verify_installed_flavor_feature(int feature) { return 0; }

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

/*#ifdef RTAPI
EXPORT_SYMBOL(flavor_name);
EXPORT_SYMBOL(flavor_feature);
#endif*/
