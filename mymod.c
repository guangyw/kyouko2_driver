#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kernel_stat.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/pci.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <asm/mman.h>
#include <linux/io.h>
#include <linux/sched.h>
#include <linux/spinlock.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Runzhen Wang");

#define DEBUG                     1

#define Rom_Fifo_Size             0x002c

#define PCI_VENDER_ID_CC          0x1234
#define PCI_DEVICE_ID_DD          0x1113

#define Config_Reboot             0x1000
#define Config_ModeSet            0x1008
#define Config_Accelleration      0x1010

#define Info_Fifo_Depth           0x4004

#define Raster_Flush              0x3FFC
#define Raster_Clear              0x3008

#define Frame_Columns             0x8000
#define Frame_Rows                0x8004
#define Frame_RowPitch            0x8008
#define Frame_Pixel_Format        0x800c
#define Frame_Start_Address       0x8010

#define Encoder_Width             0x9000
#define Encoder_Hight             0x9004
#define Encoder_OffsetX           0x9008
#define Encoder_OffsetY           0x900c
#define Encoder_Frame             0x9010

#define BUFFERA_ADDR		  0x2000
#define BUFFERA_CONFIG            0x2008

#define Draw_Clear_Color4f        0x5100

#define MAGIC_NUM                 0xcc 
#define VMODE                     _IOW(MAGIC_NUM, 0, unsigned long) 
#define BIND_DMA                  _IOW(MAGIC_NUM, 1, unsigned long) 
#define START_DMA                 _IOWR(MAGIC_NUM, 2, unsigned long) 
#define SYNC			  _IO(MAGIC_NUM, 3)

#define GRAPHICS_ON  		  1
#define GRAPHICS_OFF 		  0

#define DMA_BUFFER_NUM   	  8
#define DMA_BUFFER_SIZE           (124 * 1024)

static DECLARE_WAIT_QUEUE_HEAD(waitq);

#ifdef DEBUG
#define PRINT(x)					\
do {							\
	printk(KERN_ALERT "KERNEL : %s\n", (x));			\
} while(0)
#else
#define PRINT(x)  do {} while(0)
#endif

int bind_dma_flag = 0; // control the if case in kyouko2_mmap

struct _dma {
	void          * k_dma_addr;
	unsigned long u_dma_addr;
	dma_addr_t    dma_handle;
	unsigned long byte_count;
} dma;

struct kyouko2_card {
	unsigned long p_control_base;
	unsigned long p_control_len;
	unsigned long p_framebuffer_base;
	unsigned long p_framebuffer_len;
	unsigned int * k_control_base;
	unsigned int * k_framebuffer_base;
	struct _dma  dma[DMA_BUFFER_NUM];
	int          drain;
	int          fill;
	bool         is_full;
	struct pci_dev *pci_dev;
	unsigned long flag; // for spin_lock_irqsave
	spinlock_t    lock;
} card;

static struct pci_device_id kyouko2_pci_tbl[] = {
	{PCI_DEVICE(PCI_VENDER_ID_CC, PCI_DEVICE_ID_DD)},
	{0,}
};

static unsigned int K_READ_REG(unsigned int reg)
{
	unsigned int value;
	rmb();
	value = *(card.k_control_base + (reg >> 2));
	return value;
}

static void K_WRITE_REG(unsigned int reg, unsigned int value)
{
	udelay(1);
	*(card.k_control_base + (reg >> 2)) = value;
}

inline void sync(void)
{
	while(K_READ_REG(Info_Fifo_Depth) > 0) {
		;
	}
}

static int kyouko2_probe(struct pci_dev *pci_dev, 
			const struct pci_device_id *pci_id)
{
	if (pci_enable_device(pci_dev)) {
		return -EINVAL;
	}
	pci_set_master(pci_dev);

	card.p_control_base = pci_resource_start(pci_dev, 1);
	card.p_control_len = pci_resource_len(pci_dev, 1);
	card.p_framebuffer_base = pci_resource_start(pci_dev, 2);
	card.p_framebuffer_len = pci_resource_len(pci_dev, 2);
	card.pci_dev = pci_dev;
	
	return 0;
}

static long case_graphics_on(void)
{
        float red = 0.5;
	float green = 0.5;
	float blue = 0.4;
        float alpha = 1;
	PRINT("in case_graphics_on");
	K_WRITE_REG(Frame_Columns, 1024);
	K_WRITE_REG(Frame_Rows, 768);
	K_WRITE_REG(Frame_RowPitch, 1024*4);
	K_WRITE_REG(Frame_Pixel_Format, 0xf888);
	K_WRITE_REG(Frame_Start_Address, 0);

	K_WRITE_REG(Encoder_Width, 1024);
	K_WRITE_REG(Encoder_Hight, 768);
	K_WRITE_REG(Encoder_OffsetX, 0);
	K_WRITE_REG(Encoder_OffsetY, 0);
	K_WRITE_REG(Encoder_Frame, 0);

	K_WRITE_REG(Config_Accelleration, 0x40000000);
	K_WRITE_REG(Config_ModeSet, 0x1);

	while (K_READ_REG(Info_Fifo_Depth) != 0)
		;

	K_WRITE_REG(Draw_Clear_Color4f, *(unsigned int *)&red);
	K_WRITE_REG(Draw_Clear_Color4f + 4, *(unsigned int *)&green);
	K_WRITE_REG(Draw_Clear_Color4f + 8, *(unsigned int *)&blue);
	K_WRITE_REG(Draw_Clear_Color4f + 0xc, *(unsigned int *)&alpha);
		
	K_WRITE_REG(Raster_Flush, 0x1);
	
	while (K_READ_REG(Info_Fifo_Depth) != 0) 
		;
	K_WRITE_REG(Raster_Clear, 0x1);

	return 0;

}

static void case_start_dma(void) // to fill the ring buffer
{
	spin_lock_irqsave(&card.lock, card.flag);
	printk(KERN_ALERT "byte_count = %ld\n", card.dma[card.drain].byte_count);

	if (card.drain == card.fill) {
		card.fill = (card.fill + 1) % DMA_BUFFER_NUM;
		spin_unlock_irqrestore(&card.lock, card.flag);
		K_WRITE_REG(BUFFERA_ADDR, card.dma[card.drain].dma_handle);
		K_WRITE_REG(BUFFERA_CONFIG, card.dma[card.drain].byte_count);
		card.drain = (card.drain + 1) % DMA_BUFFER_NUM;
		return ;
	}
	card.fill = (card.fill + 1) % DMA_BUFFER_SIZE;

	if (card.fill == card.drain) { // after add 1, the ring buffer is full
		card.is_full = true;
	}

	spin_unlock_irqrestore(&card.lock, card.flag);
	wait_event_interruptible(waitq, (card.is_full == false));

}

static irqreturn_t kyouko2_irq_intr(int irq, void* dev)
{
	//unsigned int flags;
	printk(KERN_ALERT "in kyouko2_irq_intr\n");
	card.flag = K_READ_REG(0x4008);
	K_WRITE_REG(0x4008, 0xf);  // reset any/all interrupts

	if ((card.flag & 0x02) == 0)
		return (IRQ_NONE);

	spin_lock_irqsave(&card.lock, card.flag);

	if (card.drain == card.fill) {
		wake_up_interruptible(&waitq);
	}

	card.drain = (card.drain + 1) % DMA_BUFFER_NUM;
	K_WRITE_REG(BUFFERA_ADDR, card.dma[card.drain].dma_handle);
	K_WRITE_REG(BUFFERA_CONFIG, card.dma[card.drain].byte_count);

	/*K_WRITE_REG(BUFFERA_ADDR, card.dma[card.drain].dma_handle);
	K_WRITE_REG(BUFFERA_CONFIG, card.dma[card.drain].byte_count);
	card.drain = (card.drain + 1) % DMA_BUFFER_NUM;

	if (card.is_full == true) {
		card.is_full = false;
		wake_up_interruptible(&waitq);
	}*/

	spin_unlock_irqrestore(&card.lock, card.flag);

	return IRQ_HANDLED;
}

static long kyouko2_ioctl(struct file *file, unsigned int cmd,
			  unsigned long arg)
{
	int i;
	unsigned long count;
	switch (cmd) {
	case VMODE:
		if (arg == GRAPHICS_ON) {
			return case_graphics_on();
		}
		else if (arg == GRAPHICS_OFF) {
			sync();
			K_WRITE_REG(Config_Reboot, 0x1);
		}
		break;
	case BIND_DMA:
		bind_dma_flag = 1;
		for (i = 0; i < DMA_BUFFER_NUM; i++) {
			card.dma[i].k_dma_addr = pci_alloc_consistent(
						card.pci_dev, DMA_BUFFER_SIZE, 
						&card.dma[i].dma_handle);

			card.fill = i; // for kyouko2_mmap do io_remap_pfn_range

			card.dma[i].u_dma_addr = do_mmap(file, 
						0, 
						DMA_BUFFER_SIZE,
					       	PROT_READ|PROT_WRITE, MAP_SHARED, 0);
		}
		bind_dma_flag = 0;
		/*PRINT("k_dma_addr\tdma_handle\tu_dma_addr\n");
		for (i = 0; i < DMA_BUFFER_NUM; i++) {
			printk(KERN_ALERT "[%d]%lx\t%lx\t%lx\n", i, 
			       ((long unsigned int)card.dma[i].k_dma_addr),
			   	((long unsigned int )card.dma[i].dma_handle),
				((long unsigned int)card.dma[i].u_dma_addr));
		}*/

		card.fill = 0;
		card.drain = 0;
		card.is_full = false;

		// handle irq
		pci_enable_msi(card.pci_dev);
		if (request_irq(card.pci_dev->irq, (irq_handler_t)kyouko2_irq_intr, 
				IRQF_SHARED|IRQF_DISABLED, "kyouko2_irq_handler",
				&card)) {
			printk(KERN_ALERT "error in request_irq\n");
			return -1;
		}

		K_WRITE_REG(0x100c, 0x20);

		if (copy_to_user((unsigned long *)arg, 
			     &card.dma[card.drain].u_dma_addr,
			     sizeof(unsigned long)) != 0) {
			return -1;
		}

		break;
	case START_DMA:
		if (copy_from_user(&count, (unsigned long*)arg, 
				   sizeof(unsigned long)) != 0) {
			return -1;
		}	
		
		card.dma[card.fill].byte_count = count;
		printk(KERN_ALERT "drain = %d, fill = %d\n", card.drain, card.fill);
		case_start_dma();

		if (copy_to_user((unsigned long*)arg, 
			     &card.dma[card.fill].u_dma_addr,
			     sizeof(unsigned long)) != 0) {
			return -1;
		}

		break;
	case SYNC:
		sync();
		break;
	}
	return 0;
}


static int kyouko2_open(struct inode *inode, struct file *fp)
{
	unsigned int ram;
	PRINT("Function: kyouko2_open");
	card.k_control_base = ioremap(card.p_control_base, card.p_control_len);
	ram = K_READ_REG(0x0020);
	ram *= 1024 * 1024;
	card.k_framebuffer_base = ioremap(card.p_framebuffer_base, ram);
	return 0;
}

static int kyouko2_release(struct inode* inode, struct file *fp)
{
	int i = 0;
	PRINT("Function: kyouko2_release");
	for (i = 0; i < DMA_BUFFER_NUM; i++) {
		pci_free_consistent(card.pci_dev, DMA_BUFFER_SIZE, 
			    card.dma[i].k_dma_addr, card.dma[i].dma_handle);
		do_munmap(current->mm, card.dma[i].u_dma_addr, DMA_BUFFER_SIZE);
	}

	free_irq(card.pci_dev->irq, &card);
	pci_disable_msi(card.pci_dev);
	iounmap(card.k_control_base);
	iounmap(card.k_framebuffer_base);
	return 0;
}

static void kyouko2_remove(struct pci_dev *dev)
{
	pci_disable_device(dev);
	pci_clear_master(dev); 
}

static int kyouko2_mmap(struct file *fp, struct vm_area_struct *vma)
{

	if (bind_dma_flag == 0) {
		if (vma->vm_pgoff == 0x0) {
			io_remap_pfn_range(vma, vma->vm_start, 
			       (card.p_control_base >> PAGE_SHIFT),
			   (vma->vm_end - vma->vm_start), vma->vm_page_prot);
		}
        	else if(vma->vm_pgoff == 0x80000){
			io_remap_pfn_range(vma, vma->vm_start, 
				(card.p_framebuffer_base >> PAGE_SHIFT),
				(vma->vm_end - vma->vm_start), vma->vm_page_prot);
		}
	}
	else {
		io_remap_pfn_range(vma, vma->vm_start,
				   (card.dma[card.fill].dma_handle >> PAGE_SHIFT),
				   (vma->vm_end - vma->vm_start), 
				   vma->vm_page_prot);
	}
	return 0;
}

struct file_operations kyouko2_fops = {
	.open = kyouko2_open,
	.release = kyouko2_release,
	.mmap = kyouko2_mmap,
	.unlocked_ioctl = kyouko2_ioctl,
	.owner = THIS_MODULE
};

struct pci_driver kyouko2_pci_driver = {
	.name = "kyouko2",
	.id_table = kyouko2_pci_tbl,
	.probe = kyouko2_probe,
	.remove = kyouko2_remove
};

struct cdev whatever;

static int __init my_init_function(void) 
{

	cdev_init(&whatever, &kyouko2_fops);
	cdev_add(&whatever, MKDEV(500, 127), 1);
	if (pci_register_driver(&kyouko2_pci_driver)) {
		return -EINVAL;
	}

	printk(KERN_ALERT "my_init_function, kyouko2 is added\n");
	return 0;
}

static void __exit my_exit_function(void)
{
	cdev_del(&whatever);
	pci_unregister_driver(&kyouko2_pci_driver);
	printk("kyouto2 is removed\n");
}

module_init(my_init_function);
module_exit(my_exit_function);
