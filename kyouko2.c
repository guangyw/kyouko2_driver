/*
 * =====================================================================================
 *
 *       Filename:  kyouko2.c
 *
 *    Description:
 *
 *        Version:  1.0
 *        Created:  02/07/2014 13:02:28
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Guangyan Wang (), guangyw@gmail.com
 *   Organization:  Clemson University
 *
 * =====================================================================================
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kernel_stat.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/mm.h>

#include "kyouko2_ioctl.h"
#include "kyouko2_reg.h"

//#define	PCI_VENDOR_ID 0x1234			/*  */
//#define	PCI_DEVICE_ID 0x1113			/*  */
#define	DEV_MAJOR 500			/*  */
#define	DEV_MINOR 127			/*  */
#define	CONTROL_SIZE 65536			/*  */
#define	Device_RAM 0x0020			/*  */


MODULE_LICENSE("Proprietary");
MODULE_AUTHOR("Guangyan Wang");

struct pci_device_id kyouko2_dev_ids[] = {
	{PCI_DEVICE(0x1234, 0x1113)},
	{0}
};

static struct kyouko{
	unsigned long p_control_base;
	unsigned long p_fb_base;
	unsigned int *k_control_base;
	unsigned int *k_fb_base;
	struct pci_dev *pci_dev;
}kyouko2;

unsigned int K_READ_REG(unsigned int reg){
	unsigned int value;
	value = *(kyouko2.k_control_base + (reg>>2));
	return value;
}

void K_WRITE_REG(unsigned int reg, unsigned int value){
	udelay(1);
	*(kyouko2.k_control_base + (reg>>2)) = value;
}

/*void K_WRITE_REG_FB(unsigned int reg, unsigned int value){
	udelay(1);
	*(kyouko2.k_fb_base + (reg>>2)) = value;
}*/

int kyouko2_open(struct inode *inode, struct file *filp){
	unsigned int ramSize;
	printk(KERN_ALERT "opened device");
	kyouko2.k_control_base = ioremap_nocache(kyouko2.p_control_base, CONTROL_SIZE);
	printk(KERN_ALERT "k_control_base is : %x",kyouko2.k_control_base);
	ramSize = K_READ_REG(Device_RAM);
	printk(KERN_ALERT "ramSize is %d MB",ramSize);
	ramSize *= (1024*1024);
	kyouko2.k_fb_base = ioremap_nocache(kyouko2.p_fb_base, ramSize);
	printk(KERN_ALERT "k_fb_base is : %x",kyouko2.k_fb_base);
	return 0;
}

int kyouko2_mmap(struct file *filp, struct vm_area_struct *vma){
	int vma_size;
	vma_size = vma->vm_end - vma->vm_start;
	printk(KERN_ALERT "vma page offset is : %xl", vma->vm_pgoff);
	if (vma->vm_pgoff == 0x0)
		io_remap_pfn_range(vma, vma->vm_start, kyouko2.p_control_base>>PAGE_SHIFT, vma_size, vma->vm_page_prot);
	else if (vma->vm_pgoff == 0x80000)
		io_remap_pfn_range(vma, vma->vm_start, kyouko2.p_fb_base>>PAGE_SHIFT, vma_size, vma->vm_page_prot);
	return 0;
}

void sync(void){
	while(K_READ_REG(FIFO_DEPTH)>0);
}

long kyouko2_ioctl(struct file *filp, unsigned int cmd, unsigned long arg){
	switch(cmd){
		case VMODE:
			if(((int)(arg)) == GRAPHICS_ON){
				//set frame 0
				K_WRITE_REG(0x8000 + FRM_COLUMNS, 1024);
				K_WRITE_REG(0x8000 + FRM_ROWS, 768);
				K_WRITE_REG(0x8000 + FRM_ROW_PITCH, 1024*4);
				K_WRITE_REG(0x8000 + FRM_PIXEL_FORMAT, 0xF888);
				K_WRITE_REG(0x8000 + FRM_START_ADDRESS, 0);

				//set dac 0
				K_WRITE_REG(0x9000,1024);
				K_WRITE_REG(0x9000 + 4, 768);
				K_WRITE_REG(0x9000 + 8, 0);
				K_WRITE_REG(0x9000 + 12, 0);
				K_WRITE_REG(0x9000 + 16, 0);

				//set acceleration
				K_WRITE_REG(CFG_ACCELERATION, 0x40000000);
				sync();
				//write to clear buffer reg
				K_WRITE_REG(CLEAR_COLOR4F, 0x3F000000);
				K_WRITE_REG(CLEAR_COLOR4F + 4, 0x3F000000);
				K_WRITE_REG(CLEAR_COLOR4F + 8, 0x3F000000);
				K_WRITE_REG(CLEAR_COLOR4F + 12, 0x3F800000);

				//flush
				K_WRITE_REG(RASTER_FLUSH, 1);
				sync();
				//write 1 to clear buffer reg
				K_WRITE_REG(RASTER_CLEAR, 1);
			}else{
				sync();
				K_WRITE_REG(CFG_REBOOT, 1);
			}
			break;
		case SYNC:
			sync();
			break;
	}

	return 0;
}

int kyouko2_release(struct inode * inode, struct file *filp){
	//write 1 to reboot register
	return 0;
}

struct file_operations kyouko2_fops = {
	.open = kyouko2_open,
	.release = kyouko2_release,
	.owner = THIS_MODULE,
	.unlocked_ioctl = kyouko2_ioctl,
	.mmap = kyouko2_mmap
};

static int kyouko2_probe(struct pci_dev *pci_dev, const struct pci_device_id *pci_id){
	kyouko2.pci_dev = pci_dev;
	kyouko2.p_control_base = pci_resource_start(pci_dev, 1);
	kyouko2.p_fb_base = pci_resource_start(pci_dev, 2);
	pci_enable_device(pci_dev);
	pci_set_master(pci_dev);
	return 0;
}

void kyouko2_remove(struct pci_dev *pci_dev){}

struct pci_driver kyouko2_pci_drv = {
	.name = "kyouko2_drv",
	.id_table = kyouko2_dev_ids,
	.probe = kyouko2_probe,
	.remove = kyouko2_remove
};


struct cdev kyouko2_cdev;
static int kyouko2_init(void){
	int flag;
	cdev_init(&kyouko2_cdev, &kyouko2_fops);
	kyouko2_cdev.owner = THIS_MODULE;
	cdev_add(&kyouko2_cdev, MKDEV(DEV_MAJOR, DEV_MINOR), 1);
	flag = pci_register_driver(&kyouko2_pci_drv);
	printk(KERN_ALERT "Initialized Device");
	return 0;
}

static void kyouko2_exit(void){
	pci_unregister_driver(&kyouko2_pci_drv);
	cdev_del(&kyouko2_cdev);
	printk(KERN_ALERT "Device deleted");
}

module_init(kyouko2_init);
module_exit(kyouko2_exit);
