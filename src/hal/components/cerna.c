
#include "rtapi.h"		/* RTAPI realtime OS API */
#include "rtapi_app.h"		/* RTAPI realtime module decls */
#include "hal.h"		/* HAL public API decls */
#include "rtapi_string.h"

/* module information */
MODULE_AUTHOR("Cerna");
MODULE_DESCRIPTION("HAL param test");
MODULE_LICENSE("GPL");

/***********************************************************************
*                STRUCTURES AND GLOBAL VARIABLES                       *
************************************************************************/

/* other globals */
static int comp_id;		/* component ID */

/***********************************************************************
*                  LOCAL FUNCTION DECLARATIONS                         *
************************************************************************/


/***********************************************************************
*                       INIT AND EXIT CODE                             *
************************************************************************/


int rtapi_app_main(void)
{
    int retval;

    /* have good config info, connect to the HAL */
    comp_id = hal_init("cerna");
    if (comp_id < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "CERNA: ERROR: hal_init() failed\n");
	return -1;
    }
    /* was 'period' specified in the insmod command? */
    void* pointer = hal_malloc(sizeof(hal_s32_t*));
    pointer = hal_malloc(sizeof(hal_s32_t));

    //hal_data_u defval = {0};
    int num = 0;
    retval = hal_pin_s32_newf(HAL_IN, pointer, comp_id,
			  			  "cerna.%d.rawcounts", num);
    if (retval !=0){
        rtapi_print_msg(RTAPI_MSG_ERR, "Cerna test failed\n");
        return -400 + retval;
    }
    hal_s32_t * data = hal_malloc(sizeof(hal_s32_t));
    rtapi_print_msg(RTAPI_MSG_ERR, "Pointer address of owner %d pointer is %p\n", comp_id, &data);

    retval = hal_param_s32_newf(HAL_RO, data, comp_id, "cerna.testing_pin");
    if (retval != 0){
        rtapi_print_msg(RTAPI_MSG_ERR, "Cerna test failed\n");
        return -500+ retval;
    }

    *data = 5;
    rtapi_print_msg(RTAPI_MSG_ERR, "AHOJ");

    hal_ready(comp_id);
    return 0;
}

void rtapi_app_exit(void)
{
    hal_exit(comp_id);
}
