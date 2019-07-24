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
#include <gelf.h>
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

bool is_machinekit_flavor_solib_v1(const char *real_path, size_t size_of_input, void *input, flavor_module_v1_found_callback flavor_find)
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
    NN_DO(flavor_find, return flavor_find(real_path, name, id, weight))
    return false;
}

bool test_file_for_module_data(const char *real_path, const char *module_section, lib_callback function_callback)
{
    int fd = -1;
    bool retval = false;
    char *section_name = NULL;
    size_t shared_string_index = 0;
    Elf *e = NULL;
    GElf_Ehdr elf_header = {0};
    GElf_Shdr section_header = {0};
    Elf_Scn *section_descriptor = NULL;
    Elf_Data *payload_data = NULL;
    size_t payload_transferred_size = 0;

    if (elf_version(EV_CURRENT) == EV_NONE)
    {
        rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI lib find: LibELF failed to initialize");
        goto end;
    }

    fd = open(real_path, O_RDONLY | O_NOFOLLOW | O_NONBLOCK);
    if (fd < 0)
    {
        //Error when opening occured, we do not care why, just return false
        goto end;
    }

    e = elf_begin(fd, ELF_C_READ, NULL);
    if (e == NULL)
    {
        goto close_fd;
    }

    if (elf_kind(e) != ELF_K_ELF)
    {
        // Not an ELF file, we do not care about this file
        goto elf_end;
    }

    if (gelf_getehdr(e, &elf_header) == NULL)
    {
        goto elf_end;
    }
    // ET_DYN means shared object, which is not exactly .so shared library, but to determine
    // shared library would be more expensive than read all sections for Machinekit related
    // informations
    //
    // Basically, to determine if file is shared library or PIE executable, one needs to look
    // for PT_INTERP in program headers, which is interpreter present in PIE executables
    if (elf_header.e_type != ET_DYN)
    {
        goto elf_end;
    }

    if (elf_getshdrstrndx(e, &shared_string_index) != 0)
    {
        goto elf_end;
    }

    while ((section_descriptor = elf_nextscn(e, section_descriptor)) != NULL)
    {
        if (gelf_getshdr(section_descriptor, &section_header) != &section_header)
        {
            continue;
        }

        section_name = elf_strptr(e, shared_string_index, section_header.sh_name);
        if (section_name == NULL)
        {
            continue;
        }

        if (strcmp(section_name, module_section) == 0)
        {
            // We have just found ELF object which contains the specified section, the whole
            // purpose of this function
            char payload[section_header.sh_size];

            while (payload_transferred_size < section_header.sh_size &&
                   (payload_data = elf_getdata(section_descriptor, payload_data)) != NULL)
            {
                memcpy(&payload + payload_transferred_size, payload_data->d_buf, payload_data->d_size);
                payload_transferred_size += payload_data->d_size;
            }
            rtapi_print_msg(RTAPI_MSG_DBG, "RTAPI lib find: Found possible library with ELF section %s on path %s", module_section, real_path);
            NN_DO(function_callback, retval = function_callback(real_path, payload_transferred_size, (void *)&payload))
            break;
        }
    }

elf_end:
    elf_end(e);
close_fd:
    close(fd);
end:
    return retval;
}

int for_each_node(const char *real_path, dir_found_callback directory_find, file_found_callback file_find)
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
            NN_DO(directory_find, count += directory_find((const char *)&real_node_path))
            break;
        case DT_LNK:
        case DT_REG:
            NN_DO_COND(file_find, count++, file_find((const char *)&real_node_path))
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
                NN_DO(directory_find, count += directory_find((const char *)&real_node_path))
            }
            else if (S_ISREG(entry_stat->st_mode) || S_ISLNK(entry_stat->st_mode))
            {
                NN_DO_COND(file_find, count++, file_find((const char *)&real_node_path))
            }
            break;
        }
    }

    closedir(folder);
end:
    return count;
}