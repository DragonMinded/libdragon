/**
 * @file ioctl.h
 * @brief ioctl Implementation
 * @ingroup system
 */
#ifndef __LIBDRAGON_IOCTL_H
#define __LIBDRAGON_IOCTL_H

/** 
 * @brief Generate an ioctl Command Code
 *
 * @param[in] type
 *            A 16-bit number, often a character literal, specific to a subsystem
 *            or driver
 *
 * @param[in] nr
 *            A 16-bit number identifying the specific command, unique for a given
 *            value of 'type'
 * @return An ioctl Command Code
 */
#define _IO(type, nr) (((type) & 0xFFFF) << 16)|((nr) & 0xFFFF)

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Perform IO Control Request
 *
 * @param[in] fd
 *            File handle
 * @param[in] cmd
 *            Request ioctl command code 
 * @param[in] argp
 *            Pointer to a request-specific data structure
 *
 * @return Zero on success, or a negative value on error.
 */
int ioctl(int fd, unsigned long cmd, void *argp);

#ifdef __cplusplus
}
#endif


#endif
