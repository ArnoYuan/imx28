#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/delay.h>


#define FM24_DEV_NAME       "fm24c02"


typedef struct fm24_dev {
    struct device* dev;
    struct cdev cdev;
    dev_t devno;
    struct class* class;
} fm24_dev_t;


typedef struct {
    int busnum;
    const char* name;
    uint32_t addr;
    size_t page_size;
    size_t chip_size;
    size_t addr_size;
    size_t addr_mask;
    uint32_t delay;
} fm24_devinfo_t;

typedef struct {
    struct i2c_client client;
    fm24_devinfo_t* devinfo;
} fm24_client_t;

static fm24_dev_t fm24_dev;


const fm24_devinfo_t fm24_devinfos[] = {
    {1, "fm24c02", 0x50, 8, 2048 / 8, 1, 0x00, 5},
};


static int fm24_open(struct inode* inode, struct file* file)
{
    unsigned int minor = iminor(inode);
    printk(KERN_INFO "fm24 open minor=%d\n", minor);
    fm24_devinfo_t* info = fm24_devinfos + minor;

    struct i2c_adapter* adap = i2c_get_adapter(info->busnum);

    printk(KERN_INFO "fm24 open\n");

    if (!adap)
        return -ENODEV;
    
    fm24_client_t* fm24_client = (fm24_client_t*)kzalloc(sizeof(*fm24_client), GFP_KERNEL);
    if (!fm24_client)
    {
        i2c_put_adapter(adap);
        return -ENOMEM;
    }

    snprintf(fm24_client->client.name, I2C_NAME_SIZE, info->name);
    fm24_client->client.adapter = adap;
    fm24_client->client.driver = &fm24_dev;
    fm24_client->devinfo = info;
    file->private_data = fm24_client;

    return 0;
}

static int fm24_release(struct inode* inode, struct file* file)
{
    printk(KERN_INFO "fm24 release\n");
    fm24_client_t* fm24_client = file->private_data;
    kfree(fm24_client);
    return 0;
}


static int fm24_i2c_write(fm24_client_t* fm24, uint32_t address, const char __user* buf, size_t len)
{
    struct i2c_msg msg;
    int ret = 0;

    if ((address < 0) || (address >= fm24->devinfo->chip_size))
    {
        return -EINVAL;
    }

    if ((address + len > fm24->devinfo->chip_size) || (address + len < 0))
    {
        return -EINVAL;
    }
    msg.addr = fm24->devinfo->addr;
    msg.flags = 0;
    msg.len = fm24->devinfo->addr_size + len;
    msg.buf = (uint8_t*)kzalloc(fm24->devinfo->addr_size + len, sizeof(uint8_t));
    if (fm24->devinfo->addr_size == 1)
    {
        msg.buf[0] = address & 0xFF;
    }
    else if (fm24->devinfo->addr_size == 2)
    {
        msg.buf[0] = (address >> 8) & 0xFF;
        msg.buf[1] = (address & 0xFF);
    }
    else
    {
        printk(KERN_ERR "fm24 memroy address is error.\n");
        ret = -EINVAL;
        goto out;
    }
    if (copy_from_user(msg.buf + fm24->devinfo->addr_size, buf, len))
    {
        ret = -EFAULT;
        goto out;
    }
    ret = i2c_transfer(fm24->client.adapter, &msg, 1);
    if (ret < 0)
    {
        goto out;
    }
    ret = len;
out:
    kfree(msg.buf);
    return ret;
}

static int fm24_i2c_read(fm24_client_t* fm24, uint32_t address, char __user* buf, size_t len)
{
    int ret = 0;
    struct i2c_msg msgs[2];
    msgs[0].buf = (uint8_t*)kzalloc(fm24->devinfo->addr_size, sizeof(uint8_t));
    if (fm24->devinfo->addr_size == 1)
        msgs[0].buf[0] = address & 0xFF;
    else if (fm24->devinfo->addr_size == 2)
    {
        msgs[0].buf[0] = (address >> 8) & 0xFF;
        msgs[0].buf[1] = address & 0xFF;
    }
    else
    {
        printk(KERN_INFO "fm24 memory address is error.\n");
        kfree(msgs[0].buf);
        return -EINVAL;
    }
    
    msgs[0].len = fm24->devinfo->addr_size;
    msgs[0].addr = fm24->devinfo->addr;
    msgs[0].flags = 0;
    msgs[1].buf = kzalloc(len, sizeof(uint8_t));
    if (copy_from_user(msgs[1].buf, buf, len))
    {
        ret = -EFAULT;
        goto out;
    }
    
    msgs[1].addr = fm24->devinfo->addr;
    msgs[1].flags = I2C_M_RD;
    msgs[1].len = len;
    ret = i2c_transfer(fm24->client.adapter, msgs, 2);
    if (ret > 0)
    {
        if (copy_to_user(buf, msgs[1].buf, len))
        {
            ret = -EFAULT;
            goto out;
        }
    }
    return len;

out:
    kfree(msgs[0].buf);
    kfree(msgs[1].buf);
    return ret;
}

static loff_t fm24_llseek(struct file* file, loff_t offset, int whence)
{
    printk(KERN_INFO "fm24 llseek: offset=%d, whence=%d\n", (int)offset, (int)whence);
    fm24_client_t* fm24 = file->private_data;
    loff_t ret = 0;
    switch(whence)
    {
    case SEEK_SET:
        printk(KERN_INFO "[SET][%d] offset is %d\n", whence, (int)offset);
        if (offset < 0 || offset >= fm24->devinfo->chip_size)
        {
            ret = -EINVAL;
            break;
        }
        file->f_pos = (unsigned int)offset;
        ret = file->f_pos;
        break;
    case SEEK_CUR:
        printk(KERN_INFO "[CUR][%d] offset is %d\n", whence, (int)offset);
        if (file->f_pos + offset <= 0 || file->f_pos + offset > fm24->devinfo->chip_size)
        {
            ret = -EINVAL;
            break;
        }
        file->f_pos += offset;
        ret = file->f_pos;
        break;
    case SEEK_END:
        printk(KERN_INFO "[END][%d] offset is %d\n", whence, (int)offset);
        if (file->f_pos + offset < 0 || file->f_pos + offset > fm24->devinfo->chip_size)
        {
            ret = -EINVAL;
            break;
        }
        ret = file->f_pos;
        break;
    default:
        ret = -EINVAL;
    }
    printk(KERN_INFO "llseek end.\n");
    return ret;
}

static ssize_t fm24_read(struct file* file, char __user* buf, size_t len, loff_t* offset)
{
    printk(KERN_INFO ">>>read %d bytes from %d\n", (int)len, (int)*offset);
    fm24_client_t* fm24 = file->private_data;

    int ret = len;
    int result = 0;
    fm24_devinfo_t* info = fm24->devinfo;
    int rd_len = 0;
    uint32_t dev_addr;
    uint32_t address = *offset;
    while (len)
    {
        dev_addr = ((address >> 8) & info->addr_mask) | info->addr;

        if ((address & (info->page_size - 1)) + len > info->page_size)
            rd_len = info->page_size - (address & (info->page_size - 1));
        else
            rd_len = len;
        
        if ((result = fm24_i2c_read(fm24, address, buf, rd_len)) < 0)
        {
            return result;
        }
        len -= rd_len;
        address += rd_len;
        buf += rd_len;
    }
    *offset += ret;
    return ret;
}

static ssize_t fm24_write(struct file* file, const char __user* buf, size_t len, loff_t* offset)
{
    printk(KERN_INFO "write %d bytes to %d\n", (int)len, (int)*offset);
    fm24_client_t* fm24 = file->private_data;
    fm24_devinfo_t* info = fm24->devinfo;
    int result = 0;
    int wr_len = 0;
    uint32_t dev_addr;
    uint32_t address = *offset;
    int ret = len;

    while (len)
    {
        dev_addr = ((address >> 8) & info->addr_mask) | info->addr;

        if ((address & (info->page_size - 1)) + len > info->page_size)
            wr_len = info->page_size - (address & (info->page_size - 1));
        else
            wr_len = len;
        
        if ((result = fm24_i2c_write(fm24, address, buf, wr_len)) < 0)
            return result;
        len -= wr_len;
        address += wr_len;
        buf += wr_len;
        mdelay(info->delay);
    }
    *offset += ret;
    return ret;
}

static const struct file_operations fm24_ops = {
    .owner = THIS_MODULE,
    .open = fm24_open,
    .release = fm24_release,
    .llseek = fm24_llseek,
    .read = fm24_read,
    .write = fm24_write,
};

static struct i2c_driver fm24_driver = {
    .driver = {
        .name = "fm24",
        .owner = THIS_MODULE,
    },
};


static int __init fm24cxx_init(void)
{
    int ret = 0;
    int major = 0;
    int count = 0;
    struct device* dev;
    int i = 0;

    printk(KERN_INFO "FM24 chip driver\n");

    ret = alloc_chrdev_region(&fm24_dev.devno, 0, ARRAY_SIZE(fm24_devinfos), FM24_DEV_NAME);
    if (ret < 0)
    {
        printk(KERN_ERR "alloc FM24 chip driver cdev failed.\n");
        return ret;
    }
    printk(KERN_INFO "major devno is %d\n", MAJOR(fm24_dev.devno));

    major = MAJOR(fm24_dev.devno);
    fm24_dev.class = class_create(THIS_MODULE, FM24_DEV_NAME);
    if (!fm24_dev.class)
    {
        printk(KERN_INFO "class create failed.\n");
        ret = -EBUSY;
        goto fail0;
    }


    cdev_init(&fm24_dev.cdev, &fm24_ops);
    if ((ret = cdev_add(&fm24_dev.cdev, MKDEV(major, 0), ARRAY_SIZE(fm24_devinfos))) < 0)
    {
        printk(KERN_INFO "cdev add failed.\n");
        goto fail1;
    }
    
    ret = i2c_add_driver(&fm24_driver);
    if (ret < 0)
    {
        printk(KERN_ERR "fm24 i2c driver add failed.\n");
        goto fail2;
    }

    for (i = 0; i < ARRAY_SIZE(fm24_devinfos); i++)
    {
        dev = device_create(fm24_dev.class, NULL, MKDEV(major, i), NULL, fm24_devinfos[i].name);
        if (IS_ERR(dev))
        {
            ret = PTR_ERR(dev);
            goto fail3;
        }
    }

    return 0;
fail3:
    while (count--)
    {
        device_destroy(fm24_dev.class, MKDEV(major, count));
    }
    i2c_del_driver(&fm24_driver);
fail2:
    cdev_del(&fm24_dev.cdev);
fail1:
    class_destroy(fm24_dev.class);
fail0:
    unregister_chrdev_region(fm24_dev.devno, 1);
    return ret;

}


static void __exit fm24cxx_exit(void)
{
    int i = 0;
    int major = MAJOR(fm24_dev.devno);
    for (i = 0; i < ARRAY_SIZE(fm24_devinfos); i++)
    {
        device_destroy(fm24_dev.class, MKDEV(major, i));
    }
    i2c_del_driver(&fm24_driver);
    cdev_del(&fm24_dev.cdev);
    class_destroy(fm24_dev.class);
    unregister_chrdev_region(fm24_dev.devno, ARRAY_SIZE(fm24_devinfos));
}

module_init(fm24cxx_init);
module_exit(fm24cxx_exit);

MODULE_AUTHOR("Arno");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("FM24CXX Chip driver.");