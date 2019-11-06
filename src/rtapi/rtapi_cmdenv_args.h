/********************************************************************
* Description:  rtapi_cmdenv_args.h
*               This file, 'rtapi_cmdenv_args.h', defines functions
*               used for exporting and manipulation of command line arguments
*               passed to each application as a 'int argc, char** argv'
*               and operations on environment variables
*
* Copyright (C) 2019        Jakub Fišer <jakub DOT fiser AT erythio DOT net>
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

#ifndef RTAPI_CMDENV_ARGS_H
#define RTAPI_CMDENV_ARGS_H

#ifdef __cplusplus
extern "C"
{
#endif

    /* Function callback declaration for operating on commandline arguments
    */
    typedef int (*cmdline_data_callback)(int *argc, char **argv, void *cloobj);

    /* Changing of the process name to new values specified as a new_name
    * This function has to be called from the main process thread,
    * calling from other threads will cause failure and FALSE will be returned
    * 
    * Return value: TRUE  on successful completion
    *               FALSE on an error
    */
    bool rtapi_set_process_name(const char *const new_name);

    const char *const rtapi_get_process_name(void);

    /* Function for execution of cmdline_process_function on newly created copy commandline
    *  arguments data, argument counter and argument vector
    *  After cmdline_proces_function returns, all variables are discarded with every change made
    * 
    * Return value: INT return value on CMDLINE_PROCESS_FUNCTION return on successful completion
    *               -ERROR CODE INT                                     on an error
    */
    int rtapi_execute_on_cmdline_copy(cmdline_data_callback cmdline_process_function, void *cloobj);

    /* Function for execution of cmdline_process_function on original (hot) commandline
    *  arguments data, argument counter and argument vector
    *  After cmdline_proces_function returns, housekeeping is done to save changes made
    * 
    * Return value: INT return value on CMDLINE_PROCESS_FUNCTION return on successful completion
    *               -ERROR CODE INT                                     on an error
    */
    int rtapi_execute_on_original_cmdline(cmdline_data_callback cmdline_process_function, void *cloobj);

    char *rtapi_getenv(const char *name);

    int rtapi_putenv(char *string);
#ifdef __cplusplus
}
#endif

#endif