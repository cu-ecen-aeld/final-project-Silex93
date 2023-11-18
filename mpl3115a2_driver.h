//
// Created by silex on 13/11/2023.
//

#ifndef TEMP_SENSOR_DRIVER_MPL3115A2_DRIVER_H
#define TEMP_SENSOR_DRIVER_MPL3115A2_DRIVER_H

#ifdef __KERNEL__
#include <linux/types.h>
#include <linux/regmap.h>
#include <linux/i2c.h>
#else
#include <stddef.h> // size_t
#include <stdint.h> // uintx_t
#include <stdbool.h>
#endif



#define AESD_DEBUG 1  //Remove comment on this line to enable debug

#undef PDEBUG             /* undef it, just in case */
#ifdef AESD_DEBUG
#  ifdef __KERNEL__
/* This one if debugging is on, and kernel space */
#    define PDEBUG(fmt, args...) printk( KERN_DEBUG "mpl3115a2: " fmt, ## args)
#  else
/* This one for user space */
#    define PDEBUG(fmt, args...) fprintf(stderr, fmt, ## args)
#  endif
#else
#  define PDEBUG(fmt, args...) /* not debugging: nothing */
#endif



#endif //TEMP_SENSOR_DRIVER_MPL3115A2_DRIVER_H


