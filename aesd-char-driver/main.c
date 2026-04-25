/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include "aesdchar.h"
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Ahmed Gad"); 
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    struct aesd_dev *dev;
    
    PDEBUG("open");
    dev = container_of(inode->i_cdev,struct aesd_dev, cdev);
    filp->private_data = dev;
    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    /**
     * TODO: handle release
     */
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = 0;
    struct aesd_dev *dev = filp->private_data;

    if (mutex_lock_interruptible(&dev->lock))
        return -ERESTARTSYS;
    size_t entry_offset;
    struct aesd_buffer_entry* entry = aesd_circular_buffer_find_entry_offset_for_fpos(&dev->buffer,*f_pos,&entry_offset);
    if (entry == NULL)
    {
        PDEBUG("position out of bound !");
        mutex_unlock(&dev->lock);
        return retval;
    }
    size_t bytes_to_copy = min(count, entry->size - entry_offset);
    if(copy_to_user(buf, entry->buffptr + entry_offset, bytes_to_copy))
    {
        PDEBUG("cannot copy to user!");
        mutex_unlock(&dev->lock);
        return -EFAULT;
    }
    *f_pos += bytes_to_copy; 
    retval = bytes_to_copy;
    mutex_unlock(&dev->lock);
    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    struct aesd_dev *dev = filp->private_data;
    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
    if (mutex_lock_interruptible(&dev->lock))
        return -ERESTARTSYS;
    
    size_t new_size = dev->partial_size + count;  
    dev->partial_write = krealloc(dev->partial_write, new_size, GFP_KERNEL);

    if(dev->partial_write == NULL)
    {
        PDEBUG("cannot allocate memory for partial write buffer!");
        mutex_unlock(&dev->lock);
        return retval;
    }
    if(copy_from_user(dev->partial_write + dev->partial_size ,buf,count))
    {
        PDEBUG("cannot copy data from user!");
        mutex_unlock(&dev->lock);
        kfree(dev->partial_write);
        dev->partial_write = NULL;
        dev->partial_size = 0;
        return -EFAULT;
    }
    dev->partial_size = new_size;

    for (size_t i = dev->partial_size - count; i < dev->partial_size; i++)
    {
        if(dev->partial_write[i] == '\n')
        {
            struct aesd_buffer_entry entry;

            entry.buffptr = dev->partial_write;
            entry.size = i + 1;
            size_t remaining = dev->partial_size - (i + 1);

            if (dev->buffer.full) 
            {
                PDEBUG("buffer full, overwriting oldest entry...");
                kfree(dev->buffer.entry[dev->buffer.out_offs].buffptr);
            }
            if (remaining > 0) {
            char *new_partial = kmalloc(remaining, GFP_KERNEL);
            if (new_partial == NULL) 
            {
                kfree(dev->partial_write);
                dev->partial_write = NULL;
                dev->partial_size = 0;
                mutex_unlock(&dev->lock);
                return -ENOMEM;
            }
            memcpy(new_partial, dev->partial_write + (i + 1), remaining);
            aesd_circular_buffer_add_entry(&dev->buffer, &entry);
            dev->partial_write = new_partial;
            dev->partial_size = remaining;
            } 
            else 
            {
                aesd_circular_buffer_add_entry(&dev->buffer, &entry);
                dev->partial_write = NULL;
                dev->partial_size = 0;
            }
            break;
        }
    }
    mutex_unlock(&dev->lock);
    retval = count;
    return retval;
}
struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}



int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
            "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device,0,sizeof(struct aesd_dev));

    mutex_init(&aesd_device.lock);

    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    return result;

}

void aesd_cleanup_module(void)
{
    dev_t devno = MKDEV(aesd_major, aesd_minor);
    struct aesd_buffer_entry *entryptr;
    int index = 0;

    cdev_del(&aesd_device.cdev);

    AESD_CIRCULAR_BUFFER_FOREACH(entryptr, &aesd_device.buffer, index)
    {
        if (entryptr->buffptr)
            kfree(entryptr->buffptr);
    }
    kfree(aesd_device.partial_write);

    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
