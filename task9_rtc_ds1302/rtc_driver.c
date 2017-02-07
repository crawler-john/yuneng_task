#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <asm/uaccess.h>

#define DEVICE_NAME "rtc"
#define RTC_MAJOR 238
//#define RST AT91_PIN_PC25
//#define SCLK AT91_PIN_PC20
//#define IO AT91_PIN_PC27

#define GPIO_TO_PIN(bank, gpio) (32 * (bank) + (gpio))

void ds1302_writebyte(unsigned char dat)
{
	unsigned char i;
	unsigned char sda;

	gpio_set_value(GPIO_TO_PIN(1, 25), 0);//at91_set_gpio_value(SCLK, 0);
	udelay(2);
    gpio_request(GPIO_TO_PIN(1, 26), "rtc_io");
    gpio_direction_output(GPIO_TO_PIN(1, 26), 1);//at91_set_gpio_output(IO, dat & 0x01);
	udelay(2);
	for(i=0; i<8; i++){
		sda = dat & 0x01;
		gpio_set_value(GPIO_TO_PIN(1, 26), sda);//at91_set_gpio_value(IO, sda);
		udelay(2);
		gpio_set_value(GPIO_TO_PIN(1, 25), 1);//at91_set_gpio_value(SCLK, 1);
		udelay(2);
		gpio_set_value(GPIO_TO_PIN(1, 25), 0);//at91_set_gpio_value(SCLK, 0);
		udelay(1);
		dat >>= 1;
	}
}

unsigned char ds1302_readbyte(void)
{
	unsigned char i, dat = 0x00;

    gpio_request(GPIO_TO_PIN(1, 26), "rtc_io");
    gpio_direction_input(GPIO_TO_PIN(1, 26));//at91_set_gpio_input(IO,1);

	udelay(2);
	for(i=0; i<8; i++){
		dat >>= 1;
		if(1 == gpio_get_value(GPIO_TO_PIN(1, 26)))//at91_get_gpio_value(IO))
			dat |= 0x80;
		gpio_set_value(GPIO_TO_PIN(1, 25), 1);//at91_set_gpio_value(SCLK, 1);
		udelay(2);
		gpio_set_value(GPIO_TO_PIN(1, 25), 0);//at91_set_gpio_value(SCLK, 0);
		udelay(2);
	}

	return dat;
}

unsigned char ds1302_read(unsigned char cmd)
{
	unsigned char data;

	gpio_set_value(GPIO_TO_PIN(1, 27), 0);//at91_set_gpio_value(RST, 0);
	udelay(1);
	gpio_set_value(GPIO_TO_PIN(1, 25), 0);//at91_set_gpio_value(SCLK, 0);
	udelay(1);
	gpio_set_value(GPIO_TO_PIN(1, 27), 1);//at91_set_gpio_value(RST, 1);
	udelay(1);
	ds1302_writebyte(cmd);
udelay(2);
	data = ds1302_readbyte();
	udelay(1);
	gpio_set_value(GPIO_TO_PIN(1, 25), 1);//at91_set_gpio_value(SCLK, 1);
	udelay(1);
	gpio_set_value(GPIO_TO_PIN(1, 27), 0);//at91_set_gpio_value(RST, 0);

	return data;
}

void ds1302_write(unsigned char cmd, unsigned char data)
{
	gpio_set_value(GPIO_TO_PIN(1, 27), 0);//at91_set_gpio_value(RST, 0);
	udelay(1);
	gpio_set_value(GPIO_TO_PIN(1, 25), 0);//at91_set_gpio_value(SCLK, 0);
	udelay(1);
	gpio_set_value(GPIO_TO_PIN(1, 27), 1);//at91_set_gpio_value(RST, 1);
	udelay(1);

	ds1302_writebyte(cmd);
	ds1302_writebyte(data);
	udelay(1);
	gpio_set_value(GPIO_TO_PIN(1, 25), 1);//at91_set_gpio_value(SCLK, 1);
	udelay(1);
	gpio_set_value(GPIO_TO_PIN(1, 27), 0);//at91_set_gpio_value(RST, 0);
}

static ssize_t rtc_write(struct file *filp, const char __user *buff, size_t count, loff_t *offp)
{
	int ret=0;
	unsigned year, month, day, hour, minute, second;
	unsigned char datetime[20] = {'\0'};

	if(copy_from_user(datetime,buff,count))
		ret = -EFAULT;
	else
		ret = count;

	printk("%s\n", datetime);

	year = ((datetime[2] - 0x30) << 4) | (datetime[3] - 0x30);
	month = ((datetime[4] - 0x30) << 4) | (datetime[5] - 0x30);
	day = ((datetime[6] - 0x30) << 4) | (datetime[7] - 0x30);
	hour = ((datetime[8] - 0x30) << 4) | (datetime[9] - 0x30);
	minute = ((datetime[10] - 0x30) << 4) | (datetime[11] - 0x30);
	second = ((datetime[12] - 0x30) << 4) | (datetime[13] - 0x30);

	printk("%x, %x, %x, %x, %x, %x\n", year, month, day, hour, minute, second);

	ds1302_write(0x8C, year);
	ds1302_write(0x88, month);
	ds1302_write(0x86, day);
	ds1302_write(0x84, hour);
	ds1302_write(0x82, minute);
	ds1302_write(0x80, second);

    return ret;
}

static int rtc_read(struct file *filp,char __user *buff,size_t count,loff_t *offp)
{
	int ret;
	unsigned char year, month, day, hour, minute, second;
	char datetime[20] = {'\0'};
	char temp[5] = {'\0'};

	year = ds1302_read(0x8D);
	month = ds1302_read(0x89);
	day = ds1302_read(0x87);
	hour = ds1302_read(0x85);
	minute = ds1302_read(0x83);
	second = ds1302_read(0x81);

	strcat(datetime, "20");
	printk("year: %x, month: %x, day: %x, hour: %x, minute: %x, second: %x\n", year, month, day, hour, minute, second);

	sprintf(temp, "%d", ((year >> 4) & 0x0f));
	strcat(datetime, temp);
	memset(temp, '\0', 5);

	sprintf(temp, "%d", (year & 0x0f));
	strcat(datetime, temp);
	memset(temp, '\0', 5);

	sprintf(temp, "%d", ((month >> 4) & 0x0f));
	strcat(datetime, temp);
	memset(temp, '\0', 5);

	sprintf(temp, "%d", (month & 0x0f));
	strcat(datetime, temp);
	memset(temp, '\0', 5);

	sprintf(temp, "%d", ((day >> 4) & 0x0f));
	strcat(datetime, temp);
	memset(temp, '\0', 5);

	sprintf(temp, "%d", (day & 0x0f));
	strcat(datetime, temp);
	memset(temp, '\0', 5);

	sprintf(temp, "%d", ((hour >> 4) & 0x07));
	strcat(datetime, temp);
	memset(temp, '\0', 5);

	sprintf(temp, "%d", (hour & 0x0f));
	strcat(datetime, temp);
	memset(temp, '\0', 5);

	sprintf(temp, "%d", ((minute >> 4) & 0x0f));
	strcat(datetime, temp);
	memset(temp, '\0', 5);

	sprintf(temp, "%d", (minute & 0x0f));
	strcat(datetime, temp);
	memset(temp, '\0', 5);

	sprintf(temp, "%d", ((second >> 4) & 0x0f));
	strcat(datetime, temp);
	memset(temp, '\0', 5);

	sprintf(temp, "%d", (second & 0x0f));
	strcat(datetime, temp);
	memset(temp, '\0', 5);


	ret=copy_to_user(buff,datetime,strlen(datetime));

	return ret;
}

static int rtc_open(struct inode *inode, struct file *file)
{
    gpio_request(GPIO_TO_PIN(1, 26), "rtc_io");
    gpio_direction_output(GPIO_TO_PIN(1, 26), 1);//at91_set_GPIO_periph(IO, 1);
	udelay(1);

	gpio_request(GPIO_TO_PIN(1, 27), "rtc_rst");
	gpio_direction_output(GPIO_TO_PIN(1, 27), 0);//at91_set_gpio_output(RST,0);
	udelay(1);
	gpio_request(GPIO_TO_PIN(1, 25), "rtc_sclk");
	gpio_direction_output(GPIO_TO_PIN(1, 25), 0);//at91_set_gpio_output(SCLK,0);
	udelay(1);

	ds1302_write(0x8e, 0x00);	//关闭写保护
	
	return 0;
}

static struct file_operations rtc_fops = {
	.owner  =   THIS_MODULE,
	.open  =   rtc_open,
	.write  =   rtc_write,
	.read = rtc_read
};

static int __init rtc_init(void)
{
	int ret;
	ret=register_chrdev(RTC_MAJOR,DEVICE_NAME,&rtc_fops);
	if(ret<0)
	{
		printk(DEVICE_NAME "can't register major number\n");
		return ret;
	}

	printk(DEVICE_NAME "initialized\n");

	return 0;
}

static void __exit rtc_exit(void)
{
	unregister_chrdev(RTC_MAJOR,DEVICE_NAME);
}

MODULE_LICENSE("GPL");
module_init(rtc_init);
module_exit(rtc_exit);
