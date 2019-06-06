#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/irqreturn.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include "../arch/arm/mach-mx28/mx28_pins.h"


#define BUTTON_DEV_MAJOR           0

#define BUTTON_DEV_NAME            "button"
#define BUTTON1_PIN                MXS_PIN_TO_GPIO(PINID_LCD_D17)
#define BUTTON2_PIN                MXS_PIN_TO_GPIO(PINID_LCD_D18)
#define BUTTON3_PIN                MXS_PIN_TO_GPIO(PINID_SSP0_DATA4)
#define BUTTON4_PIN                MXS_PIN_TO_GPIO(PINID_SSP0_DATA5)
#define BUTTON5_PIN                MXS_PIN_TO_GPIO(PINID_SSP0_DATA6)

#define BUTTON_BUFSZ               20

typedef struct {
    int index;
    const char* name;
    unsigned long gpio;
    int irq;
} imx_button_t;

typedef enum {
    BUTTON_RELEASED = 0,
    BUTTON_PRESSED,
    BUTTON_DOWN,
    BUTTON_UP,
} button_status_t;

static int button_dev_major = BUTTON_DEV_MAJOR;

const imx_button_t imx_buttons[] = {
    {.index = 0, .name = "button1", .gpio = BUTTON1_PIN,},
    {.index = 1, .name = "button2", .gpio = BUTTON2_PIN,},
    {.index = 2, .name = "button3", .gpio = BUTTON3_PIN,},
    {.index = 3, .name = "button4", .gpio = BUTTON4_PIN,},
    {.index = 4, .name = "button5", .gpio = BUTTON5_PIN,},
};
#define IMX_BUTTON_NUM         (sizeof(imx_buttons)/sizeof(imx_buttons[0]))

typedef struct {
    button_status_t status;
    imx_button_t* imx_button;
    struct timer_list timer;
    void* private_data;
} button_t;

typedef struct {
    struct cdev cdev;
    button_t buttons[IMX_BUTTON_NUM];
    unsigned char buf[BUTTON_BUFSZ];
    unsigned int head, tail;
} button_dev_t;

void button_timer_callback(unsigned long arg)
{
    button_t* button = (button_t*)arg;
    button_dev_t* button_dev = (button_dev_t*)button->private_data;
    int level = gpio_get_value(button->imx_button->gpio);
    if (button->status == BUTTON_DOWN)
    {
        if (!level)
        {
            button_dev->buf[button_dev->head++] = button->imx_button->index;
            if (button_dev->head == BUTTON_BUFSZ)
                button_dev->head = 0;
            if (button_dev->head == button_dev->tail)
            {
                button_dev->tail++;
                if (button_dev->tail == BUTTON_BUFSZ)
                    button_dev->tail = 0;
            }
            button->status = BUTTON_PRESSED;
 
            printk("%s pressed!\n", button->imx_button->name);
        }
        else
        {
            button->status = BUTTON_UP;
            enable_irq(gpio_to_irq(button->imx_button->gpio));
            return;
        }
        
    }
    else if (button->status == BUTTON_PRESSED)
    {
        if (level)
        {
            button->status = BUTTON_UP;
            printk("%s releasing!\n", button->imx_button->name);
        }
    }
    else if (button->status == BUTTON_UP)
    {
        if (level)
        {
            button->status = BUTTON_RELEASED;
            enable_irq(gpio_to_irq(button->imx_button->gpio));
            printk("%s released!\n", button->imx_button->name);
            return;
        }
    }
    //setup_timer(&button->timer, button_timer_callback, (unsigned long)button);
    button->timer.expires = jiffies + HZ / 100;
    add_timer(&button->timer);
}

static irqreturn_t button_irq(int irq, void* dev_id)
{
    button_t* button = (button_t*)dev_id;
    int level = 0;

    printk("enter button irq\n");
    level = gpio_get_value(button->imx_button->gpio);
    printk("%s level = %d\n",button->imx_button->name, level ? 1 : 0);
    if (!level)
    {
        disable_irq_nosync(gpio_to_irq(button->imx_button->gpio));
        button->status = BUTTON_DOWN;
        
        button->timer.expires = jiffies + HZ / 100;
        add_timer(&button->timer);
    }

    printk("exit button irq\n");
    return IRQ_RETVAL(IRQ_HANDLED);
}

static int button_gpio_init(button_dev_t* button_dev, button_t* button, imx_button_t* imx_button)
{
    int ret = 0;
    int irqno = 0;

    button->private_data = button_dev;
    button->imx_button = imx_button;
    setup_timer(&button->timer, button_timer_callback, (unsigned long)button);
    gpio_free(imx_button->gpio);
    ret = gpio_request(imx_button->gpio, imx_button->name);
    if (ret != 0)
    {
        printk(KERN_ERR "%s gpio init failed.\n", imx_button->name);
        return ret;
    }

    gpio_direction_input(imx_button->gpio);
    irqno = gpio_to_irq(imx_button->gpio);
    printk("request irqno:%d\n", irqno);
    set_irq_type(irqno, IRQF_TRIGGER_FALLING);
    //set_irq_type(irqno, IRQ_TYPE_EDGE_FALLING);
    ret = request_irq(irqno, button_irq, IRQF_DISABLED, imx_button->name, button);
    if (ret != 0)
    {
        printk(KERN_ERR "%s request irq failed!, irq:%d\n", imx_button->name, irqno);
        return ret;
    }

    return 0;
}

static void button_gpio_deinit(button_t* button)
{
    int irqno = gpio_to_irq(button->imx_button->gpio);
    printk("free irqno:%d\n", irqno);
    free_irq(irqno, button);
    gpio_free(button->imx_button->gpio);
}

static int button_open(struct inode* inode, struct file* file)
{
    button_dev_t* button_dev = container_of(inode->i_cdev, button_dev_t, cdev);


    file->private_data = button_dev;

    return 0;
}

static int button_release(struct inode* inode, struct file* file)
{

    return 0;
}

static ssize_t button_read(struct file* file, char __user* rd_data, size_t rd_len, loff_t* offset)
{
    button_dev_t* button_dev = file->private_data;
    int ret = 0;
    while (rd_len)
    {
        if (button_dev->head != button_dev->tail)
        {
            put_user(button_dev->buf[button_dev->tail++], rd_data++);
            if (button_dev->tail >= BUTTON_BUFSZ)
                button_dev->tail = 0;
            ret++;
        }
        else
        {
            break;
        }
        
        rd_len--;
    }


    return ret;
}

const struct file_operations button_ops = {
    .owner = THIS_MODULE,
    .open = button_open,
    .release = button_release,
    .read = button_read,
};

button_dev_t* button_dev = NULL;


int __init button_init(void)
{
    int ret = 0;
    int i = 0;
    printk("enter button init\n");
    dev_t devno = MKDEV(button_dev_major, 0);

    button_dev = kzalloc(sizeof(button_dev_t), GFP_KERNEL);

    if (button_dev_major)
    {
        ret = register_chrdev_region(devno, 1, BUTTON_DEV_NAME);
        if (ret != 0)
        {
            printk(KERN_ERR "register chrdev region failed, major=%d\n", button_dev_major);
            return ret;
        }
    }
    else
    {
        ret = alloc_chrdev_region(&devno, 0, 1, BUTTON_DEV_NAME);
        if (ret != 0)
        {
            printk(KERN_ERR "alloc chrdev region failed\n");
            return ret;
        }
        button_dev_major = MAJOR(devno);
    }
    devno = MKDEV(button_dev_major, 0);

    cdev_init(&button_dev->cdev, &button_ops);
    ret = cdev_add(&button_dev->cdev, devno, 1);
    if (ret < 0)
        goto fail;

    for (i = 0; i < IMX_BUTTON_NUM; i++)
    {
        button_gpio_init(button_dev, button_dev->buttons + i, (imx_button_t*)imx_buttons + i);
    }
    button_dev->head = 0;
    button_dev->tail = 0;

    printk("exit button init\n");
    return 0;
fail:
    unregister_chrdev_region(MKDEV(button_dev_major, 0), 1);
    return ret;
}

void __exit button_exit(void)
{
    int i = 0;
    printk("enter button exit\n");

    for (i = 0; i < IMX_BUTTON_NUM; i++)
    {
        button_gpio_deinit(button_dev->buttons + i);
    }

    cdev_del(&button_dev->cdev);
    
    unregister_chrdev_region(MKDEV(button_dev_major, 0), 1);
    kfree(button_dev);
    printk("exit button exit\n");
}

module_init(button_init);
module_exit(button_exit);

MODULE_AUTHOR("Arno");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("gpio button interrupt module");