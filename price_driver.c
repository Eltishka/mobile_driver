#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/random.h>   // Нужен для get_random_u32
#include <linux/slab.h>

#define DEVICE_NAME "price_feed"
#define CLASS_NAME "price_class"
#define BUFFER_SIZE 64

MODULE_LICENSE("GPL");
MODULE_AUTHOR("team");
MODULE_DESCRIPTION("A simple Linux char driver for simulating price volatility");
MODULE_VERSION("0.1");

static dev_t dev_number;
static struct cdev c_dev;
static struct class *dev_class;
static struct device *dev_device;

static int driver_open(struct inode *inode, struct file *file);
static int driver_release(struct inode *inode, struct file *file);
static ssize_t driver_read(struct file *file, char __user *buf, size_t count, loff_t *offset);

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = driver_open,
    .release = driver_release,
    .read = driver_read,
};

static int driver_open(struct inode *inode, struct file *file) {
    printk(KERN_INFO "price_feed: Device opened\n");
    return 0;
}

static int driver_release(struct inode *inode, struct file *file) {
    printk(KERN_INFO "price_feed: Device closed\n");
    return 0;
}

static ssize_t driver_read(struct file *file, char __user *buf, size_t count, loff_t *offset) {
    char kernel_buffer[BUFFER_SIZE];
    int delta;
    int bytes_to_copy;
    int ret;
    msleep(500);
    
    delta = (get_random_u32() % 2001) - 1000;

    int len = snprintf(kernel_buffer, BUFFER_SIZE, "%d\n", delta);

    if (count < len) {
        bytes_to_copy = count;
    } else {
        bytes_to_copy = len;
    }

    ret = copy_to_user(buf, kernel_buffer, bytes_to_copy);
    if (ret) {
        printk(KERN_ERR "price_feed: Failed to send data to user\n");
        return -EFAULT;
    }

    printk(KERN_INFO "price_feed: Sent delta %d to user\n", delta);
    return bytes_to_copy;
}

static int __init price_driver_init(void) {
    int ret;

    printk(KERN_INFO "price_feed: Initializing driver...\n");

    ret = alloc_chrdev_region(&dev_number, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        printk(KERN_ERR "price_feed: Failed to allocate major number\n");
        return ret;
    }
    printk(KERN_INFO "price_feed: Registered with Major %d\n", MAJOR(dev_number));

    cdev_init(&c_dev, &fops);
    c_dev.owner = THIS_MODULE;

    ret = cdev_add(&c_dev, dev_number, 1);
    if (ret < 0) {
        unregister_chrdev_region(dev_number, 1);
        printk(KERN_ERR "price_feed: Failed to add cdev\n");
        return ret;
    }

   
    dev_class = class_create(CLASS_NAME);
    if (IS_ERR(dev_class)) {
        cdev_del(&c_dev);
        unregister_chrdev_region(dev_number, 1);
        printk(KERN_ERR "price_feed: Failed to create class\n");
        return PTR_ERR(dev_class);
    }

    dev_device = device_create(dev_class, NULL, dev_number, NULL, DEVICE_NAME);
    if (IS_ERR(dev_device)) {
        class_destroy(dev_class);
        cdev_del(&c_dev);
        unregister_chrdev_region(dev_number, 1);
        printk(KERN_ERR "price_feed: Failed to create device\n");
        return PTR_ERR(dev_device);
    }

    printk(KERN_INFO "price_feed: Driver initialized successfully\n");
    return 0;
}

static void __exit price_driver_exit(void) {
    device_destroy(dev_class, dev_number);
    class_destroy(dev_class);
    cdev_del(&c_dev);
    unregister_chrdev_region(dev_number, 1);
    printk(KERN_INFO "price_feed: Driver unloaded\n");
}

module_init(price_driver_init);
module_exit(price_driver_exit);

// out in float type, kotirovki Google... ... ... with init list of kotir, beginning settings, float, kotirovka ... count 