#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/random.h>
#include <linux/slab.h>
#include <linux/kfifo.h>
#include <linux/mutex.h>

#define DEVICE_NAME "price_feed"
#define CLASS_NAME "price_class"
#define BUFFER_SIZE 4096
#define MAX_TICKERS 256
#define TICKER_LEN 8
#define PRICE_ENTRY_SIZE 12
#define CONFIG_PATH "/etc/price_driver/config.csv"
#define MAX_LINE_LEN 256
#define MAX_DELTA_PERCENT 5

MODULE_LICENSE("GPL");
MODULE_AUTHOR("team");
MODULE_DESCRIPTION("Linux char driver for price feed with CSV config");
MODULE_VERSION("0.2");

typedef struct {
    char ticker[TICKER_LEN];



    long upper_bound;
    long lower_bound;
    long current_price;
} ticker_t;

typedef struct {
    ticker_t *tickers;
    int count;
    struct mutex lock;
} ticker_config_t;

static dev_t dev_number;
static struct cdev c_dev;
static struct class *dev_class;
static struct device *dev_device;
static ticker_config_t ticker_config = {NULL, 0};

static int driver_open(struct inode *inode, struct file *file);
static int driver_release(struct inode *inode, struct file *file);
static ssize_t driver_read(struct file *file, char __user *buf, size_t count, loff_t *offset);
static int load_config_from_csv(const char *path);
static void cleanup_config(void);

static void generate_delta(ticker_t *ticker);
static void pack_price_data(char *buffer, ticker_t *ticker);

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

static void pack_price_data(char *buffer, ticker_t *ticker) {
    int i;
    uint32_t *price_ptr;
    
    for (i = 0; i < TICKER_LEN; i++) {
        if (ticker->ticker[i] != '\0') {
            buffer[i] = ticker->ticker[i];
        } else {
            buffer[i] = '\0';
        }
    }
    
    price_ptr = (uint32_t *)(buffer + TICKER_LEN);

    *price_ptr = (uint32_t)ticker->current_price;
}

static ssize_t pack_all_prices(char *buffer, size_t max_size) {
    int i;
    ssize_t offset = 0;
    
    for (i = 0; i < ticker_config.count; i++) {
        if (offset + PRICE_ENTRY_SIZE > max_size) {
            break;
        }
        
        generate_delta(&ticker_config.tickers[i]);
        pack_price_data(buffer + offset, &ticker_config.tickers[i]);
        
        printk(KERN_DEBUG "price_feed: %s price = %ld\n", 
               ticker_config.tickers[i].ticker, 
               ticker_config.tickers[i].current_price);
        
        offset += PRICE_ENTRY_SIZE;
    }
    
    return offset;
}



static void generate_delta(ticker_t *ticker) {
    long max_delta = ticker->upper_bound - ticker->lower_bound;
    max_delta = (max_delta * MAX_DELTA_PERCENT) / 100;
    
    u32 rand_val = get_random_u32();


    long delta = ((long)(rand_val % 1000) - 500);
    delta = (delta * max_delta) / 500;
    

    long new_price = ticker->current_price + delta;
    
    if (new_price > ticker->upper_bound) {
        new_price = ticker->upper_bound;
    } else if (new_price < ticker->lower_bound) {
        new_price = ticker->lower_bound;
    }
    
    ticker->current_price = new_price;

}

static int load_config_from_csv(const char *path) {
    struct file *fp;
    loff_t file_size;
    char *file_content;
    char *line_start, *line_end;
    int line_count = 0;
    int ret = 0;
    ticker_t *temp_tickers;
    
    fp = filp_open(path, O_RDONLY, 0);
    if (IS_ERR(fp)) {
        printk(KERN_ERR "price_feed: Cannot open config file %s\n", path);
        return -ENOENT;
    }
    
    file_size = i_size_read(file_inode(fp));
    if (file_size <= 0 || file_size > 65536) {
        printk(KERN_ERR "price_feed: Invalid config file size\n");
        filp_close(fp, NULL);
        return -EINVAL;
    }
    
    file_content = kmalloc(file_size + 1, GFP_KERNEL);
    if (!file_content) {
        filp_close(fp, NULL);
        return -ENOMEM;
    }
    
    ret = kernel_read(fp, file_content, file_size, &fp->f_pos);
    if (ret != file_size) {
        printk(KERN_ERR "price_feed: Failed to read config file\n");
        kfree(file_content);
        filp_close(fp, NULL);
        return -EIO;
    }
    file_content[file_size] = '\0';
    filp_close(fp, NULL);
    
    temp_tickers = kmalloc(sizeof(ticker_t) * MAX_TICKERS, GFP_KERNEL);
    if (!temp_tickers) {
        kfree(file_content);
        return -ENOMEM;
    }
    
    line_start = file_content;
    while (line_start && line_count < MAX_TICKERS) {
        line_end = strchr(line_start, '\n');
        if (line_end) {
            *line_end = '\0';
        }
        
        if (line_start[0] != '\0' && line_start[0] != '#') {
            char *token;
            char line_copy[MAX_LINE_LEN];
            char *rest = line_copy;
            int field = 0;
            
            strncpy(line_copy, line_start, MAX_LINE_LEN - 1);
            line_copy[MAX_LINE_LEN - 1] = '\0';
            
            while ((token = strsep(&rest, ",")) && field < 4) {
                char *p = token;
                while (*p == ' ') p++;
                
                if (field == 0) {
                    strncpy(temp_tickers[line_count].ticker, p, TICKER_LEN - 1);
                    temp_tickers[line_count].ticker[TICKER_LEN - 1] = '\0';
                } else if (field == 1) {

                    kstrtol(p, 10, &temp_tickers[line_count].upper_bound);
                } else if (field == 2) {

                    kstrtol(p, 10, &temp_tickers[line_count].lower_bound);
                } else if (field == 3) {

                    kstrtol(p, 10, &temp_tickers[line_count].current_price);
                }
                field++;
            }
            
            if (field == 4) {

                printk(KERN_INFO "price_feed: Loaded %s [%ld-%ld] initial=%ld\n",
                    temp_tickers[line_count].ticker,
                    temp_tickers[line_count].lower_bound,
                    temp_tickers[line_count].upper_bound,
                    temp_tickers[line_count].current_price);
                line_count++;
            }
        }
        
        if (line_end) {
            line_start = line_end + 1;
        } else {
            break;
        }
    }
    
    kfree(file_content);
    
    mutex_lock(&ticker_config.lock);
    if (ticker_config.tickers) {
        kfree(ticker_config.tickers);
    }
    ticker_config.tickers = temp_tickers;
    ticker_config.count = line_count;
    current_ticker_idx = 0;
    mutex_unlock(&ticker_config.lock);
    
    printk(KERN_INFO "price_feed: Loaded %d tickers from config\n", line_count);
    return 0;
}

static void cleanup_config(void) {
    mutex_lock(&ticker_config.lock);
    if (ticker_config.tickers) {
        kfree(ticker_config.tickers);
        ticker_config.tickers = NULL;
    }
    ticker_config.count = 0;
    mutex_unlock(&ticker_config.lock);
}

static ssize_t driver_read(struct file *file, char __user *buf, size_t count, loff_t *offset) {
    char *kernel_buffer;
    ssize_t data_size;
    int ret;
    
    if (count < PRICE_ENTRY_SIZE) {
        return -EINVAL;
    }
    
    kernel_buffer = kmalloc(BUFFER_SIZE, GFP_KERNEL);
    if (!kernel_buffer) {
        printk(KERN_ERR "price_feed: Failed to allocate kernel buffer\n");
        return -ENOMEM;
    }
    
    mutex_lock(&ticker_config.lock);
    
    if (ticker_config.count == 0) {
        mutex_unlock(&ticker_config.lock);
        kfree(kernel_buffer);
        printk(KERN_ERR "price_feed: No tickers configured\n");
        return -ENODATA;
    }
    
    data_size = pack_all_prices(kernel_buffer, BUFFER_SIZE);
    
    printk(KERN_INFO "price_feed: Generated %ld bytes of price data for %d tickers\n", 
           data_size, ticker_config.count);
    
    mutex_unlock(&ticker_config.lock);
    
    if (data_size > count) {
        data_size = count;
    }
    
    ret = copy_to_user(buf, kernel_buffer, data_size);
    kfree(kernel_buffer);
    
    if (ret) {
        printk(KERN_ERR "price_feed: Failed to send data to user\n");
        return -EFAULT;
    }
    
    msleep(100);
    return data_size;
}

static int __init price_driver_init(void) {
    int ret;

    printk(KERN_INFO "price_feed: Initializing driver...\n");
    
    mutex_init(&ticker_config.lock);
    
    ret = load_config_from_csv(CONFIG_PATH);
    if (ret < 0) {
        printk(KERN_WARNING "price_feed: Config not loaded, will use empty config\n");
    }

    ret = alloc_chrdev_region(&dev_number, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        printk(KERN_ERR "price_feed: Failed to allocate major number\n");
        cleanup_config();
        return ret;
    }
    printk(KERN_INFO "price_feed: Registered with Major %d\n", MAJOR(dev_number));

    cdev_init(&c_dev, &fops);
    c_dev.owner = THIS_MODULE;

    ret = cdev_add(&c_dev, dev_number, 1);
    if (ret < 0) {
        unregister_chrdev_region(dev_number, 1);
        cleanup_config();
        printk(KERN_ERR "price_feed: Failed to add cdev\n");
        return ret;
    }

    dev_class = class_create(CLASS_NAME);
    if (IS_ERR(dev_class)) {
        cdev_del(&c_dev);
        unregister_chrdev_region(dev_number, 1);
        cleanup_config();
        printk(KERN_ERR "price_feed: Failed to create class\n");
        return PTR_ERR(dev_class);
    }

    dev_device = device_create(dev_class, NULL, dev_number, NULL, DEVICE_NAME);
    if (IS_ERR(dev_device)) {
        class_destroy(dev_class);
        cdev_del(&c_dev);
        unregister_chrdev_region(dev_number, 1);
        cleanup_config();
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
    cleanup_config();
    printk(KERN_INFO "price_feed: Driver unloaded\n");
}

module_init(price_driver_init);
module_exit(price_driver_exit); 