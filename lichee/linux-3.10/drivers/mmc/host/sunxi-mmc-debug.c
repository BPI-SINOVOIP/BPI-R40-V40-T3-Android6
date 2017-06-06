#include <linux/clk.h>
#include <linux/clk/sunxi.h>

#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/scatterlist.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/reset.h>

#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>
#include <linux/stat.h>


#include <linux/mmc/host.h>
#include "sunxi-mmc.h"
#include "sunxi-mmc-debug.h"
#include "sunxi-mmc-export.h"
#include "sunxi-mmc-sun50iw1p1-2.h"


#define GPIO_BASE_ADDR	0x1c20800
#define CCMU_BASE_ADDR	0x1c20000

static struct device_attribute dump_register[3];


void sunxi_mmc_dumphex32(struct sunxi_mmc_host* host, char* name, char* base, int len)
{
	u32 i;
	printk("dump %s registers:", name);
	for (i=0; i<len; i+=4) {
		if (!(i&0xf))
			printk("\n0x%p : ", base + i);
		printk("0x%08x ",__raw_readl(host->reg_base+i));
	}
	printk("\n");
}

void sunxi_mmc_dump_des(struct sunxi_mmc_host* host, char* base, int len)
{
	u32 i;
	printk("dump des mem\n");
	for (i=0; i<len; i+=4) {
		if (!(i&0xf))
			printk("\n0x%p : ", base + i);
		printk("0x%08x ",*(u32 *)(base+i));
	}
	printk("\n");
}



static ssize_t maual_insert_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
#if 0
	int ret;

	ret = snprintf(buf, PAGE_SIZE, "Usage: \"echo 1 > insert\" to scan card\n");
	return ret;
#else
	int ret;

	struct platform_device *pdev = to_platform_device(dev);
	struct mmc_host	*mmc = platform_get_drvdata(pdev);
	
	//ret = snprintf(buf, PAGE_SIZE, "Insert State: %d\n", mmc->rescan_pre_state);
	ret = snprintf(buf, PAGE_SIZE, "Insert State: %d\n", mmc->present_r);
	
	return ret;
#endif
}

static ssize_t maual_insert_store(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t count)
{
	int ret;
	char *end;
	unsigned long insert = simple_strtoul(buf, &end, 0);
	struct platform_device *pdev = to_platform_device(dev);
	struct mmc_host	*mmc = platform_get_drvdata(pdev);
	if (end == buf) {
		ret = -EINVAL;
		return ret;
	}
#if 0
	dev_info(dev, "insert %ld\n", insert);
	if(insert)
		mmc_detect_change(mmc,0);
	else
		dev_info(dev, "no detect change\n");
	//sunxi_mmc_rescan_card(0);
#else
    if (insert) {
        mmc->fremove = 1;
        dev_err(dev, "maual_insert_store: host->fremove = %d\n", mmc->fremove);
        mmc_detect_change(mmc,msecs_to_jiffies(50));
    } else {
        mmc->fremove = 0;
        dev_err(dev, "maual_insert_store: host->fremove = %d\n", mmc->fremove);
        mmc_detect_change(mmc,msecs_to_jiffies(50));
    }
#endif
	ret = count;
	return ret;
}


void sunxi_dump_reg(struct mmc_host *mmc)
{
	int i = 0;
	struct sunxi_mmc_host *host = mmc_priv(mmc);	
	void __iomem *gpio_ptr =  ioremap(GPIO_BASE_ADDR, 0x300);
	void __iomem *ccmu_ptr =  ioremap(CCMU_BASE_ADDR, 0x400);

	printk("Dump %s (p%x) regs :\n" , mmc_hostname(mmc), host->phy_index);
	for (i=0; i<0x180; i+=4) {
		if (!(i&0xf))
			printk("\n0x%p : ", (host->reg_base + i));
		printk("%08x ", readl(host->reg_base + i));
	}
	printk("\n");


	printk("Dump gpio regs:\n");
	
	for (i=0; i<0x120; i+=4) {
		if (!(i&0xf))
			printk("\n0x%p : ", (gpio_ptr + i));
		printk("%08x ", readl(gpio_ptr + i));
	}
	printk("\n");

	printk("Dump gpio irqc regs:\n");
	for (i=0x200; i<0x260; i+=4) {
		if (!(i&0xf))
			printk("\n0x%p : ", (gpio_ptr + i));
		printk("%08x ", readl(gpio_ptr + i));
	}
	printk("\n");


	printk("Dump ccmu regs:gating\n");
	for (i=0x60; i<=0x80; i+=4) {
		if (!(i&0xf))
			printk("\n0x%p : ", (ccmu_ptr + i));
		printk("%08x ", readl(ccmu_ptr + i));
	}
	printk("\n");


	printk("Dump ccmu regs:module clk\n");
	for (i=0x80; i<=0x100; i+=4) {
		if (!(i&0xf))
			printk("\n0x%p : ", (ccmu_ptr + i));
		printk("%08x ", readl(ccmu_ptr + i));
	}
	printk("\n");

	printk("Dump ccmu regs:reset\n");
	for (i=0x2c0; i<=0x2e0; i+=4) {
		if (!(i&0xf))
			printk("\n0x%p : ", (ccmu_ptr + i));
		printk("%08x ", readl(ccmu_ptr + i));
	}
	printk("\n");

	iounmap(gpio_ptr);
	iounmap(ccmu_ptr);

}


/*
static ssize_t dump_register_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	char *p = buf;
	int i = 0;
	struct platform_device *pdev = to_platform_device(dev);
	struct mmc_host	*mmc = platform_get_drvdata(pdev);
	struct sunxi_mmc_host *host = mmc_priv(mmc);
	void __iomem *gpio_ptr =  ioremap(GPIO_BASE_ADDR, 0x300);
	void __iomem *ccmu_ptr =  ioremap(CCMU_BASE_ADDR, 0x400);


	p += sprintf(p, "Dump sdmmc regs:\n");
	for (i=0; i<0x180; i+=4) {
		if (!(i&0xf))
			p += sprintf(p, "\n0x%08llx : ", (u64)(host->reg_base + i));
		p += sprintf(p, "%08x ", readl(host->reg_base + i));
	}
	p += sprintf(p, "\n");


	p += sprintf(p, "Dump gpio regs:\n");
	
	for (i=0; i<0x120; i+=4) {
		if (!(i&0xf))
			p += sprintf(p, "\n0x%08llx : ", (u64)(gpio_ptr + i));
		p += sprintf(p, "%08x ", readl(gpio_ptr + i));
	}
	p += sprintf(p, "\n");

	p += sprintf(p, "Dump gpio irqc regs:\n");
	for (i=0x200; i<0x260; i+=4) {
		if (!(i&0xf))
			p += sprintf(p, "\n0x%08llx : ", (u64)(gpio_ptr + i));
		p += sprintf(p, "%08x ", readl(gpio_ptr + i));
	}
	p += sprintf(p, "\n");


	p += sprintf(p, "Dump ccmu regs:gating\n");
	for (i=0x60; i<=0x80; i+=4) {
		if (!(i&0xf))
			p += sprintf(p, "\n0x%08llx : ", (u64)(ccmu_ptr + i));
		p += sprintf(p, "%08x ", readl(ccmu_ptr + i));
	}
	p += sprintf(p, "\n");


	p += sprintf(p, "Dump ccmu regs:module clk\n");
	for (i=0x80; i<=0x100; i+=4) {
		if (!(i&0xf))
			p += sprintf(p, "\n0x%08llx : ", (u64)(ccmu_ptr + i));
		p += sprintf(p, "%08x ", readl(ccmu_ptr + i));
	}
	p += sprintf(p, "\n");

	p += sprintf(p, "Dump ccmu regs:reset\n");
	for (i=0x2c0; i<=0x2e0; i+=4) {
		if (!(i&0xf))
			p += sprintf(p, "\n0x%08llx : ", (u64)(ccmu_ptr + i));
		p += sprintf(p, "%08x ", readl(ccmu_ptr + i));
	}
	p += sprintf(p, "\n");


	iounmap(gpio_ptr);
	iounmap(ccmu_ptr);

	//dump_reg(host);

	return p-buf;

}
*/


static ssize_t dump_host_reg_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	char *p = buf;
	int i = 0;
	struct platform_device *pdev = to_platform_device(dev);
	struct mmc_host	*mmc = platform_get_drvdata(pdev);
	struct sunxi_mmc_host *host = mmc_priv(mmc);

	p += sprintf(p, "Dump sdmmc regs:\n");
	for (i=0; i<0x180; i+=4) {
		if (!(i&0xf))
			p += sprintf(p, "\n0x%p : ", (host->reg_base + i));
		p += sprintf(p, "%08x ", readl(host->reg_base + i));
	}
	p += sprintf(p, "\n");

	return p-buf;

}



static ssize_t dump_gpio_reg_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	char *p = buf;
	int i = 0;
	void __iomem *gpio_ptr =  ioremap(GPIO_BASE_ADDR, 0x300);

	p += sprintf(p, "Dump gpio regs:\n");
	for (i=0; i<0x120; i+=4) {
		if (!(i&0xf))
			p += sprintf(p, "\n0x%p : ", (gpio_ptr + i));
		p += sprintf(p, "%08x ", readl(gpio_ptr + i));
	}
	p += sprintf(p, "\n");

	p += sprintf(p, "Dump gpio irqc regs:\n");
	for (i=0x200; i<0x260; i+=4) {
		if (!(i&0xf))
			p += sprintf(p, "\n0x%p : ", (gpio_ptr + i));
		p += sprintf(p, "%08x ", readl(gpio_ptr + i));
	}
	p += sprintf(p, "\n");

	iounmap(gpio_ptr);

	return p-buf;

}



static ssize_t dump_ccmu_reg_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	char *p = buf;
	int i = 0;
	void __iomem *ccmu_ptr =  ioremap(CCMU_BASE_ADDR, 0x400);


	p += sprintf(p, "Dump ccmu\n");
	for (i=0x0; i<=0x400; i+=4) {
		if (!(i&0xf))
			p += sprintf(p, "\n0x%p : ", (ccmu_ptr + i));
		p += sprintf(p, "%08x ", readl(ccmu_ptr + i));
	}
	p += sprintf(p, "\n");

	iounmap(ccmu_ptr);
	//dump_reg(host);
	return p-buf;

}



static ssize_t dump_clk_dly_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	char *p = buf;
	struct platform_device *pdev = to_platform_device(dev);
	struct mmc_host	*mmc = platform_get_drvdata(pdev);
	struct sunxi_mmc_host *host = mmc_priv(mmc);

	if(host->sunxi_mmc_dump_dly_table){
		host->sunxi_mmc_dump_dly_table(host);
	}else{
		dev_warn(mmc_dev(mmc),"not found the dump dly table\n");
	}

	return p-buf;
}



int mmc_create_sys_fs(struct sunxi_mmc_host* host,struct platform_device *pdev)
{
	int ret;

	host->maual_insert.show = maual_insert_show;
	host->maual_insert.store = maual_insert_store;
	sysfs_attr_init(host->maual_insert.attr);
	host->maual_insert.attr.name = "sunxi_insert";
	host->maual_insert.attr.mode = S_IRUGO | S_IWUSR;
	ret = device_create_file(&pdev->dev, &host->maual_insert);
	if(ret)
		return ret;
	
	host->dump_register = dump_register;
	host->dump_register[0].show = dump_host_reg_show;
	sysfs_attr_init(host->dump_register[0].attr);
	host->dump_register[0].attr.name = "sunxi_dump_host_register";
	host->dump_register[0].attr.mode = S_IRUGO;
	ret = device_create_file(&pdev->dev, &host->dump_register[0]);
	if(ret)
		return ret;

	host->dump_register[1].show = dump_gpio_reg_show;
	sysfs_attr_init(host->dump_register[1].attr);
	host->dump_register[1].attr.name = "sunxi_dump_gpio_register";
	host->dump_register[1].attr.mode = S_IRUGO;
	ret = device_create_file(&pdev->dev, &host->dump_register[1]);
	if(ret)
		return ret;


	host->dump_register[2].show = dump_ccmu_reg_show;
	sysfs_attr_init(host->dump_register[2].attr);
	host->dump_register[2].attr.name = "sunxi_dump_ccmu_register";
	host->dump_register[2].attr.mode = S_IRUGO;
	ret = device_create_file(&pdev->dev, &host->dump_register[2]);
	if(ret)
		return ret;


	host->dump_clk_dly.show = dump_clk_dly_show;
	sysfs_attr_init(host->dump_clk_dly.attr);
	host->dump_clk_dly.attr.name = "sunxi_dump_clk_dly";
	host->dump_clk_dly.attr.mode = S_IRUGO;
	ret = device_create_file(&pdev->dev, &host->dump_clk_dly);
	if(ret)
		return ret;

	
	return ret;
}

void mmc_remove_sys_fs(struct sunxi_mmc_host* host,struct platform_device *pdev)
{
	device_remove_file(&pdev->dev, &host->maual_insert);
	device_remove_file(&pdev->dev, &host->dump_register[0]);
	device_remove_file(&pdev->dev, &host->dump_register[1]);
	device_remove_file(&pdev->dev, &host->dump_register[2]);
	device_remove_file(&pdev->dev, &host->dump_clk_dly);
}

#if defined(MMC_FORCE_REMOVE) && defined(CONFIG_PROC_FS)    // Added by hedong, 201603

static void *proc_seq_start(struct seq_file *seq, loff_t *pos)  
{  
    if (0 == *pos)  
    {  
        ++*pos;  
        return (void *)1; // return anything but NULL, just for test  
    }  
    return NULL;  
}  
  
static void *proc_seq_next(struct seq_file *seq, void *v, loff_t *pos)  
{  
    // only once, so no next  
    return NULL;  
}  
  
static void proc_seq_stop(struct seq_file *seq, void *v)  
{  
    // clean sth.  
    // nothing to do  
}  

static int fremove_seq_show(struct seq_file *seq, void *v)  
{
    struct mmc_host	*mmc = seq->private;
    
    seq_printf(seq, "fremove: %d\n", mmc->fremove);  
  
    return 0; //!! must be 0, or will show nothing T.T  
}

static const struct seq_operations fremove_proc_seq_ops = {
	.start = proc_seq_start,
	.next  = proc_seq_next,
	.stop  = proc_seq_stop,
	.show  = fremove_seq_show,
};

static int fremove_proc_open(struct inode *inode, struct file *file)
{
	struct seq_file *seq;
	int res;

	res = seq_open(file, &fremove_proc_seq_ops);
	if (!res) {
		/* recover the pointer buried in proc_dir_entry data */
		seq = file->private_data;
		seq->private = PDE_DATA(inode);
	}

	return res;
}

static ssize_t fremove_seq_write(struct file *file, const char __user *buffer, size_t count, loff_t *f_pos)  
{
    struct seq_file *seq = file->private_data;
    struct mmc_host	*mmc = seq->private;
    char str[100];
    u32 fremove;
    
    if (copy_from_user(str, buffer, count)) {  
        return -EFAULT;  
    }  
  
    fremove = simple_strtoul(str, NULL, 10);
    mmc->fremove = fremove;
    printk("%s: fremove = %d\n", __func__, fremove);
  
    return count;  
}  

static const struct file_operations fremove_proc_fops = {
	.owner   = THIS_MODULE,
	.open    = fremove_proc_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = seq_release,
	.write   = fremove_seq_write,
};

static int insert_seq_show(struct seq_file *seq, void *v)  
{
    struct mmc_host	*mmc = seq->private;
    
    seq_printf(seq, "insert: %d\n", mmc->present_r);  
  
    return 0; //!! must be 0, or will show nothing T.T  
}

static const struct seq_operations insert_proc_seq_ops = {
	.start = proc_seq_start,
	.next  = proc_seq_next,
	.stop  = proc_seq_stop,
	.show  = insert_seq_show,
};

static int insert_proc_open(struct inode *inode, struct file *file)
{
	struct seq_file *seq;
	int res;

	res = seq_open(file, &insert_proc_seq_ops);
	if (!res) {
		/* recover the pointer buried in proc_dir_entry data */
		seq = file->private_data;
		seq->private = PDE_DATA(inode);
	}

	return res;
}

static const struct file_operations insert_proc_fops = {
	.owner   = THIS_MODULE,
	.open    = insert_proc_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = seq_release,
};

void mmc_create_procfs(struct sunxi_mmc_host* host,struct platform_device *pdev)
{
    struct mmc_host	*mmc = platform_get_drvdata(pdev);
	struct device *dev = &pdev->dev;
	char mmc_proc_rootname[32] = {0};

    printk("%s\n", __func__);

    //make mmc dir in proc fs path
	snprintf(mmc_proc_rootname, sizeof(mmc_proc_rootname),
			"driver/%s", dev_name(dev));
	mmc->proc_root = proc_mkdir(mmc_proc_rootname, NULL);
	if (IS_ERR(mmc->proc_root))
		dev_err(dev, "%s: failed to create procfs \"driver/mmc\".\n", dev_name(dev));

    mmc->proc_fremove = proc_create_data("fremove", 0644, mmc->proc_root, &fremove_proc_fops, mmc);
	if (IS_ERR(mmc->proc_fremove))
		dev_err(dev, "%s: failed to create procfs \"fremove\".\n", dev_name(dev));
	
	mmc->proc_insert = proc_create_data("insert", 0644, mmc->proc_root, &insert_proc_fops, mmc);
	if (IS_ERR(mmc->proc_insert))
		dev_err(dev, "%s: failed to create procfs \"insert\".\n", dev_name(dev));
}

void mmc_remove_procfs(struct sunxi_mmc_host *host,struct platform_device *pdev)
{
    struct mmc_host	*mmc = platform_get_drvdata(pdev);
	struct device *dev = &pdev->dev;
	char mmc_proc_rootname[32] = {0};

    printk("%s\n", __func__);
    
	snprintf(mmc_proc_rootname, sizeof(mmc_proc_rootname),
		"driver/%s", dev_name(dev));
	remove_proc_entry("fremove", mmc->proc_fremove);
	remove_proc_entry("insert", mmc->proc_insert);
	remove_proc_entry(mmc_proc_rootname, NULL);
}
#endif
