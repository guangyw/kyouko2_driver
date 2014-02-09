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

struct u_kyouko2_device {
	unsigned int *u_controal_base;
}kyouko2;

#define	KYOUKO_CONTROL_SIZE (65536)			/*  */
#define	Device_Ram (0x0020)			/*  */

unsigned int U_READ_REG(unsigned int reg){
	return (*(u_control_base + (reg>>2)));
}

int main(){
	int fd;
	int result;
	fd = open("/dev/kyouko2", O_RDWR);
	kyouko2.u_control_base = mmap(0,KYOUKO_CONTROL_SIZE,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
	result = U_READ_REG(Device_Ram);
	printf("Ram Size in MB is: %d\n", result);
	close(fd);
	return 0;
}
