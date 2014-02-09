/*
 * =====================================================================================
 *
 *       Filename:  main.c
 *
 *    Description:
 *
 *        Version:  1.0
 *        Created:  02/09/2014 14:14:15
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Guangyan Wang (), guangyw@gmail.com
 *   Organization:  Clemson University
 *
 * =====================================================================================
 */

#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <sys/mman.h>
#include "kyouko2_ioctl.h"
#include "kyouko2_reg.h"

struct u_kyouko2_device {
	unsigned int *u_control_base;
	unsigned int *u_fb_base;
}kyouko2;

#define	KYOUKO_CONTROL_SIZE (65536)			/*  */
#define	Device_Ram (0x0020)			/*  */

unsigned int U_READ_REG(unsigned int reg){
	return (*(kyouko2.u_control_base + (reg>>2)));
}

void U_WRITE_FB(unsigned int reg, unsigned int value){
	*(kyouko2.u_fb_base + reg) = value;
}

void U_WRITE_REG(unsigned int reg, unsigned int value){
	*(kyouko2.u_control_base + (reg>>2)) = value;
}

int main(){
	int fd;
	int result;
	int i;
	fd = open("/dev/kyouko2", O_RDWR);
	kyouko2.u_control_base = mmap(0,KYOUKO_CONTROL_SIZE,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
	result = U_READ_REG(Device_Ram);
	printf("Ram Size in MB is: %d\n", result);
	ramSize = result * 1024 * 1024;
	kyouko2.u_fb_base = mmap(0,ramSize, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0x80000000);

	//draw red line
	ioctl(fd,VMODE,GRAPHICS_ON);
	ioctl(SYNC);
	for(i=200*1024; i<201*1024;i++){
		U_WRITE_FB(i,0xFF0000);
	}
	U_WRITE_REG(RASTER_FLUSH,0);
	ioctl(SYNC);
	sleep(5);
	ioctl(fd,VMODE,GRAPHICS_OFF);
	close(fd);
	return 0;
}
