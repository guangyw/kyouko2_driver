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

typedef struct{
	unsigned int stride:5;
	unsigned int has_v4:1;
	unsigned int has_c3:1;
	unsigned int has_c4:1;
	unsigned int unused:4;
	unsigned int prim_type:2;
	unsigned int count:10;
	unsigned int opcode:8;
}kyouko2_dma_hdr;

kyouko2_dma_hdr dma_hdr = {
	.stride = 5,
	.has_v4 = 1,
	.has_c3 = 1,
	.has_c4 = 1,
	.unused = 0,
	.prim_type = 0,
	.count = 3,
	.opcode = 0x14
};

#define	KYOUKO_CONTROL_SIZE (65536)			/*  */
#define	Device_Ram (0x0020)			/*  */
unsigned long arg;
unsigned int countByte;

unsigned int U_READ_REG(unsigned int reg){
	return (*(kyouko2.u_control_base + (reg>>2)));
}

void U_WRITE_FB(unsigned int reg, unsigned int value){
	*(kyouko2.u_fb_base + reg) = value;
}

void U_WRITE_REG(unsigned int reg, unsigned int value){
	*(kyouko2.u_control_base + (reg>>2)) = value;
}

void u_sync(void){
	while(U_READ_REG(FIFO_DEPTH)>0);
}

void draw_fifo(void){
	float color[3][4] = {
		0.3, 0.4, 0.5, 1.0,
		0.8,0.1,0.4,1.0,
		0.2,0.9,0.4,1.0};

	float position[3][4] = {
		-0.3,0.1,0.0,1.0,
		0.2,0.4,0.0,1.0,
		0.8,-0.5,0.0,1.0};
	int i,j;
	//select triangle
	U_WRITE_REG(RASTER_PRIMITIVE, 1);
	//load vertex position
	for(i=0;i<3;++i){
		for(j=0;j<4;++j){
			U_WRITE_REG(VTX_COORD4F+4*j, *(unsigned int*)&position[i][j]);
			U_WRITE_REG(VTX_COLOR4F+4*j, *(unsigned int*)&color[i][j]);
		}
		U_WRITE_REG(RASTER_EMIT,0);
	}
	//reset primitive
	U_WRITE_REG(RASTER_PRIMITIVE, 0);
}

void triangle(void){
	float color[3][4] = {
		0.3, 0.4, 0.5, 1.0,
		0.8,0.1,0.4,1.0,
		0.2,0.9,0.4,1.0};

	float position[3][4] = {
		-0.3,0.1,0.0,1.0,
		0.2,0.4,0.0,1.0,
		0.8,-0.5,0.0,1.0};
	unsigned int i, j;
	countByte = 0;
	unsigned int* buf = (unsigned int*)(arg);
	buf[countByte++] = *(unsigned int*)&dma_hdr;
	for(i=0; i<3; ++i){
		for(j=0; j<3; ++j){
			buf[countByte++] = *(unsigned int*)&color[i][j];
		}
	}
	for(i=0; i<3; ++i){
		for(j=0; j<3; ++j){
			buf[countByte++] = *(unsigned int*)&position[i][j];
		}
	}
}

int main(){
	int fd;
	int result;
	int i;
	int ramSize;
	fd = open("/dev/kyouko2", O_RDWR);
	kyouko2.u_control_base = mmap(0,KYOUKO_CONTROL_SIZE,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
	result = U_READ_REG(Device_Ram);
	printf("Ram Size in MB is: %d\n", result);
	ramSize = result * 1024 * 1024;
	kyouko2.u_fb_base = mmap(0,ramSize, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0x80000000);

	//draw red line
	ioctl(fd,VMODE,GRAPHICS_ON);
	ioctl(fd,SYNC);
	//u_sync();
	for(i=200*1024; i<201*1024;i++){
		U_WRITE_FB(i,0xFF0000);
	}
	U_WRITE_REG(RASTER_FLUSH,1);
	ioctl(fd,SYNC);
	//draw_fifo();
	U_WRITE_REG(RASTER_FLUSH,1);
	ioctl(fd,SYNC);
	//u_sync();
	sleep(3);
	ioctl(fd,BIND_DMA,&arg);
	ioctl(fd,SYNC);
	arg = countByte; //test
	printf("number of byte user level is : %d\n", countByte);
	ioctl(fd,START_DMA,&arg);
	ioctl(fd,SYNC);
	printf("buffer address %lx\n", arg);
	ioctl(fd,VMODE,GRAPHICS_OFF);
	//U_WRITE_REG(CFG_REBOOT,1);
	close(fd);
	return 0;
}
