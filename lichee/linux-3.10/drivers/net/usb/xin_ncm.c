/*
 * xin_ncm.c
 *
 * Host driver for xin100 NCM-similar solution
 *
 */

//#define __XIN_NCM_DEBUG__


#include <linux/module.h>
#include <linux/init.h>
#include <linux/if_ether.h>
#include <linux/netdevice.h>
#include <linux/slab.h>
#include <linux/ctype.h>
#include <linux/ethtool.h>
#include <linux/workqueue.h>
#include <linux/mii.h>
#include <linux/crc32.h>
#include <linux/usb.h>
#include <linux/timer.h>
#include <linux/spinlock.h>
#include <linux/atomic.h>
#include <linux/usb/usbnet.h>

#ifdef __XIN_NCM_DEBUG__
#define LOGINFO     KERN_ERR
#define LOGDEBUG    KERN_ERR 
#define LOGERR      KERN_ERR
#else
#define LOGINFO     KERN_INFO
#define LOGDEBUG    KERN_DEBUG
#define LOGERR      KERN_ERR
#endif

#define XIN100_VENDOR_ID	0x055F
#define XIN100_PRODUCT_ID	0x017B

//#define XIN100_VENDOR_ID	0x045E
//#define XIN100_PRODUCT_ID	0x007A

#define	DRIVER_VERSION				"10-Jan-2015"

#define ETH_LENGTH_OF_ADDRESS 6
#define IP_LENGTH_OF_ADDRESS 4

#define ETH_P_IP   0x0800    // IPv4 protocol
#define ETH_P_ARP  0x0806    // ARP protocol

#define NIC_UshortByteSwap(n) (((n & 0xff) << 8)|((n & 0xff00) >> 8))
#define NIC_UlongByteSwap(n) (((n & 0xff) << 24)|((n & 0xff00) << 8)|((n & 0xff0000) >> 8)|((n & 0xff000000) >> 24))

#define NIC_ntohs(x)    NIC_UshortByteSwap(x)
#define NIC_htons(x)    NIC_UshortByteSwap(x)
#define NIC_ntohl(x)    NIC_UlongByteSwap(x)
#define NIC_htonl(x)    NIC_UlongByteSwap(x)

#define NIC_min(_a, _b)      (((_a) < (_b)) ? (_a) : (_b))
#define NIC_max(_a, _b)      (((_a) > (_b)) ? (_a) : (_b))


#define NIC_NCM_ALIGNMENT              (512)

#define COPY_MAC(dest, src)    memcpy((dest), (src), ETH_LENGTH_OF_ADDRESS)
#define CLEAR_MAC(dest)        memset((dest), 0, ETH_LENGTH_OF_ADDRESS)
#define MAC_EQUAL(a,b)        (memcmp((a), (b), ETH_LENGTH_OF_ADDRESS) == 0)

#define COPY_IP(dest, src)    memcpy((dest), (src), IP_LENGTH_OF_ADDRESS)
#define CLEAR_IP(dest)        memset((dest), IP_LENGTH_OF_ADDRESS)
#define IP_EQUAL(a, b)        (memcmp((a), (b), IP_LENGTH_OF_ADDRESS) == 0)

#define HARDWARE_TYPE 0x0001
#define ARP_REQUEST 0x0001
#define ARP_REPLY   0x0002

struct ARP_PACKET
{
    struct ethhdr        ether_header;
    u16       hardware_type;            // 0x0001
    u16       protocol_type;            // 0x0800    
    u8        HardwareSize;             // 0x06
    u8        protocolSize;             // 0x04
    u16       Operation;                // 0x0001 for ARP request, 0x0002 for ARP reply
    u8        m_ARP_MAC_Source[ETH_LENGTH_OF_ADDRESS];
    u8        m_ARP_IP_Source[IP_LENGTH_OF_ADDRESS];
    u8        m_ARP_MAC_Destination[ETH_LENGTH_OF_ADDRESS];
    u8        m_ARP_IP_Destination[IP_LENGTH_OF_ADDRESS];
} __attribute__ ((packed));

struct ARP_RESPONSE 
{
    struct ARP_PACKET    PacketData;
}__attribute__ ((packed));

#define TX_RX_SPEED (150 * 1024 * 1024) //150Mbps

#define USB_XIN_NOTIFY_NETWORK_CONNECTION	0x00
#define USB_XIN_NOTIFY_RESPONSE_AVAILABLE	0x01
#define USB_XIN_NOTIFY_SERIAL_STATE		0x20
#define USB_XIN_NOTIFY_SPEED_CHANGE		0x2a

struct usb_xin_notification {
	__u8	bmRequestType;
	__u8	bNotificationType;
	__le16	wValue;
	__le16	wIndex;
	__le16	wLength;
} __attribute__ ((packed));

#define USB_XIN_NCM_NTH16_SIGN		0x484D434E /* NCMH */

struct usb_xin_ncm_nth16 {
	__le32	dwSignature;
	__le16	wHeaderLength;
	__le16	wSequence;
	__le16	wBlockLength;
	__le16	wNdpIndex;
} __attribute__ ((packed));

#define USB_XIN_NCM_NDP16_NOCRC_SIGN	0x304D434E /* NCM0 */

struct usb_xin_ncm_dpe16 {
	__le16	wDatagramIndex;
	__le16	wDatagramLength;
} __attribute__((__packed__));

struct usb_xin_ncm_ndp16 {
	__le32	dwSignature;
	__le16	wLength;
	__le16	wPacketNumbers;
	struct	usb_xin_ncm_dpe16 dpe16[0];
} __attribute__ ((packed));

/* Maximum NTB length */
#define	XIN_NCM_NTB_MAX_SIZE_TX		        16384	/* bytes */
#define	XIN_NCM_NTB_MAX_SIZE_RX			16384	/* bytes */

/* Minimum value for MaxDatagramSize, ch. 6.2.9 */
#define	XIN_NCM_MIN_DATAGRAM_SIZE		1514	/* bytes */
#define XIN_NCM_MIN_IP_SIZE             1500

#define XIN_USB_MAX_PACKET_SIZE         512

#define	XIN_NCM_DPT_DATAGRAMS_MAX		32

/* Restart the timer, if amount of datagrams is less than given value */
#define	XIN_NCM_RESTART_TIMER_DATAGRAM_CNT	3

/* The following macro defines the minimum header space */
#define XIN_NCM_NDP_HDR_SIZE \
	(sizeof(struct usb_xin_ncm_ndp16) + \
	(XIN_NCM_DPT_DATAGRAMS_MAX + 1) * sizeof(struct usb_xin_ncm_dpe16))
	
#define	XIN_NCM_HDR_SIZE \
	(sizeof(struct usb_xin_ncm_nth16) + XIN_NCM_NDP_HDR_SIZE)

#define MAC_ADDRESS_SIZE (6)

#define XIN100_USB_VENDOR_REQUEST_READ_MACADDRESS (0xA2)


struct xin_ncm_data {
	struct usb_xin_ncm_nth16 nth16;
	struct usb_xin_ncm_ndp16 ndp16;
	struct usb_xin_ncm_dpe16 dpe16[XIN_NCM_DPT_DATAGRAMS_MAX + 1];
};

struct xin_ncm_ctx {
	struct xin_ncm_data rx_ncm;
	struct xin_ncm_data tx_ncm;

	struct timer_list tx_timer;

	struct net_device *netdev;
	struct usb_device *udev;
	struct usb_host_endpoint *in_ep;
	struct usb_host_endpoint *out_ep;
	struct usb_host_endpoint *status_ep;
	struct usb_interface *intf;

	spinlock_t mtx;

	u32 tx_timer_pending;
	u32 tx_curr_offset;
	u32 tx_curr_frame_num;
	u32 rx_speed;
	u32 tx_speed;
	u16 tx_seq;
	u16 connected;

	u8  server_mac_address[ETH_LENGTH_OF_ADDRESS];

	u8  big_buffer[XIN_NCM_NTB_MAX_SIZE_TX + 1];
};

static void xin_ncm_tx_timeout(unsigned long arg);
struct sk_buff * xin_ncm_tx_fixup(struct usbnet *dev, struct sk_buff *skb, gfp_t flags);
int xin_ncm_rx_fixup(struct usbnet *dev, struct sk_buff *skb_in);
static const struct driver_info xin_ncm_info;
static struct usb_driver xin_ncm_driver;
static const struct ethtool_ops xin_ncm_ethtool_ops;

static const struct usb_device_id xin_ncm_devs[] = {
	{
		USB_DEVICE_INTERFACE_PROTOCOL(XIN100_VENDOR_ID, XIN100_PRODUCT_ID, 0xFF),
		.driver_info = (unsigned long) &xin_ncm_info,
	},
	{},
};

//static u8 g_Xin_NCM_Index = 0;

MODULE_DEVICE_TABLE(usb, xin_ncm_devs);

static int xin_ncm_get_mac_address(struct usbnet *dev, unsigned char *data)
{
	unsigned char * macaddress = NULL;
	int ret = -ENODEV;

	if((dev == NULL) || (data == NULL))
	{
		printk(LOGERR "xin_ncm: dev or data is NULL in xin_ncm_get_mac_address!!!\n");
		return -ENODEV;
	}

	macaddress = (unsigned char*)kmalloc(MAC_ADDRESS_SIZE, GFP_KERNEL);
	if (!macaddress)
	{
		printk(LOGERR "xin_ncm: failed to allocate memory for mac address!!!\n");
		return -ENOMEM;
	}

	ret = usb_control_msg(dev->udev, usb_rcvctrlpipe(dev->udev, 0),
		XIN100_USB_VENDOR_REQUEST_READ_MACADDRESS,
		USB_DIR_IN | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
		0, 0, macaddress, MAC_ADDRESS_SIZE, USB_CTRL_GET_TIMEOUT);

	if (ret < 0)
	{	printk(LOGERR "xin_ncm: failed to send control message to get mac address from device, error = %d!!!\n", ret);
                kfree(macaddress);
		macaddress = NULL;
		return ret;
	}

	printk(LOGINFO "xin_ncm: MAC Address from device: %02x - %02x - %02x - %02x - %02x - %02x\n", 
			macaddress[0], macaddress[1],macaddress[2],macaddress[3],macaddress[4],macaddress[5]);

	memcpy(data, macaddress, MAC_ADDRESS_SIZE);

	kfree(macaddress);
	macaddress = NULL;

	return ret;
}

static void
xin_ncm_get_drvinfo(struct net_device *net, struct ethtool_drvinfo *info)
{
	struct usbnet *dev = netdev_priv(net);

	strncpy(info->driver, dev->driver_name, sizeof(info->driver));
	strncpy(info->version, DRIVER_VERSION, sizeof(info->version));
	strncpy(info->fw_version, dev->driver_info->description, sizeof(info->fw_version));
	usb_make_path(dev->udev, info->bus_info, sizeof(info->bus_info));
}

static void
xin_ncm_find_endpoints(struct xin_ncm_ctx *ctx, struct usb_interface *intf)
{
	struct usb_host_endpoint *e;
	u8 ep;

	for (ep = 0; ep < intf->cur_altsetting->desc.bNumEndpoints; ep++) 
	{

		e = intf->cur_altsetting->endpoint + ep;
		switch (e->desc.bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) 
		{
		case USB_ENDPOINT_XFER_INT:
			if (usb_endpoint_dir_in(&e->desc)) 
			{
				if (ctx->status_ep == NULL)
				{
					ctx->status_ep = e;
				}
			}
			break;

		case USB_ENDPOINT_XFER_BULK:
			if (usb_endpoint_dir_in(&e->desc)) 
			{
				if (ctx->in_ep == NULL)
				{	
					ctx->in_ep = e;
				}
			}
			else 
			{
				if (ctx->out_ep == NULL)
				{
					ctx->out_ep = e;
				}
			}
			break;

		default:
			break;
		}
	}
}

static void xin_ncm_free(struct xin_ncm_ctx *ctx)
{
	if (ctx == NULL)
	{
		return;
	}

	del_timer_sync(&ctx->tx_timer);

	kfree(ctx);
}

static int xin_ncm_bind(struct usbnet *dev, struct usb_interface *intf)
{
	struct xin_ncm_ctx *ctx = NULL;
	struct usb_driver *driver = NULL;
	unsigned char xin_ncm_mac_address[MAC_ADDRESS_SIZE] = {0};
	int status = -ENODEV;

	printk(LOGINFO "xin_ncm: binding interface number %d\n", intf->cur_altsetting->desc.bInterfaceNumber);

	if(intf->cur_altsetting->desc.bNumEndpoints != 3)
	{
		printk(LOGERR "xin_ncm: invalid interface %d for xin ncm because endpoint number is %d, instead of 3!\n",  
				intf->cur_altsetting->desc.bInterfaceNumber,
				intf->cur_altsetting->desc.bNumEndpoints);
		return -ENODEV;
	}

	status = xin_ncm_get_mac_address(dev, &xin_ncm_mac_address[0]);
	if(status < 0)
	{
	    printk(LOGERR "xin_ncm: failed to get mac addr status, error = %d\n", status);
            return status;
	}

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (ctx == NULL)
	{
		printk(LOGERR "xin_ncm: failed to allocate ctx in xin_ncm_bind\n");
		return -ENODEV;
	}

	init_timer(&ctx->tx_timer);
	spin_lock_init(&ctx->mtx);
	ctx->netdev = dev->net;

	dev->data[0] = (unsigned long)ctx;

	driver = driver_of(intf);

	ctx->udev = dev->udev;
	ctx->intf = intf;

	dev->hard_mtu =	XIN_NCM_MIN_DATAGRAM_SIZE;

	if (ctx->netdev->mtu != (XIN_NCM_MIN_DATAGRAM_SIZE - ETH_HLEN))
	{
		ctx->netdev->mtu = XIN_NCM_MIN_DATAGRAM_SIZE - ETH_HLEN;
	}

	xin_ncm_find_endpoints(ctx, ctx->intf);

	if ((ctx->in_ep == NULL) || (ctx->out_ep == NULL) || (ctx->status_ep == NULL))
	{
		printk(LOGERR "xin_ncm: some necessary endpoints are not available\n");
		goto error;
	}

	dev->net->ethtool_ops = &xin_ncm_ethtool_ops;

	usb_set_intfdata(ctx->intf, dev);

/*
    	dev->net->dev_addr[0] = 0x02;
    	dev->net->dev_addr[1] = 0x50;   
    	dev->net->dev_addr[2] = 0xF2;   
    	dev->net->dev_addr[3] = 0x00;    
    	dev->net->dev_addr[4] = 0x01;
    	//dev->net->dev_addr[5] = ++g_Xin_NCM_Index;
	dev->net->dev_addr[5] = 0x01;
	
    	ctx->server_mac_address[0] = 0x02;
    	ctx->server_mac_address[1] = 0x50;   
    	ctx->server_mac_address[2] = 0xF2;   
    	ctx->server_mac_address[3] = 0x00;    
    	ctx->server_mac_address[4] = 0x01;
	//ctx->server_mac_address[5] = 0xFF - g_Xin_NCM_Index;	
	ctx->server_mac_address[5] = 0x00;	
*/
	dev->net->dev_addr[0] = xin_ncm_mac_address[0];
    	dev->net->dev_addr[1] = xin_ncm_mac_address[1];   
    	dev->net->dev_addr[2] = xin_ncm_mac_address[2];   
    	dev->net->dev_addr[3] = xin_ncm_mac_address[3];    
    	dev->net->dev_addr[4] = xin_ncm_mac_address[4];
	dev->net->dev_addr[5] = xin_ncm_mac_address[5];
	
    	ctx->server_mac_address[0] = xin_ncm_mac_address[0];
    	ctx->server_mac_address[1] = xin_ncm_mac_address[1];   
    	ctx->server_mac_address[2] = xin_ncm_mac_address[2];   
    	ctx->server_mac_address[3] = xin_ncm_mac_address[3];    
    	ctx->server_mac_address[4] = xin_ncm_mac_address[4];
	ctx->server_mac_address[5] = 0xFF - xin_ncm_mac_address[5];

	printk(LOGINFO "MAC-Address: "
				"%02x-%02x-%02x-%02x-%02x-%02x\n",
				dev->net->dev_addr[0], dev->net->dev_addr[1],
				dev->net->dev_addr[2], dev->net->dev_addr[3],
				dev->net->dev_addr[4], dev->net->dev_addr[5]);

	dev->in = usb_rcvbulkpipe(dev->udev, ctx->in_ep->desc.bEndpointAddress & USB_ENDPOINT_NUMBER_MASK);
	dev->out = usb_sndbulkpipe(dev->udev, ctx->out_ep->desc.bEndpointAddress & USB_ENDPOINT_NUMBER_MASK);
	dev->status = ctx->status_ep;
	dev->rx_urb_size = XIN_NCM_NTB_MAX_SIZE_RX;

	//
	// We should get an event when network connection is "connected" or
	// "disconnected". Set network connection in "disconnected" state
	// (carrier is OFF) during attach, so the IP network stack does not
	// start IPv6 negotiation and more.
	//
	//netif_carrier_off(dev->net); // let link on by default!!!
	ctx->tx_speed = ctx->rx_speed = TX_RX_SPEED;	

	//modify net name 
	strcpy(dev->net->name,"lte%d");

	return 0;

error:
	xin_ncm_free((struct xin_ncm_ctx *)dev->data[0]);
	dev->data[0] = 0;
	printk(LOGERR "xincomm_ncm: bind() failure\n");
	return -ENODEV;
}

static void xin_ncm_unbind(struct usbnet *dev, struct usb_interface *intf)
{
	struct xin_ncm_ctx *ctx = (struct xin_ncm_ctx *)dev->data[0];

	if (ctx == NULL)
	{
		return;		// no setup
	}

	xin_ncm_free(ctx);
}

static void xin_ncm_zero_fill(u8 *ptr, u32 first, u32 end, u32 max)
{
	if (first >= max)
	{
		return;
	}
	if (first >= end)
	{
		return;
	}
	if (end > max)
	{
		end = max;
	}

	memset(ptr + first, 0, end - first);
}

static struct sk_buff *xin_ncm_fill_tx_frame(struct usbnet *dev, struct sk_buff *skb)
{
	struct sk_buff *skb_out;
	u32 rem;
	u16 n = 0;
	u32 count = 0;
	u8 ready2send = 0;
	struct ethhdr *ether = NULL;
    	struct ARP_RESPONSE*     pARPResponse = NULL;
	struct ARP_PACKET*       pARPRequest = NULL;
	u8 Broadcast_MAC_Address[ETH_LENGTH_OF_ADDRESS] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
	struct xin_ncm_ctx *ctx = (struct xin_ncm_ctx *)dev->data[0];
	u32 maxdatagram = 0;
	u8	*pDest=NULL;
	//printk(LOGINFO "xin_ncm: xin_ncm_fill_tx_frame entry\n");

	//
	// there are two call-stacks for this function,
	// 1) if the input skb is NULL, that means it came from the timer callback
	// 2) if the input skb is not NULL, that means it came from TCP/IP 

    	if (skb == NULL)
	{
	    	//printk(LOGINFO "xin_ncm: timer expired, so send the data. length = %d\n", ctx->tx_curr_offset);
	    	ready2send = 1;

		if(ctx->tx_curr_offset == 0)
		{
			//printk(LOGINFO "xin_ncm: in xin_ncm_fill_tx_frame, while skb is NULL and tx_curr_offset is zero!\n");
			goto exit_no_skb;
		}
	}
	else if((skb != NULL) && (ctx->tx_curr_frame_num > XIN_NCM_DPT_DATAGRAMS_MAX))
	{
	    	// no room for this skb
	    	printk(LOGERR "xin_ncm: !!!no room for skb!!!\n");

		dev_kfree_skb_any(skb);
		ctx->netdev->stats.tx_dropped++;
		skb = NULL;
		goto exit_no_skb;
	}

	if(skb != NULL)
	{
		//printk(LOGINFO "xin_ncm: xin_ncm_fill_tx_frame, skb length = %d\n", skb->len);
		if((skb->len <= sizeof(struct ethhdr)) || (skb->len > XIN_NCM_MIN_DATAGRAM_SIZE))
		{
			printk(LOGERR "xin_ncm: invalid skb_len: %d (should between 15 ~ 1514)\n", skb->len);

			dev_kfree_skb_any(skb);
			ctx->netdev->stats.tx_dropped++;
			skb = NULL;
			goto exit_no_skb;
		}

		ether = (struct ethhdr* )skb->data;
		pDest=(u8*)skb->data+14;

		if(ether->h_proto == NIC_htons (ETH_P_ARP))
		{
			//printk(LOGINFO "xin_ncm: ARP request\n");
			pARPRequest = (struct ARP_PACKET*)skb->data;
            


//		    	if (  MAC_EQUAL (&pARPRequest->ether_header.h_dest, Broadcast_MAC_Address)
      		  	 if(MAC_EQUAL (&pARPRequest->ether_header.h_source, ctx->netdev->dev_addr)     
	  		  &&  pARPRequest->ether_header.h_proto == NIC_htons (ETH_P_ARP)
		          &&  pARPRequest->hardware_type == NIC_htons (HARDWARE_TYPE)
			  &&  pARPRequest->protocol_type == NIC_htons (ETH_P_IP)
			  &&  pARPRequest->Operation == NIC_htons (ARP_REQUEST)
     		          &&  pARPRequest->HardwareSize == ETH_LENGTH_OF_ADDRESS
			  &&  pARPRequest->protocolSize == IP_LENGTH_OF_ADDRESS
			//&&  MAC_EQUAL (pARPRequest->m_ARP_MAC_Source, Adapter->CurrentAddress)
			  &&  !(IP_EQUAL(pARPRequest->m_ARP_IP_Destination, pARPRequest->m_ARP_IP_Source)) 
			//&&  IP_EQUAL(pARPRequest->m_ARP_IP_Destination, &Adapter->ServerIP.IPAddress)
			    )
			{
				//printk(LOGINFO "xin_ncm: before allocate skb for ARP reply\n");
			        skb_out = alloc_skb(XIN_NCM_MIN_DATAGRAM_SIZE, GFP_ATOMIC);				
			    	if (skb_out == NULL) 
				{
					printk(LOGERR "xin_ncm: failed to create skb_buff for ARP reply\n");
					if (skb != NULL) 
					{
						dev_kfree_skb_any(skb);
						ctx->netdev->stats.tx_dropped++;
					}
					goto exit_no_skb;
			    	}
				
                		// Currently, we just give ARP reply to all ARP request, and don't care about whether
                		// this ARP request is for us or not.
                		//
                		// Actually, there is an issue that after DHCP, PC will send an ARP to the IP it allocated
                		// by server, and it hope that no response from the IP because this IP will be used by PC itself,
                		// so if anyone give a response, then that means that PC cannot use this IP.
                		//					  
				    	  
				// valid ARP request, so prepare the ARP reply to be returned to TCP/IP
				//printk(LOGINFO "xin_ncm: before fill data into arp reply skb\n");
				pARPResponse = (struct ARP_RESPONSE *)skb_out->data;
				memset(pARPResponse, 0, sizeof(struct ARP_RESPONSE));		 
      		        
				//printk(LOGINFO "xin_ncm: before fill packet data into arp reply\n");
		  		//Initialize ARP reply fields
		        	pARPResponse->PacketData.hardware_type = NIC_htons (HARDWARE_TYPE);
		        	pARPResponse->PacketData.protocol_type = NIC_htons (ETH_P_IP);
		        	pARPResponse->PacketData.HardwareSize = ETH_LENGTH_OF_ADDRESS;
		        	pARPResponse->PacketData.protocolSize = IP_LENGTH_OF_ADDRESS;
		        	pARPResponse->PacketData.Operation = NIC_htons (ARP_REPLY);
                    
				//printk(LOGINFO "xin_ncm: before fill mac address into arp reply\n");
		  		//ARP addresses   
				COPY_MAC (&pARPResponse->PacketData.m_ARP_MAC_Source, ctx->server_mac_address);
				COPY_MAC (&pARPResponse->PacketData.m_ARP_MAC_Destination, ctx->netdev->dev_addr);
				COPY_IP(&pARPResponse->PacketData.m_ARP_IP_Source, pARPRequest->m_ARP_IP_Destination);
				COPY_IP(&pARPResponse->PacketData.m_ARP_IP_Destination, pARPRequest->m_ARP_IP_Source);

				//printk(LOGINFO "xin_ncm: before fill ether header into arp reply\n");
				// fill the etherhet header 
				COPY_MAC (&pARPResponse->PacketData.ether_header.h_source, ctx->server_mac_address);
				COPY_MAC (&pARPResponse->PacketData.ether_header.h_dest, ctx->netdev->dev_addr);
				pARPResponse->PacketData.ether_header.h_proto = NIC_htons(ETH_P_ARP);
				
                		//skb->len = sizeof(struct ARP_PACKET);

				skb_put(skb_out, sizeof(struct ARP_PACKET));
				
				//printk(LOGINFO "xin_ncm: send APR reply\n");
				
				usbnet_skb_return(dev, skb_out);
					
				//printk(LOGINFO "xin_ncm: after send arp reply\n");			    		 
			}
			else
			{
				printk(LOGINFO "xin_ncm: receive an ARP request, but we cannot handle it!\n");
			}
			
			printk(LOGINFO "xin_ncm: complete ARP handle\n");	

			dev_kfree_skb_any(skb);
			skb = NULL;
			goto exit_no_skb;
		}
		else
		{
                  	
			//printk(LOGINFO "xin_ncm: IP\n");
			if(ctx->tx_curr_offset == 0)
			{
				//printk(LOGINFO "xin_ncm: a new tx transfer\n");
								
				// make room for NTH and NDP
				ctx->tx_curr_offset = XIN_NCM_HDR_SIZE;

				//zero buffer till the first IP datagram
				xin_ncm_zero_fill(ctx->big_buffer, 0, ctx->tx_curr_offset, ctx->tx_curr_offset);
				ctx->tx_curr_frame_num = 0;

				//printk(LOGINFO "xin_ncm: after init big buffer, tx_curr_offset is %d\n", ctx->tx_curr_offset);
			}

			// from the second IP datagram, we should make sure that it is 512-byte aligned!
			if((ctx->tx_curr_frame_num != 0) && ((ctx->tx_curr_offset % NIC_NCM_ALIGNMENT) != 0))
			{
				//printk(LOGINFO "xin_ncm: handle alignment before copy data, before align, tx_curr_offset is %d\n", ctx->tx_curr_offset);
				count = ctx->tx_curr_offset / NIC_NCM_ALIGNMENT;
				ctx->tx_curr_offset = (count + 1) * NIC_NCM_ALIGNMENT;	
				//printk(LOGINFO "xin_ncm: after align, tx_curr_offset is %d\n", ctx->tx_curr_offset);
			}	

			//printk(LOGINFO "xin_ncm: remove the ethernet header of skb in\n");		

			skb_pull(skb, sizeof(struct ethhdr)); // remove the ethernet header

			if((ctx->tx_curr_offset + skb->len) < XIN_NCM_NTB_MAX_SIZE_TX)
			{
				/*printk(LOGINFO "xin_ncm: there is enough room for this skb data, %d + %d < %d\n", 
					ctx->tx_curr_offset,  skb->len, XIN_NCM_NTB_MAX_SIZE_TX);*/

				memcpy(((u8 *)ctx->big_buffer) + ctx->tx_curr_offset, skb->data, skb->len);

				/*printk(LOGINFO "xin_ncm: after copy skb data, fill ndp[%d], length = %d, offset = %d\n", 
					ctx->tx_curr_frame_num, skb->len, ctx->tx_curr_offset);		*/		

				ctx->tx_ncm.dpe16[ctx->tx_curr_frame_num].wDatagramLength = cpu_to_le16(skb->len);
				ctx->tx_ncm.dpe16[ctx->tx_curr_frame_num].wDatagramIndex = cpu_to_le16(ctx->tx_curr_offset);				
				ctx->tx_curr_frame_num++;
				ctx->tx_curr_offset += skb->len;
				
				/*printk(LOGINFO "xin_ncm: after copy data from skb, frame numbers = %d, offset = %d\n", 
					ctx->tx_curr_frame_num, ctx->tx_curr_offset);*/	
					
            	                /*if(pDest[9]==0x1)
				{

					//printk(LOGINFO "xin_ncm: ping icmp send pDest[0]=0x%x\n", pDest[0]);	
			    	        ready2send = 1;		

				}*/

			
			}
			else
			{
				// no enough room for this skb, so just drop it!!!
				printk(LOGERR "xin_ncm: strange, no room for this skb!!! something wrong!!!\n");
			    	ctx->netdev->stats.tx_dropped++;
			    	ready2send = 1;
			}

			//printk(LOGINFO "xin_ncm: complete handle IP datagram for tx skb\n");

			dev_kfree_skb_any(skb);
			skb = NULL;	
		}		
	}


	// free up any dangling skb
	if (skb != NULL) 
	{
		printk(LOGERR "xin_ncm: something wrong because skb is still non-NULL!!!\n");
		dev_kfree_skb_any(skb);
		skb = NULL;
		ctx->netdev->stats.tx_dropped++;
	}

	rem = XIN_NCM_NTB_MAX_SIZE_TX - ctx->tx_curr_offset;
	
	/*
	printk(LOGINFO "xin_ncm: XIN_NCM_NTB_MAX_SIZE_TX - ctx->tx_curr_offset: %d - %d = %d\n", XIN_NCM_NTB_MAX_SIZE_TX, ctx->tx_curr_offset, rem);

	printk(LOGINFO "xin_ncm: checking whether sending the data now, or waiting for more data... rem size = %d, frame number = %d\n", rem, ctx->tx_curr_frame_num);*/


	maxdatagram = XIN_NCM_DPT_DATAGRAMS_MAX;

	if ( (ready2send == 0)
	  && (ctx->tx_curr_frame_num < maxdatagram)
	  && (rem >= (XIN_NCM_MIN_IP_SIZE + NIC_NCM_ALIGNMENT))) 
	{
		// wait for more frames
		/*printk(LOGINFO "xin_ncm: waiting for more frames...frame numbers = %d, length = %d\n", ctx->tx_curr_frame_num, ctx->tx_curr_offset);*/
		// set the pending count
		if (ctx->tx_curr_frame_num < XIN_NCM_RESTART_TIMER_DATAGRAM_CNT)
		{
			//printk(LOGINFO "xin_ncm: waiting, reset the timer pending count\n");
			ctx->tx_timer_pending = 0;
		}
		goto exit_no_skb;
	}

	/*
	printk(LOGINFO "xin_ncm: all these three are TRUE?? ready2send = %d(?), %d < %d(?), %d >= %d(?)\n", ready2send, ctx->tx_curr_frame_num, maxdatagram,
				rem,  (XIN_NCM_MIN_IP_SIZE + NIC_NCM_ALIGNMENT));

	printk(LOGINFO "xin_ncm: ready to send the data in big buffer...frame numbers = %d, length = %d\n", ctx->tx_curr_frame_num, ctx->tx_curr_offset);*/

	// zero the rest of the DPEs plus the last NULL entry
	n = ctx->tx_curr_frame_num;
	for (; n <= XIN_NCM_DPT_DATAGRAMS_MAX; n++) 
	{
		ctx->tx_ncm.dpe16[n].wDatagramLength = 0;
		ctx->tx_ncm.dpe16[n].wDatagramIndex = 0;
	}

	//printk(LOGINFO "xin_ncm: before fill nth16\n");

	// fill out 16-bit NTB header
	ctx->tx_ncm.nth16.dwSignature = cpu_to_le32(USB_XIN_NCM_NTH16_SIGN);
	ctx->tx_ncm.nth16.wHeaderLength = cpu_to_le16(sizeof(ctx->tx_ncm.nth16));
	ctx->tx_ncm.nth16.wSequence = cpu_to_le16(ctx->tx_seq);
	ctx->tx_ncm.nth16.wBlockLength = cpu_to_le16(ctx->tx_curr_offset);
	ctx->tx_ncm.nth16.wNdpIndex = cpu_to_le16(sizeof(ctx->tx_ncm.nth16));

	//printk(LOGINFO "xin_ncm: copy nth16 to big buffer\n");

	memcpy(ctx->big_buffer, &(ctx->tx_ncm.nth16), sizeof(ctx->tx_ncm.nth16));
	ctx->tx_seq++;

	//printk(LOGINFO "xin_ncm: before fill ndp16\n");

	// fill out 16-bit NDP table
	ctx->tx_ncm.ndp16.dwSignature =	cpu_to_le32(USB_XIN_NCM_NDP16_NOCRC_SIGN);
	rem = sizeof(ctx->tx_ncm.ndp16) + ((XIN_NCM_DPT_DATAGRAMS_MAX + 1) * sizeof(struct usb_xin_ncm_dpe16));
	ctx->tx_ncm.ndp16.wLength = cpu_to_le16(rem);
	ctx->tx_ncm.ndp16.wPacketNumbers = cpu_to_le16(ctx->tx_curr_frame_num); // reserved

	//printk(LOGINFO "xin_ncm: copy ndp16 to big buffer\n");
	memcpy(((u8 *)ctx->big_buffer) + sizeof(ctx->tx_ncm.nth16), &(ctx->tx_ncm.ndp16), sizeof(ctx->tx_ncm.ndp16));

	//printk(LOGINFO "xin_ncm: copy dpe16 to big buffer\n");

	memcpy(((u8 *)ctx->big_buffer) + sizeof(ctx->tx_ncm.nth16) + sizeof(ctx->tx_ncm.ndp16),
					&(ctx->tx_ncm.dpe16),
					(XIN_NCM_DPT_DATAGRAMS_MAX + 1) * sizeof(struct usb_xin_ncm_dpe16));

	if((ctx->tx_curr_offset % XIN_USB_MAX_PACKET_SIZE) == 0)
	{
		//printk(LOGINFO "xin_ncm: fix ZLP issue\n");
		ctx->tx_curr_offset++;
	}

	//printk(LOGINFO "xin_ncm: allocate the skb for sending data in big buffer to device\n");
	skb_out = alloc_skb(XIN_NCM_NTB_MAX_SIZE_TX, GFP_ATOMIC);
	if(skb_out == NULL)
	{
		printk(LOGERR "xin_ncm: failed to allocate the skb for sending data in big buffer to device\n");
		goto exit_no_skb;		
	}
	skb_put(skb_out, ctx->tx_curr_offset);

	memcpy(skb_out->data, ctx->big_buffer, ctx->tx_curr_offset);

	dev->net->stats.tx_packets += ctx->tx_curr_frame_num;

	ctx->tx_curr_offset = 0;

	//printk(LOGINFO "xin_ncm: send the skb to device\n");
	
	return skb_out;
	
exit_no_skb:

	//printk(LOGINFO "xin_ncm: waiting, or wrong\n");
	return NULL;
}

static void xin_ncm_tx_timeout_start(struct xin_ncm_ctx *ctx)
{
	//printk(LOGINFO "xin_ncm: xin_ncm_tx_timeout_start\n");

	if (timer_pending(&ctx->tx_timer) == 0) 
	{
		ctx->tx_timer.function = &xin_ncm_tx_timeout;
		ctx->tx_timer.data = (unsigned long)ctx;
		ctx->tx_timer.expires = jiffies + ((HZ + 999) / 1000);
		add_timer(&ctx->tx_timer);
	}
}

static void xin_ncm_tx_timeout(unsigned long arg)
{
	struct xin_ncm_ctx *ctx = (struct xin_ncm_ctx *)arg;
	u8 restart;

	//printk(LOGINFO "xin_ncm: xin_ncm_tx_timeout entry\n");

	spin_lock(&ctx->mtx);
	if (ctx->tx_timer_pending != 0) 
	{
		ctx->tx_timer_pending--;
		restart = 1;
	} 
	else 
	{
		restart = 0;
	}

	spin_unlock(&ctx->mtx);

	if (restart) 
	{
		//printk(LOGINFO "xin_ncm: restart the timer\n");
		spin_lock(&ctx->mtx);
		xin_ncm_tx_timeout_start(ctx);
		spin_unlock(&ctx->mtx);
	} 
	else if (ctx->netdev != NULL) 
	{
		//printk(LOGINFO "xin_ncm: before start the pending data in big buffer through timer!\n");
		usbnet_start_xmit(NULL, ctx->netdev);
	}

	//printk(LOGINFO "xin_ncm: xin_ncm_tx_timeout exit\n");
}

struct sk_buff *xin_ncm_tx_fixup(struct usbnet *dev, struct sk_buff *skb, gfp_t flags)
{
	struct sk_buff *skb_out = NULL;
	struct xin_ncm_ctx *ctx = (struct xin_ncm_ctx *)dev->data[0];

	//printk(LOGINFO "xin_ncm: tx_fixup is called\n");

	//
	// The Ethernet API we are using does not support transmitting
	// multiple Ethernet frames in a single call. This driver will
	// accumulate multiple Ethernet frames and send out a larger
	// USB frame when the USB buffer is full or when a single jiffies
	// timeout happens.
	//
	if (ctx == NULL)
	{
		goto error;
	}

	spin_lock(&ctx->mtx);

	skb_out = xin_ncm_fill_tx_frame(dev, skb);

	if (ctx->tx_curr_offset != 0 && (skb_out == NULL)) // Start timer, if there is data in big buffer
	{
		//printk(LOGINFO "xin_ncm: in tx_fixup, there is pending data, so start the timer\n");
		xin_ncm_tx_timeout_start(ctx);
	}

	spin_unlock(&ctx->mtx);

	if(skb_out != NULL)
	{
		//printk(LOGINFO "xin_ncm: info in the exit of tx_fixup: skb_out data length: %d\n", skb_out->len);
	}

	return skb_out;

error:
	if (skb != NULL)
	{
		printk(LOGERR "xin_ncm: error in tx_fixup\n");
		dev_kfree_skb_any(skb);
	}

	return NULL;
}

int xin_ncm_rx_fixup(struct usbnet *dev, struct sk_buff *skb_in)
{
	struct sk_buff *skb;
	struct xin_ncm_ctx *ctx;
	int sumlen;
	int actlen;
	int temp;
	int nframes;
	int x;
	int offset;
	int ndplength;
	struct ethhdr* eth;
	u8 Broadcast_IP_Address[IP_LENGTH_OF_ADDRESS] = {0xFF, 0xFF, 0xFF, 0xFF};  
	u8 Broadcast_MAC_Address[ETH_LENGTH_OF_ADDRESS] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

	//printk(LOGINFO "xin_ncm: rx_fixup extry\n");

	ctx = (struct xin_ncm_ctx *)dev->data[0];
	if (ctx == NULL)
	{
		goto error;
	}

	actlen = skb_in->len;
	sumlen = XIN_NCM_NTB_MAX_SIZE_RX;

	if (actlen < (sizeof(ctx->rx_ncm.nth16) + sizeof(ctx->rx_ncm.ndp16))) 
	{
		printk(LOGERR "xin_ncm: the incoming frame is too short\n");
		goto error;
	}

	memcpy(&(ctx->rx_ncm.nth16), ((u8 *)skb_in->data), sizeof(ctx->rx_ncm.nth16));

	if (le32_to_cpu(ctx->rx_ncm.nth16.dwSignature) != USB_XIN_NCM_NTH16_SIGN)
	{
		printk(LOGERR "xin_ncm: invalid NTH16 signature <%u>\n", le32_to_cpu(ctx->rx_ncm.nth16.dwSignature));
		goto error;
	}

	temp = le16_to_cpu(ctx->rx_ncm.nth16.wBlockLength);
	if (temp > sumlen) 
	{
		printk(LOGERR "xin_ncm: unsupported NTB block length %u/%u\n", temp, sumlen);
		goto error;
	}

	temp = le16_to_cpu(ctx->rx_ncm.nth16.wNdpIndex);
	if ((temp + sizeof(ctx->rx_ncm.ndp16)) > actlen) 
	{
		printk(LOGERR "xin_ncm: invalid DPT16 index\n");
		goto error;
	}

	memcpy(&(ctx->rx_ncm.ndp16), ((u8 *)skb_in->data) + temp, sizeof(ctx->rx_ncm.ndp16));

	if (le32_to_cpu(ctx->rx_ncm.ndp16.dwSignature) != USB_XIN_NCM_NDP16_NOCRC_SIGN) 
	{
		printk(LOGERR "xin_ncm: invalid DPT16 signature <%u>\n", le32_to_cpu(ctx->rx_ncm.ndp16.dwSignature));
		goto error;
	}

	ndplength = XIN_NCM_NDP_HDR_SIZE;
	if (le16_to_cpu(ctx->rx_ncm.ndp16.wLength) != ndplength) 
	{
		printk(LOGERR "xin_ncm: invalid DPT16 length %u, expected %u\n",
			 le32_to_cpu(ctx->rx_ncm.ndp16.wLength), ndplength);
		goto error;
	}

	if ((temp + ndplength ) > actlen) 
	{
		printk(LOGERR "xin_ncm: Invalid ndp header\n");
		goto error;
	}

	temp += sizeof(ctx->rx_ncm.ndp16);

	memcpy(&(ctx->rx_ncm.dpe16), ((u8 *)skb_in->data) + temp,
				(XIN_NCM_DPT_DATAGRAMS_MAX + 1) * (sizeof(struct usb_xin_ncm_dpe16)));

	nframes = le16_to_cpu(ctx->rx_ncm.ndp16.wPacketNumbers);
	nframes = NIC_min(nframes, XIN_NCM_DPT_DATAGRAMS_MAX);

	//printk(LOGDEBUG "xin_ncm: nframes = %u\n", nframes);

	for (x = 0; x < nframes; x++)
	{
		//printk(LOGINFO "xin_ncm: a new datagram, index = %d\n", x);
		offset = le16_to_cpu(ctx->rx_ncm.dpe16[x].wDatagramIndex);
		temp = le16_to_cpu(ctx->rx_ncm.dpe16[x].wDatagramLength);

		if ((offset == 0) || (temp == 0)) 
		{
			printk(LOGERR "xin_ncm: invalid dpe entry, index = %d, offset = %d, length = %d", x, offset, temp);
			continue;
		}

		if (((offset + temp) > actlen) || (temp > XIN_NCM_MIN_IP_SIZE)) 
		{
			printk(LOGERR "xin_ncm: invalid frame detected (ignored)"
					"offset[%u]=%u, length=%u, skb=%p\n",
					x, offset, temp, skb_in);			
			continue;
		} 
		else 
		{
			//printk(LOGINFO "xin_ncm: datagram[%d]: offset = %d, length = %d\n", x, offset, temp);
			skb = alloc_skb(temp + sizeof(struct ethhdr), GFP_ATOMIC);//skb_clone(skb_in, GFP_ATOMIC);
			if (!skb)
			{
				printk(LOGERR "xin_ncm: failed to allocate skb\n");
				goto error;
			}
			skb_put(skb, temp + sizeof(struct ethhdr));	

			//printk(LOGINFO "xin_ncm: before fill ethernet header to skb out\n");		

			// 
			// Add ethernet header in the head of this buffer
			// 
			// If the DestIP is FF-FF-FF-FF, then we put all 0xFF in the DestMac, 
			// otherwise, put device MAC in the DestMac
			//
			eth = (struct ethhdr*)skb->data;
			if(IP_EQUAL(skb_in->data, &Broadcast_IP_Address))
			{
			    	COPY_MAC(&eth->h_dest[0], &Broadcast_MAC_Address);
			}
			else
			{
			    	COPY_MAC(&eth->h_dest[0], &ctx->netdev->dev_addr[0]);
			}
			COPY_MAC(&eth->h_source[0], &ctx->server_mac_address[0]);
			eth->h_proto = NIC_htons(ETH_P_IP);

			//printk(LOGINFO "xin_ncm: before copy data from skb in to skb out in rx_fixup\n");	
			
			memcpy((u8*)skb->data + sizeof(struct ethhdr), ((u8 *)skb_in->data) + offset, temp);
			
			//printk(LOGINFO "xin_ncm: before send data to TCP/IP\n");

			usbnet_skb_return(dev, skb);

			//printk(LOGINFO "xin_ncm: after send data to TCP/IP\n");
		}
	}

	//printk(LOGINFO "xin_ncm: rx fixup exit\n");	

	return 1;
error:
	printk(LOGINFO "xin_ncm: rx_fixup failed exit\n");
	return 0;
}

static void xin_ncm_status(struct usbnet *dev, struct urb *urb)
{
	struct xin_ncm_ctx *ctx;
	struct usb_xin_notification *event;

	ctx = (struct xin_ncm_ctx *)dev->data[0];

	if(ctx == NULL)
	{
		printk(LOGERR "xin_ncm: ctx is NULL in xin_ncm_status\n");
		return;
	}

	if (urb->actual_length < sizeof(*event))
	{
		printk(LOGERR "xin_ncm: invalid notification, length = %d\n", urb->actual_length);
		return;
	}

	event = urb->transfer_buffer;

	switch (event->bNotificationType) {
	case USB_XIN_NOTIFY_NETWORK_CONNECTION:
		//
		//  According to the CDC NCM specification ch.7.1
		//  USB_CDC_NOTIFY_NETWORK_CONNECTION notification shall be
		//  sent by device after USB_CDC_NOTIFY_SPEED_CHANGE.
		//
		ctx->connected = event->wValue;

		printk(KERN_INFO KBUILD_MODNAME ": %s: network connection: %sconnected\n",
			ctx->netdev->name, ctx->connected ? "" : "dis");

		if (ctx->connected)
		{
			netif_carrier_on(dev->net);
		}
		else 
		{
			netif_carrier_off(dev->net);
			ctx->tx_speed = ctx->rx_speed = 0;
		}

		break;

	default:
		dev_err(&dev->udev->dev, "NCM: unexpected notification 0x%02x!\n", event->bNotificationType);
		break;
	}
}

static int xin_ncm_check_connect(struct usbnet *dev)
{
	struct xin_ncm_ctx *ctx;

	ctx = (struct xin_ncm_ctx *)dev->data[0];
	if (ctx == NULL)
	{
		return 1;	// disconnected
	}

	return !ctx->connected;
}

static int xin_ncm_manage_power(struct usbnet *dev, int status)
{
	dev->intf->needs_remote_wakeup = status;
	return 0;
}

static const struct driver_info xin_ncm_info = {
	.description = "xin_ncm",
	.flags = FLAG_POINTTOPOINT | FLAG_NO_SETINT | FLAG_MULTI_PACKET,
	.bind = xin_ncm_bind,
	.unbind = xin_ncm_unbind,
	.check_connect = xin_ncm_check_connect,
	.manage_power = xin_ncm_manage_power,
	.status = xin_ncm_status,
	.rx_fixup = xin_ncm_rx_fixup,
	.tx_fixup = xin_ncm_tx_fixup,
};

static struct usb_driver xin_ncm_driver = {
	.name = "xin_ncm",
	.id_table = xin_ncm_devs,
	.probe = usbnet_probe,
	.disconnect = usbnet_disconnect,
	.suspend = usbnet_suspend,
	.resume = usbnet_resume,
	.reset_resume =	usbnet_resume,
	.supports_autosuspend = 1,
};

static const struct ethtool_ops xin_ncm_ethtool_ops = {
	.get_drvinfo = xin_ncm_get_drvinfo,
	.get_link = usbnet_get_link,
	.get_msglevel = usbnet_get_msglevel,
	.set_msglevel = usbnet_set_msglevel,
	.get_settings = usbnet_get_settings,
	.set_settings = usbnet_set_settings,
	.nway_reset = usbnet_nway_reset,
};

//module_usb_driver(xin_ncm_driver); // for kernel 3.3

static int __init xin_ncm_init(void)
{
	printk(KERN_INFO KBUILD_MODNAME ": " DRIVER_VERSION " xin_ncm_init, HZ = %d\n", HZ);
	
	return usb_register(&xin_ncm_driver);
}

module_init(xin_ncm_init);

static void __exit xin_ncm_exit(void)
{
	usb_deregister(&xin_ncm_driver);
}

module_exit(xin_ncm_exit);


MODULE_AUTHOR("Zhang");
MODULE_DESCRIPTION("USB NCM-similar host driver for Xin100 of XinComm");
MODULE_LICENSE("Dual BSD/GPL");
