#include <linux/module.h>
#include <linux/export-internal.h>
#include <linux/compiler.h>

MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};



static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0x2f5de9ec, "param_ops_uint" },
	{ 0xedc03953, "iounmap" },
	{ 0xaf56600a, "arm64_use_ng_mappings" },
	{ 0xb3f7646e, "kthread_should_stop" },
	{ 0x20bc8744, "wake_up_process" },
	{ 0x92997ed8, "_printk" },
	{ 0xd45b3bc6, "kthread_stop" },
	{ 0xcb661d89, "kthread_create_on_node" },
	{ 0x40863ba1, "ioremap_prot" },
	{ 0xf9a482f9, "msleep" },
	{ 0x474e54d2, "module_layout" },
};

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "525C06A224FC505F7CF9EBF");
