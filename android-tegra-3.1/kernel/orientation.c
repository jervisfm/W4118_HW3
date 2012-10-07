#include <linux/orientation.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/syscalls.h>

void print_orientation(struct dev_orientation orient)
{
	printk("Azimuth: %d\n", orient.azimuth);
	printk("Pitch: %d\n", orient.pitch);
	printk("Roll: %d\n", orient.roll);
}

SYSCALL_DEFINE1(set_orientation, struct dev_orientation*, orient)
{
	if (copy_from_user(&k_orient, orient,
				sizeof(struct dev_orientation)) != 0)
		return -EFAULT;

	print_orientation(k_orient);

	return 0;
}
