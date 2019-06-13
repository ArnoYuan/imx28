#include <linux/init.h>
#include <linux/module.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/irqreturn.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include "../arch/arm/mach-mx28/mx28_pins.h"


#define BUTTON1_PIN                MXS_PIN_TO_GPIO(PINID_LCD_D17)
#define BUTTON2_PIN                MXS_PIN_TO_GPIO(PINID_LCD_D18)
#define BUTTON3_PIN                MXS_PIN_TO_GPIO(PINID_SSP0_DATA4)
#define BUTTON4_PIN                MXS_PIN_TO_GPIO(PINID_SSP0_DATA5)
#define BUTTON5_PIN                MXS_PIN_TO_GPIO(PINID_SSP0_DATA6)


typedef enum {
    BUTTON_PRESSING = 0,
    BUTTON_PRESSED,
    BUTTON_RELEASING,
    BUTTON_RELEASED,
} button_state_t;

typedef struct {
    const char* name;
    button_state_t state;
    unsigned int key;
    unsigned long gpio;
    struct timer_list timer;
} imx_button_t;

typedef struct {
    struct input_dev* input_dev;
} button_dev_t;

imx_button_t imx_buttons[] = {
    {.name = "button1", .key = KEY_A, .gpio = BUTTON1_PIN, .state = BUTTON_RELEASED},
    {.name = "button2", .key = KEY_B, .gpio = BUTTON2_PIN, .state = BUTTON_RELEASED},
    {.name = "button3", .key = KEY_C, .gpio = BUTTON3_PIN, .state = BUTTON_RELEASED},
    {.name = "button4", .key = KEY_D, .gpio = BUTTON4_PIN, .state = BUTTON_RELEASED},
    {.name = "button5", .key = KEY_E, .gpio = BUTTON5_PIN, .state = BUTTON_RELEASED},
};
#define IMX_BUTTONS_NUM             ARRAY_SIZE(imx_buttons)

button_dev_t* button_dev = NULL;

static void imx_button_timer(unsigned long arg)
{
    int i = (int)arg;
    int level = gpio_get_value(imx_buttons[i].gpio);
    if (imx_buttons[i].state == BUTTON_PRESSING)
    {
        if (!level)
        {
            imx_buttons[i].state = BUTTON_PRESSED;
            input_report_key(button_dev->input_dev, imx_buttons[i].key, 0);
            input_sync(button_dev->input_dev);
            printk(">>>%s pressed.\n", imx_buttons[i].name);
        }
        else
        {
            imx_buttons[i].state = BUTTON_RELEASED;
            return;
        }
        
    }
    else if (imx_buttons[i].state == BUTTON_PRESSED)
    {
        if (level)
        {
            imx_buttons[i].state = BUTTON_RELEASING;
        }
        
    }
    else if (imx_buttons[i].state == BUTTON_RELEASING)
    {
        if (level)
        {
            imx_buttons[i].state = BUTTON_RELEASED;
            input_report_key(button_dev->input_dev, imx_buttons[i].key, 1);
            input_sync(button_dev->input_dev);
            printk(">>>>%s released\n", imx_buttons[i].name);
            return;
        }
        else
        {
            imx_buttons[i].state = BUTTON_PRESSED;
        }
        
    }

    imx_buttons[i].timer.expires = jiffies + HZ / 50;
    add_timer(&imx_buttons[i].timer);
}

static irqreturn_t imx_button_irq(int irq, void* dev_id)
{
    int gpio_id = (int)dev_id;
    int level = gpio_get_value(imx_buttons[gpio_id].gpio);
    if (imx_buttons[gpio_id].state == BUTTON_RELEASED && !level)
    {
        imx_buttons[gpio_id].state = BUTTON_PRESSING;
        imx_buttons[gpio_id].timer.expires = jiffies + HZ / 50;
        add_timer(&imx_buttons[gpio_id].timer);
    }
    printk(">>>>>>>%s, level = %d\n",imx_buttons[gpio_id].name, level ? 1 : 0);

    return IRQ_HANDLED;
}

static int __init button_init(void)
{
    int i = 0;
    int ret = 0;
    int irqno = 0;
    button_dev = kzalloc(sizeof(imx_button_t), GFP_KERNEL);

    button_dev->input_dev = input_allocate_device();

    button_dev->input_dev->name = "imx-keys";
    set_bit(EV_KEY, button_dev->input_dev->evbit);
    
    for (i = 0; i < IMX_BUTTONS_NUM; i++)
    {
        setup_timer(&imx_buttons[i].timer, imx_button_timer, (unsigned long)i);
        set_bit(imx_buttons[i].key, button_dev->input_dev->keybit);
        gpio_free(imx_buttons[i].gpio);
        if ((ret = gpio_request(imx_buttons[i].gpio, "imx-button")) != 0)
        {
            printk(KERN_ERR "gpio: %d request failed,\n", (int)imx_buttons[i].gpio);
            return ret;
        }
        gpio_direction_input(imx_buttons[i].gpio);
        irqno = gpio_to_irq(imx_buttons[i].gpio);
        set_irq_type(irqno, IRQ_TYPE_EDGE_FALLING);
        if ((ret = request_irq(irqno, imx_button_irq, IRQF_DISABLED, "button", (void*)i)) != 0)
        {
            printk(KERN_ERR "request irq: %d failed.\n", irqno);
            return ret;
        }

        printk(">>>>%s init pass.\n", imx_buttons[i].name);
    }
    if ((ret = input_register_device(button_dev->input_dev)) != 0)
    {
        printk(KERN_ERR "input register device failed.\n");
        return ret;
    }

    return 0;
}

static void __exit button_exit(void)
{
    int i = 0;
    int irqno = 0;

    for (i = 0; i < IMX_BUTTONS_NUM; i++)
    {
        irqno = gpio_to_irq(imx_buttons[i].gpio);
        free_irq(irqno, (void*)i);
        gpio_free(imx_buttons[i].gpio);
    }

    input_unregister_device(button_dev->input_dev);
    kfree(button_dev);
}


module_init(button_init);
module_exit(button_exit);

MODULE_AUTHOR("Arno");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("gpio button interrupt module");