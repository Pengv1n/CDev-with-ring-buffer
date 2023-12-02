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

static int __init circ_cdev_init(void)
{
	int ret;

	printk("I am in INIT FUNCTION");
	ret = alloc_chrdev_region(&device_num, minor, MAX_DEVICE, DRIVER_NAME);
	if (ret < 0)
	{
		printk("Register Device failed");
		return -1;
	}
	major = MAJOR(device_num);

	my_class = class_create(DRIVER_NAME);
	if (!my_class)
	{
		printk("Class creation failed");
		return -1;
	}

	for (int i = 0; i < MAX_DEVICE; ++i)
	{
		device_num = MKDEV(major, minor + i);
		cdev_init(&devices[i].mycdev, &my_fops);
                cdev_add(&devices[i].mycdev, device_num, 1);
		sema_init(&(devices[i].sem), 1);
		init_waitqueue_head(&(devices[i].inq));
		init_waitqueue_head(&(devices[i].outq));

		my_device = device_create(my_class, NULL, device_num, NULL, "circ_cdev%d", i);
		if (my_device == NULL)
		{
			class_destroy(my_class);
			printk("Device creation failed");
			return -1;
		}

		devices[i].minor_num = minor + i;
		devices[i].KERN_BUFF.buf = kzalloc(circ_buff_size, GFP_KERNEL);
		printk("Done alloc buffer with size: %d", circ_buff_size);
		if (!devices[i].KERN_BUFF.buf)
		{
			class_destroy(my_class);
			printk("Error kmalloc buffer");
			return -1;
		}
	}
	return 0;
}

static void __exit circ_cdev_exit(void)
{
	printk("I am in EXIT FUNCTION");
	for (int i = 0; i < MAX_DEVICE; ++i)
	{
		kfree(devices[i].KERN_BUFF.buf);
		device_num = MKDEV(major, minor + i);
		cdev_del(&devices[i].mycdev);
		device_destroy(my_class, device_num);
	}

	class_destroy(my_class);

	device_num = MKDEV(major, minor);
	unregister_chrdev_region(device_num, MAX_DEVICE);
}

int my_open(struct inode *inode, struct file *filp)
{
	my_privatedata *dev = container_of(inode->i_cdev, my_privatedata, mycdev);
	filp->private_data = dev;
	dev->KERN_SIZE = circ_buff_size;
	printk("In character driver open function device node %d", dev->minor_num);
	return 0;
}

int my_release(struct inode *inode, struct file *filp)
{
	my_privatedata *dev = filp->private_data;
	printk("I am in release function and minor number is %d", dev->minor_num);
	return 0;
}

ssize_t my_write(struct file *filp, const char __user *usr_buff, size_t count, loff_t *ppos)
{
	my_privatedata *dev = filp->private_data;
	printk("Character driver write function");

	if (down_interruptible(&dev->sem))
		return -ERESTARTSYS;

	while (CIRC_SPACE(dev->KERN_BUFF.head, dev->KERN_BUFF.tail, dev->KERN_SIZE) < 1) {
		DEFINE_WAIT(wait);
		
		up(&dev->sem);
		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;
		prepare_to_wait(&dev->outq, &wait, TASK_INTERRUPTIBLE);
		if (CIRC_SPACE(dev->KERN_BUFF.head, dev->KERN_BUFF.tail, dev->KERN_SIZE) < 1)
			schedule();
		finish_wait(&dev->outq, &wait);
		if (signal_pending(current))
			return -EINTR;
		if (down_interruptible(&dev->sem))
			return -EINTR;
	}

	char ch;
	int i = -1;
	while (++i < count && CIRC_SPACE(dev->KERN_BUFF.head, dev->KERN_BUFF.tail, dev->KERN_SIZE) >= 1)
	{
		if (get_user(ch, &usr_buff[i]))
		{
			up(&dev->sem);
			return -EFAULT;
		}
		if (ch == '\0')
			break;
		dev->KERN_BUFF.buf[dev->KERN_BUFF.head] = ch;
		printk("In character driver write function and value of KERN_BUFF is: %d %d %ld %s",
			       	CIRC_SPACE(dev->KERN_BUFF.head,dev->KERN_BUFF.tail,dev->KERN_SIZE),
			       	dev->KERN_BUFF.head,
			       	strlen(dev->KERN_BUFF.buf),
			       	dev->KERN_BUFF.buf);
		dev->KERN_BUFF.head = (dev->KERN_BUFF.head + 1) & (dev->KERN_SIZE - 1);
	}
	
	//struct timespec t = current_kernel_time();
	up(&dev->sem);
	wake_up_interruptible(&dev->inq);

	return 0;
}

ssize_t my_read(struct file *filp, char __user *usr_buf, size_t count, loff_t *ppos)
{
	my_privatedata *dev = filp->private_data;
	printk("Character driver read function");

	if (down_interruptible(&dev->sem))
        	return -ERESTARTSYS;

	while (CIRC_CNT(dev->KERN_BUFF.head, dev->KERN_BUFF.tail, dev->KERN_SIZE) < 1)
	{
		up(&dev->sem); /* release the lock */
        	if (filp->f_flags & O_NONBLOCK)
            		return -EAGAIN;
        	printk("\"%s\" reading: going to sleep\n", current->comm);
        	if (wait_event_interruptible(dev->inq, (CIRC_CNT(dev->KERN_BUFF.head, dev->KERN_BUFF.tail, dev->KERN_SIZE) >= 1)))
            		return -ERESTARTSYS; /* signal: tell the fs layer to handle it */
        	/* otherwise loop, but first reacquire the lock */
        	if (down_interruptible(&dev->sem))
            		return -ERESTARTSYS;
	}

	char ch;
	size_t size = count;
	while (count > 1 && CIRC_CNT(dev->KERN_BUFF.head, dev->KERN_BUFF.tail, dev->KERN_SIZE) >= 1)
	{
		ch = dev->KERN_BUFF.buf[dev->KERN_BUFF.tail];

		if (put_user(ch, &usr_buf[size - count]))
		{
			up(&dev->sem);
			return -EFAULT;
		}
		dev->KERN_BUFF.tail = (dev->KERN_BUFF.tail + 1) & (dev->KERN_SIZE - 1);
		--count;
	}
	if (put_user('\0', &usr_buf[size - count]))
	{
		up(&dev->sem);
		return -EFAULT;
	}

	printk("Data '%s' from kernel buffer", usr_buf);
	up(&dev->sem);
	wake_up_interruptible(&dev->outq);

	return 0;
}

long my_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	//my_privatedata *dev = filp->private_data;

	switch (cmd)
	{
		case IONBLOCK:
		{
			printk("In ioctl IONBLOCK with cmd - %d", cmd);
			
			unsigned int flag;
			flag = O_NONBLOCK;
			spin_lock(&filp->f_lock);
			if (arg)
				filp->f_flags |= flag;
			else
				filp->f_flags &= ~flag;
			spin_unlock(&filp->f_lock);
			break;
		}
		case GET_LAST_OP:
		{
			printk("In ioctl GET_LAST_OP with cmd - %d", cmd);
			//char *output = (char *)arg;

			struct timespec64 time;
			//unsigned long local_time;

			ktime_get_ts64(&time);
			break;
		}
		default:
		{
			printk("In ioctl with not handled cmd");
			break;
		}
	}
	return 0;
}

module_init(circ_cdev_init);
module_exit(circ_cdev_exit);
