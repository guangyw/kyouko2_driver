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
#include <linux/mman.h>
#include <asm/mman.h>
#include <linux/io.h>
#include <linux/spinlock.h>

#include "kyouko2_ioctl.h"
#include "kyouko2_reg.h"

//#define	PCI_VENDOR_ID 0x1234			/*  */
//#define	PCI_DEVICE_ID 0x1113			/*  */
#define	DEV_MAJOR 500			/*  */
#define	DEV_MINOR 127			/*  */
#define	CONTROL_SIZE 65536			/*  */
#define	Device_RAM 0x0020			/*  */
//dma buffer
#define NUM_BUFFER 8
#define BUFFER_SIZE (124*1024)

DECLARE_WAIT_QUEUE_HEAD(dma_snooze);
static spinlock_t lock;
static unsigned long flags;

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

struct dma_buffer{
	unsigned int *k_base_addr;
	unsigned long u_base_addr;
	dma_addr_t dma_handle;
	unsigned int count;
}dma_buffers[NUM_BUFFER];

struct buffer_status{
	unsigned int cur;
	unsigned int fill;
	unsigned int drain;
}buffer_status;

unsigned int K_READ_REG(unsigned int reg){
	unsigned int value;
	rmb();
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
	printk(KERN_ALERT "k_control_base is : %x\n",kyouko2.k_control_base);
	ramSize = K_READ_REG(Device_RAM);
	printk(KERN_ALERT "ramSize is %d MB",ramSize);
	ramSize *= (1024*1024);
	kyouko2.k_fb_base = ioremap_nocache(kyouko2.p_fb_base, ramSize);
	printk(KERN_ALERT "k_fb_base is : %x\n",kyouko2.k_fb_base);
	return 0;
}

int kyouko2_mmap(struct file *filp, struct vm_area_struct *vma){
	int vma_size;
	vma_size = vma->vm_end - vma->vm_start;
	//printk(KERN_ALERT "vma page offset is : %x\n", vma->vm_pgoff);
	if (vma->vm_pgoff == 0x0){
		printk(KERN_ALERT "mmapping control base address\n");
		io_remap_pfn_range(vma, vma->vm_start, kyouko2.p_control_base>>PAGE_SHIFT, vma_size, vma->vm_page_prot);
	}else if (vma->vm_pgoff == 0x80000){
		printk(KERN_ALERT "mmapping frame buffer base address\n");
		io_remap_pfn_range(vma, vma->vm_start, kyouko2.p_fb_base>>PAGE_SHIFT, vma_size, vma->vm_page_prot);
	}else{
		printk(KERN_ALERT "mmapping buffer\n");
		io_remap_pfn_range(vma, vma->vm_start, dma_buffers[buffer_status.cur].dma_handle, vma_size, vma->vm_page_prot);
	}
	return 0;
}

void sync(void){
	while(K_READ_REG(FIFO_DEPTH)>0);
}

irqreturn_t dma_intr(int irq, void *dev_id, struct pt_regs *regs){
	unsigned int flags;
	flags = K_READ_REG(STATUS);
	K_WRITE_REG(STATUS,0xF);
	if(flags & 0x02 == 0)
		return (IRQ_NONE);
	spin_lock_irqsave(&lock, flags);
	if(buffer_status.fill == buffer_status.drain){
		wake_up_interruptible(&dma_snooze);
	}
	buffer_status.drain = (buffer_status.drain + 1) % NUM_BUFFER;
	K_WRITE_REG(BUFFERA_ADDR, dma_buffers[buffer_status.drain].dma_handle);
	K_WRITE_REG(BUFFERA_CONFIG, dma_buffers[buffer_status.drain].count);
	spin_unlock_irqrestore(&lock, flags);
	return (IRQ_HANDLED);
}

void initiate_transfer(void){
	//local irq need to be replaced by spinlock
	bool fill_flag = false;
	spin_lock_irqsave(&lock,flags);
	//local_irq_save(flag);
	if(buffer_status.fill == buffer_status.drain){
		//local_irq_restore(flag);
		buffer_status.fill = (buffer_status.fill+1)%NUM_BUFFER;
		spin_unlock_irqrestore(&lock,flags);
		K_WRITE_REG(BUFFERA_ADDR, dma_buffers[buffer_status.drain].dma_handle);
		K_WRITE_REG(BUFFERA_CONFIG, dma_buffers[buffer_status.drain].count);
		return;
	}
	buffer_status.fill = (buffer_status.fill + 1) % NUM_BUFFER;
	if(buffer_status.fill == buffer_status.drain){
		fill_flag = true;
	}
	spin_unlock_irqrestore(&lock,flags);
	wait_event_interruptible(dma_snooze, fill_flag == false);
	//wait_event_interruptible(dma_snooze, buffer_status.fill != buffer_status.drain);
	//local_irq_restore(flag);
	return;
}

long kyouko2_ioctl(struct file *filp, unsigned int cmd, unsigned long arg){
	unsigned int i;
	unsigned int result;
	unsigned long count;
	switch(cmd){
		case VMODE:
			switch(arg){
				case GRAPHICS_ON:
					printk(KERN_ALERT "Graphics on \n");
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
				//modeset
					K_WRITE_REG(CFG_MODESET,1);
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
					break;
				case GRAPHICS_OFF:
					printk(KERN_ALERT "Turning off graphic mode\n");
					//sync();
					K_WRITE_REG(CFG_REBOOT, 1);
					break;
				default:
					printk(KERN_ALERT "Argument not found\n");
					break;
			}
			break;
		case SYNC:
			printk(KERN_ALERT "SYNCING\n");
			sync();
			break;
		case START_DMA:
			//what is count in user space?
			//printk(KERN_ALERT "arg possibly is %d",*(unsigned long*)arg); //can not access this region
			copy_from_user(&count,(unsigned long*)arg,sizeof(unsigned long));
			printk(KERN_ALERT "count is : %d\n", count);
			if(count != 0){
				printk(KERN_ALERT "initiating transmission");
				dma_buffers[buffer_status.fill].count = count;
				initiate_transfer();
				//copy_to_user((unsigned long*)arg, &dma_buffers[buffer_status.drain].u_base_addr, sizeof(unsigned long));
				copy_to_user((unsigned long*)arg, &dma_buffers[buffer_status.fill].u_base_addr, sizeof(unsigned long));
			}
			break;

		case BIND_DMA:
			printk(KERN_ALERT "IN BINDING DMA\n");
			for(i=0; i < NUM_BUFFER; ++i){
				dma_buffers[i].k_base_addr = pci_alloc_consistent(kyouko2.pci_dev,BUFFER_SIZE,&dma_buffers[i].dma_handle);
				printk(KERN_ALERT "k_base_addr %lx", dma_buffers[i].k_base_addr);
				buffer_status.cur = i;
				dma_buffers[i].u_base_addr = do_mmap(filp,0,BUFFER_SIZE,PROT_READ|PROT_WRITE,MAP_SHARED,0x10000000);
				printk(KERN_ALERT "u_base_addr %lx",dma_buffers[i].u_base_addr);
				dma_buffers[i].count = 0;
			}
			//enable message interrupt
			pci_enable_msi(kyouko2.pci_dev);
			result = request_irq(kyouko2.pci_dev->irq,(irq_handler_t)dma_intr,IRQF_SHARED|IRQF_DISABLED,"dma_intr",&kyouko2);
			K_WRITE_REG(CFG_INTERRUPT,0x02);

			buffer_status.fill = 0;
			buffer_status.drain = 0;
			copy_to_user((unsigned long*)arg, &dma_buffers[buffer_status.fill].u_base_addr, sizeof(unsigned long));
			break;
		default:
			printk(KERN_ALERT "No ioctl cmd found\n");
			break;
	}

	return 0;
}

int kyouko2_release(struct inode * inode, struct file *filp){
	//write 1 to reboot register
	int i;
	sync();
	for(i=0; i<NUM_BUFFER; ++i){
		pci_free_consistent(kyouko2.pci_dev, BUFFER_SIZE, dma_buffers[i].k_base_addr, dma_buffers[i].dma_handler);
		do_mummap(dma_buffers[i].u_base_addr, BUFFER_SIZE);
	}
	free_irq(kyouko2.pci_dev->irq, &kyouko2);
	pci_disable_msi(kyouko2.pci_dev);
	iounmap(kyouko2.k_control_base);
	iounmap(kyouko2.k_fb_base);
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
	printk(KERN_ALERT "Initialized Device\n");
	return 0;
}

static void kyouko2_exit(void){
	pci_unregister_driver(&kyouko2_pci_drv);
	cdev_del(&kyouko2_cdev);
	printk(KERN_ALERT "Device deleted\n");
}

module_init(kyouko2_init);
module_exit(kyouko2_exit);
