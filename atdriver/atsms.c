#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/cdev.h>

/* copy_from_user */
#include <linux/uaccess.h>


#include "n_atsms.h"


#define ATSMS_DBG 1

#ifdef ATSMS_DBG
#define ATSMS_DBGMSG(...) printk(KERN_ALERT __VA_ARGS__)
#else
#define ATSMS_DBGMSG(...)
#endif

struct atsms_dev {
	dev_t dev;
	struct cdev cdev;
};


/**
 * Module param to get major device number
 */
static int majorparam = 0;
static struct atsms_dev atsdev;

/**
 * Openning method for atsms char device
 */
static int ats_open(struct inode *i, struct file *f)
{
	return 0;
}

/**
 * Closing method for atsms char device
 */
static int ats_close(struct inode *i, struct file *f)
{
	return 0;
}

/**
 * Reading method for atsms char device
 */
static ssize_t ats_read(struct file *f, char __user *buf, size_t size,
		loff_t *off)
{
	return 0;
}



/**
 * Writting method for atsms char device
 */
static ssize_t ats_write(struct file *f, const char __user * buf, size_t size,
		loff_t *off)
{
	return size;
}


/**
 * File operation object
 */
static struct file_operations atfops = {
	.owner = THIS_MODULE,
	.open = ats_open,
	.release = ats_close,
	.read = ats_read,
	.write = ats_write,
};

__init static int atsms_init(void)
{
	int error;


	ATSMS_DBGMSG("atsms: module init with major %d\n", majorparam);

	atsld_init();

        if (!majorparam) {
                error = alloc_chrdev_region(&atsdev.dev, 0, 1, "atsms");
        } else {
                atsdev.dev = MKDEV(majorparam, 0);
                error = register_chrdev_region(atsdev.dev, 1, "atsms");
        }

	if (error < 0)
		goto err;

	cdev_init(&atsdev.cdev, &atfops);

	error = cdev_add(&atsdev.cdev, atsdev.dev, 1);
	if (error < 0)
		goto errcdev;

	return 0;


errcdev:
	unregister_chrdev_region(atsdev.dev, 1);
err:
	return error;
}


__exit static void atsms_exit(void)
{
	ATSMS_DBGMSG("atsms: module exit\n");

	cdev_del(&atsdev.cdev);
	unregister_chrdev_region(atsdev.dev, 1);

	atsld_exit();
}

module_param(majorparam, int, S_IRUGO);

module_init(atsms_init);
module_exit(atsms_exit);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Remi Pommarel repk@triplefau.lt");
MODULE_DESCRIPTION("Driver for sending sms over AT-command driven device");

