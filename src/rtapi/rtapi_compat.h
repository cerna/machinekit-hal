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

#ifndef RTAPI_COMPAT_H
#define RTAPI_COMPAT_H

#include "rtapi_bitops.h"
#include <limits.h> // provides PATH_MAX
#include <stdbool.h>

// these functions must work with or without rtapi.h included
#if !defined(SUPPORT_BEGIN_DECLS)
#if defined(__cplusplus)
#define SUPPORT_BEGIN_DECLS extern "C" {
#define SUPPORT_END_DECLS }
#else
#define SUPPORT_BEGIN_DECLS
#define SUPPORT_END_DECLS
#endif
#endif

SUPPORT_BEGIN_DECLS

/* Callback function called on each discovered ELF section in file
 * Function has to determine by way of return value the usefulnes of operation
 * Parameters: elf_file_path: File path of pertinent ELF file
 *             section_name:  Name of the ELF section for which this function is called
 *             size_of_data:  Size in chars of section payload
 *             section_data:  Actual section payload
 *             continuing:    Bool variable which stops traversing over more sections
 *             cloobj:        User passed opaque object used for simulating closures
*/
typedef bool (*elf_section_found_callback)(const char *const elf_file_path, const char *const section_name, const size_t size_of_data, const void *const section_data, bool *continuing, void *cloobj);

extern long int simple_strtol(const char *nptr, char **endptr, int base);

// whatever is written is printf-style
int rtapi_fs_write(const char *path, const char *format, ...);

// read a string from a sysfs entry.
// strip trailing newline.
// returns length of string read (>= 0)
// or <0: -errno from open or read.
// filename is printf-style
int rtapi_fs_read(char *buf, const size_t maxlen, const char *name, ...);


/*
 * Look up a parameter value in rtapi.ini [global] section.  Returns 0 if
 * successful, 1 otherwise.  Maximum n-1 bytes of the value and a
 * trailing \0 is copied into *result.
 *
 * Beware:  this function calls exit(-1) if rtapi.ini cannot be
 * successfully opened!
 */

extern int get_rtapi_config(char *result, const char *param, int n);

// diagnostics: retrieve the rpath this binary was linked with
//
// returns malloc'd memory - caller MUST free returned string if non-null
// example:  cc -g -Wall -Wl,-rpath,/usr/local/lib -Wl,-rpath,/usr/lib foo.c -o foo
// rtapi_get_rpath() will return "/usr/local/lib:/usr/lib"

extern const char *rtapi_get_rpath(void);

// inspection of Elf objects (.so, .ko):
// retrieve raw data of Elf section section_name.
// returned in *dest on success.
// caller must free().
// returns size, or < 0 on failure.
int get_elf_section(const char *const fname, const char *section_name, void **dest);

// split the null-delimited strings in an .rtapi_caps Elf section into an argv.
// caller must free.
const char **get_caps(const char *const fname);

// given a path to an elf binary, and a capability name, return its value
// or NULL if not present.
// caller must free().
const char *get_cap(const char *const fname, const char *cap);


// given a module name, return the integer capability mask of tags.
int rtapi_get_tags(const char *mod_name);

/* Function used for extracting an ELF sections from file
 * When new section is found, the section_discovered_callback_function is called with
 * arguments of pointers to section name, size and data (directly mapped into memory
 * and valid during call), pointer to bool continuing variable which control traversing of
 * ELF sections and opaque cloobj for user use
 * 
 * Return value is set directly by section_discovered_callback_function (last call the most important)
 * or in case file does not have ELF sections or an error occured, false is returned
 * 
 * Reason for implementing callback for each found ELF section is that somedoby may want
 * to extract two or more sections from one ELF file. This way it should be possible to
 * do so without opening the same file twice
 * 
 * Parameters: elf_file_real_path:                    File path of pertinent ELF file
 *             section_discovered_callback_function:  Function callback which is called on each discovered ELF section
 *             cloobj:                                User passed opaque object used for simulating closures
*/ 
bool scan_file_for_elf_sections(const char *const elf_file_real_path, elf_section_found_callback section_discovered_callback_function, void *cloobj);


SUPPORT_END_DECLS

#endif // RTAPI_COMPAT_H
