#include "circ_cdev.h"

void set_last_op(char *out, const char *op)
{
	struct timespec64 curr_tm;
        struct tm tm1;

        ktime_to_timespec64_cond(ktime_get_real(), &curr_tm);
        time64_to_tm(curr_tm.tv_sec, 0, &tm1);

        sprintf(out, "%s TIME: %.4lu-%.2d-%.2d %.2d:%.2d:%.2d PID: %u",
                        op,
			tm1.tm_year + 1900,
                        tm1.tm_mon,
                        tm1.tm_mday,
                        tm1.tm_hour + 3,
                        tm1.tm_min,
                        tm1.tm_sec,
                        current->pid);
}

static int __init circ_cdev_init(void)
{
	int ret;

	pr_debug("circ_cdev: I am in INIT FUNCTION");
	ret = alloc_chrdev_region(&device_num, minor, MAX_DEVICE, DRIVER_NAME);
	if (ret < 0)
	{
		pr_err("circ_cdev: Register Device failed");
		return -1;
	}
	major = MAJOR(device_num);

	my_class = class_create(DRIVER_NAME);
	if (!my_class)
	{
		pr_err("circ_cdev: Class creation failed");
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
			pr_err("circ_cdev: Device creation failed");
			return -1;
		}

		devices[i].minor_num = minor + i;
		devices[i].KERN_BUFF.buf = kzalloc(circ_buff_size, GFP_KERNEL);
		pr_debug("circ_cdev: Done alloc buffer with size: %d", circ_buff_size);
		if (!devices[i].KERN_BUFF.buf)
		{
			class_destroy(my_class);
			pr_err("circ_cdev: Error kmalloc buffer");
			return -1;
		}
	}
	return 0;
}

static void __exit circ_cdev_exit(void)
{
	pr_debug("circ_cdev: I am in EXIT FUNCTION");
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
	pr_debug("circ_cdev: In character driver open function device node %d", dev->minor_num);
	return 0;
}

int my_release(struct inode *inode, struct file *filp)
{
	my_privatedata *dev = filp->private_data;
	pr_debug("circ_cdev: I am in release function and minor number is %d", dev->minor_num);
	return 0;
}

ssize_t my_write(struct file *filp, const char __user *usr_buff, size_t count, loff_t *ppos)
{
	my_privatedata *dev = filp->private_data;
	pr_debug("circ_cdev: Character driver write function UID: %d", current->pid);

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
		dev->KERN_BUFF.buf[dev->KERN_BUFF.head] = ch;
		dev->KERN_BUFF.head = (dev->KERN_BUFF.head + 1) & (dev->KERN_SIZE - 1);
	}

	set_last_op(dev->last_op, "WRITE");
	
	up(&dev->sem);
	wake_up_interruptible(&dev->inq);

	return i;
}

ssize_t my_read(struct file *filp, char __user *usr_buf, size_t count, loff_t *ppos)
{
	my_privatedata *dev = filp->private_data;
	pr_debug("circ_cdev: Character driver write function UID: %d", current->pid);

	if (down_interruptible(&dev->sem))
        	return -ERESTARTSYS;

	while (CIRC_CNT(dev->KERN_BUFF.head, dev->KERN_BUFF.tail, dev->KERN_SIZE) < 1)
	{
		up(&dev->sem);
        	if (filp->f_flags & O_NONBLOCK)
            		return -EAGAIN;
        	pr_debug("circ_cdev: \"%s\" reading: going to sleep\n", current->comm);
        	if (wait_event_interruptible(dev->inq, (CIRC_CNT(dev->KERN_BUFF.head, dev->KERN_BUFF.tail, dev->KERN_SIZE) >= 1)))
            		return -ERESTARTSYS;
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

	pr_debug("circ_cdev: Data '%s' from kernel buffer", usr_buf);

	set_last_op(dev->last_op, "READ");

	up(&dev->sem);
	wake_up_interruptible(&dev->outq);

	return strlen(usr_buf);
}

long my_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	my_privatedata *dev = filp->private_data;

	switch (cmd)
	{
		case IONBLOCK:
		{
			pr_debug("circ_cdev: In ioctl IONBLOCK with cmd - %d", cmd);
			
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
			pr_debug("circ_cdev: In ioctl GET_LAST_OP with cmd - %d", cmd);

			if (copy_to_user((char*)arg, dev->last_op, strlen(dev->last_op)))
				return -EFAULT;

			break;
		}
		default:
		{
			pr_warn("circ_cdev: In ioctl with not handled cmd");
			break;
		}
	}
	return 0;
}

module_init(circ_cdev_init);
module_exit(circ_cdev_exit);
