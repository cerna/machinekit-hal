#ifndef RTAPI_LIB_FIND_H
#define RTAPI_LIB_FIND_H

#include <stdbool.h>
#include <stddef.h>

/*
 * Function callback declaration for when the ELF file matching the criteria is found
*/
typedef bool (*lib_callback)(const char *real_path, size_t payload_size, void *payload);

/*
 * Function callback declaration for when new directory is found
*/
typedef int (*dir_found_callback)(const char *real_path);

/*
 * Function callback declaration for when new file is found
*/
typedef bool (*file_found_callback)(const char *real_path);

/*
 * Function callback declaration for when new flavour module of version 1 is found
*/
typedef bool (*flavor_module_v1_found_callback)(const char *real_path, char *name, unsigned int weight);

/*
 * Function testing for file specified as a real_path if is ELF file of ET_DYN and contains
 * the section specified as a module_section
 * If so, then function_callback is called and return value is set to true, otherwise set to
 * false
*/
bool test_file_for_module_data(const char *real_path, const char *module_section, lib_callback function_callback);

/*
 * Function which will find all folders and files (regular and symlinks) in given real_path
 * and call callback functions on them
 * Symlinks are resolved to real paths
 * The return value represents number of found files
*/

int for_each_node(const char *real_path, dir_found_callback directory_find, file_found_callback file_find);

/*
 * Function for parsing important values from *void blob to identify flavour module
 * The return value represents if the found library is flavour module version 1
 * ATTENTION: In case the flavor_find is called, passed pointer to *name points
 * to malloced memory and the callee is responsible for freeing
*/
bool is_machinekit_flavor_solib_v1(const char *real_path, size_t size_of_input, void *input, flavor_module_v1_found_callback flavor_find);

#endif /* RTAPI_LIB_FIND_H */