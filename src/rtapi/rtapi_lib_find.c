/********************************************************************
* Description:  rtapi_lib_find.c
*
*               This file, 'rtapi_lib_find.c', implements simple API for programs
                needing a way how to find specifically tagged shared object (shared
                libraries) and parse out pertinent information.
*
* Copyright (C) 2019       Jakub Fišer <jakub DOT fiser AT erythio DOT net
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public
* License as published by the Free Software Foundation; either
* version 2.1 of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public
* License along with this library; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*
********************************************************************/

#include <fcntl.h>
#include <stdbool.h>
#include <dirent.h>
#include <unistd.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#include <sys/stat.h>
#include <sys/types.h>

#include "rtapi.h"

#include "rtapi_lib_find.h"
#include "rtapi_compat.h"

#define NN_DO(val, action) \
    do                     \
    {                      \
        if (val != NULL)   \
        {                  \
            action;        \
        }                  \
    } while (false);

#define NN_DO_COND(val, action, cond) \
    do                                \
    {                                 \
        NN_DO(val, if (cond) {        \
            action;                   \
        })                            \
    } while (false);

bool is_machinekit_flavor_solib_v1(const char *const real_path, size_t size_of_input, void *input, flavor_module_v1_found_callback flavor_find, void *cloobj)
{
    // In this function, we are assuming and hoping that libELF will translate the payload
    int retval = 0;
    unsigned int api;
    unsigned int weight;
    unsigned int id;

    if (size_of_input < sizeof(unsigned int) * 3 + 1)
    {
        // We want payload which has one unsigned integer for API version number,
        // one unsigned integer for flavour weight and at least empty null
        // terminated string for flavour name
        rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI lib find: Found possible library with wrong byte payload on path %s", real_path);
        return false;
    }
    api = *((unsigned int *)input);
    input = input + sizeof(unsigned int);
    if (api != 1)
    {
        rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI lib find: Found possible library with wrong API version %d on path %s", api, real_path);
        return false;
    }
    weight = *((unsigned int *)input);
    input = input + sizeof(unsigned int);
    id = *((unsigned int *)input);
    input = input + sizeof(unsigned int);
    char name[(size_of_input - 3 * sizeof(unsigned int))\sizeof(char)];
    retval = snprintf(name, size_of_input - 3 * sizeof(unsigned int), "%s", (char *)input);
    if (retval < 1)
    {
        rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI lib find: Found possible library with wrong name %s on path %s", name, real_path);
        return false;
    }
    rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI lib find: Found real library on path %s", real_path);
    NN_DO(flavor_find, return flavor_find(real_path, name, id, weight, cloobj))
    return false;
}

/**** test_file_for_module_data function ****/
struct test_file_for_module_data_cloobj
{
    const char *module_section_name;
    const lib_callback function_lib_callback;
    void *saved_cloobj;
};
static test_file_section_discovered(const char *const elf_file_path, const char *const section_name, const size_t size_of_data, const void *const section_data, bool *continuing, void *cloobj)
{
    if (strcmp(section_name, ((struct test_file_for_module_data_cloobj *)cloobj)->module_section_name) == 0)
    {
        *continuing = false;
        NN_DO(((struct test_file_for_module_data_cloobj *)cloobj)->function_lib_callback, return ((struct test_file_for_module_data_cloobj *)cloobj)->function_lib_callback(elf_file_path, size_of_data, section_data, ((struct test_file_for_module_data_cloobj *)cloobj)->saved_cloobj));
    }
}
bool test_file_for_module_data(const char *const real_path, const char *module_section, lib_callback function_callback, void *cloobj)
{
    struct test_file_for_module_data_cloobj ca = {.module_section_name = module_section, .function_lib_callback = function_callback, .saved_cloobj = cloobj};
    return scan_file_for_elf_sections(real_path, test_file_section_discovered, &ca);
}
/**** END test_file_for_module_data function *****/

int for_each_node(const char *const real_path, dir_found_callback directory_find, file_found_callback file_find, void *cloobj)
{
    DIR *folder = NULL;
    struct dirent *entry = NULL;
    struct stat *entry_stat = NULL;
    char folder_node_path[PATH_MAX] = {0};
    char real_node_path[PATH_MAX] = {0};
    int count = 0;

    folder = opendir(real_path);
    if (folder == NULL)
    {
        int error = errno;
        rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI lib find: There was an error opening folder %s (%d)->%s", real_path, error, strerror(error));
        goto end;
    }
    while ((entry = readdir(folder)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            // Both . and .. are folders and realpath will screate from them absolute paths
            // on which the recursive part of this function will cycle in
            continue;
        }
        sprintf(folder_node_path, "%s/%s", real_path, entry->d_name);
        if (realpath((const char *)&folder_node_path, real_node_path) != real_node_path)
        {
            int error = errno;
            rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI lib find: There was an error getting information about real path of node (%d)->%s, %s", error, strerror(error), folder_node_path);
            continue;
        }
        switch (entry->d_type)
        {
        case DT_DIR:
            NN_DO(directory_find, count += directory_find((const char *)&real_node_path, cloobj))
            break;
        case DT_LNK:
        case DT_REG:
            NN_DO_COND(file_find, count++, file_find((const char *)&real_node_path, cloobj))
            break;
        // We are out of luck and have to determine type of file the more expesive way
        case DT_UNKNOWN:
            if (fstatat(dirfd(folder), entry->d_name, entry_stat, AT_SYMLINK_NOFOLLOW) != 0)
            {
                int error = errno;
                rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI lib find: There was an error getting information about node (%d)->%s", error, strerror(error));
                continue;
            }
            if (S_ISDIR(entry_stat->st_mode))
            {
                NN_DO(directory_find, count += directory_find((const char *)&real_node_path, cloobj))
            }
            else if (S_ISREG(entry_stat->st_mode) || S_ISLNK(entry_stat->st_mode))
            {
                NN_DO_COND(file_find, count++, file_find((const char *)&real_node_path, cloobj))
            }
            break;
        }
    }

    if (closedir(folder) < 0)
    {
        int error = errno;
        rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI lib find: There was an error closing folder %s: (%d)->%s", real_path, error, strerror(error));
    }
end:
    return count;
}

int get_paths_of_library_module(const char *settings_parameter, dir_found_callback directory_path_callback_function, void *cloobj)
{
    // TODO: Rework it so the string in .ini can be ';' delimited and actually call the directory_path_callback_function
    // for each path
    int retval = -1;
    int count = 0;
    char path[PATH_MAX] = {0};

    retval = get_rtapi_config(path, settings_parameter, PATH_MAX);
    if (retval)
    {
        // Error occured
        goto end;
    }
    NN_DO(directory_path_callback_function, count += directory_path_callback_function(path, cloobj))
end:
    return count;
}