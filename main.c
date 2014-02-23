#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <sys/mman.h>
#include <linux/ioctl.h>
#include "kyouko2_reg.h"
#include <time.h>
#include <stdlib.h>

#define VMODE _IOW(0xcc,0,unsigned long)
#define BIND_DMA _IOW(0xcc,1,unsigned long)
#define START_DMA _IOWR(0xcc,2,unsigned long)
#define SYNC _IO(0xcc,3)
#define FLUSH _IO(0xcc,4)
#define GRAPHICS_ON 1
#define GRAPHICS_OFF 0

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
	.has_v4 = 0,
	.has_c3 = 1,
	.has_c4 = 0,
	.unused = 0,
	.prim_type = 1,
	.count = 90,
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

float randPos(){
	//srand(time(NULL));
	return 2*((float)rand()/(float)RAND_MAX) - 1;
}

float randColor(){
	//srand(time(NULL));
	return (float)rand()/(float)RAND_MAX;
}

void triangle(void){
	struct vertice{
		float color[3][3];
		float pos[3][3];
	};
	struct vertice vertices[50];
	unsigned int i, j, k;
	for(k=0; k<30; ++k){
		for(i=0; i<3; ++i){
			for(j=0; j<3; ++j){
				vertices[k].color[i][j] = randColor();
				vertices[k].pos[i][j] = randPos();
			}
		}
	}

	/*  struct vertice vertices[] = {
		{{{0.3,0.4,0.2},{0.6,0.5,0.9},{0.1,0.3,0.5}},{{0.6,0.2,0},{0.4,0.3,0},{-0.5,0.4,0}}},
		{{{0.1,0.1,0.1},{0.5,0.4,0.5},{0.8,0.8,0.8}},{{0.2,0.1,0},{0.4,0.6,0},{0.5,-0.5,0}}},
		{0}
	};*/
/*	float color[3][4] = {
		0.3, 0.4, 0.5, 1.0,
		0.8,0.1,0.4,1.0,
		0.2,0.9,0.4,1.0};

	float position[3][4] = {
		-0.3,0.1,0.0,1.0,
		0.2,0.4,0.0,1.0,
		0.8,-0.5,0.0,1.0};
		*/
	countByte = 0;
	unsigned int* buf = (unsigned int*)(arg);
	buf[countByte++] = *(unsigned int*)&dma_hdr;
	for(k=0; k<30; ++k){
	for(i=0; i<3; ++i){
		for(j=0; j<3; ++j){
		//	buf[countByte++] = *(unsigned int*)&color[i][j];
			buf[countByte++] = *(unsigned int*)&(vertices[k].color[i][j]);
		}
	}
	for(i=0; i<3; ++i){
		for(j=0; j<3; ++j){
			//buf[countByte++] = *(unsigned int*)&position[i][j];
			buf[countByte++] = *(unsigned int*)&(vertices[k].pos[i][j]);
		}
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
	ioctl(fd,BIND_DMA,&arg);
	ioctl(fd,SYNC);
	//printf("user buffer address %lx\n", arg);
	triangle();
	//ioctl(fd,SYNC);
	//sleep(3);
	//U_WRITE_REG(RASTER_EMIT,0);

	//U_WRITE_REG(RASTER_FLUSH,1);
	arg = countByte; //test
	printf("number of byte user level is : %d\n", countByte);
	ioctl(fd,SYNC);
	ioctl(fd,START_DMA,&arg);
	//sleep(1);
	U_WRITE_REG(RASTER_FLUSH,1);
	sleep(3);
	ioctl(fd,SYNC);
	printf("buffer address %lx\n", arg);
	ioctl(fd,VMODE,GRAPHICS_OFF);
	U_WRITE_REG(CFG_REBOOT,1);
	close(fd);
	return 0;
}
