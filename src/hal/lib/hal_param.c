/* 
 * HAL param API is a deprecated construct, DO NOT USE for new code under
 * any circumstances!
 * 
*/

#include "config.h"
#include "rtapi.h"		/* RTAPI realtime OS API */
#include "hal.h"		/* HAL public API decls */
#include "hal_priv.h"		/* HAL private decls */
#include "hal_internal.h"

/***********************************************************************
*               "PARAM" FUNCTIONS - DO NOT USE                         *
************************************************************************/

static int hal_param_newfv(hal_type_t type,
			   hal_param_dir_t dir,
			   volatile void *data_addr,
			   int owner_id,
			   const char *fmt,
			   va_list ap)
{
    char name[HAL_NAME_LEN + 1];
    int sz;
    sz = rtapi_vsnprintf(name, sizeof(name), fmt, ap);
    if(sz == -1 || sz > HAL_NAME_LEN) {
        HALFAIL_RC(ENOMEM, "length %d invalid too long for name starting '%s'\n",
	       sz, name);
    }
    return hal_param_new(name, type, dir, (void *) data_addr, owner_id);
}

int hal_param_bit_newf(hal_param_dir_t dir, hal_bit_t * data_addr,
		       int owner_id, const char *fmt, ...)
{
    va_list ap;
    int ret;
    va_start(ap, fmt);
    ret = hal_param_newfv(HAL_BIT, dir, (void*)data_addr, owner_id, fmt, ap);
    va_end(ap);
    return ret;
}

int hal_param_float_newf(hal_param_dir_t dir, hal_float_t * data_addr,
			 int owner_id, const char *fmt, ...)
{
    va_list ap;
    int ret;
    va_start(ap, fmt);
    ret = hal_param_newfv(HAL_FLOAT, dir, (void*)data_addr, owner_id, fmt, ap);
    va_end(ap);
    return ret;
}

int hal_param_u32_newf(hal_param_dir_t dir, hal_u32_t * data_addr,
		       int owner_id, const char *fmt, ...)
{
    va_list ap;
    int ret;
    va_start(ap, fmt);
    ret = hal_param_newfv(HAL_U32, dir, (void*)data_addr, owner_id, fmt, ap);
    va_end(ap);
    return ret;
}

int hal_param_s32_newf(hal_param_dir_t dir, hal_s32_t * data_addr,
		       int owner_id, const char *fmt, ...)
{
    va_list ap;
    int ret;
    va_start(ap, fmt);
    ret = hal_param_newfv(HAL_S32, dir, (void*)data_addr, owner_id, fmt, ap);
    va_end(ap);
    return ret;
}

// printf-style version of hal_param_new()
int hal_param_newf(hal_type_t type,
		   hal_param_dir_t dir,
		   volatile void * data_addr,
		   int owner_id,
		   const char *fmt, ...)
{
    va_list ap;
    void *p;
    va_start(ap, fmt);
    p = halg_param_newfv(1, type, dir, data_addr, owner_id, fmt, ap);
    va_end(ap);
    return p == NULL ? _halerrno : 0;
}

hal_pin_t * halg_param_newf(const int use_hal_mutex,
		    hal_type_t type,
		    hal_param_dir_t dir,
		    volatile void * data_addr,
		    int owner_id,
		    const char *fmt, ...)
{
    va_list ap;
    void *p;
    va_start(ap, fmt);
    p = halg_param_newfv(use_hal_mutex, type, dir, data_addr, owner_id, fmt, ap);
    va_end(ap);
    return p;
}


/* this is a generic function that does the majority of the work. */
// v2
hal_param_t *halg_param_newfv(const int use_hal_mutex,
			      hal_type_t type,
			      hal_param_dir_t dir,
			      volatile void *data_addr,
			      int owner_id,
			      const char *fmt, va_list ap)
{
	hal_data_u defval={0};
    void* hal_pointer = hal_malloc(sizeof(void*));
	size_t halsize=0;
	switch(type){
		case HAL_S32:
			halsize = sizeof(hal_s32_t);
			break;
		case HAL_U32:
			halsize = sizeof(hal_u32_t);
			break;
		case HAL_FLOAT:
			halsize = sizeof(hal_float_t);
			break;
		case HAL_BIT:
			halsize = sizeof(hal_bit_t);
			break;
		default:
			halsize = sizeof(hal_float_t);
	}
	hal_pointer = hal_malloc(halsize);
	data_addr = hal_pointer;

	rtapi_print_msg(RTAPI_MSG_ERR, "PF: Pointer address of owner %d pointer is %p\n", owner_id, &data_addr);

    return halg_pin_newfv(use_hal_mutex, type, (hal_pin_dir_t)dir, hal_pointer, owner_id,
			  			  defval, fmt, ap);
}
