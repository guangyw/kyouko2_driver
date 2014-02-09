/*
 * =====================================================================================
 *
 *       Filename:  kyouko2_reg.h
 *
 *    Description:  address of kyouko2
 *
 *        Version:  1.0
 *        Created:  02/09/2014 14:50:20
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Guangyan Wang (), guangyw@gmail.com
 *   Organization:  Clemson University
 *
 * =====================================================================================
 */

#define	CFG_REBOOT 0x1000			/*  */
#define	CFG_MODESET 0x1008			/*  */
#define	CFG_INTERRUPT 0x100C			/*  */
#define	CFG_ACCELERATION 0x1010

//stream buffer
#define	SB_A_ADDRESS 0x2000
#define	SB_A_CONFIG 0x2008			/*  */
#define	SB_B_ADDRESS 0x2004			/*  */
#define	SB_B_CONFIG 0x200C			/*  */

//Rasterization
#define RASTER_PRIMITIVE 0x3000
#define	RASTER_EMIT 0x3004			/*  */
#define	RASTER_CLEAR 0x3008			/*  */
#define	RASTER_TARGET_FRAME 0x3100			/*  */
#define	RASTER_FLUSH 0x3FFC			/*  */

//Info
#define	FIFO_DEPTH 0x4004               /*  */
#define	STATUS 0x4008			/*  */

//Drawing
#define	VTX_COORD4F 0x5000			/*  */
#define	VTX_COLOR4F 0x5010		/*  */
#define	VTX_TRANSFORM16F 0x5080			/*  */
#define	CLEAR_COLOR4F 0x5100			/*  */

//Frames base 0x8000
#define	FRM_BASE 0x8000			/*  */
#define	FRM_COLUMNS 0x0000			/*  */
#define	FRM_ROWS 0x0004			/*  */
#define	FRM_ROW_PITCH 0x0008			/*  */
#define FRM_PIXEL_FORMAT 0x000C
#define	FRM_START_ADDRESS 0x0010			/*  */
