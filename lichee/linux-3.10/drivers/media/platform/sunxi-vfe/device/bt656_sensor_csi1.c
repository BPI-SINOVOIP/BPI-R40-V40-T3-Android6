/* **************************************************************************************
 *bt656_sensor1.c
 *A V4L2 driver for OV2710_AW6131 cameras
 *Copyright (c) 2014 by Allwinnertech Co., Ltd.http://www.allwinnertech.com
 *	Version		Author		Date				Description
 *	1.0			wxh	   2016/1/15		bt656_sensor1 YUV sensor Support
 ****************************************************************************************
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/videodev2.h>
#include <linux/clk.h>
#include <media/v4l2-device.h>
#include <media/v4l2-chip-ident.h>
#include <media/v4l2-mediabus.h>
#include <linux/io.h>
#include "camera.h"
#include "sensor_helper.h"

MODULE_AUTHOR("wxh");
MODULE_DESCRIPTION("A low-level driver for bt656_sensor1  sensors");
MODULE_LICENSE("GPL");

/*for internel driver debug*/
#define DEV_DBG_EN      1
#ifdef DEV_DBG_EN
#define vfe_dev_dbg(x, arg...)	printk("[bt656_sensor1]"x,	##arg)
#else
#define vfe_dev_dbg(x, arg...)
#endif
#define vfe_dev_err(x, arg...)	printk("[bt656_sensor1]"x,	##arg)
#define vfe_dev_print(x, arg...)	printk("[bt656_sensor1 ]"x,	##arg)
#define LOG_ERR_RET(x)  { \
							int ret;  \
							ret = x; \
							if (ret < 0) {\
								vfe_dev_err("error at %s\n", __func__);  \
								return ret; \
							} \
						}

/*define module timing*/
#define MCLK              (24*1000*1000)
#define VREF_POL          V4L2_MBUS_VSYNC_ACTIVE_LOW
#define HREF_POL          V4L2_MBUS_HSYNC_ACTIVE_HIGH
//#define CLK_POL           V4L2_MBUS_PCLK_SAMPLE_RISING
#define CLK_POL           V4L2_MBUS_PCLK_SAMPLE_FALLING
#define V4L2_IDENT_SENSOR  0x6131

/*
 * Our nominal (default) frame rate.
 */
#ifdef FPGA
#define SENSOR_FRAME_RATE	15
#else
#define SENSOR_FRAME_RATE	25
#endif

/*
 * The bt656_sensor i2c address
 */

//#define OV2710_WRITE_ADDR	(0x36)
//#define OV2710_READ_ADDR	(0x37)

/*static struct delayed_work sensor_s_ae_ratio_work;*/
static struct v4l2_subdev *glb_sd;
#define SENSOR_NAME "bt656_sensor1"

/*
 * Information we maintain about a known sensor.
 */
struct sensor_format_struct;	/* coming later */

struct cfg_array {		/* coming later */
	struct regval_list *regs;
	int size;
};

static inline struct sensor_info *to_state(struct v4l2_subdev *sd)
{
	return container_of(sd, struct sensor_info, sd);
}

/*
 * The default register settings
 *
 */
//static struct regval_list sensor_default_regs[] = 



static struct regval_list sensor_720p_30fps_regs[] = {
	/*add by Nathan 20160118 */
	

};

static struct regval_list sensor_480p_30fps_regs[] = {
	/*add by Nathan 20160118 */
	

};

static struct regval_list sensor_640x360p_30fps_regs[] = {
	/*add by Nathan 20160118 */
	

};

static struct regval_list sensor_320x240_30fps_regs[] = {
	/*add by Nathan 20160118 */
	
};

/*
 * Here we'll try to encapsulate the changes for just the output
 * video format.
 */

static struct regval_list sensor_fmt_yuv422_yuyv[] = {

};

static struct regval_list sensor_fmt_yuv422_yvyu[] = {

};

static struct regval_list sensor_fmt_yuv422_vyuy[] = {

};

static struct regval_list sensor_fmt_yuv422_uyvy[] = {

};

static struct regval_list sensor_fmt_raw[] = {

};

static int sensor_g_exp(struct v4l2_subdev *sd, __s32 *value)
{
	struct sensor_info *info = to_state(sd);

	*value = info->exp;
	vfe_dev_dbg("sensor_get_exposure = %d\n", info->exp);
	return 0;
}

/*
static int sensor_s_exp(struct v4l2_subdev *sd, unsigned int exp_val)
{
	struct sensor_info *info = to_state(sd);

	info->exp = exp_val;
	return 0;
}
*/

static int sensor_g_gain(struct v4l2_subdev *sd, __s32 *value)
{
	struct sensor_info *info = to_state(sd);

	*value = info->gain;
	vfe_dev_dbg("sensor_get_gain = %d\n", info->gain);
	return 0;
}

/*
static int sensor_s_gain(struct v4l2_subdev *sd, int gain_val)
{
	struct sensor_info *info = to_state(sd);

	info->gain = gain_val;

	return 0;

}
*/

static int sensor_s_exp_gain(struct v4l2_subdev *sd,
			     struct sensor_exp_gain *exp_gain)
{
	int exp_val, gain_val;
	struct sensor_info *info = to_state(sd);

	exp_val = exp_gain->exp_val;
	gain_val = exp_gain->gain_val;

	info->exp = exp_val;
	info->gain = gain_val;
	return 0;
}

static int sensor_s_sw_stby(struct v4l2_subdev *sd, int on_off)
{
	int ret = 0;
	return ret;
}

/*
 * Stuff that knows about the sensor.
 */

static int sensor_power(struct v4l2_subdev *sd, int on)
{
	int ret;
	ret = 0;
	switch (on) {
	case CSI_SUBDEV_STBY_ON:
		vfe_dev_dbg("CSI_SUBDEV_STBY_ON!\n");
		cci_lock(sd);
		vfe_gpio_write(sd, PWDN, CSI_GPIO_HIGH);
		vfe_set_mclk(sd, OFF);
		cci_unlock(sd);
		break;
	case CSI_SUBDEV_STBY_OFF:
		vfe_dev_dbg("CSI_SUBDEV_STBY_OFF!\n");
		cci_lock(sd);
		vfe_set_mclk_freq(sd, MCLK);
		vfe_set_mclk(sd, ON);
		//usleep_range(10000, 12000);
		usleep_range(200, 300);
		vfe_gpio_write(sd, PWDN, CSI_GPIO_LOW);
		//usleep_range(10000, 12000);
		usleep_range(200, 300);
		cci_unlock(sd);
		ret = sensor_s_sw_stby(sd, CSI_GPIO_LOW);
		if (ret < 0)
			vfe_dev_err("soft stby off falied!\n");
		//usleep_range(10000, 12000);
		usleep_range(200, 300);

		break;
	case CSI_SUBDEV_PWR_ON:
		vfe_dev_dbg("CSI_SUBDEV_PWR_ON!\n");
		cci_lock(sd);
		vfe_gpio_set_status(sd, PWDN, 1);	/*set the gpio to output */
		vfe_gpio_set_status(sd, RESET, 1);	/*set the gpio to output */
		vfe_gpio_write(sd, RESET, CSI_GPIO_HIGH);
		vfe_gpio_write(sd, PWDN, CSI_GPIO_HIGH);
		//usleep_range(1000, 1200);
		usleep_range(200, 300);
		vfe_set_pmu_channel(sd, AFVDD, ON);	/*1.2V  CVDD_12 */
		vfe_set_pmu_channel(sd, DVDD, ON);	/*VCAM_D 1.5v */
		//usleep_range(1000, 1200);
		usleep_range(200, 300);
		vfe_set_pmu_channel(sd, IOVDD, ON);	/*VCAM_IO 2.8v */

		//usleep_range(1000, 1200);
		usleep_range(200, 300);
		vfe_set_pmu_channel(sd, AVDD, ON);	/* VCAM_AF 3.3v */
		//usleep_range(1000, 1200);
		usleep_range(200, 300);

		vfe_gpio_write(sd, PWDN, CSI_GPIO_LOW);
		//usleep_range(10000, 12000);
		usleep_range(200, 300);
		vfe_gpio_write(sd, RESET, CSI_GPIO_LOW);
		//usleep_range(30000, 32000);
		usleep_range(200, 300);
		vfe_gpio_write(sd, RESET, CSI_GPIO_HIGH);
		vfe_set_mclk_freq(sd, MCLK);
		vfe_set_mclk(sd, ON);
		//usleep_range(10000, 12000);
		usleep_range(200, 300);
		cci_unlock(sd);
		break;

	case CSI_SUBDEV_PWR_OFF:

		vfe_dev_dbg("CSI_SUBDEV_PWR_OFF!\n");
		cci_lock(sd);
		vfe_gpio_set_status(sd, PWDN, 1);	/*set the gpio to output */
		vfe_gpio_set_status(sd, RESET, 1);	/*set the gpio to output */
		vfe_gpio_write(sd, RESET, CSI_GPIO_LOW);
		vfe_gpio_write(sd, PWDN, CSI_GPIO_HIGH);
		vfe_set_mclk(sd, OFF);
		vfe_set_pmu_channel(sd, AVDD, OFF);	/* VCAM_AF 3.3v */
		//usleep_range(10000, 12000);
		usleep_range(200, 300);
		vfe_set_pmu_channel(sd, DVDD, OFF);	/*VCAM_D 1.5v */
		//usleep_range(10000, 12000);
		usleep_range(200, 300);
		vfe_set_pmu_channel(sd, IOVDD, OFF);	/*VCAM_IO 2.8v */
		vfe_set_pmu_channel(sd, AFVDD, OFF);	/*1.2V  CVDD_12 */
		vfe_gpio_set_status(sd, RESET, 0);	/*set the gpio to input */
		vfe_gpio_set_status(sd, PWDN, 0);	/*set the gpio to input */
		cci_unlock(sd);

		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int sensor_reset(struct v4l2_subdev *sd, u32 val)
{
	switch (val) {
	case 0:
		vfe_gpio_write(sd, RESET, CSI_GPIO_HIGH);
		//usleep_range(10000, 12000);
		usleep_range(200, 300);
		break;
	case 1:
		vfe_gpio_write(sd, RESET, CSI_GPIO_LOW);
		//usleep_range(10000, 12000);
		usleep_range(200, 300);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int sensor_debug(struct v4l2_subdev *sd)
{
	int ret;
	struct regval_list regs;

	vfe_dev_dbg("********into  sensor_debug********\n");

	regs.addr = 0xfffe;
	regs.data = 0x80;
	ret = sensor_write(sd, regs.addr, regs.data);

	regs.addr = 0x50;
	ret = sensor_read(sd, regs.addr, &regs.data);
	vfe_dev_dbg("********read reg[0x50]= %x ********\n", regs.data);

	regs.addr = 0x58;
	ret = sensor_read(sd, regs.addr, &regs.data);
	if (ret < 0) {
		vfe_dev_err("sensor_read reg[0x58] at sensor_debug!\n");
		return ret;
	}
	vfe_dev_dbg("********read reg[0x58]= %x ********\n", regs.data);

	return 0;
}
#if 0
static int sensor_detect(struct v4l2_subdev *sd)
{
	int ret;
	struct regval_list regs;

	regs.addr = 0xfffd;
	regs.data = 0x80;
	ret = sensor_write(sd, regs.addr, regs.data);

	sensor_write(sd, 0xfffd, 0x80);
	sensor_write(sd, 0xfffe, 0x80);

	regs.addr = 0xfffe;
	regs.data = 0x80;
	ret = sensor_write(sd, regs.addr, regs.data);

	regs.addr = 0xfffe;
	ret = sensor_read(sd, regs.addr, &regs.data);

	regs.addr = 0x0003;
	ret = sensor_read(sd, regs.addr, &regs.data);
	if (ret < 0) {
		vfe_dev_err("sensor_read err at sensor_detect!\n");
		return ret;
	}
	vfe_dev_dbg("OV2710_aw6131 chip  ID =%x\n ", regs.data);

	return 0;
}
#endif

static int sensor_init(struct v4l2_subdev *sd, u32 val)
{
	int ret;
	struct sensor_info *info = to_state(sd);

	vfe_dev_dbg("sensor_init\n");

	/*Make sure it is a target sensor */
	#if 0
	ret = sensor_detect(sd);
	if (ret) {
		vfe_dev_err("chip found is not an target chip.\n");
		return ret;
	}
#endif

	vfe_get_standby_mode(sd, &info->stby_mode);

	if ((info->stby_mode == HW_STBY || info->stby_mode == SW_STBY)
	    && info->init_first_flag == 0) {
		vfe_dev_print("stby_mode and init_first_flag = 0\n");
		return 0;
	}

	info->focus_status = 0;
	info->low_speed = 0;
	info->width = HD720_WIDTH;
	info->height = HD720_HEIGHT;
	info->hflip = 0;
	info->vflip = 0;
	info->gain = 0;

	info->tpf.numerator = 1;
	info->tpf.denominator = 25;
 #if 0
	ret = sensor_write_array(sd, sensor_default_regs, ARRAY_SIZE(sensor_default_regs));
	if (ret < 0) {
		vfe_dev_err("write sensor_default_regs error\n");
		return ret;
	}

	sensor_debug(sd);
	#endif

	if (info->stby_mode == 0)
		info->init_first_flag = 0;

	info->preview_first_flag = 1;

	return 0;
}

static long sensor_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	int ret = 0;
	struct sensor_info *info = to_state(sd);
	switch (cmd) {
	case GET_CURRENT_WIN_CFG:
		if (info->current_wins != NULL) {
			memcpy(arg, info->current_wins,
			       sizeof(struct sensor_win_size));
			ret = 0;
		} else {
			vfe_dev_err("empty wins!\n");
			ret = -1;
		}
		break;
	case SET_FPS:
		break;
	case ISP_SET_EXP_GAIN:
		sensor_s_exp_gain(sd, (struct sensor_exp_gain *)arg);
		break;
	default:
		return -EINVAL;
	}
	return ret;
}

static struct sensor_format_struct {
	__u8 *desc;
	enum v4l2_mbus_pixelcode mbus_code;
	struct regval_list *regs;
	int regs_size;
	int bpp;   /* Bytes per pixel */
} sensor_formats[] = { 
	 {
	.desc = "BT656 1CH",
	.mbus_code = V4L2_MBUS_FMT_UYVY8_2X8,
	.regs = NULL,
	.regs_size = 0,
	.bpp = 2,
	},
};
#define N_FMTS ARRAY_SIZE(sensor_formats)

/*
 * Then there is the issue of window sizes.  Try to capture the info here.
 */
static struct sensor_win_size sensor_win_sizes[] = {
/* Full HD 1920 *1080 25fps*/
 {
	 // .height = 710,
		     .width = HD720_WIDTH,
		     .height = HD720_HEIGHT,
		     .hoffset = 0,
		     .voffset = 0,
		     .regs = sensor_720p_30fps_regs,
		     .regs_size = ARRAY_SIZE(sensor_720p_30fps_regs),
		     .set_size = NULL,
	 } 
};

#define N_WIN_SIZES (ARRAY_SIZE(sensor_win_sizes))

static int sensor_enum_fmt(struct v4l2_subdev *sd, unsigned index,
			   enum v4l2_mbus_pixelcode *code)
{
	if (index >= N_FMTS)
		return -EINVAL;

	*code = sensor_formats[index].mbus_code;
	return 0;
}

static int sensor_enum_size(struct v4l2_subdev *sd,
			    struct v4l2_frmsizeenum *fsize)
{
	if (fsize->index > N_WIN_SIZES - 1)
		return -EINVAL;

	fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
	fsize->discrete.width = sensor_win_sizes[fsize->index].width;
	fsize->discrete.height = sensor_win_sizes[fsize->index].height;

	return 0;
}

static int sensor_try_fmt_internal(struct v4l2_subdev *sd,
				   struct v4l2_mbus_framefmt *fmt,
				   struct sensor_format_struct **ret_fmt,
				   struct sensor_win_size **ret_wsize)
{
	int index;
	struct sensor_win_size *wsize;
	struct sensor_info *info = to_state(sd);

	for (index = 0; index < N_FMTS; index++)
		if (sensor_formats[index].mbus_code == fmt->code)
			break;

	if (index >= N_FMTS)
		return -EINVAL;

	if (ret_fmt != NULL)
		*ret_fmt = sensor_formats + index;

	/*
	 * Fields: the sensor devices claim to be progressive.
	 */
	fmt->field = V4L2_FIELD_NONE;

	/*
	 * Round requested image size down to the nearest
	 * we support, but not below the smallest.
	 */
	for (wsize = sensor_win_sizes; wsize < sensor_win_sizes + N_WIN_SIZES;
	     wsize++)
		if (fmt->width >= wsize->width && fmt->height >= wsize->height)
			break;

	if (wsize >= sensor_win_sizes + N_WIN_SIZES)
		wsize--;	/* Take the smallest one */
	if (ret_wsize != NULL)
		*ret_wsize = wsize;
	/*
	 * Note the size we'll actually handle.
	 */
	fmt->width = wsize->width;
	fmt->height = wsize->height;
	info->current_wins = wsize;
	return 0;
}

static int sensor_try_fmt(struct v4l2_subdev *sd,
			  struct v4l2_mbus_framefmt *fmt)
{
	return sensor_try_fmt_internal(sd, fmt, NULL, NULL);
}

static int sensor_g_mbus_config(struct v4l2_subdev *sd,
				struct v4l2_mbus_config *cfg)
{
	cfg->type = V4L2_MBUS_BT656;
	cfg->flags = CLK_POL | CSI_CH_0;

	return 0;
}

static int sensor_s_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *fmt)
{
	int ret;
	struct sensor_format_struct *sensor_fmt;
	struct sensor_win_size *wsize;
	struct sensor_info *info = to_state(sd);

	vfe_dev_dbg("sensor_s_fmt\n");

	ret = sensor_try_fmt_internal(sd, fmt, &sensor_fmt, &wsize);
	if (ret)
		return ret;

	if (info->capture_mode == V4L2_MODE_VIDEO) {
		/*video */
	} else if (info->capture_mode == V4L2_MODE_IMAGE) {
		/*image */
	}

	//LOG_ERR_RET
		//(sensor_write_array
		    //(sd, sensor_fmt->regs, sensor_fmt->regs_size))

	    ret = 0;
	if (wsize->regs)
		//LOG_ERR_RET(sensor_write_array(sd, wsize->regs, wsize->regs_size))

		    if (wsize->set_size)
			LOG_ERR_RET(wsize->set_size(sd))

			    info->fmt = sensor_fmt;
	info->width = wsize->width;
	info->height = wsize->height;

	vfe_dev_print("s_fmt set width = %d, height = %d\n", wsize->width,
		      wsize->height);

	if (info->capture_mode == V4L2_MODE_VIDEO) {
		/*video */
	} else {
		/*capture image */
	}

	//sensor_debug(sd);

	return 0;
}

/*
 * Implement G/S_PARM.  There is a "high quality" mode we could try
 * to do someday; for now, we just do the frame rate tweak.
 */
static int sensor_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	struct v4l2_captureparm *cp = &parms->parm.capture;
	struct sensor_info *info = to_state(sd);

	if (parms->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;

	memset(cp, 0, sizeof(struct v4l2_captureparm));
	cp->capability = V4L2_CAP_TIMEPERFRAME;
	cp->capturemode = info->capture_mode;

	return 0;
}

static int sensor_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	struct v4l2_captureparm *cp = &parms->parm.capture;
	struct sensor_info *info = to_state(sd);

	vfe_dev_dbg("sensor_s_parm\n");

	if (parms->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;

	if (info->tpf.numerator == 0)
		return -EINVAL;

	info->capture_mode = cp->capturemode;

	return 0;
}

static int sensor_queryctrl(struct v4l2_subdev *sd, struct v4l2_queryctrl *qc)
{
	/* Fill in min, max, step and default value for these controls. */
	/* see include/linux/videodev2.h for details */

	switch (qc->id) {
	case V4L2_CID_GAIN:
		return v4l2_ctrl_query_fill(qc, 1 * 16, 16 * 16, 1, 16);
	case V4L2_CID_EXPOSURE:
		return v4l2_ctrl_query_fill(qc, 1, 65536 * 16, 1, 1);
	case V4L2_CID_FRAME_RATE:
		return v4l2_ctrl_query_fill(qc, 15, 120, 1, 30);
	}
	return -EINVAL;
}

static int sensor_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	switch (ctrl->id) {
	case V4L2_CID_GAIN:
		return sensor_g_gain(sd, &ctrl->value);
	case V4L2_CID_EXPOSURE:
		return sensor_g_exp(sd, &ctrl->value);
	}
	return -EINVAL;
}

static int sensor_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
#if 0
	struct v4l2_queryctrl qc;
	int ret;

	qc.id = ctrl->id;
	ret = sensor_queryctrl(sd, &qc);
	if (ret < 0) {
		return ret;
	}

	if (ctrl->value < qc.minimum || ctrl->value > qc.maximum) {
		vfe_dev_err("max gain qurery is %d,min gain qurey is %d\n",
			    qc.maximum, qc.minimum);
		return -ERANGE;
	}

	switch (ctrl->id) {
	case V4L2_CID_GAIN:
		return sensor_s_gain(sd, ctrl->value);
	case V4L2_CID_EXPOSURE:
		return sensor_s_exp(sd, ctrl->value);
	}
	return -EINVAL;
#else
	return 0;
#endif
}

static int sensor_g_chip_ident(struct v4l2_subdev *sd,
			       struct v4l2_dbg_chip_ident *chip)
{
	//struct i2c_client *client = v4l2_get_subdevdata(sd);

	//return v4l2_chip_ident_i2c_client(client, chip, V4L2_IDENT_SENSOR, 0);
	return 0;
}

/* ----------------------------------------------------------------------- */

static const struct v4l2_subdev_core_ops sensor_core_ops = {
	.g_chip_ident = sensor_g_chip_ident,
	.g_ctrl = sensor_g_ctrl,
	.s_ctrl = sensor_s_ctrl,
	.queryctrl = sensor_queryctrl,
	.reset = sensor_reset,
	.init = sensor_init,
	.s_power = sensor_power,
	.ioctl = sensor_ioctl,
};

static const struct v4l2_subdev_video_ops sensor_video_ops = {
	.enum_mbus_fmt = sensor_enum_fmt,
	.enum_framesizes = sensor_enum_size,
	.try_mbus_fmt = sensor_try_fmt,
	.s_mbus_fmt = sensor_s_fmt,
	.s_parm = sensor_s_parm,
	.g_parm = sensor_g_parm,
	.g_mbus_config = sensor_g_mbus_config,
};

static const struct v4l2_subdev_ops sensor_ops = {
	.core = &sensor_core_ops,
	.video = &sensor_video_ops,
};

/* ----------------------------------------------------------------------- */
static struct cci_driver cci_drv = {
	.name = SENSOR_NAME,
	.addr_width = CCI_BITS_8,
	.data_width = CCI_BITS_8,
};

static int sensor_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct v4l2_subdev *sd;
	struct sensor_info *info;
	info = kzalloc(sizeof(struct sensor_info), GFP_KERNEL);
	if (info == NULL)
		return -ENOMEM;
	sd = &info->sd;
	glb_sd = sd;
	cci_dev_probe_helper(sd, client, &sensor_ops, &cci_drv);

	info->fmt = &sensor_formats[0];
	info->af_first_flag = 1;
	info->init_first_flag = 1;

	printk("[richard]sensor_probe\n");

	return 0;
}

static int sensor_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd;
	sd = cci_dev_remove_helper(client, &cci_drv);
	kfree(to_state(sd));
	return 0;
}

static const struct i2c_device_id sensor_id[] = {
	{SENSOR_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, sensor_id);

static struct i2c_driver sensor_driver = {
	.driver = {
		   .owner = THIS_MODULE,
		   .name = SENSOR_NAME,
		   },
	.probe = sensor_probe,
	.remove = sensor_remove,
	.id_table = sensor_id,
};

static __init int init_sensor(void)
{
	return cci_dev_init_helper(&sensor_driver);
}

static __exit void exit_sensor(void)
{
	cci_dev_exit_helper(&sensor_driver);
}

module_init(init_sensor);
module_exit(exit_sensor);
