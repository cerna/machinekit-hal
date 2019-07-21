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
        //rtapi_print_msg(RTAPI_MSG_ERR,"RTAPI: libELF failed to initialize");
        goto end;
    }

    fd = open(real_path, O_RDONLY);
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
        //rtapi_print_msg(RTAPI_MSG_DBG,"%l->%s"error,strerror(error));
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
            //rtapi_print_msg(RTAPI_MSG_DBG,"%l->%s"error,strerror(error));
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
                //rtapi_print_msg(RTAPI_MSG_DBG,"%l->%s"error,strerror(error));
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