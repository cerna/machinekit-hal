#ifndef RTAPI_LIB_FIND_H
#define RTAPI_LIB_FIND_H

#include <stdbool.h>
#include <stddef.h>

/*
 * Function callback declaration for when the ELF file matching the criteria is found
*/
typedef bool (*lib_callback)(const char *real_path, size_t payload_size, void* payload);

/*
 * Function callback declaration for when new directory is found
*/
typedef int (*dir_found_callback)(const char *real_path);

/*
 * Function callback declaration for when new file is found
*/
typedef bool (*file_found_callback)(const char *real_path);
/*
 * Function testing for file specified as a real_path if is ELF file of ET_DYN and contains
 * the section specified as a module_section
 * If so, then function_callback is called and return value is set to true, otherwise set to
 * false
*/
bool test_file_for_module_data(const char *real_path, const char *module_section, lib_callback function_callback);

/*
 * Function
 * The return value represents number of found files
*/

int for_each_node(const char *real_path, dir_found_callback directory_find, file_found_callback file_find);

#endif /* RTAPI_LIB_FIND_H */