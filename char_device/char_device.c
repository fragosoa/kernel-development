#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>        /* file_operations, register_chrdev_region */
#include <linux/cdev.h>      /* cdev struct */
#include <linux/device.h>    /* class_create, device_create */
#include <linux/uaccess.h>   /* copy_to_user, copy_from_user */
#include <linux/string.h>

#define DEVICE_NAME "chardev"
#define CLASS_NAME  "chardev_class"
#define BUFFER_SIZE 1024

/* --- Global state --- */

static dev_t dev_number;           /* will hold our major/minor number */
static struct cdev my_cdev;        /* represents our character device */
static struct class *my_class;     /* used to create /dev/chardev automatically */
static struct device *my_device;

/* Internal message buffer — this is our "notepad" */
static char message[BUFFER_SIZE];
static int  message_len = 0;

/* ------------------------------------------------------------------ */
/*  File operations — these are called when user does open/read/write  */
/* ------------------------------------------------------------------ */

static int dev_open(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "chardev: device opened\n");
    return 0;
}

static int dev_release(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "chardev: device closed\n");
    return 0;
}

/*
 * Called when a user runs: cat /dev/chardev
 * We copy our internal message buffer to user space.
 */
static ssize_t dev_read(struct file *file, char __user *user_buf,
                         size_t count, loff_t *offset)
{
    int bytes_to_send;

    /* If the user already read everything, signal end-of-file */
    if (*offset >= message_len)
        return 0;

    bytes_to_send = message_len - *offset;
    if (bytes_to_send > count)
        bytes_to_send = count;

    /* copy_to_user(destination, source, size) — never use memcpy for this */
    if (copy_to_user(user_buf, message + *offset, bytes_to_send))
        return -EFAULT;

    *offset += bytes_to_send;
    printk(KERN_INFO "chardev: sent %d bytes to user\n", bytes_to_send);
    return bytes_to_send;
}

/*
 * Called when a user runs: echo "hello" > /dev/chardev
 * We copy the data from user space into our internal buffer.
 */
static ssize_t dev_write(struct file *file, const char __user *user_buf,
                          size_t count, loff_t *offset)
{
    int bytes_to_store = count;

    if (bytes_to_store > BUFFER_SIZE - 1)
        bytes_to_store = BUFFER_SIZE - 1;

    /* copy_from_user(destination, source, size) */
    if (copy_from_user(message, user_buf, bytes_to_store))
        return -EFAULT;

    message[bytes_to_store] = '\0';  /* null-terminate the string */
    message_len = bytes_to_store;

    printk(KERN_INFO "chardev: received %d bytes from user: %s\n",
           bytes_to_store, message);
    return bytes_to_store;
}

/* Map the operations above to the kernel's file_operations struct */
static struct file_operations fops = {
    .owner   = THIS_MODULE,
    .open    = dev_open,
    .release = dev_release,
    .read    = dev_read,
    .write   = dev_write,
};

/* ------------------------------------------------------------------ */
/*  Module init — runs when you do: sudo insmod char_device.ko         */
/* ------------------------------------------------------------------ */

static int __init chardev_init(void)
{
    int ret;

    /* Step 1: Ask the kernel for a major/minor device number */
    ret = alloc_chrdev_region(&dev_number, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        printk(KERN_ERR "chardev: failed to allocate device number\n");
        return ret;
    }
    printk(KERN_INFO "chardev: registered with major=%d minor=%d\n",
           MAJOR(dev_number), MINOR(dev_number));

    /* Step 2: Initialize and register the cdev with our file operations */
    cdev_init(&my_cdev, &fops);
    my_cdev.owner = THIS_MODULE;

    ret = cdev_add(&my_cdev, dev_number, 1);
    if (ret < 0) {
        unregister_chrdev_region(dev_number, 1);
        printk(KERN_ERR "chardev: failed to add cdev\n");
        return ret;
    }

    /* Step 3: Create a device class (shows up under /sys/class/) */
    my_class = class_create(CLASS_NAME);
    if (IS_ERR(my_class)) {
        cdev_del(&my_cdev);
        unregister_chrdev_region(dev_number, 1);
        printk(KERN_ERR "chardev: failed to create class\n");
        return PTR_ERR(my_class);
    }

    /* Step 4: Create the /dev/chardev file automatically */
    my_device = device_create(my_class, NULL, dev_number, NULL, DEVICE_NAME);
    if (IS_ERR(my_device)) {
        class_destroy(my_class);
        cdev_del(&my_cdev);
        unregister_chrdev_region(dev_number, 1);
        printk(KERN_ERR "chardev: failed to create device\n");
        return PTR_ERR(my_device);
    }

    printk(KERN_INFO "chardev: device created at /dev/%s\n", DEVICE_NAME);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Module exit — runs when you do: sudo rmmod char_device             */
/* ------------------------------------------------------------------ */

static void __exit chardev_exit(void)
{
    /* Clean up in reverse order of how we set things up */
    device_destroy(my_class, dev_number);
    class_destroy(my_class);
    cdev_del(&my_cdev);
    unregister_chrdev_region(dev_number, 1);
    printk(KERN_INFO "chardev: module unloaded\n");
}

module_init(chardev_init);
module_exit(chardev_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("beginner");
MODULE_DESCRIPTION("A simple character device driver");
