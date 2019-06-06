#include <linux/init.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
#include "../arch/arm/mach-mx28/mx28_pins.h"


#define LED_DEV_MINORS      1
#define LED_DEV_MAJOR       0
#define LED_DEV_NAME        "led"
#define LED_PIN             MXS_PIN_TO_GPIO(PINID_LCD_D23)

unsigned long led_dev_major = LED_DEV_MAJOR;

typedef struct {
    struct cdev cdev;
} led_dev_t;

static int led_open(struct inode* inode, struct file* file)
{
    int ret = gpio_request(LED_PIN, "LED");
    if (ret < 0)
    {
        printk("gpio request failed.\n");
        //return -1;
    }
        
    return 0;
}

static int led_release(struct inode* inode, struct file* file)
{
    gpio_free(LED_PIN);
    return 0;
}

static int led_read(struct file* file, char __user* rd_data, size_t rd_len, loff_t* offset)
{
    int level = gpio_direction_input(LED_PIN);
    put_user(level, rd_data);
    return 0;
}

static int led_write(struct file* file, char __user* wr_data, size_t wr_len, loff_t* offset)
{
    int level = 0;
    get_user(level, wr_data);
    printk("led write level = %d\n", level);
    gpio_direction_output(LED_PIN, level);
    return 0;
}

static int led_unlocked_ioctl(struct file* file, unsigned int cmd, unsigned long arg)
{
    printk("ioctl cmd=%d\n", cmd);
    switch (cmd)
    {
    case 0:
        gpio_direction_output(LED_PIN, 0);
        break;
    case 1:
        gpio_direction_output(LED_PIN, 1);
        break;
    }
    return 0;
}

const struct file_operations led_ops = {
    .owner = THIS_MODULE,
    .open = led_open,
    .release = led_release,
    .read = led_read,
    .write = led_write,
    .unlocked_ioctl = led_unlocked_ioctl,
};

led_dev_t* led_dev = NULL;

int __init led_init(void)
{
    int i = 0;
    int ret = 0;
    dev_t devno;

    led_dev = kzalloc(sizeof(led_dev_t) * LED_DEV_MINORS, GFP_KERNEL);
    
    if (led_dev_major)
    {
        devno = MKDEV(led_dev_major, 0);
        register_chrdev_region(devno, LED_DEV_MINORS, LED_DEV_NAME);
    }
    else
    {
        alloc_chrdev_region(&devno, 0, LED_DEV_MINORS, LED_DEV_NAME);
    }

    led_dev_major = MAJOR(devno);

    for (i = 0; i < LED_DEV_MINORS; i++)
    {
        cdev_init(&led_dev[i].cdev, &led_ops);
        devno = MKDEV(led_dev_major, i);
        if ((ret = cdev_add(&led_dev[i].cdev, devno, 1)) != 0)
        {
            printk("cdev_add failed\n");
            return ret;
        }
    }

    return 0;
}

void __exit led_exit(void)
{
    int i = 0;

    for (i = 0; i < LED_DEV_MINORS; i++)
    {
        cdev_del(&led_dev[i].cdev);
    }
    dev_t devno = MKDEV(led_dev_major, 0);
    unregister_chrdev_region(devno, LED_DEV_MINORS);
}

module_init(led_init);
module_exit(led_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Arno");