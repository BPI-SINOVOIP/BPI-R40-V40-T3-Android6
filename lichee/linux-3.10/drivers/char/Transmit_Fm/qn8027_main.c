/*
 * qn8027 fm sender.
 *
*/
#include "qndriver.h"
#include "qn8027.h"
#include <linux/module.h>
#include <linux/init.h>
#include <linux/pm.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/miscdevice.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/hwmon.h>
#include <linux/printk.h>
#include <linux/clk.h>
#include <linux/gpio.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/pinconf-sunxi.h>
#include <linux/pinctrl/consumer.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#define QN8027_DRV_NAME	"qn8027"
#define R_TXRX_MASK    0x20
UINT8   qnd_Crystal = QND_CRYSTAL_DEFAULT;
UINT8   qnd_PrevMode;
UINT8   qnd_Country  = COUNTRY_CHINA ;
UINT16  qnd_CH_START = 7600;
UINT16  qnd_CH_STOP  = 10800;
UINT8   qnd_CH_STEP  = 1;
static struct fm_status tr_fm_status;
struct qn8027_data_s {
	int twi_id;
	struct device *hwmon_dev;
	struct i2c_client *i2c_dev;
	int enable;
	int channel;

} qn8027_data;
UINT8 QN8027_WriteReg(UINT8 Regis_Addr, UINT8 Data)
{
	int ret;
	char tdata[2];
	struct i2c_msg msg[] = {
		{
			.addr	= qn8027_data.i2c_dev->addr,
			.flags	= 0,
			.len	= 2,
			.buf	= tdata,
		},
	};
	tdata[0] = Regis_Addr;
	tdata[1] = Data;
	ret = i2c_transfer(qn8027_data.i2c_dev->adapter, msg, 1);
	if (ret != 1)
		pr_err("%s i2c write error: %x, %x\n", __func__, tdata[0], tdata[1]);

	return ret;
}
UINT8 QN8027_ReadReg(UINT8 Regis_Addr)
{
	int ret;
	char rxdata;
	struct i2c_msg msgs[] = {
		{
			.addr	= qn8027_data.i2c_dev->addr,
			.flags	= 0,
			.len	= 1,
			.buf	= &Regis_Addr,
		},
		{
			.addr	= qn8027_data.i2c_dev->addr,
			.flags	= I2C_M_RD,
			.len	= 1,
			.buf	= &rxdata,
		},
	};
	ret = i2c_transfer(qn8027_data.i2c_dev->adapter, msgs, 2);
	if (ret != 2) {
		pr_info("msg %s i2c read error: %d\n", __func__, ret);
		rxdata = 0;
	}
	return rxdata;
}
static ssize_t qn8027_channel_show(struct device *dev,
								  struct device_attribute *attr, char *buf)
{
	pr_err("%s qn8027_channel_show \n", __FUNCTION__);
	return sprintf(buf, "%d\n", qn8027_data.channel);
}

static ssize_t qn8027_channel_store(struct device *dev,
								   struct device_attribute *attr,
								   const char *buf, size_t count)
{
	pr_err("%s qn8027_channel_store \n", __FUNCTION__);
	unsigned long data;
	int error;

	error = strict_strtoul(buf, 10, &data);
	pr_err("%s qn8027_channel_store data = %ld\n", __FUNCTION__, data);
	if (error) {
		pr_err("%s strict_strtoul error\n", __FUNCTION__);
		goto exit;
	}
	if (qn8027_data.channel == data) {
		goto exit;
	}
	QND_TuneToCH(data/10000);
	qn8027_data.channel = data;

exit:
	return count;
}

static ssize_t qn8027_enable_show(struct device *dev,
								  struct device_attribute *attr, char *buf)
{
pr_err("%s qn8027_enable_show \n", __FUNCTION__);
	return sprintf(buf, "%d\n", qn8027_data.enable);
}
static ssize_t qn8027_enable_store(struct device *dev,
								   struct device_attribute *attr,
								   const char *buf, size_t count)
{
	pr_err("%s qn8027_enable_store \n", __FUNCTION__);
	unsigned long data;
	int error;
	error = strict_strtoul(buf, 10, &data);
	pr_err("%s qn8027_enable_store data = %ld \n", __FUNCTION__, data);
	if (error) {
		pr_err("%s strict_strtoul error\n", __FUNCTION__);
		goto exit;
	}
	if (qn8027_data.enable == data) {
		goto exit;
	}
	if (data) {
		QND_SetSysMode(QND_MODE_WAKEUP | QND_MODE_TX);
	} else {
		QND_SetSysMode(QND_MODE_SLEEP);
	}
	qn8027_data.enable = data;

exit:
	return count;
}

static DEVICE_ATTR(channel, S_IRUGO | S_IWUGO,
				   qn8027_channel_show, qn8027_channel_store);

static DEVICE_ATTR(enable, S_IRUGO | S_IWUGO,
				   qn8027_enable_show, qn8027_enable_store);

static struct attribute *qn8027_attributes[] = {
	&dev_attr_channel.attr,
	&dev_attr_enable.attr,
	NULL
};

static struct attribute_group qn8027_attribute_group = {
	.attrs = qn8027_attributes
};

void QN8027_Delay(UINT16 ms)
{
	msleep(ms);
}

struct QND_IOOperations qn8027_ioops = {
	.QND_WriteReg = QN8027_WriteReg,
	.QND_ReadReg = QN8027_ReadReg,
	.QND_Delay = QN8027_Delay,
};


/**********************************************************************
void QNF_SetRegBit(UINT8 reg, UINT8 bitMask, UINT8 data_val)
**********************************************************************
Description: set register specified bit

Parameters:
    reg:        register that will be set
    bitMask:    mask specified bit of register
    data_val:    data will be set for specified bit
Return Value:
    None
**********************************************************************/
void QNF_SetRegBit(UINT8 reg, UINT8 bitMask, UINT8 data_val) 
{
    UINT8 temp;
    temp = QN8027_ReadReg(reg);
    temp &= (UINT8)(~bitMask);
    temp |= data_val & bitMask;
//    temp |= data_val;
    QN8027_WriteReg(reg, temp);
}

/**********************************************************************
UINT16 QNF_GetCh()
**********************************************************************
Description: get current channel frequency

Parameters:
    None
Return Value:
    channel frequency
**********************************************************************/
UINT16 QNF_GetCh(void) 
{
    UINT8 tCh;
    UINT8  tStep; 
    UINT16 ch = 0;
    // set to reg: CH_STEP
    tStep = QN8027_ReadReg(CH_STEP);
    tStep &= CH_CH;
    ch  =  tStep ;
    tCh= QN8027_ReadReg(CH);    
    ch = (ch<<8)+tCh;
    return CHREG2FREQ(ch);
}

/**********************************************************************
UINT8 QNF_SetCh(UINT16 freq)
**********************************************************************
Description: set channel frequency 

Parameters:
    freq:  channel frequency to be set
Return Value:
    1: success
**********************************************************************/
UINT8 QNF_SetCh(UINT16 freq) 
{
    // calculate ch parameter used for register setting
    UINT8 tStep;
    UINT8 tCh;
    UINT16 f; 

        f = FREQ2CHREG(freq); 
        // set to reg: CH
        tCh = (UINT8) f;

        QN8027_WriteReg(CH, tCh);
        // set to reg: CH_STEP
        tStep = QN8027_ReadReg(CH_STEP);

        tStep &= ~CH_CH;
        tStep |= ((UINT8) (f >> 8) & CH_CH);

        QN8027_WriteReg(CH_STEP, tStep);

    return 1;
}

/**********************************************************************
void QNF_SetAudioMono(UINT8 modemask, UINT8 mode) 
**********************************************************************
Description:    Set audio output to mono.

Parameters:
  modemask: mask register specified bit
  mode
        QND_RX_AUDIO_MONO:    RX audio to mono
        QND_RX_AUDIO_STEREO:  RX audio to stereo    
        QND_TX_AUDIO_MONO:    TX audio to mono
        QND_TX_AUDIO_STEREO:  TX audio to stereo 
Return Value:
  None
**********************************************************************/
void QNF_SetAudioMono(UINT8 modemask, UINT8 mode) 
{
    QNF_SetRegBit(SYSTEM2,modemask, mode);
}

/**********************************************************************
void QN_ChipInitialization()
**********************************************************************
Description: chip first step initialization, called only by QND_Init()

Parameters:
    None
Return Value:
    None
**********************************************************************/
void QN_ChipInitialization(void)
{
    QN8027_WriteReg(0x00,0x81);
    QN8027_WriteReg(0x00,0x81);
    QN8027_Delay(20);
    QN8027_WriteReg(0x03, 0x10);
    QN8027_WriteReg(0x04, 0x21);
    QN8027_WriteReg(0x00, 0x41);
    QN8027_WriteReg(0x00, 0x01);
    QN8027_Delay(20);
    QN8027_WriteReg(0x01,0x18);
    QN8027_WriteReg(0x00,0x21);    
    QN8027_WriteReg(0x02,0xB9);
	
}

/**********************************************************************
int QND_Init()
**********************************************************************
Description: Initialize device to make it ready to have all functionality ready for use.

Parameters:
    None
Return Value:
    0: Device is ready to use.
    1: Device is not ready to serve function.
**********************************************************************/
UINT8 QND_Init(void) 
{
    QN_ChipInitialization();
    return 0;
}

/**********************************************************************
void QND_SetSysMode(UINT16 mode)
***********************************************************************
Description: Set device system mode(like: sleep ,wakeup etc) 
Parameters:
    mode:  set the system mode , it will be set by  some macro define usually:
    
    SLEEP (added prefix: QND_MODE_, same as below):  set chip to sleep mode
    WAKEUP: wake up chip 
    TX:     set chip work on TX mode
    RX:     set chip work on RX mode
    FM:     set chip work on FM mode
    AM:     set chip work on AM mode
    TX|FM:  set chip work on FM,TX mode
    RX|AM;  set chip work on AM,RX mode
    RX|FM:    set chip work on FM,RX mode
Return Value:
    None     
**********************************************************************/
void QND_SetSysMode(UINT16 mode) 
{    
    UINT8 val;
    switch(mode)        
    {        
    case QND_MODE_SLEEP:                       //set sleep mode        
        QNF_SetRegBit(SYSTEM1, R_TXRX_MASK, 0); 
        break;        
    default:    
            val = (UINT8)(mode >> 8);        
            if (val)
            {
                val = val >> 1;
                if (val & 0x20)
                {
                    if((QN8027_ReadReg(SYSTEM1) & R_TXRX_MASK) != val)
                    {
                        QNF_SetRegBit(SYSTEM1, R_TXRX_MASK, val); 
                    }  
                }
            }    
        break;        
    }    
}

/**********************************************************************
void QND_TuneToCH(UINT16 ch)
**********************************************************************
Description: Tune to the specific channel. call QND_SetSysMode() before 
call this function
Parameters:
    ch
    Set the frequency (10kHz) to be tuned,
    eg: 101.30MHz will be set to 10130.
Return Value:
    None
**********************************************************************/
void QND_TuneToCH(UINT16 ch) 
{
    QNF_SetCh(ch);
}

/**********************************************************************
void QND_SetCountry(UINT8 country)
***********************************************************************
Description: Set start, stop, step for RX and TX based on different
             country
Parameters:
country:
Set the chip used in specified country:
    CHINA:
    USA:
    JAPAN:
Return Value:
    None     
**********************************************************************/
void QND_SetCountry(UINT8 country) 
{
    qnd_Country = country;
    switch(country)
    {
    case COUNTRY_CHINA:
        qnd_CH_START = 7600;
        qnd_CH_STOP = 10800;
        qnd_CH_STEP = 1;
        break;
    case COUNTRY_USA:
        qnd_CH_START = 8810;
        qnd_CH_STOP = 10790;
        qnd_CH_STEP = 2;
        break;
    case COUNTRY_JAPAN:
        break;
    }
}

/**********************************************************************
UINT8 QND_TXSetPower( UINT8 gain)
**********************************************************************
Description:    Sets FM transmit power attenuation.

Parameters:
    gain: The transmission power attenuation value, for example, 
          setting the gain = 0x13, TX attenuation will be -6db
          look up table see below
BIT[5:4]
            00    0db
            01    -6db
            10    -12db
            11    -18db
BIT[3:0]    unit: db
            0000    124
            0001    122.5
            0010    121
            0011    119.5
            0100    118
            0101    116.5
            0110    115
            0111    113.5
            1000    112
            1001    110.5
            1010    109
            1011    107.5
            1100    106
            1101    104.5
            1110    103
            1111    101.5
for example:
  0x2f,    //111111    89.5
  0x2e,    //111110    91
  0x2d,    //111101    92.5
  0x2c,    //111100    94
  0x1f,    //111011 95.5
  0x1e,    //111010 97
  0x1d,    //111001 98.5
  0x1c,    //111000 100
  0x0f,    //001111    101.5
  0x0e,    //001110    103
  0x0d,    //001101    104.5
  0x0c,    //001100    106
  0x0b,    //001011    107.5
  0x0a,    //001010    109
  0x09,    //001001    110.5
  0x08,    //001000    112
  0x07,    //000111    113.5
  0x06,    //000110    115
  0x05,    //000101    116.5
  0x04,    //000100    118
  0x03,    //000011    119.5
  0x02,    //000010    121
  0x01,    //000001    122.5
  0x00     //000000    124
**********************************************************************/
void QND_TXSetPower( UINT8 gain)
{
    UINT8 value = 0;
    value |= 0x40;  
    value |= gain;
    QN8027_WriteReg(PAG_CAL, value);
}

/**********************************************************************
void QND_TXConfigAudio(UINT8 optiontype, UINT8 option )
**********************************************************************
Description: Config the TX audio (eg: volume,mute,etc)
Parameters:
  optiontype:option :
    QND_CONFIG_AUTOAGC:‘option'set auto AGC, 0:disable auto AGC,1:enable auto AGC.
    QND_CONFIG_SOFTCLIP;‘option’set softclip,0:disable soft clip, 1:enable softclip.
    QND_CONFIG_MONO;‘option’set mono,0: QND_AUDIO_STEREO, 1: QND_AUDIO_STEREO
    QND_CONFIG_AGCGAIN;‘option’set AGC gain, range:0000~1111
Return Value:
  none
**********************************************************************/
void QND_TXConfigAudio(UINT8 optiontype, UINT8 option )
{
    switch(optiontype)
    {
    case QND_CONFIG_MONO:
        if (option)
            QNF_SetAudioMono(0x10, QND_TX_AUDIO_MONO);
        else
            QNF_SetAudioMono(0x10, QND_TX_AUDIO_STEREO);
        break;

    case QND_CONFIG_MUTE:
        if (option) 
            QNF_SetRegBit(23, 0x30,0);
        else    
            QNF_SetRegBit(23, 0x30,0x30);
        break;
    case QND_CONFIG_AGCGAIN:
        QNF_SetRegBit(0x04, 0x70,(UINT8)(option<<4));
        break;
    default:
        break;
    }
}

/**********************************************************************
UINT8 QND_RDSEnable(UINT8 mode) 
**********************************************************************
Description: Enable or disable chip to work with RDS related functions.
Parameters:
          on: QND_RDS_ON:  Enable chip to receive/transmit RDS data.
                QND_RDS_OFF: Disable chip to receive/transmit RDS data.
Return Value:
           QND_SUCCESS: function executed
**********************************************************************/
UINT8 QND_RDSEnable(UINT8 on) 
{
    UINT8 val;

    QND_LOG("=== QND_SetRDSMode === ");
    // get last setting
    val = QN8027_ReadReg(0x12);
    if (on == QND_RDS_ON) 
    {
        val |= RDSEN;
    } 
    else if (on == QND_RDS_OFF)
    {
        val &= ~RDSEN;
    }
    else 
    {
        return 0;
    }
    QN8027_WriteReg(0x12, val);
    return 1;
}

/**********************************************************************
void QND_RDSHighSpeedEnable(UINT8 on) 
**********************************************************************
Description: Enable or disable chip to work with RDS related functions.
Parameters:
  on: 
    1: enable 4x rds to receive/transmit RDS data.
    0: disable 4x rds, enter normal speed.
Return Value:
  none
**********************************************************************/
void QND_RDSHighSpeedEnable(UINT8 on) 
{
    QNF_SetRegBit(0x2, 0x40, on?0x40:0x00);
}

/**********************************************************************
UINT8 QND_DetectRDSSignal(void)
**********************************************************************
Description: detect the RDSS signal .

Parameters:
    None
Return Value:
    the value of STATUS3
**********************************************************************/
UINT8 QND_RDSDetectSignal(void) 
{
    return 0;
}

/**********************************************************************
void QND_RDSLoadData(UINT8 *rdsRawData, UINT8 upload)
**********************************************************************
Description: Load (TX) or unload (RX) RDS data to on-chip RDS buffer. 
             Before calling this function, always make sure to call the 
             QND_RDSBufferReady function to check that the RDS is capable 
             to load/unload RDS data.
Parameters:
  rdsRawData : 
    8 bytes data buffer to load (on TX mode) or unload (on RXmode) 
    to chip RDS buffer.
  Upload:   
    1-upload
    0--download
Return Value:
    QND_SUCCESS: rds data loaded to/from chip
**********************************************************************/
void QND_RDSLoadData(UINT8 *rdsRawData, UINT8 upload) 
{
    UINT8 i;
    if (upload) 
    {     
        for (i = 0; i <= 7; i++) 
        {
            QN8027_WriteReg(RDSD0 + i, rdsRawData[i]);
        }    
    } 
}

/**********************************************************************
UINT8 QND_RDSCheckBufferReady(void)
**********************************************************************
Description: Check chip RDS register buffer status before doing load/unload of
RDS data.

Parameters:
    None
Return Value:
    QND_RDS_BUFFER_NOT_READY: RDS register buffer is not ready to use.
    QND_RDS_BUFFER_READY: RDS register buffer is ready to use. You can now
    load (for TX) or unload (for RX) data from/to RDS buffer
**********************************************************************/
UINT8 QND_RDSCheckBufferReady(void) 
{
    if (QN8027_ReadReg(SYSTEM1) & TXREQ) 
    {       
        QN8027_WriteReg(SYSTEM2,QN8027_ReadReg(SYSTEM2)^RDSTXRDY);  
        return QND_RDS_BUFFER_READY;
    }
    return QND_RDS_BUFFER_NOT_READY;
}

static int qn8027_remove(struct i2c_client *client)
{
	int result = 0;
	sysfs_remove_group(&qn8027_data.hwmon_dev->kobj, &qn8027_attribute_group);
	if (qn8027_data.hwmon_dev) {
		hwmon_device_unregister(qn8027_data.hwmon_dev);
		qn8027_data.hwmon_dev = NULL;
	}

	return result;
}

static int qn8027_i2c_test(void)
{
	unsigned char cid2;
	cid2 = QN8027_ReadReg(CID2);
	cid2 = cid2 >> 4;
	pr_info("%s: csi2 = %x\n",__func__, cid2);
	if (cid2 == 0x04) {
		return 0;
	}
	return -ENODEV;
}

static int  qn8027_probe(struct i2c_client *client,
								  const struct i2c_device_id *id)
{
	int result;
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		result = -ENODEV;
		goto out;
	}
	qn8027_data.i2c_dev = client;
	i2c_set_clientdata(client, &qn8027_data);
	QND_Init();
	if (qn8027_i2c_test()) {
		pr_err("%s error, no device found!\n", __func__);
		return -ENODEV;
	}
	qn8027_data.hwmon_dev = hwmon_device_register(&client->dev);
	if (!qn8027_data.hwmon_dev) {
		pr_info("qn8027 register hwmon error!\n");
		result = -ENODEV;
		goto out;
	}
	result = sysfs_create_group(&qn8027_data.hwmon_dev->kobj, &qn8027_attribute_group);
	if (result) {
		dev_err(&client->dev, "create sys failed\n");
	}
	QND_SetSysMode(QND_MODE_SLEEP);
out:
	return result;
}

int qn8027_i2c_detect(struct i2c_client *client, struct i2c_board_info *info)
{
	struct i2c_adapter *adapter = client->adapter;

	if (qn8027_data.twi_id == adapter->nr) {
		pr_info("%s: Detected chip %s at adapter %d, address 0x%02x\n",
				__func__, QN8027_DRV_NAME, i2c_adapter_id(adapter), client->addr);

		strlcpy(info->type, QN8027_DRV_NAME, I2C_NAME_SIZE);
		return 0;
	} else {
		return -ENODEV;
	}
}
static const struct i2c_device_id qn8027_id[] = {
	{ QN8027_DRV_NAME, 1 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, qn8027_id);

static 	const unsigned short i2c_addr[2] = {0x58 >> 1, I2C_CLIENT_END};


#ifdef CONFIG_PM
static int qn8027_suspend(struct device *dev)
{
	pr_info("%s\n", __func__);
	QND_SetSysMode(QND_MODE_SLEEP);
	return 0;
}
static int qn8027_resume(struct device *dev)
{
	pr_info("%s\n", __func__);
	if (qn8027_data.enable) {
		QND_SetSysMode(QND_MODE_WAKEUP | QND_MODE_TX);
	}
	return 0;
}

static const struct dev_pm_ops qn8027_pm_ops = {
	.suspend	= qn8027_suspend,
	.resume		= qn8027_resume,
};
#endif
static const struct of_device_id transmit_fm_id_match[] = {
	{.compatible = "allwinner,sun8i11p3-para-transmit-fm", },		
};

static struct i2c_driver qn8027_i2c_driver = {
	.class = I2C_CLASS_HWMON,
	.driver = {
		.name	= QN8027_DRV_NAME,
		.owner	= THIS_MODULE,
#ifdef CONFIG_PM
		.pm	= &qn8027_pm_ops,
#endif
	   .of_match_table =transmit_fm_id_match,
	},
	.probe	= qn8027_probe,
	.remove	= qn8027_remove,
	.id_table = qn8027_id,
	.address_list	= i2c_addr,
	.detect = qn8027_i2c_detect,
};

static int __init qn8027_init(void)	
{
	int err = 0;
	int ret=-1;
	struct device_node *np = NULL;
	np = of_find_node_by_name(NULL,"Transmit_fm");
	if (!np) {
		 pr_err("ERROR! get Transmit_fm, func:%s, line:%d\n",__FUNCTION__, __LINE__);
		 goto exit;
	}
	ret = of_property_read_u32(np,"fm_used",&tr_fm_status.fm_used);
	if (ret) {
		 pr_err("ERROR! get transmit_fm_used failed, func:%s, line:%d\n",__FUNCTION__, __LINE__);
		 goto exit;
	}
	
	if (!tr_fm_status.fm_used) {
		pr_err("%s: fm.used is null\n", __func__);
		goto exit;
	}
	ret = of_property_read_u32(np, "used_id", &tr_fm_status.fm_i2c_id);
	if (ret) {
		 pr_err("get fm twi_id is fail, %d\n", ret);
		 goto exit;
	}
	else
		{
		 qn8027_data.twi_id = tr_fm_status.fm_i2c_id;
	}
    err = i2c_add_driver(&qn8027_i2c_driver);
	if (err) {
		pr_err("%s i2c_add_driver error\n", __FUNCTION__);
		goto exit;
	}
exit:
	return err;
}
static void __exit qn8027_exit(void)
{
	i2c_del_driver(&qn8027_i2c_driver);
}
module_init(qn8027_init);
module_exit(qn8027_exit);
MODULE_DESCRIPTION("fm transmit driver");
MODULE_AUTHOR("weiweiyin");
MODULE_LICENSE("GPL");

