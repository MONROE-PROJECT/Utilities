/*
 * Linux 2.6 and 3.x driver for PCEngines APU push-button switch
 *
 * This is a simple character driver that will allow the state of the
 * APU push-button switch to be read from userland.  The driver is
 * intended for easy use in scripts.  The driver will create the
 * device node /dev/apu_button_s1.  Reading the device node, for example
 * "cat /dev/apu_button_s1" will return the string "1\n" or "0\n".
 *
 * The switch state is actually read when the device node is opened.
 * The driver will return the result string once, then indicate an
 * end-of-file condition.
 *
 * Copyright (c) 2014, Mark Schank
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define DEVICE_NAME "s1"
#define CLASS_NAME "apu_button"
#define GPIOADDR	(0xFED801BB)		// Address of GPIO 187

#include <linux/device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/mutex.h>
#include <asm/uaccess.h>
#include <asm/io.h>

static struct class*  my_class  = NULL;
static struct device* my_device = NULL;
static int my_major;

static char read_buf[16];
static size_t  read_len;
static size_t  read_ix;
static unsigned int *b1;

static DEFINE_MUTEX(my_mutex);

static int my_device_open(struct inode* inode, struct file* filp)
{
	u8 curr_state;

	/* Only allow read access */
	if(((filp->f_flags & O_ACCMODE) == O_WRONLY) || ((filp->f_flags & O_ACCMODE) == O_RDWR)){
		return -EACCES;
	}

	/* Prevent concurrent accesses. */
	 if(!mutex_trylock(&my_mutex)){
		 return -EBUSY;
	 }

	/* Read the switch state and buffer the response. */
	 curr_state = ioread8(b1);
	 read_len = sprintf(read_buf, "%d\n", ((curr_state & (1 << 7)) != (1 << 7)));
	 read_ix = 0;

	 return 0;
}

static int my_device_close(struct inode* inode, struct file* filp)
{
	/* Release the lock */
	mutex_unlock(&my_mutex);
	return 0;
}

static ssize_t my_device_read(struct file *filp, char *buf, size_t count, loff_t *f_pos)
{
	size_t rlen;

	/* Has the response been read? */
	if(read_ix >= read_len)
		return(0);

	/* limit to requested size */
	if(count < read_len)
		rlen = count;
	else
		rlen = read_len;

	/* Copy response to user's buffer */
	if(copy_to_user(buf, &read_buf[read_ix], rlen))
		return -EFAULT;

	read_ix += rlen;

	return sizeof(rlen);
}

static struct file_operations fops = {
	.read = my_device_read,
	.open = my_device_open,
	.release = my_device_close
};

static int __init apubutton_module_init(void)
{
	int ret;
	u8 curr_state;
	
	my_major = register_chrdev(0, DEVICE_NAME, &fops);
	if(my_major < 0) {
		printk(KERN_ERR "apu_button register_chrdev() failed with %d\n", my_major);
		return my_major;
	}

	my_class = class_create(THIS_MODULE, CLASS_NAME);
	if(IS_ERR(my_class)) {
		ret = PTR_ERR(my_class);
		printk(KERN_ERR "apu_button class_create() failed\n");
		unregister_chrdev(my_major, DEVICE_NAME);
		return ret;
	}

	my_device = device_create(my_class, NULL, MKDEV(my_major, 0), NULL, CLASS_NAME "_" DEVICE_NAME);
	if(IS_ERR(my_device)) {
		ret = PTR_ERR(my_device);
		printk(KERN_ERR "apu_button device_create() failed\n");
		class_unregister(my_class);
		class_destroy(my_class);
		unregister_chrdev(my_major, DEVICE_NAME);
		return ret;
	}

	mutex_init(&my_mutex);
	b1 = ioremap(GPIOADDR, 1);

	/* Set GPIO as an input.  This should have already been
	 * done by the bios
	 */
	curr_state = ioread8(b1);
	iowrite8(curr_state | (1 << 5), b1);

	return 0;
}

static void __exit apubutton_module_exit(void)
{
	device_destroy(my_class, MKDEV(my_major, 0));
	class_unregister(my_class);
	class_destroy(my_class);
	unregister_chrdev(my_major, DEVICE_NAME);
}

module_init(apubutton_module_init);
module_exit(apubutton_module_exit);

MODULE_AUTHOR("M. Schank");
MODULE_DESCRIPTION("PCengines APU push-button switch driver");
MODULE_VERSION("0.5");
MODULE_LICENSE("GPL");
