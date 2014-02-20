#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
 .name = KBUILD_MODNAME,
 .init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
 .exit = cleanup_module,
#endif
 .arch = MODULE_ARCH_INIT,
};

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0xf176b55a, "module_layout" },
	{ 0xd6b8e852, "request_threaded_irq" },
	{ 0x1b03bfb7, "pci_enable_msi_block" },
	{ 0x8bf35476, "dma_ops" },
	{ 0x290cfad1, "do_mmap_pgoff" },
	{ 0x2925f10f, "x86_dma_fallback_dev" },
	{ 0x71de9b3f, "_copy_to_user" },
	{ 0x77e2f33, "_copy_from_user" },
	{ 0xa1c76e0a, "_cond_resched" },
	{ 0xcf21d241, "__wake_up" },
	{ 0xfa66f77c, "finish_wait" },
	{ 0x5c8b5ce8, "prepare_to_wait" },
	{ 0x1000e51, "schedule" },
	{ 0x9a839442, "current_task" },
	{ 0xc8b57c27, "autoremove_wake_function" },
	{ 0x8f64aa4, "_raw_spin_unlock_irqrestore" },
	{ 0x9327f5ce, "_raw_spin_lock_irqsave" },
	{ 0xeae3dfd6, "__const_udelay" },
	{ 0x42c8de35, "ioremap_nocache" },
	{ 0xd782157e, "remap_pfn_range" },
	{ 0x5ab68bf2, "__pci_register_driver" },
	{ 0x558a1061, "cdev_add" },
	{ 0x10816b80, "cdev_init" },
	{ 0x27e1a049, "printk" },
	{ 0xc19f373b, "cdev_del" },
	{ 0xfb8ce836, "pci_unregister_driver" },
	{ 0x6b706957, "pci_set_master" },
	{ 0x2ba3870b, "pci_enable_device" },
	{ 0xb4390f9a, "mcount" },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";


MODULE_INFO(srcversion, "9E64ED7167D255AAC766F54");
