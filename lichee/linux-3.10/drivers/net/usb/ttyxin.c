/*
 * Linux Virtual COM Port Driver
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/usb.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/tty_flip.h>
#include <linux/serial.h>
#include <asm/uaccess.h>
#include <asm/byteorder.h>
#include <asm/unaligned.h>

//#define BIG_ENDIAN

#define VID 0x055f
#define PID 0x017B

//#define VID 0x046D
//#define PID 0x0A0C

#define DRIVER_VERSION "v1.0"
#define DRIVER_AUTHOR "Zhang"
#define DRIVER_DESC "Linux Virtual COM Driver"

/* Module information */
MODULE_AUTHOR( DRIVER_AUTHOR );
MODULE_DESCRIPTION( DRIVER_DESC );
MODULE_LICENSE("GPL");

#define XIN_TTY_MAJOR		240	/* experimental range */
#define XIN_TTY_MINORS		3	/* only have 3 devices ttyVCOM0 ttyVCOM1 ttyVCOM2*/
#define XIN_BUFFER_SIZE    10240

#define HID_REQ_GET_REPORT		0x01
#define HID_REQ_SET_REPORT		0x09

#define OUTPUT_REPORT 0x02
#define HID_INTERFACE_NUMBER 0x01

/* Our fake UART values */
#define MCR_DTR		0x01
#define MCR_RTS		0x02
#define MCR_LOOP	0x04
#define MSR_CTS		0x08
#define MSR_CD		0x10
#define MSR_RI		0x20
#define MSR_DSR		0x40

static DEFINE_MUTEX(global_mutex);

typedef unsigned short USHORT;
typedef unsigned char UCHAR;
typedef unsigned int UINT;

typedef struct __USB_HDR
{
	USHORT magicNumber; //0x55AA
	USHORT length;
	USHORT m_type: 4;
	USHORT sub_type: 12;
	UCHAR inOutFlag; // 0: read; 1: write
	UCHAR reserved;
}__attribute__((__packed__)) USB_HDR; // pay attention to ALIGNMENT

#define MAX_BUFFER_SIZE 10240
#define DATA_BUFFER_SIZE (MAX_BUFFER_SIZE - sizeof(USB_HDR))
#define MAX_REAL_DATA_SIZE DATA_BUFFER_SIZE

#define WRITE_PACKET_SIZE 60
#define READ_PACKET_SIZE (512 - 4)

//#define WRITE_PACKET_SIZE 60
//#define READ_PACKET_SIZE (2)

// for main type
#define MAIN_TYPE_IP 0
#define MAIN_TYPE_DEBUG 1

// for sub type
#define SUB_TYPE_IP        0
#define SUB_TYPE_MLOG      1
#define SUB_TYPE_Trace     2
#define SUB_TYPE_AtCommand 3
#define SUB_TYPE_ROM_UPDATE (256)
#define SUB_TYPE_INVALID   0xFF

// for in out flag
#define DIRECTION_READ  0
#define DIRECTION_WRITE 1

// for magic number
#define MAGIC_NUMBER 0x55AA

#define tran_swab16(x) ((USHORT)(						   \
		   (((USHORT)(x) & (USHORT)0x00ff) << 8) |				   \
		   (((USHORT)(x) & (USHORT)0xff00) >> 8)))
 
#define tran_swab32(x) ((unsigned)(						   \
		   (((unsigned)(x) & (unsigned)0x000000ff) << 24) | 		   \
		   (((unsigned)(x) & (unsigned)0x0000ff00) <<  8) | 		   \
		   (((unsigned)(x) & (unsigned)0x00ff0000) >>  8) | 		   \
		   (((unsigned)(x) & (unsigned)0xff000000) >> 24)))

typedef struct __USB_BUF
{
	USB_HDR Hdr;
	UCHAR Buffer[DATA_BUFFER_SIZE];
} __attribute__((__packed__)) USB_BUF;  // pay attention to ALIGNMENT

struct xin_serial {
	struct tty_struct	*tty;		/* pointer to the tty for this device */

	/* for tiocmget and tiocmset functions */
	int			msr;		/* MSR shadow */
	int			mcr;		/* MCR shadow */

        struct tty_port         port;

        struct device*          dev;

        int opened;
};

struct xin_hid {
	struct usb_device *usbdev;

	// interrupt in xfer
	struct urb *irq;
	char * irq_buf; 
	dma_addr_t irq_dma;

	// buffer for read hid data
	USB_BUF read_buffer;
	int read_buffer_total_length;
	int read_buffer_index;

	// buffer for write hid data
	USB_BUF write_buffer;
	char* send_buffer;
};

static struct xin_serial *xin_table[XIN_TTY_MINORS];	/* initially all NULL */
static struct xin_hid* g_xinhid = NULL;
static struct tty_driver *xin_tty_driver = NULL;


/********************
ttyVCOM0: AT command
ttyVCOM1: 


*********************/

static int xin_get_subtype_by_tty(struct xin_serial *xin)
{
	int i = 0;
	int subtype = SUB_TYPE_INVALID;
	for(i = 0; i < XIN_TTY_MINORS; i++)
	{
		if(xin == xin_table[i])
		{
		    break;
		}
	}

	if(i == XIN_TTY_MINORS)
	{
		return SUB_TYPE_INVALID;
	}

	if(i == 0)
	{
		subtype = SUB_TYPE_AtCommand;
	}
        else if(i == 1)
	{
		subtype = SUB_TYPE_ROM_UPDATE;
	}

	return subtype;
}

static struct xin_serial * xin_get_xin_serial_by_subtype(int subtype)
{
	struct xin_serial * serial = NULL;
	if(subtype == SUB_TYPE_AtCommand)
	{
		serial = xin_table[0]; 
	}
        else if(subtype == SUB_TYPE_ROM_UPDATE)
	{
		serial = xin_table[1]; 
	}
	else //all other type for logs
	{
		serial = xin_table[2];
	}
	
	if(!serial)
	{		
		//printk(KERN_ERR "[%s] destination VCOM does not exist\n", __FUNCTION__);
		goto exit;
	}

	if (!serial->tty)
	{
		//printk(KERN_ERR "[%s] no destination ttyVCOM is not opened\n", __FUNCTION__);

		/* port was not opened */
                serial = NULL;
		goto exit;
	}

exit:
	return serial;
}

static int xin_install(struct tty_driver *driver, struct tty_struct* tty)
{
	struct xin_serial *xin;
	int index;
	int result = 0;

	mutex_lock(&global_mutex);

	/* initialize the pointer in case something fails */
	tty->driver_data = NULL;

	/* get the serial object associated with this tty pointer */
	index = tty->index;
	printk(KERN_ERR "[%s] install ttyVCOM%d\n", __FUNCTION__, index);
	xin = xin_table[index];
	if (xin == NULL) {
		printk(KERN_ERR "[%s] ttyVCOM%d is not ready!\n", __FUNCTION__, index );

                result = -EINVAL;
		goto Exit;
	}
        
        //printk(KERN_ERR "[%s] before call install, xin_tty_driver->kref.refcount = %d\n", __FUNCTION__, xin_tty_driver->kref.refcount);
       // printk(KERN_ERR "[%s] index = %d, before call install: port->kref.refcount = %d\n", __FUNCTION__, index, xin->port.kref.refcount);

        result = tty_port_install(&xin->port, driver, tty);
        if(result)
        {
		printk(KERN_ERR "[%s] failed to call tty_port_install for ttyVCOM%d !\n", __FUNCTION__, index );

		goto Exit;         
        }

	/* save our structure within the tty structure */
	tty->driver_data = xin;
	xin->tty = tty;
        //printk(KERN_ERR "[%s] after call install, xin_tty_driver->kref.refcount = %d\n", __FUNCTION__, xin_tty_driver->kref.refcount);
       // printk(KERN_ERR "[%s] index = %d, after call install: port->kref.refcount = %d\n", __FUNCTION__, index, xin->port.kref.refcount);

	printk(KERN_ERR "[%s] install ttyVCOM%d OK\n", __FUNCTION__, index);

Exit:
	mutex_unlock(&global_mutex);
	return result;
}

static int xin_open(struct tty_struct *tty, struct file *file)
{
	struct xin_serial *xin = tty->driver_data;
	int index;
	int result = 0;

	mutex_lock(&global_mutex);

	index = tty->index;
	printk(KERN_ERR "[%s] open ttyVCOM%d\n", __FUNCTION__, index);
	if (xin != xin_table[index]) {
		printk(KERN_ERR "[%s] open information for ttyVCOM%d is changed!\n", __FUNCTION__, index );

                result = -EINVAL;
		goto Exit;
	}

        //printk(KERN_ERR "[%s] before call open, xin_tty_driver->kref.refcount = %d\n", __FUNCTION__, xin_tty_driver->kref.refcount);
        //printk(KERN_ERR "[%s] index = %d, before call open: port->kref.refcount = %d\n", __FUNCTION__, index, xin->port.kref.refcount);
        result = tty_port_open(&xin->port, tty, file);
        if(result == 0)
        { 
            printk(KERN_ERR "[%s] open ttyVCOM%d OK\n", __FUNCTION__, index);
        }
        else
        {
            printk(KERN_ERR "[%s] failed to call tty_port_open for ttyVCOM%d\n", __FUNCTION__, index);
        }
       //printk(KERN_ERR "[%s] after call open, xin_tty_driver->kref.refcount = %d\n", __FUNCTION__, xin_tty_driver->kref.refcount);
        //printk(KERN_ERR "[%s] index = %d, after call open: port->kref.refcount = %d\n", __FUNCTION__, index, xin->port.kref.refcount);

Exit:
	mutex_unlock(&global_mutex);
	return result;
}

static void xin_close(struct tty_struct *tty, struct file *file)
{
	struct xin_serial *xin = tty->driver_data;
        int index;

	printk(KERN_ERR "[%s] close ttyVCOM%d\n", __FUNCTION__, tty->index);

        index = tty->index;	
	if (xin != xin_table[index]) {
	    printk(KERN_ERR "[%s] close information for ttyVCOM%d is changed!\n", __FUNCTION__, index );
            return;
	}

        //mutex_lock(&global_mutex);
        if(xin == NULL || g_xinhid == NULL)
        {
             printk(KERN_ERR "[%s] xin is free, xin_tty_driver->kref.refcount = %d\n", __FUNCTION__, xin_tty_driver->kref.refcount);
             return;
        }

        //printk(KERN_ERR "[%s] before call close, xin_tty_driver->kref.refcount = %d\n", __FUNCTION__, xin_tty_driver->kref.refcount);
        //printk(KERN_ERR "[%s] index = %d, before call close: port->kref.refcount = %d\n", __FUNCTION__, tty->index, xin->port.kref.refcount);

        tty_port_close(&xin->port, tty, file);

        //printk(KERN_ERR "[%s] after call close, xin_tty_driver->kref.refcount = %d\n", __FUNCTION__, xin_tty_driver->kref.refcount);
        //printk(KERN_ERR "[%s] index = %d, after call close: port->kref.refcount = %d\n", __FUNCTION__, tty->index, xin->port.kref.refcount);

        

        //mutex_unlock(&global_mutex); 
}	

static void xin_tty_cleanup(struct tty_struct *tty)
{
	
	struct xin_serial *xin = tty->driver_data;
 	int index = 0;

	index = tty->index;	
	if (xin != xin_table[index]) {
	    printk(KERN_ERR "[%s] xin_tty_cleanup error ttyVCOM%d is changed!\n", __FUNCTION__, index );
            return;
	}

	printk(KERN_ERR "[%s] xin_tty_cleanup ttyVCOM%d\n", __FUNCTION__, tty->index);
	 //printk(KERN_ERR "[%s]  index = %d, before tty_port_cleanup: port->kref.refcount = %d\n", __FUNCTION__, tty->index, xin->port.kref.refcount);	 
	
	//tty_port_put(&xin->port);
	tty_port_destroy(&xin->port);
	 //printk(KERN_ERR "[%s]  index = %d, after tty_port_cleanup: port->kref.refcount = %d\n", __FUNCTION__, tty->index, xin->port.kref.refcount);
}

static void xin_tty_hangup(struct tty_struct *tty)
{
	
	struct xin_serial *xin = tty->driver_data;
 	int index = 0;

	index = tty->index;	
	if (xin != xin_table[index]) {
	    printk(KERN_ERR "[%s] xin_tty_hangup error ttyVCOM%d is changed!\n", __FUNCTION__, index );
            return;
	}

	printk(KERN_ERR "[%s] xin_tty_hangup ttyVCOM%d\n", __FUNCTION__, tty->index);
	 //printk(KERN_ERR "[%s]  index = %d, before tty_port_hangup: port->kref.refcount = %d\n", __FUNCTION__, tty->index, xin->port.kref.refcount);	 
	
	tty_port_hangup(&xin->port);
	 //printk(KERN_ERR "[%s]  index = %d, after tty_port_hangup: port->kref.refcount = %d\n", __FUNCTION__, tty->index, xin->port.kref.refcount);
}

static int xin_write(struct tty_struct *tty, 
		      const unsigned char *buffer, int count)
{
	struct xin_serial *xin = tty->driver_data;
	struct xin_hid* xinhid = g_xinhid;
	int subtype = SUB_TYPE_INVALID;
	int retval = -EINVAL;
	int copylength = 0;
	int copy_index = 0;
	char* startptr = NULL;
	UCHAR *puChar = NULL;
	USHORT toSendLen = 0;

	mutex_lock(&global_mutex);
	
	//printk(KERN_ERR "[%s] ttyVCOM%d write %d bytes\n", __FUNCTION__, tty->index, count);

	if(xinhid == NULL)
	{
		printk(KERN_ERR "[%s] xin hid device is not connected\n", __FUNCTION__);	
		retval = -ENODEV;
		goto exit;
	}

	if (!xin)
	{	
		printk(KERN_ERR "[%s] ttyVCOM%d doesnot exist\n", __FUNCTION__, tty->index);
		retval = -ENODEV;
		goto exit;
	}

	if(!count)
	{
		printk(KERN_ERR "[%s] data length is ZERO, so shortcut!\n", __FUNCTION__);
	        retval = 0;
		goto exit;		
	}

	subtype = xin_get_subtype_by_tty(xin);
	if(subtype == SUB_TYPE_INVALID)
	{
		printk(KERN_ERR "[%s] cannot get subtype for ttyVCOM%d\n", __FUNCTION__, tty->index);
		retval = count; //only ttyVCOM0 ttyVCOM1 can write
		goto exit;
	}
	
	//printk(KERN_ERR "[%s] send data from ttyVCOM%d to hid device- \n", __FUNCTION__, tty->index);

	copylength = count > MAX_REAL_DATA_SIZE ? MAX_REAL_DATA_SIZE : count;
	memset(&xinhid->write_buffer, 0, sizeof(USB_BUF));
	memcpy(&xinhid->write_buffer.Buffer[0], buffer, copylength);
	toSendLen = sizeof(USB_HDR) + copylength;
	#ifdef BIG_ENDIAN
	xinhid->write_buffer.Hdr.magicNumber = tran_swab16(MAGIC_NUMBER);//MAGIC_NUMBER;
	xinhid->write_buffer.Hdr.length = tran_swab16(toSendLen);//sizeof(USB_HDR) + copylength;
	puChar = ((UCHAR*)(&xinhid->write_buffer.Hdr)+2*sizeof(USHORT));
	//mtype
	*puChar = MAIN_TYPE_DEBUG&0xf;
	//subtype
	*puChar = (*puChar&0xf)|((subtype&0xf)<<4);
	*(puChar+1) = (subtype>>4)&0xff;	
    	xinhid->write_buffer.Hdr.inOutFlag = DIRECTION_WRITE;	
	#else
	xinhid->write_buffer.Hdr.magicNumber = MAGIC_NUMBER;
	xinhid->write_buffer.Hdr.length = toSendLen;
	xinhid->write_buffer.Hdr.m_type = MAIN_TYPE_DEBUG;
    	xinhid->write_buffer.Hdr.inOutFlag = DIRECTION_WRITE;
	xinhid->write_buffer.Hdr.sub_type = subtype;
	#endif

	copylength = 0;
	copy_index = 0;
	while(1)
	{
		if(xinhid->write_buffer.Hdr.length - copy_index >= WRITE_PACKET_SIZE)
		{
			copylength = WRITE_PACKET_SIZE;
		}
		else
		{
		    copylength = xinhid->write_buffer.Hdr.length - copy_index;
		}
		startptr = ((char*)(&xinhid->write_buffer)) + copy_index;
		memset(xinhid->send_buffer, 0, WRITE_PACKET_SIZE);
		memcpy(xinhid->send_buffer, startptr, copylength);		
	        retval = usb_control_msg(xinhid->usbdev, usb_sndctrlpipe(xinhid->usbdev, 0),
				HID_REQ_SET_REPORT,
				USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE,
				(OUTPUT_REPORT << 8) | 0,
				HID_INTERFACE_NUMBER, xinhid->send_buffer, WRITE_PACKET_SIZE,
				USB_CTRL_SET_TIMEOUT);
		if(retval < 0)
		{
			printk(KERN_ERR "[%s] failed to write to HID device!, result = %d\n",__FUNCTION__, retval);
			goto exit;
		}
	    
		if(retval < WRITE_PACKET_SIZE)
		{
		    printk(KERN_ERR "[%s] failed to write to HID device!, result = %d != %d\n", __FUNCTION__, retval, copylength);
		}
		copy_index += copylength;

		if(copy_index >= xinhid->write_buffer.Hdr.length)
		{
		    //printk(KERN_ERR "[%s] write complete!\n", __FUNCTION__);
			break;
		}
	}
	
	retval = count;
		
exit:
	mutex_unlock(&global_mutex);
	return retval;
}

static int xin_write_room(struct tty_struct *tty) 
{
	struct xin_serial *xin = tty->driver_data;
	int room = -EINVAL;

	if (!xin)
		return -ENODEV;

	/* calculate how much room is left in the device */
	room = XIN_BUFFER_SIZE;

	return room;
}

#define RELEVANT_IFLAG(iflag) ((iflag) & (IGNBRK|BRKINT|IGNPAR|PARMRK|INPCK))

static void xin_set_termios(struct tty_struct *tty, struct ktermios *old_termios)
{
	unsigned int cflag;

	cflag = tty->termios.c_cflag;

	/* check that they really want us to change something */
	if (old_termios) {
		if ((cflag == old_termios->c_cflag) &&
		    (RELEVANT_IFLAG(tty->termios.c_iflag) == 
		     RELEVANT_IFLAG(old_termios->c_iflag))) {
			printk(KERN_DEBUG " - nothing to change...\n");
			return;
		}
	}

	/* get the byte size */
	switch (cflag & CSIZE) {
		case CS5:
			printk(KERN_DEBUG " - data bits = 5\n");
			break;
		case CS6:
			printk(KERN_DEBUG " - data bits = 6\n");
			break;
		case CS7:
			printk(KERN_DEBUG " - data bits = 7\n");
			break;
		default:
		case CS8:
			printk(KERN_DEBUG " - data bits = 8\n");
			break;
	}
	
	/* determine the parity */
	if (cflag & PARENB)
		if (cflag & PARODD)
			printk(KERN_DEBUG " - parity = odd\n");
		else
			printk(KERN_DEBUG " - parity = even\n");
	else
		printk(KERN_DEBUG " - parity = none\n");

	/* figure out the stop bits requested */
	if (cflag & CSTOPB)
		printk(KERN_DEBUG " - stop bits = 2\n");
	else
		printk(KERN_DEBUG " - stop bits = 1\n");

	/* figure out the hardware flow control settings */
	if (cflag & CRTSCTS)
		printk(KERN_DEBUG " - RTS/CTS is enabled\n");
	else
		printk(KERN_DEBUG " - RTS/CTS is disabled\n");
	
	/* determine software flow control */
	/* if we are implementing XON/XOFF, set the start and 
	 * stop character in the device */
	if (I_IXOFF(tty) || I_IXON(tty)) {
		unsigned char stop_char  = STOP_CHAR(tty);
		unsigned char start_char = START_CHAR(tty);

		/* if we are implementing INBOUND XON/XOFF */
		if (I_IXOFF(tty))
			printk(KERN_DEBUG " - INBOUND XON/XOFF is enabled, \n"
				"XON = %2x, XOFF = %2x", start_char, stop_char);
		else
			printk(KERN_DEBUG" - INBOUND XON/XOFF is disabled\n");

		/* if we are implementing OUTBOUND XON/XOFF */
		if (I_IXON(tty))
			printk(KERN_DEBUG" - OUTBOUND XON/XOFF is enabled, \n"
				"XON = %2x, XOFF = %2x", start_char, stop_char);
		else
			printk(KERN_DEBUG" - OUTBOUND XON/XOFF is disabled\n");
	}

	/* get the baud rate wanted */
	printk(KERN_DEBUG " - baud rate = %d", tty_get_baud_rate(tty));
}


static int xin_tiocmget(struct tty_struct *tty)
{
	struct xin_serial *xin = tty->driver_data;

	unsigned int result = 0;
	unsigned int msr = xin->msr;
	unsigned int mcr = xin->mcr;

	result = ((mcr & MCR_DTR)  ? TIOCM_DTR  : 0) |	/* DTR is set */
             ((mcr & MCR_RTS)  ? TIOCM_RTS  : 0) |	/* RTS is set */
             ((mcr & MCR_LOOP) ? TIOCM_LOOP : 0) |	/* LOOP is set */
             ((msr & MSR_CTS)  ? TIOCM_CTS  : 0) |	/* CTS is set */
             ((msr & MSR_CD)   ? TIOCM_CAR  : 0) |	/* Carrier detect is set*/
             ((msr & MSR_RI)   ? TIOCM_RI   : 0) |	/* Ring Indicator is set */
             ((msr & MSR_DSR)  ? TIOCM_DSR  : 0);	/* DSR is set */

	return result;
}

static int xin_tiocmset(struct tty_struct *tty, unsigned int set, unsigned int clear)
{
	struct xin_serial *xin = tty->driver_data;
	unsigned int mcr = xin->mcr;

	if (set & TIOCM_RTS)
		mcr |= MCR_RTS;
	if (set & TIOCM_DTR)
		mcr |= MCR_RTS;

	if (clear & TIOCM_RTS)
		mcr &= ~MCR_RTS;
	if (clear & TIOCM_DTR)
		mcr &= ~MCR_RTS;

	/* set the new MCR value in the device */
	xin->mcr = mcr;
	return 0;
}

static int xin_hid_alloc_mem(struct usb_device *dev, struct xin_hid *xinhid)
{
	if (!(xinhid->irq = usb_alloc_urb(0, GFP_KERNEL)))
		return -1;

	if (!(xinhid->irq_buf = usb_alloc_coherent(dev, READ_PACKET_SIZE, GFP_ATOMIC, &xinhid->irq_dma)))
		return -1;

	if (!(xinhid->send_buffer = kmalloc(WRITE_PACKET_SIZE, GFP_KERNEL)))
		return -1;	

	return 0;
}

static void xin_hid_free_mem(struct usb_device *dev, struct xin_hid *xinhid)
{
	usb_free_urb(xinhid->irq);
	usb_free_coherent(dev, READ_PACKET_SIZE, xinhid->irq_buf, xinhid->irq_dma);
	kfree(xinhid->send_buffer);
}


static void xin_irq(struct urb *urb)
{
	struct xin_hid *xinhid = urb->context;
	int i;
	int datalength = 0;
	int copylength = 0;
	char* startptr = NULL;
	USHORT magicNum = 0xff;
	UCHAR mType = 0;
	UCHAR *puChar = NULL;
	
        struct xin_serial * xin = NULL;
	int subtype = SUB_TYPE_INVALID;

	if(xinhid == NULL || g_xinhid == NULL)
	{
	    printk(KERN_ERR "[%s] xinhid is NULL\n", __FUNCTION__);
		return;
	}

	switch (urb->status) {
	case 0:			/* success */
		break;
	case -ECONNRESET:	/* unlink */
	case -ENOENT:
	case -ESHUTDOWN:
		printk(KERN_ERR "[%s] irq complete error in place 1, status: %d\n", __FUNCTION__, urb->status);
		return;
	/* -EPIPE:  should clear the halt */
	default:		/* error */
		printk(KERN_ERR "[%s] irq complete error in place 2, status: %d\n", __FUNCTION__, urb->status);
		goto resubmit;
	}

	// 
	datalength = urb->actual_length;
	//printk(KERN_ERR "[%s] get %d bytes from xin100 hid device\n", __FUNCTION__, datalength);
	if(xinhid->read_buffer_total_length == 0)
	{
		copylength = datalength;
	}
	else if( datalength + xinhid->read_buffer_index >= xinhid->read_buffer_total_length)
	{
		copylength = xinhid->read_buffer_total_length - xinhid->read_buffer_index;
	}
	else
	{
		copylength = datalength;
	}

	startptr = ((char*)&xinhid->read_buffer) + xinhid->read_buffer_index;
	memcpy(startptr, urb->transfer_buffer, copylength);
	if(xinhid->read_buffer_total_length == 0)
	{
	        #ifdef BIG_ENDIAN	
	    	xinhid->read_buffer_total_length = tran_swab16(xinhid->read_buffer.Hdr.length);
		#else
		xinhid->read_buffer_total_length = xinhid->read_buffer.Hdr.length;
		#endif
		if(xinhid->read_buffer_total_length > MAX_BUFFER_SIZE)
		{
			//device should not send much messages but AT responses.
			/*printk(KERN_ERR "[%s] wrong length %d in header (should <= 2048 )\n", __FUNCTION__, xinhid->read_buffer_total_length);*/
			xinhid->read_buffer_total_length = MAX_BUFFER_SIZE;
		}
	}
	xinhid->read_buffer_index += copylength;

	if(xinhid->read_buffer_index >= xinhid->read_buffer_total_length)
	{
	    // get the whole data 
	    //printk(KERN_ERR "[%s] get whole %d bytes from xin100 hid device\n", __FUNCTION__, xinhid->read_buffer_total_length);

		// send the data to tty com port
        #ifdef BIG_ENDIAN	
	magicNum = tran_swab16(xinhid->read_buffer.Hdr.magicNumber);
	#else
	magicNum = xinhid->read_buffer.Hdr.magicNumber;
	#endif
        if(magicNum != MAGIC_NUMBER)
        {
			printk(KERN_ERR "[%s] wrong magic number: 0x%04x (should 0x55AA)\n", __FUNCTION__, xinhid->read_buffer.Hdr.magicNumber);
        	goto cleanbuffer;
        }

		#ifdef BIG_ENDIAN	
		puChar = (UCHAR*)(&xinhid->read_buffer.Hdr);
		mType = *(puChar+2*sizeof(USHORT))&0xf;
		#else
		mType = xinhid->read_buffer.Hdr.m_type;
		#endif
		//if(xinhid->read_buffer.Hdr.m_type != MAIN_TYPE_DEBUG)
		if(mType != MAIN_TYPE_DEBUG)
		{
			printk(KERN_ERR "[%s] wrong major type\n", __FUNCTION__);
			goto cleanbuffer;
		}

		#ifdef BIG_ENDIAN	
		puChar = (UCHAR*)(&xinhid->read_buffer.Hdr);
		subtype = ((UINT)(*(puChar+2*sizeof(USHORT)+sizeof(UCHAR)))<<4)|(((*(puChar+2*sizeof(USHORT)))>>4)&0xf);
		#else
		subtype = xinhid->read_buffer.Hdr.sub_type;
		#endif
                xin = xin_get_xin_serial_by_subtype(subtype);
		if(xin != NULL)
		{
			if(subtype == SUB_TYPE_AtCommand || subtype == SUB_TYPE_ROM_UPDATE)
			{
				tty_insert_flip_string(&xin->port, &xinhid->read_buffer.Buffer[0], xinhid->read_buffer_total_length - sizeof(USB_HDR));
				tty_flip_buffer_push(&xin->port);
			}
			else
			{
				tty_insert_flip_string(&xin->port, &xinhid->read_buffer, xinhid->read_buffer_total_length);
				tty_flip_buffer_push(&xin->port);
			}
			
		}
		else
		{
			//printk(KERN_ERR "[%s] no available tty for the received data\n", __FUNCTION__);
			goto cleanbuffer;
		}
		
cleanbuffer:
		memset(&xinhid->read_buffer, 0, sizeof(USB_BUF));
		xinhid->read_buffer_index = 0;
		xinhid->read_buffer_total_length = 0;		
	}

resubmit:
	i = usb_submit_urb (urb, GFP_ATOMIC);
	if (i)
		printk(KERN_ERR "[%s] can't resubmit intr, status %d\n", __FUNCTION__, i);
}

static void xin_port_destruct(struct tty_port *port)
{
    printk(KERN_ERR "[%s] just return\n", __FUNCTION__);
}
static const struct tty_port_operations xin_port_ops = {
    .destruct = xin_port_destruct,
};


static int xin_probe(struct usb_interface *iface,
			 const struct usb_device_id *id)
{
	struct usb_device *dev = interface_to_usbdev(iface);
	struct usb_host_interface *interface;
	struct usb_endpoint_descriptor *endpoint;
	struct xin_hid *xinhid;
        struct xin_serial *xin = NULL;
	int pipe, maxp;
	int error = -ENOMEM;
	int i = 0;

	printk(KERN_ERR "[%s] xin_probe start...\n", __FUNCTION__);
	
	mutex_lock(&global_mutex);
	if(g_xinhid)
	{
		printk(KERN_ERR "[%s] there is already connected xin100 hid device!\n", __FUNCTION__);
		mutex_unlock(&global_mutex);	
		return error;
	}	
	mutex_unlock(&global_mutex);	

	interface = iface->cur_altsetting;
	if (interface->desc.bNumEndpoints != 1)
	{
		printk(KERN_ERR "[%s] endpoint numbers for HID interface is wrong! (should be 1)!\n", __FUNCTION__);
		return -ENODEV;
	}

	endpoint = &interface->endpoint[0].desc;
	if (!usb_endpoint_is_int_in(endpoint))
	{
		printk(KERN_ERR "[%s] endpoint type for HID interface is wrong! (should be interrupt in)!\n", __FUNCTION__);
		return -ENODEV;
	}

	pipe = usb_rcvintpipe(dev, endpoint->bEndpointAddress);
	maxp = usb_maxpacket(dev, pipe, usb_pipeout(pipe));

	xinhid = kzalloc(sizeof(struct xin_hid), GFP_KERNEL);
	if (!xinhid)
	{
		printk(KERN_ERR "[%s] failed to allocate memory for struct xinhid!\n", __FUNCTION__);
		goto fail1;
	}

	if (xin_hid_alloc_mem(dev, xinhid))
	{
		printk(KERN_ERR "[%s] failed to allocate memory for urb and buffer!\n", __FUNCTION__);
		goto fail2;
	}

	xinhid->read_buffer_total_length = 0;
	xinhid->read_buffer_index = 0;
	
	xinhid->usbdev = dev;

	usb_fill_int_urb(xinhid->irq, dev, pipe,
			 xinhid->irq_buf, (maxp > 1024 ? 1024 : maxp),
			 xin_irq, xinhid, endpoint->bInterval);
	xinhid->irq->transfer_dma = xinhid->irq_dma;
	xinhid->irq->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

	g_xinhid = xinhid; // place it here to avoid urb complete before this probe return!!!

	if (usb_submit_urb(xinhid->irq, GFP_ATOMIC))
	{
		error = -EIO;		
		printk(KERN_ERR "[%s] failed to submit interrupt in urb!\n", __FUNCTION__);
		g_xinhid = NULL;
		goto fail2;
	}

	usb_set_intfdata(iface, xinhid);

	printk(KERN_ERR "[%s] xin_probe before register tty vcom deivce, xin_tty_driver = 0x%08x\n", __FUNCTION__, (int)xin_tty_driver);

	for (i = 0; i < XIN_TTY_MINORS; ++i)
        {
                xin_table[i] = NULL;

                xin = kmalloc(sizeof(struct xin_serial), GFP_KERNEL);
		if (!xin)
		{
		    printk(KERN_ERR "[%s] failed to allocate memory for ttyVCOM%d\n", __FUNCTION__ ,i);
		    error = -ENOMEM;
		    goto fail3;
		}

                tty_port_init(&xin->port);
                xin->port.ops = &xin_port_ops;

		xin->tty = NULL;
                xin->dev = &iface->dev;
		xin_table[i] = xin;
		xin->msr = (MSR_CD | MSR_DSR | MSR_CTS);

                printk(KERN_ERR "[%s] register ttyVCOM%d\n", __FUNCTION__ ,i);

               //printk(KERN_ERR "[%s] before call register, xin_tty_driver->kref.refcount = %d\n", __FUNCTION__, xin_tty_driver->kref.refcount);
                //printk(KERN_ERR "[%s] index = %d, before call register: port->kref.refcount = %d\n", __FUNCTION__, i, xin->port.kref.refcount);

		tty_port_register_device(&xin->port, xin_tty_driver, i, xin->dev);

                //printk(KERN_ERR "[%s] after call register, xin_tty_driver->kref.refcount = %d\n", __FUNCTION__, xin_tty_driver->kref.refcount);
                //printk(KERN_ERR "[%s] index = %d, after call register: port->kref.refcount = %d\n", __FUNCTION__, i, xin->port.kref.refcount);
        }

	printk(KERN_ERR "[%s] xin_probe success!\n", __FUNCTION__);
	
	return 0;

fail3:
        usb_kill_urb(xinhid->irq);
        
	for (i = 0; i < XIN_TTY_MINORS; ++i)
        {
                xin = xin_table[i];
                if(xin)
               	{                        
                	tty_unregister_device(xin_tty_driver, i);
                        tty_port_destroy(&xin->port);
                        kfree(xin);
                        xin_table[i] = NULL;
                }
        }

fail2:	
	xin_hid_free_mem(dev, xinhid);
fail1:	
	kfree(xinhid);
	return error;
}

static void xin_disconnect(struct usb_interface *intf)
{
	struct xin_hid *xinhid = usb_get_intfdata (intf);
        struct xin_serial *xin = NULL;
        struct tty_struct* tty;
	int i = 0;
	printk(KERN_ERR "[%s] xin_disconnect\n", __FUNCTION__);

	mutex_lock(&global_mutex);

        printk(KERN_ERR "[%s] xin_disconnect, after get mutex\n", __FUNCTION__);
	g_xinhid = NULL;
	for (i = 0; i < XIN_TTY_MINORS; ++i)
	{
                printk(KERN_ERR "[%s] xin_disconnect, i = %d\n", __FUNCTION__, i);
		xin = xin_table[i];
		if (xin) {
                        printk(KERN_ERR "[%s] xin_disconnect, i = %d, xin = 0x%08x\n", __FUNCTION__, i, (int)xin);
                        tty = tty_port_tty_get(&xin->port);
                        if(tty)
                        {
                           // printk(KERN_ERR "[%s] xin_disconnect, i = %d, before hangup tty\n", __FUNCTION__, i);
                            tty_vhangup(tty);
                            //printk(KERN_ERR "[%s] xin_disconnect, i = %d, before tty_kref_put tty\n", __FUNCTION__, i);                            
                            tty_kref_put(xin->tty);
                            //printk(KERN_ERR "[%s] xin_disconnect, i = %d, after hangup tty\n", __FUNCTION__, i);
                        }
                        //printk(KERN_ERR "[%s] xin_disconnect, i = %d, before unregistger device\n", __FUNCTION__, i);
                        tty_unregister_device(xin_tty_driver, i);
                        //printk(KERN_ERR "[%s] xin_disconnect, i = %d, after unregistger device\n", __FUNCTION__, i);
			tty_port_destroy(&xin->port);
						
                        printk(KERN_ERR "[%s] unregister ttyVCOM%d\n", __FUNCTION__ ,i);

			/* free the memory */
			kfree(xin);
			xin_table[i] = NULL;
		}	
        }

	usb_set_intfdata(intf, NULL);
	if (xinhid) {
                printk(KERN_ERR "[%s] xin_disconnect, before Kill urb\n", __FUNCTION__);
		usb_kill_urb(xinhid->irq);
		xin_hid_free_mem(interface_to_usbdev(intf), xinhid);
		kfree(xinhid);
		xinhid = NULL;
	}
        

	mutex_unlock(&global_mutex);

        printk(KERN_ERR "[%s] xin_disconnect, xin_tty_driver->kref.refcount = %d\n", __FUNCTION__, xin_tty_driver->kref.refcount);

	printk(KERN_ERR "[%s] xin_disconnect success\n", __FUNCTION__);
}

static struct tty_operations serial_ops = {
        .install = xin_install,
	.open = xin_open,
	.close = xin_close,
        .cleanup = xin_tty_cleanup,
	.hangup = xin_tty_hangup,
	.write = xin_write,
	.write_room = xin_write_room,
	.set_termios = xin_set_termios,
	.tiocmget =	xin_tiocmget,	
	.tiocmset =	xin_tiocmset,
};

static const struct usb_device_id	products [] = {

#define	XIN100_HID_INTERFACE \
	.bInterfaceClass	= 0xff, \
	.bInterfaceSubClass	= 0xff, \
	.bInterfaceProtocol	= 0xff

/* 
 * Xin100 USB HID uses just one USB interface.
 */
{
	.match_flags	=   USB_DEVICE_ID_MATCH_INT_INFO
			  | USB_DEVICE_ID_MATCH_DEVICE,
	.idVendor		= VID,
	.idProduct		= PID,
	XIN100_HID_INTERFACE
},{ },		// END
};
MODULE_DEVICE_TABLE(usb, products);

static struct usb_driver xin_hid_driver = {
	.name =		"xin_hid",
	.id_table =	products,
	.probe =	xin_probe,
	.disconnect =	xin_disconnect,
};
static int __init xin_init(void)
{
	int retval;

	printk(KERN_ERR "[%s] init xin com driver\n", __FUNCTION__);

	/* allocate the tty driver */
	xin_tty_driver = alloc_tty_driver(XIN_TTY_MINORS);
	if (!xin_tty_driver)
	{
	    printk(KERN_ERR "[%s] failed to alloc tty driver\n", __FUNCTION__);
		return -ENOMEM;
	}

	printk(KERN_ERR "[%s] xin_tty_driver is 0x%08x\n", __FUNCTION__, (int)xin_tty_driver);

	/* initialize the tty driver */
	xin_tty_driver->owner = THIS_MODULE;
	xin_tty_driver->driver_name = "vcom";
	xin_tty_driver->name = "ttyVCOM";
	xin_tty_driver->major = XIN_TTY_MAJOR,
        xin_tty_driver->minor_start = 0,
	xin_tty_driver->type = TTY_DRIVER_TYPE_SERIAL,
	xin_tty_driver->subtype = SERIAL_TYPE_NORMAL,
	xin_tty_driver->flags = TTY_DRIVER_REAL_RAW | TTY_DRIVER_DYNAMIC_DEV,
	xin_tty_driver->init_termios = tty_std_termios;
	xin_tty_driver->init_termios.c_cflag = B9600 | CS8 | CREAD | HUPCL | CLOCAL;
	xin_tty_driver->init_termios.c_lflag &= ~(ECHO|ECHONL|ICANON|ECHOE);
	tty_set_operations(xin_tty_driver, &serial_ops);

	/* register the tty driver */
	retval = tty_register_driver(xin_tty_driver);
	if (retval) {
		printk(KERN_ERR "[%s] failed to register xin tty driver\n", __FUNCTION__);
		put_tty_driver(xin_tty_driver);
		return retval;
	}

	retval = usb_register(&xin_hid_driver);
	if(retval)
	{
		printk(KERN_ERR "[%s] failed to register xin hid driver\n", __FUNCTION__);
		put_tty_driver(xin_tty_driver);
		tty_unregister_driver(xin_tty_driver);
		return retval;
	}

	printk(KERN_ERR DRIVER_DESC " " DRIVER_VERSION "\n");
        //printk(KERN_ERR "[%s] xin_tty_driver->kref.refcount = %d\n", __FUNCTION__, xin_tty_driver->kref.refcount);
	return retval;
}

static void __exit xin_exit(void)
{
	printk(KERN_ERR "[%s] uninit xin com driver\n", __FUNCTION__);

        printk(KERN_ERR "[%s] xin_tty_driver->kref.refcount = %d\n", __FUNCTION__, xin_tty_driver->kref.refcount); 

        usb_deregister(&xin_hid_driver);
	tty_unregister_driver(xin_tty_driver);
	put_tty_driver(xin_tty_driver);
	xin_tty_driver = NULL;

	printk(KERN_ERR "[%s] uninit xin com driver success\n", __FUNCTION__);
}

module_init(xin_init);
module_exit(xin_exit);




