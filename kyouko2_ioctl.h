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

#define VMODE _IOW(0xcc,0,unsigned long)
#define BIND_DMA _IOW(0xcc,1,unsigned long)
#define START_DMA _IOWR(0xcc,2,unsigned long)
#define SYNC _IO(0xcc,3)
#define FLUSH _IO(0xcc,4)
#define GRAPHICS_ON 1
#define GRAPHICS_OFF 0

