#include <linux/module.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/circ_buf.h>
#include <linux/cdev.h>
#include <linux/printk.h>
#include <linux/wait.h>
#include <linux/time.h>
#include <linux/timex.h>
#include <linux/rtc.h>
#include <linux/timekeeping.h>
#include <linux/cred.h>

#define IONBLOCK _IO('L', 0)
#define GET_LAST_OP _IO('L', 1)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Georgiy");

#define MAX_DEVICE 1
#define DRIVER_NAME "circ_cdev"

int my_open(struct inode *inode, struct file *filp);
ssize_t my_write(struct file *filp, const char __user *usr_buf, size_t count, loff_t *ppos);
ssize_t my_read(struct file *filp, char __user *usr_buf, size_t count, loff_t *ppos);
int my_release(struct inode *inode, struct file *filp);
long my_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);

typedef struct privatedata
{
        wait_queue_head_t inq, outq;
        int minor_num;
        struct cdev mycdev;
        struct circ_buf KERN_BUFF;
        int KERN_SIZE;
        struct semaphore sem;
        char last_op[127];
} my_privatedata;

my_privatedata devices[MAX_DEVICE];

dev_t device_num;
int major;
int minor = 1;

static uint circ_buff_size = 16;

module_param(circ_buff_size, uint, S_IRUGO);

struct class *my_class;
struct device *my_device;

struct file_operations my_fops = {
        .owner   = THIS_MODULE,
        .open    = my_open,
        .release = my_release,
        .write   = my_write,
        .read    = my_read,
        .unlocked_ioctl = my_ioctl,
};

