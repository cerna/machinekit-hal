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
    bool change_process_name(const char *const new_name);

#ifdef __cplusplus
}
#endif

#endif