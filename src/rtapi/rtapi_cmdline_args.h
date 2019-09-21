/********************************************************************
* Description:  rtapi_cmdline_args.h
*               This file, 'rtapi_cmdline_args.h', defines functions
*               used for exporting and manipulation of command line arguments
*               passed to each application as a 'int argc, char** argv'
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

#ifndef RTAPI_CMDLINE_ARGS_H
#define RTAPI_CMDLINE_ARGS_H

#ifdef __cplusplus
extern "C"
{
#endif

    /* Initialization of process memory used for storing command line arguments
    * This function has to be run from the main process thread and should be
    * run at the start of the program as soon as possible
    * 
    * Return value: 0              on successful completion
    *               negative ERRNO on an error (then the process should be terminated)
    */
    int cmdline_args_init(int argc, char **argv);

    void cmdline_args_exit(void);

    /* Changing of the process name to new values specified as a new_name
    * This function has to be called from the main process thread,
    * calling from other threads will cause failure and FALSE will be returned
    * 
    * Return value: TRUE  on successful completion
    *               FALSE on an error
    */
    bool set_process_name(const char *const new_name);

    const char *const get_process_name(void);

#ifdef __cplusplus
}
#endif

#endif