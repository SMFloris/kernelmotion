#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kref.h>
#include <linux/uaccess.h>
#include <linux/usb.h>
#include <linux/mutex.h>

/* Define these values to match your devices */
#define USB_LEAP_VENDOR_ID	0xf182
#define USB_LEAP_PRODUCT_ID	0x0003

#define USB_LEAP_MINOR_BASE	200
#define WRITES_IN_FLIGHT	8

#define MAX_TRANSFER 16407

#define MIN(a,b) (((a) <= (b)) ? (a) : (b))

/* table of devices that work with this driver */
static const struct usb_device_id leap_table[] = {
    { USB_DEVICE ( USB_LEAP_VENDOR_ID, USB_LEAP_PRODUCT_ID ) },
    { }					/* Terminating entry */
};
MODULE_DEVICE_TABLE ( usb, leap_table );

struct usb_leap {
    struct usb_device	*udev;			/* the usb device for this device */
    struct usb_interface	*interface;		/* the interface for this device */
    struct semaphore	limit_sem;		/* limiting the number of writes in progress */
    struct usb_anchor	submitted;		/* in case we need to retract our submissions */
    struct urb		*bulk_in_urb;		/* the urb to read data with */
    unsigned char           *bulk_in_buffer;	/* the buffer to receive data */
    size_t			bulk_in_size;		/* the size of the receive buffer */
    size_t			bulk_in_filled;		/* number of bytes in the buffer */
    size_t			bulk_in_copied;		/* already copied to user space */
    __u8			bulk_in_endpointAddr;	/* the address of the bulk in endpoint */
    __u8			bulk_out_endpointAddr;	/* the address of the bulk out endpoint */
    int			errors;			/* the last request tanked */
    bool			ongoing_read;		/* a read is going on */
    spinlock_t		err_lock;		/* lock for errors */
    struct kref		kref;
    struct mutex		io_mutex;		/* synchronize I/O with disconnect */
    wait_queue_head_t	bulk_in_wait;		/* to wait for an ongoing read */
};
#define to_leap_dev(d) container_of(d, struct usb_leap, kref)

static struct usb_driver leap_driver;
static void leap_draw_down ( struct usb_leap *dev );

static void leap_delete ( struct kref *kref )
{
    struct usb_leap *dev = to_leap_dev ( kref );

    usb_free_urb ( dev->bulk_in_urb );
    usb_put_dev ( dev->udev );
    kfree ( dev->bulk_in_buffer );
    kfree ( dev );
}

static void leap_read_bulk_callback(struct urb *urb)
{
	struct usb_leap *dev;
	
	dev = urb->context;
	
	printk("CALLBACK!\n");
	
	spin_lock(&dev->err_lock);
	/* sync/async unlink faults aren't errors */
	if (urb->status) {
		if (!(urb->status == -ENOENT ||
			urb->status == -ECONNRESET ||
			urb->status == -ESHUTDOWN))
			printk("Nonzero write bulk status received: %d\n",urb->status);
		
		dev->errors = urb->status;
	} else {
		dev->bulk_in_filled = urb->actual_length;
	}
	dev->ongoing_read = 0;
	spin_unlock(&dev->err_lock);
	
	printk("Primit %d bytes\n",urb->actual_length);
	
	wake_up_interruptible(&dev->bulk_in_wait);
}

static int leap_open ( struct inode *inode, struct file *file )
{
    struct usb_leap *dev;
    struct usb_interface *interface;
    unsigned int subminor;
    int retval = 0;

    subminor = iminor(inode);
    printk ("Deschid device-ul! %d\n", subminor);

    interface = usb_find_interface ( &leap_driver, subminor );
    if ( !interface ) {
        printk ( "Error, can't find device for minor %d\n",subminor );
        retval = -ENODEV;
        goto exit;
    }
    
    printk("Gasit Device subminor!\n");

    dev = usb_get_intfdata ( interface );
    if ( !dev ) {
        retval = -ENODEV;
        goto exit;
    }
    printk("Gasit device din interfata!\n");


    retval = usb_autopm_get_interface ( interface );
    if ( retval != 0 ) {
	printk("CACAT\n");
        goto exit;
    }

    printk("Gasit interfata counter!\n");

    /* increment our usage count for the device */
    kref_get ( &dev->kref );

    /* save our object in the file's private structure */
    file->private_data = dev;

exit:
    return retval;
}

static int leap_flush(struct file *file, fl_owner_t id)
{
	struct usb_leap *dev;
	int res;
	
	dev = file->private_data;
	if (dev == NULL)
		return -ENODEV;
	
	/* wait for io to stop */
	mutex_lock(&dev->io_mutex);
	leap_draw_down(dev);
	
	/* read out errors, leave subsequent opens a clean slate */
	spin_lock_irq(&dev->err_lock);
	res = dev->errors ? (dev->errors == -EPIPE ? -EPIPE : -EIO) : 0;
	dev->errors = 0;
	spin_unlock_irq(&dev->err_lock);
	
	mutex_unlock(&dev->io_mutex);
	
	return res;
}

static int leap_do_read_io(struct usb_leap *dev, size_t count)
{
	int rv;
	
	printk("DOING IO!\n");
	
	/* prepare a read */
	usb_fill_bulk_urb(dev->bulk_in_urb,
			  dev->udev,
		  usb_rcvbulkpipe(dev->udev,
				   dev->bulk_in_endpointAddr),
		   dev->bulk_in_buffer,
			dev->bulk_in_size,
			  leap_read_bulk_callback,
		   dev);
	/* tell everybody to leave the URB alone */
	spin_lock_irq(&dev->err_lock);
	dev->ongoing_read = 1;
	spin_unlock_irq(&dev->err_lock);
	
	/* submit bulk in urb, which means no data to deliver */
	dev->bulk_in_filled = 0;
	dev->bulk_in_copied = 0;
	
	/* do it */
	rv = usb_submit_urb(dev->bulk_in_urb, GFP_KERNEL);
	if (rv < 0) {
		printk("Failed submitting read urb, error %d \n", rv);
		rv = (rv == -ENOMEM) ? rv : -EIO;
		spin_lock_irq(&dev->err_lock);
		dev->ongoing_read = 0;
		spin_unlock_irq(&dev->err_lock);
	}
	
	printk("Returning from IO with %d status!\n", rv);
	
	return rv;
}

static ssize_t leap_read(struct file *file, char *buffer, size_t count,
						 loff_t *ppos)
{
	struct usb_leap *dev;
	int rv, read_cnt;
	bool ongoing_io;
	
	printk("Reading this shit!\n");
	
	dev = file->private_data;
	
	/* if we cannot read at all, return EOF */
	if (!dev->bulk_in_urb || !count)
		return 0;
	
	printk("Bulk in is ok and count also!\n");
	
	/* Read the data from the bulk endpoint */
	rv = usb_bulk_msg(dev->udev, usb_rcvbulkpipe(dev->udev, 0x83),
			  dev->bulk_in_buffer, 16407, &read_cnt, 10000);
	if (rv)
	{
		printk(KERN_ERR "Bulk message returned %d\n", rv);
		return rv;
	}
	if (copy_to_user(buffer, dev->bulk_in_buffer, MIN(count, read_cnt)))
	{
		return -EFAULT;
	}
	
	return MIN(count, read_cnt);
}

static void leap_write_bulk_callback(struct urb *urb)
{
	struct usb_leap *dev;
	
	dev = urb->context;
	
	/* sync/async unlink faults aren't errors */
	if (urb->status) {
		if (!(urb->status == -ENOENT ||
			urb->status == -ECONNRESET ||
			urb->status == -ESHUTDOWN))
			dev_err(&dev->interface->dev,
				"%s - nonzero write bulk status received: %d\n",
	   __func__, urb->status);
			
			spin_lock(&dev->err_lock);
			dev->errors = urb->status;
			spin_unlock(&dev->err_lock);
	}
	
	/* free up our allocated buffer */
	usb_free_coherent(urb->dev, urb->transfer_buffer_length,
			  urb->transfer_buffer, urb->transfer_dma);
	up(&dev->limit_sem);
}

static ssize_t leap_write(struct file *file, const char *user_buffer,
			  size_t count, loff_t *ppos)
{
	struct usb_leap *dev;
	int retval = 0;
	struct urb *urb = NULL;
	char *buf = NULL;
	size_t writesize = min(count, (size_t)MAX_TRANSFER);
	
	dev = file->private_data;
	
	printk("Writing this shit!\n");
	
	/* verify that we actually have some data to write */
	if (count == 0)
		goto exit;
	
	/*
	 * limit the number of URBs in flight to stop a user from using up all
	 * RAM
	 */
	if (!(file->f_flags & O_NONBLOCK)) {
		if (down_interruptible(&dev->limit_sem)) {
			retval = -ERESTARTSYS;
			goto exit;
		}
	} else {
		if (down_trylock(&dev->limit_sem)) {
			retval = -EAGAIN;
			goto exit;
		}
	}
	
	spin_lock_irq(&dev->err_lock);
	retval = dev->errors;
	if (retval < 0) {
		/* any error is reported once */
		dev->errors = 0;
		/* to preserve notifications about reset */
		retval = (retval == -EPIPE) ? retval : -EIO;
	}
	spin_unlock_irq(&dev->err_lock);
	if (retval < 0)
		goto error;
	
	/* create a urb, and a buffer for it, and copy the data to the urb */
	urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!urb) {
		retval = -ENOMEM;
		goto error;
	}
	
	buf = usb_alloc_coherent(dev->udev, writesize, GFP_KERNEL,
				 &urb->transfer_dma);
	if (!buf) {
		retval = -ENOMEM;
		goto error;
	}
	
	if (copy_from_user(buf, user_buffer, writesize)) {
		retval = -EFAULT;
		goto error;
	}
	
	/* this lock makes sure we don't submit URBs to gone devices */
	mutex_lock(&dev->io_mutex);
	if (!dev->interface) {		/* disconnect() was called */
		mutex_unlock(&dev->io_mutex);
		retval = -ENODEV;
		goto error;
	}
	
	/* initialize the urb properly */
	usb_fill_bulk_urb(urb, dev->udev,
			  usb_sndbulkpipe(dev->udev, 
					  dev->bulk_out_endpointAddr),
		   buf, writesize, leap_write_bulk_callback, dev);
	urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
	usb_anchor_urb(urb, &dev->submitted);
	
	/* send the data out the bulk port */
	retval = usb_submit_urb(urb, GFP_KERNEL);
	mutex_unlock(&dev->io_mutex);
	if (retval) {
		dev_err(&dev->interface->dev,
			"%s - failed submitting write urb, error %d\n",
	  __func__, retval);
		goto error_unanchor;
	}
	
	/*
	 * release our reference to this urb, the USB core will eventually free
	 * it entirely
	 */
	usb_free_urb(urb);
	
	
	return writesize;
	
	error_unanchor:
	usb_unanchor_urb(urb);
	error:
	if (urb) {
		usb_free_coherent(dev->udev, writesize, buf, urb->transfer_dma);
		usb_free_urb(urb);
	}
	up(&dev->limit_sem);
	
	exit:
	return retval;
}

static int leap_release(struct inode *inode, struct file *file)
{
	struct usb_leap *dev;
	
	dev = file->private_data;
	if (dev == NULL)
		return -ENODEV;
	
	/* allow the device to be autosuspended */
	mutex_lock(&dev->io_mutex);
	if (dev->interface)
		usb_autopm_put_interface(dev->interface);
	mutex_unlock(&dev->io_mutex);
	
	/* decrement the count on our device */
	kref_put(&dev->kref, leap_delete);
	return 0;
}

static const struct file_operations leap_fops = {
	.owner =	THIS_MODULE,
	.open =		leap_open,
	.llseek =	noop_llseek,
	.read =		leap_read,
	.flush =	leap_flush,
	.write =	leap_write,
	.release =	leap_release
};

/*
 * usb class driver info in order to get a minor number from the usb core,
 * and to have the device registered with the driver core
 */
static struct usb_class_driver leap_class = {
    .name =		"leap%d",
    .fops =		&leap_fops,
    .minor_base =	USB_LEAP_MINOR_BASE,
};

static void leap_completed_bulk(struct urb *urb){
	struct usb_leap *dev;
	
	dev = urb->context;
	
	/* sync/async unlink faults aren't errors */
	if (urb->status) {
		if (!(urb->status == -ENOENT ||
			urb->status == -ECONNRESET ||
			urb->status == -ESHUTDOWN))
			printk("Nonzero write bulk status received: %d\n", urb->status);
			
			spin_lock(&dev->err_lock);
			dev->errors = urb->status;
			spin_unlock(&dev->err_lock);
	}

	printk("SUCCCESS! am primit %d bytes", urb->transfer_buffer_length);
	
	/* free up our allocated buffer */
	usb_free_coherent(urb->dev, urb->transfer_buffer_length,
			  urb->transfer_buffer, urb->transfer_dma);
	up(&dev->limit_sem);
}
	

static int leap_probe ( struct usb_interface *interface,
                        const struct usb_device_id *id )
{
    printk ( "Probing!\n" );
    struct usb_leap *dev;
    struct usb_host_interface *iface_desc;
    struct usb_endpoint_descriptor *endpoint;
    size_t buffer_size;
    int i;
    int retval = -ENOMEM;

    /* allocate memory for our device state and initialize it */
    dev = kzalloc ( sizeof ( *dev ), GFP_KERNEL );
    if ( !dev ) {
        printk ( "Out of memory\n" );
        goto error;
    }

    kref_init ( &dev->kref );
    sema_init ( &dev->limit_sem, WRITES_IN_FLIGHT );
    mutex_init ( &dev->io_mutex );
    spin_lock_init ( &dev->err_lock );
    init_usb_anchor ( &dev->submitted );
    init_waitqueue_head ( &dev->bulk_in_wait );
	
	/* initialize buffers and enpoints */
	dev->bulk_in_size = 16384;
	dev->bulk_in_endpointAddr = 0x83;
	dev->bulk_in_buffer = kmalloc(dev->bulk_in_size*sizeof(unsigned char), GFP_KERNEL);
	if (!dev->bulk_in_buffer) {
		printk("Could not allocate bulk_in_buffer\n");
		goto error;
	}
	
	dev->bulk_in_urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!dev->bulk_in_urb) {
		printk(	"Could not allocate bulk_in_urb\n");
		goto error;
	}
	
	dev->bulk_out_endpointAddr = 0x21;

    dev->udev = usb_get_dev ( interface_to_usbdev ( interface ) );
    dev->interface = interface;
    
    
    
    unsigned char *bufferIn,*bufferOut;
    unsigned int ctrlPipe;
    bufferOut = kzalloc ( 2*sizeof ( unsigned char ), GFP_KERNEL );
    bufferIn = kzalloc ( 26*sizeof ( unsigned char ), GFP_KERNEL );
    
    usb_lock_device_for_reset(dev->udev, interface);

	    ctrlPipe = usb_sndctrlpipe(dev->udev,0);
	    retval = usb_control_msg(dev->udev,ctrlPipe,1,0x21,0x0800,1280,bufferOut,2,500);
	    if ( !retval ) {
		    printk ( "Nu am putut trimite mesajul de control 1!" );
		    goto error;
	    }
	    retval = usb_control_msg(dev->udev,ctrlPipe,1,0x21,0x0a00,1280,bufferOut,2,500);
	    if ( !retval ) {
		    printk ( "Nu am putut trimite mesajul de control 2!" );
		    goto error;
	    }
	    retval = usb_control_msg(dev->udev,ctrlPipe,1,0x21,0x0700,1280,bufferOut,2,500);
	    if ( !retval ) {
		    printk ( "Nu am putut trimite mesajul de control 3!" );
		    goto error;
	    }
	    
	    retval = usb_control_msg(dev->udev,ctrlPipe,1,0x21,0x0800,1280,bufferIn,26,500);
	    if ( !retval ) {
		    printk ( "Nu am putut trimite mesajul de control 9!" );
		    goto error;
	    }
    ctrlPipe = usb_rcvctrlpipe(dev->udev,0x80);
    retval = usb_control_msg(dev->udev,ctrlPipe,129,0xa1,0x0700,1,bufferOut,2,500);
    if ( !retval ) {
	    printk ( "Nu am putut trimite mesajul de control 4!" );
	    goto error;
    }
    retval = usb_control_msg(dev->udev,ctrlPipe,129,0xa1,0x0100,1,bufferIn,26,500);
    if ( !retval ) {
	    printk ( "Nu am putut trimite mesajul de control 6!" );
	    goto error;
    }
    retval = usb_control_msg(dev->udev,ctrlPipe,131,0xa1,0x0100,1,bufferIn,26,500);
    if ( !retval ) {
	    printk ( "Nu am putut trimite mesajul de control 7!" );
	    goto error;
    }
    retval = usb_control_msg(dev->udev,ctrlPipe,130,0xa1,0x0100,1,bufferIn,26,500);
    if ( !retval ) {
	    printk ( "Nu am putut trimite mesajul de control 7!" );
	    goto error;
    }
    ctrlPipe = usb_sndctrlpipe(dev->udev,0);
    retval = usb_control_msg(dev->udev,ctrlPipe,1,0x21,0x0700,1280,bufferOut,2,500);
    if ( !retval ) {
	    printk ( "Nu am putut trimite mesajul de control 3!" );
	    goto error;
    }
    ctrlPipe = usb_rcvctrlpipe(dev->udev,0x80);
    retval = usb_control_msg(dev->udev,ctrlPipe,129,0xa1,0x0100,1,bufferIn,26,500);
    if ( !retval ) {
	    printk ( "Nu am putut trimite mesajul de control 6!" );
	    goto error;
    }
    ctrlPipe = usb_sndctrlpipe(dev->udev,0);
    retval = usb_control_msg(dev->udev,ctrlPipe,1,0x21,0x0100,1,bufferIn,26,500);
    if ( !retval ) {
	    printk ( "Nu am putut trimite mesajul de control 8!" );
	    goto error;
    }
    ctrlPipe = usb_rcvctrlpipe(dev->udev,0x80);
    retval = usb_control_msg(dev->udev,ctrlPipe,129,0xa1,0x0100,1,bufferIn,26,500);
    if ( !retval ) {
	    printk ( "Nu am putut trimite mesajul de control 6!" );
	    goto error;
    }
    
    ctrlPipe = usb_sndctrlpipe(dev->udev,0);
    retval = usb_control_msg(dev->udev,ctrlPipe,1,0x21,0x0200,1,bufferIn,26,500);
    if ( !retval ) {
	    printk ( "Nu am putut trimite mesajul de control 1!" );
	    goto error;
    }
    retval = usb_control_msg(dev->udev,ctrlPipe,1,0x21,0x0b00,512,bufferOut,2,500);
    if ( !retval ) {
	    printk ( "Nu am putut trimite mesajul de control 1!" );
	    goto error;
    }
    retval = usb_control_msg(dev->udev,ctrlPipe,1,0x21,0x0400,1280,bufferOut,2,500);
    if ( !retval ) {
	    printk ( "Nu am putut trimite mesajul de control 1!" );
	    goto error;
    }
    retval = usb_control_msg(dev->udev,ctrlPipe,1,0x21,0x0b00,512,bufferOut,2,500);
    if ( !retval ) {
	    printk ( "Nu am putut trimite mesajul de control 1!" );
	    goto error;
    }
    retval = usb_control_msg(dev->udev,ctrlPipe,1,0x21,0x0400,1280,bufferOut,2,500);
    if ( !retval ) {
	    printk ( "Nu am putut trimite mesajul de control 1!" );
	    goto error;
    }
    retval = usb_control_msg(dev->udev,ctrlPipe,1,0x21,0x0b00,512,bufferOut,2,500);
    if ( !retval ) {
	    printk ( "Nu am putut trimite mesajul de control 1!" );
	    goto error;
    }
    retval = usb_control_msg(dev->udev,ctrlPipe,1,0x21,0x0400,1280,bufferOut,2,500);
    if ( !retval ) {
	    printk ( "Nu am putut trimite mesajul de control 1!" );
	    goto error;
    }
    retval = usb_control_msg(dev->udev,ctrlPipe,1,0x21,0x0b00,512,bufferOut,2,500);
    if ( !retval ) {
	    printk ( "Nu am putut trimite mesajul de control 1!" );
	    goto error;
    }
    retval = usb_control_msg(dev->udev,ctrlPipe,1,0x21,0x0400,1280,bufferOut,2,500);
    if ( !retval ) {
	    printk ( "Nu am putut trimite mesajul de control 1!" );
	    goto error;
    }
    retval = usb_control_msg(dev->udev,ctrlPipe,1,0x21,0x0a00,1,bufferOut,2,500);
    if ( !retval ) {
	    printk ( "Nu am putut trimite mesajul de control 1!" );
	    goto error;
    }
    
    /*
    retval = usb_control_msg(dev->udev,ctrlPipe,USB_REQ_SET_INTERFACE,0x0,0x00,0x01,NULL,0,500);
    if(retval != 0)
    {
	    printk ( "Nu am putut schimba interfata!\n" );
	    goto error;    
    }*/

    /* save our data pointer in this interface device */
    usb_set_intfdata ( interface, dev );

    /* we can register the device now, as it is ready */
    retval = usb_register_dev ( interface, &leap_class );
    if ( retval ) {
        /* something prevented us from registering this driver */
        printk ( "Not able to get a minor for this device.\n" );
        usb_set_intfdata ( interface, NULL );
        goto error;
    }

    /* let the user know what node this device is now attached to */
    printk ( "USB Leapmotion device now attached to USBLeap-%d\n",interface->minor );
    return 0;

error:
    if ( dev )
        /* this frees allocated memory */
    {
        kref_put ( &dev->kref, leap_delete );
    }
    return retval;
}

static void leap_disconnect ( struct usb_interface *interface )
{
    struct usb_leap *dev;
    int minor = interface->minor;

    dev = usb_get_intfdata ( interface );
    usb_set_intfdata ( interface, NULL );

    /* give back our minor */
    usb_deregister_dev ( interface, &leap_class );

    /* prevent more I/O from starting */
    mutex_lock ( &dev->io_mutex );
    dev->interface = NULL;
    mutex_unlock ( &dev->io_mutex );

    usb_kill_anchored_urbs ( &dev->submitted );

    /* decrement our usage count */
    kref_put ( &dev->kref, leap_delete );

    printk ( "USB Leapmotion #%d now disconnected\n", minor );
}

static void leap_draw_down ( struct usb_leap *dev )
{
	int time;
	
	time = usb_wait_anchor_empty_timeout(&dev->submitted, 1000);
	if (!time)
		usb_kill_anchored_urbs(&dev->submitted);
	usb_kill_urb(dev->bulk_in_urb);
}

static struct usb_driver leap_driver = {
    .name =		"leap",
    .probe =	leap_probe,
    .disconnect =	leap_disconnect,
    .id_table =	leap_table,
    .supports_autosuspend = 1,
};

module_usb_driver ( leap_driver );

MODULE_LICENSE ( "GPL" );
