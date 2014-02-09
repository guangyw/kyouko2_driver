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

#define	PCI_VENDOR_ID 0x1234			/*  */
#define	PCI_DEVICE_ID 0x1113			/*  */
#define	DEV_MAJOR 500			/*  */
#define	DEV_MINOR 127			/*  */
#define	CONTROL_SIZE 65536			/*  */
#define	Device_RAM 0x0020			/*  */

j

MODULE_LICENSE("Proprietary");
MODULE_AUTHOR("Guangyan Wang");

struct pci_device_id kyouko2_dev_ids[] = {
	{PCI.DEVICE(PCI_VENDOR_ID, PCI_DEVICEID)},
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

int kyouko2_open(struct inode *inode, struct file *filp){
	unsigned int ramSize;
	printk(KERN_ALERT "opened device");
	kyouko2.k_control_base = ioremap_nocache(kyouko2.p_control_base, CONTROL_SIZE);
	ramSize = K_READ_REG(Device_RAM);
	printk(KERN_ALEAT "ramSize is %d MB",ramSize);
	ramSize *= (1024*1024);
	kyouko2.k_fb_base = ioremap_nocache(kyouko2.p_fb_base, ramSize);
	return 0;
}

int kyouko2_mmap(struct file *filp, struct vm_area_struct *vma){
	io_remap_pfn_range(vma, vma->vm_start, kyouko2.p_control_base>>PAGE_SHIFT, vma->vm_end - vma->vm_start, vma->vm_page_prot);
	return 0;
}

long kyouko2_ioctl(struct file *filp, unsigned int cmd, unsigned long arg){
	return 0;
}

int kyouko2_release(void){
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
	cdev_init(&kyouko2_cdev, &kyouko2_fops);
	kyouko2_cdev.owner = THIS_MODULE;
	cdev_add(&kyouko2_cdev, MKDEV(DEV_MAJOR, DEV_MINOR), 1);
	pci_register_driver(&kyouko2_pci_drv);
	return 0;
}

static int kyouko2_exit(void){
	pci_unregister_driver(&kyouko2_pci_drv);
	cdev_del(&kyouko2_cdev);
	return 0;
}

module_init(kyouko2_init);
module_exit(kyouko2_exit);
