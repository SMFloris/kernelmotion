#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

__visible struct module __this_module
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
	{ 0x151b2616, __VMLINUX_SYMBOL_STR(module_layout) },
	{ 0x939ec4ef, __VMLINUX_SYMBOL_STR(noop_llseek) },
	{ 0x7362b4aa, __VMLINUX_SYMBOL_STR(usb_deregister) },
	{ 0xe260146, __VMLINUX_SYMBOL_STR(usb_register_driver) },
	{ 0x3931fc6b, __VMLINUX_SYMBOL_STR(usb_unanchor_urb) },
	{ 0x6fde2a01, __VMLINUX_SYMBOL_STR(usb_anchor_urb) },
	{ 0x4f6b400b, __VMLINUX_SYMBOL_STR(_copy_from_user) },
	{ 0x44171dff, __VMLINUX_SYMBOL_STR(usb_alloc_coherent) },
	{ 0x156a8a59, __VMLINUX_SYMBOL_STR(down_trylock) },
	{ 0x6dc0c9dc, __VMLINUX_SYMBOL_STR(down_interruptible) },
	{ 0x1b1316d9, __VMLINUX_SYMBOL_STR(usb_kill_urb) },
	{ 0x24f45195, __VMLINUX_SYMBOL_STR(usb_wait_anchor_empty_timeout) },
	{ 0x4395953e, __VMLINUX_SYMBOL_STR(usb_kill_anchored_urbs) },
	{ 0x3ffc98bb, __VMLINUX_SYMBOL_STR(usb_deregister_dev) },
	{ 0xfaa86f2e, __VMLINUX_SYMBOL_STR(usb_autopm_put_interface) },
	{ 0x4d1e385f, __VMLINUX_SYMBOL_STR(mutex_lock) },
	{ 0xb93784d7, __VMLINUX_SYMBOL_STR(usb_register_dev) },
	{ 0x1c631c3a, __VMLINUX_SYMBOL_STR(usb_control_msg) },
	{ 0xdf4d3f54, __VMLINUX_SYMBOL_STR(usb_lock_device_for_reset) },
	{ 0x61c4d1f7, __VMLINUX_SYMBOL_STR(usb_get_dev) },
	{ 0x797d82d4, __VMLINUX_SYMBOL_STR(usb_alloc_urb) },
	{ 0xa202a8e5, __VMLINUX_SYMBOL_STR(kmalloc_order_trace) },
	{ 0x9e88526, __VMLINUX_SYMBOL_STR(__init_waitqueue_head) },
	{ 0xc7e65311, __VMLINUX_SYMBOL_STR(__mutex_init) },
	{ 0xdd626e59, __VMLINUX_SYMBOL_STR(kmem_cache_alloc_trace) },
	{ 0xbe84a3b9, __VMLINUX_SYMBOL_STR(kmalloc_caches) },
	{ 0xf08242c2, __VMLINUX_SYMBOL_STR(finish_wait) },
	{ 0xcd48420c, __VMLINUX_SYMBOL_STR(mutex_unlock) },
	{ 0x4f8b5ddb, __VMLINUX_SYMBOL_STR(_copy_to_user) },
	{ 0x1000e51, __VMLINUX_SYMBOL_STR(schedule) },
	{ 0x2207a57f, __VMLINUX_SYMBOL_STR(prepare_to_wait_event) },
	{ 0xa1c76e0a, __VMLINUX_SYMBOL_STR(_cond_resched) },
	{ 0xdb7305a1, __VMLINUX_SYMBOL_STR(__stack_chk_fail) },
	{ 0xe8e767d7, __VMLINUX_SYMBOL_STR(mutex_lock_interruptible) },
	{ 0x4c9c6dbc, __VMLINUX_SYMBOL_STR(usb_submit_urb) },
	{ 0xe5815f8a, __VMLINUX_SYMBOL_STR(_raw_spin_lock_irq) },
	{ 0xa6bbd805, __VMLINUX_SYMBOL_STR(__wake_up) },
	{ 0x78e739aa, __VMLINUX_SYMBOL_STR(up) },
	{ 0x680dfe56, __VMLINUX_SYMBOL_STR(usb_free_coherent) },
	{ 0xe259ae9e, __VMLINUX_SYMBOL_STR(_raw_spin_lock) },
	{ 0x151461d1, __VMLINUX_SYMBOL_STR(dev_err) },
	{ 0x16305289, __VMLINUX_SYMBOL_STR(warn_slowpath_null) },
	{ 0xc14c3e54, __VMLINUX_SYMBOL_STR(usb_autopm_get_interface) },
	{ 0x9fbb8914, __VMLINUX_SYMBOL_STR(usb_find_interface) },
	{ 0x27e1a049, __VMLINUX_SYMBOL_STR(printk) },
	{ 0x37a0cba, __VMLINUX_SYMBOL_STR(kfree) },
	{ 0xa8f7747d, __VMLINUX_SYMBOL_STR(usb_put_dev) },
	{ 0x815092fd, __VMLINUX_SYMBOL_STR(usb_free_urb) },
	{ 0xbdfb6dbb, __VMLINUX_SYMBOL_STR(__fentry__) },
	{ 0x78764f4e, __VMLINUX_SYMBOL_STR(pv_irq_ops) },
	{ 0x6bf1c17f, __VMLINUX_SYMBOL_STR(pv_lock_ops) },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";

MODULE_ALIAS("usb:vF182p0003d*dc*dsc*dp*ic*isc*ip*in*");

MODULE_INFO(srcversion, "3CB4383CDC46D3B3DDCECBA");
