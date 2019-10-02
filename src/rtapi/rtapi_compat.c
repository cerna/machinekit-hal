/********************************************************************
* Copyright (C) 2012 - 2013 John Morris <john AT zultron DOT com>
*                           Michael Haberler <license AT mah DOT priv DOT at>
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
********************************************************************/

// miscellaneous functions, mostly used during startup in
// a user process; neither RTAPI nor ULAPI and universally
// available to user processes

#include "config.h"
#include "rtapi.h"
#include "rtapi_compat.h"
#include "inifile.h" /* iniFind() */

#include <stdio.h>
#include <unistd.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/utsname.h>
#include <limits.h>   /* PATH_MAX */
#include <stdlib.h>   /* exit() */
#include <string.h>   /* exit() */
#include <grp.h>      // getgroups
#include <spawn.h>    // posix_spawn
#include <sys/wait.h> // wait_pid

#include <elf.h> // get_rpath()
#include <link.h>

#define NN_DO(val, action) \
    do                     \
    {                      \
        if (val != NULL)   \
        {                  \
            action;        \
        }                  \
    } while (false);

#if __ELF_NATIVE_CLASS == 32
#define EC ELFCLASS32
#elif __ELF_NATIVE_CLASS == 64
#define EC ELFCLASS64
#endif

static FILE *rtapi_inifile = NULL;

void check_rtapi_config_open()
{
    /* Open rtapi.ini if needed.  Private function used by
       get_rtapi_config(). */
    char config_file[PATH_MAX];

    if (rtapi_inifile == NULL)
    {
        /* it's the first -i (ignore repeats) */
        /* there is a following arg, and it's not an option */
        snprintf(config_file, PATH_MAX,
                 "%s/rtapi.ini", EMC2_SYSTEM_CONFIG_DIR);
        rtapi_inifile = fopen(config_file, "r");
        if (rtapi_inifile == NULL)
        {
            fprintf(stderr,
                    "Could not open ini file '%s'\n",
                    config_file);
            exit(-1);
        }
        /* make sure file is closed on exec() */
        fcntl(fileno(rtapi_inifile), F_SETFD, FD_CLOEXEC);
    }
}

char *get_rtapi_param(const char *param)
{
    char *val;

    // Open rtapi_inifile if it hasn't been already
    check_rtapi_config_open();
    val = (char *)iniFind(rtapi_inifile, param, "global");

    return val;
}

int get_rtapi_config(char *result, const char *param, int n)
{
    /* Read a parameter value from rtapi.ini.  Copy max n-1 bytes into result
       buffer.  */
    char *val;

    val = get_rtapi_param(param);

    // Return if nothing found
    if (val == NULL)
    {
        result[0] = 0;
        return -1;
    }

    // Otherwise copy result into buffer (see 'WTF' comment in inifile.cc)
    strncpy(result, val, n - 1);
    return 0;
}

// whatever is written is printf-style
int rtapi_fs_write(const char *path, const char *format, ...)
{
    va_list args;
    int fd;
    int retval = 0;
    char buffer[4096];

    if ((fd = open(path, O_WRONLY)) > -1)
    {
        int len;
        va_start(args, format);
        len = vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);
        retval = write(fd, buffer, len);
        close(fd);
        return retval;
    }
    else
        return -ENOENT;
}

// filename is printf-style
int rtapi_fs_read(char *buf, const size_t maxlen, const char *name, ...)
{
    char fname[4096];
    va_list args;

    va_start(args, name);
    size_t len = vsnprintf(fname, sizeof(fname), name, args);
    va_end(args);

    if (len < 1)
        return -EINVAL; // name too short

    int fd, rc;
    if ((fd = open(fname, O_RDONLY)) >= 0)
    {
        rc = read(fd, buf, maxlen);
        close(fd);
        if (rc < 0)
            return -errno;
        char *s = strchr(buf, '\n');
        if (s)
            *s = '\0';
        return strlen(buf);
    }
    else
    {
        return -errno;
    }
}

const char *rtapi_get_rpath(void)
{
    const ElfW(Dyn) *dyn = _DYNAMIC;
    const ElfW(Dyn) *rpath = NULL;
    const char *strtab = NULL;
    for (; dyn->d_tag != DT_NULL; ++dyn)
    {
        if (dyn->d_tag == DT_RPATH)
        {
            rpath = dyn;
        }
        else if (dyn->d_tag == DT_STRTAB)
        {
            strtab = (const char *)dyn->d_un.d_val;
        }
    }

    if (strtab != NULL && rpath != NULL)
    {
        return strdup(strtab + rpath->d_un.d_val);
    }
    return NULL;
}

bool scan_file_for_elf_sections(const char *const elf_file_real_path, elf_section_found_callback section_discovered_callback_function, void *cloobj)
{
    int fd = -1;
    struct stat fst = {0};
    void *mapping_address = NULL;
    bool retval_section_found = false;
    int retval;

    fd = open(elf_file_real_path, O_RDONLY | O_NOFOLLOW | O_NONBLOCK);
    if (fd < 0)
    {
        rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI: ELF section scrawler: There was an error when trying to open file '%s': (%d)->%s\n", elf_file_real_path, fd, strerror(fd));
        goto end;
    }
    // It's economic to do direct read from tested file here and determine if the file has at least
    // ELF file magic bytes than do it after mmaping into memory
    // Especially if we want to run this function on big set of arbitrary files
    char tested_file_header[4];
    if (read(fd, (void *)&tested_file_header, 4 * sizeof(char) != 4 * sizeof(char)))
    {
        int error = errno;
        rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI: ELF section scrawler: There was an error when trying to read 4 chars from open file '%s': (%d)->%s\n", elf_file_real_path, error, strerror(error));
        goto close_fd;
    }
    if (tested_file_header[EI_MAG0] != ELFMAG0 || tested_file_header[EI_MAG1] != ELFMAG1 ||
        tested_file_header[EI_MAG2] != ELFMAG2 || tested_file_header[EI_MAG3] != ELFMAG3)
    {
        // Not an ELF file
        goto close_fd;
    }
    retval = fstat(fd, &fst);
    if (retval)
    {
        int error = errno;
        rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI: ELF section scrawler: There was an error when trying to fstat file '%s': (%d)->%s\n", elf_file_real_path, error, strerror(error));
        goto close_fd;
    }
    mapping_address = mmap(NULL, fst.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (mapping_address == MAP_FAILED)
    {
        int error = errno;
        rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI: ELF section scrawler: There was an error when trying to mmap file '%s' into memory: (%d)->%s\n", elf_file_real_path, error, strerror(error));
        goto close_fd;
    }

    char *elf_file = (char *)mapping_address;

    // The idea is: We are looking for libraries compiled for and runnable on this system,
    // so we are filtering on the CLASS type
    if (elf_file[EI_CLASS] == EC)
    {
        ElfW(Ehdr) *elf_header = (ElfW(Ehdr) *)elf_file;
        if (elf_header->e_version != EV_CURRENT)
        {
            // Unknown EVL version
            rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI: ELF section scrawler: The ELF file '%s' has an unknown version of: (%d)\n", elf_file_real_path, elf_header->e_version);
            goto close_mmap;
        }
        if (elf_header->e_type != ET_DYN)
        {
            // The file is not shared library or PIE executable
            goto close_mmap;
        }
        if (elf_header->e_shoff == 0)
        {
            // No section header
            goto close_mmap;
        }
        ElfW(Shdr) *section_header_table = (ElfW(Shdr) *)(elf_file + elf_header->e_shoff);
        ElfW(Shdr) *section_name_string_table = &section_header_table[elf_header->e_shstrndx];
        size_t section_size;
        // Section number could either be really 0 or just too big number
        if (elf_header->e_shnum == 0)
        {
            // Here should be the actual number of sections if the number is too big for
            // e_shnum
            if (section_header_table[0].sh_size == 0)
            {
                goto close_mmap;
            }
            section_size = section_header_table[0].sh_size;
        }
        else
        {
            section_size = elf_header->e_shnum;
        }
        if (section_name_string_table == SHN_UNDEF)
        {
            // Section name string table is undefined, nothing to do
            goto close_mmap;
        }
        char *string_table_address = (char *)(elf_file + section_name_string_table->sh_offset);
        bool continuing = true;
        // Fist section should be unused
        for (int i = 1; (i < section_size) && continuing; i++)
        {
            NN_DO(section_discovered_callback_function, retval_section_found = section_discovered_callback_function(elf_file_real_path, (const char *const)(string_table_address + section_header_table[i].sh_name), (const size_t)section_header_table[i].sh_size, (const char *const)(elf_file + section_header_table[i].sh_offset), &continuing, cloobj));
        }
    }

close_mmap:
    retval = munmap(mapping_address, fst.st_size);
    if (retval)
    {
        int error = errno;
        rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI: ELF section scrawler: The ELF file '%s' signaled error while munmapping: (%d)\n", error, strerror(error));
    }
close_fd:
    retval = close(fd);
    if (retval)
    {
        int error = errno;
        rtapi_print_msg(RTAPI_MSG_ERR, "RTAPI: ELF section scrawler: The ELF file '%s' signaled error while closing the FD: (%d)\n", error, strerror(error));
    }
end:
    return retval_section_found;
}

/**** get_elf_section ****/
static struct get_elf_section_cloobj
{
    size_t size;
    void *data;
    const char *secname;
};
static bool find_section_name_and_allocate(const char *const elf_file_path, const char *const section_name, const size_t size_of_data, const void *const section_data, bool *continuing, void *cloobj)
{
    struct get_elf_section_cloobj *co = (struct get_elf_section_cloobj *)cloobj;
    if (strcmp(co->secname, section_name) == 0)
    {
        *continuing = false;
        co->size = size_of_data;
        co->data = malloc(size_of_data * sizeof(char));
        if (co->data != NULL)
        {
            memcpy(co->data, section_data, size_of_data);
            return true;
        }
    }
    return false;
}
int get_elf_section(const char *const fname, const char *section_name, void **dest)
{
    char file_real_path[PATH_MAX] = {0};
    struct get_elf_section_cloobj co = {.size = 0, .data = NULL, .secname = section_name};
    int retval = -1;

    if (realpath(fname, file_real_path) == NULL)
    {
        int error = errno;
        perror("rtapi_compat.c: could not resolve realpath of file %s. Error: (%d)->%s", fname, error, strerror(error));
        goto end;
    }
    if (scan_file_for_elf_sections((const char *const)file_real_path, find_section_name_and_allocate, &co))
    {
        *dest = co.data;
        retval = co.size;
    }

end:
    return retval;
}
/**** END get_elf_section function ****/

const char **get_caps(const char *const fname)
{
    void *dest;
    int n = 0;
    char *s;

    int csize = get_elf_section(fname, RTAPI_TAGS, &dest);
    if (csize < 0)
        return 0;

    for (s = dest; s < ((char *)dest + csize); s += strlen(s) + 1)
        n++;

    const char **rv = malloc(sizeof(char *) * (n + 1));
    if (rv == NULL)
    {
        perror("rtapi_compat.c:  get_caps() malloc");
        return NULL;
    }
    n = 0;
    for (s = dest;
         s < ((char *)dest + csize);
         s += strlen(s) + 1)
        rv[n++] = s;

    rv[n] = NULL;
    return rv;
}

const char *get_cap(const char *const fname, const char *cap)
{
    if ((cap == NULL) || (fname == NULL))
        return NULL;

    const char **cv = get_caps(fname);
    if (cv == NULL)
        return NULL;

    const char **p = cv;
    size_t len = strlen(cap);

    while (p && *p && strlen(*p))
    {
        if (strncasecmp(*p, cap, len) == 0)
        {
            const char *result = strdup(*p + len + 1); // skip over '='
            free(cv);
            return result;
        }
    }
    free(cv);
    return NULL;
}

int rtapi_get_tags(const char *mod_name)
{
    char modpath[PATH_MAX];
    int result = 0, n = 0;
    char *cp1 = "";

    if (get_rtapi_config(modpath, "RTLIB_DIR", PATH_MAX) != 0)
    {
        perror("rtapi_compat.c:  Can't get  RTLIB_DIR");
        return -1;
    }
    strcat(modpath, "/modules/");
    strcat(modpath, mod_name);
    strcat(modpath, ".so");

    const char **caps = get_caps(modpath);
    char **p = (char **)caps;
    while (p && *p && strlen(*p))
    {
        cp1 = *p++;
        if (strncmp(cp1, "HAL=", 4) == 0)
        {
            n = strtol(&cp1[4], NULL, 10);
            result |= n;
        }
    }
    free(caps);
    return result;
}

// those are ok to use from userland RT modules:
#if defined(RTAPI)
EXPORT_SYMBOL(rtapi_fs_read);
EXPORT_SYMBOL(rtapi_fs_write);
#endif