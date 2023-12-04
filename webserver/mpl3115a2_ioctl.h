
#ifndef MPL3115A2_IOCTL_H
#define MPL3115A2_IOCTL_H

#ifdef __KERNEL__
#include <asm-generic/ioctl.h>
#include <linux/types.h>
#else
#include <sys/ioctl.h>
#include <stdint.h>
#endif

/**
 * A structure to be passed by IOCTL from user space to kernel space, describing the type
 * of seek performed on the mpl3115a2char driver
 */
struct mpl3115a2_write {
    /**
     * The register address
     */
    uint8_t reg_address;
    /**
     * The data to write
     */
    uint8_t data;
};

// Pick an arbitrary unused value from https://github.com/torvalds/linux/blob/master/Documentation/userspace-api/ioctl/ioctl-number.rst
#define MPL3115A2_IOC_MAGIC 0x16

// Define a write command from the user point of view, use command number 1
#define MPL3115A2_IOC_WRITE _IOWR(MPL3115A2_IOC_MAGIC, 1, struct mpl3115a2_write)
/**
 * The maximum number of commands supported, used for bounds checking
 */
#define MPL3115A2CHAR_IOC_MAXNR 1

#endif /* MPL3115A2_IOCTL_H */