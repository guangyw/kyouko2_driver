/*
 * =====================================================================================
 *
 *       Filename:  ioctl.h
 *
 *    Description:
 *
 *        Version:  1.0
 *        Created:  02/09/2014 14:43:52
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Guangyan Wang (), guangyw@gmail.com
 *   Organization:  Clemson University
 *
 * =====================================================================================
 */

#include <linux/ioctl.h>

#define VMODE _IOW(0xCC, 0, unsigned long)
#define BIND_DMA _IOW(0xCC, 1, unsigned long)
#define START_DMA _IOW(0xCC, 2, unsigned long)
#define SYNC _IO(0xCC, 3)

#define GRAPHICS_ON _IOW(0xCC, 4, unsigned long)
#define GRAPHICS_OFF _IOW(0xCC, 5, unsigned long)
