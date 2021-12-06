
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/i2c.h>
#include <linux/fs.h>
#include <linux/of.h>
#include <linux/uaccess.h>
#include <linux/delay.h>

#define HD44780_LINE_1_ADDR			0x0
#define HD44780_LINE_2_ADDR			0x40
#define HD44780_LINE_3_ADDR			0x14
#define HD44780_LINE_4_ADDR			0x54

/* HD44780 commands */
#define HD44780_CMD_CLEAR			0x01
#define HD44780_CMD_RETURN_HOME		0x02
#define HD44780_CMD_ENTRY_MODE		0x04
#define HD44780_CMD_DISPLAY_CTRL	0x08
#define HD44780_CMD_SHIFT			0x10
#define HD44780_CMD_FUNC_SET		0x20
#define HD44780_CMD_CGRAM_ADDR		0x40
#define HD44780_CMD_DDRAM_ADDR		0x80

#define BV(X)						(1 << (X))

/* Commands entry mode */
#define HD44780_ARG_EM_INCREMENT	BV(1)
#define HD44780_ARG_EM_SHIFT		(1)

/* Commands display control */
#define HD44780_ARG_DC_DISPLAY_ON	BV(2)
#define HD44780_ARG_DC_CURSOR_ON	BV(1)
#define HD44780_ARG_DC_CURSOR_BLINK	(1)

/* Commands function set */
#define HD44780_ARG_FS_8_BIT		BV(4)
#define HD44780_ARG_FS_2_LINES		BV(3)
#define HD44780_ARG_FS_FONT_5X10	BV(2)

/* Font format */
#define HD44780_FONT_5X8			(0)
#define HD44780_FONT_5X10			(1)

/* Delays */
#define init_delay()				mdelay(5)
#define short_delay()				udelay(60)
#define long_delay()				mdelay(3)
#define toggle_delay()				udelay(1)

#define LCD_LINES	(4)
#define LCD_FONT	HD44780_FONT_5X8
#define DEV_NAME	"lcd2004"

struct hd44780_dev {
	struct i2c_client * client;
	struct miscdevice lcd2004_miscdevice;
	char name[8]; /* lcd2004XX */
};

static const uint8_t line_addr[] = {HD44780_LINE_1_ADDR,
									HD44780_LINE_2_ADDR,
									HD44780_LINE_3_ADDR,
									HD44780_LINE_4_ADDR};

static int pcf8574_write(struct hd44780_dev * dev, uint8_t data) {
	int ret;

	ret = i2c_smbus_write_byte(dev->client, data);

	return ret;
}

static int hd44780_write_nibble(struct hd44780_dev * dev, uint8_t b, bool rs,
		bool bl) {
	int ret;

	uint8_t data = (((b >> 3) & 1) << 7) |
			  (((b >> 2) & 1) << 6) |
			  (((b >> 1) & 1) << 5) |
			  ((b & 1 ) << 4) |
			  (rs ? 1 : 0) |
			  (bl ? 1 << 3 : 0);

	ret = pcf8574_write(dev, data | 1  << 2);

	if(ret < 0) {
		return ret;
	}

	ret = pcf8574_write(dev, data);

	return ret;
}

static int hd44780_write_byte(struct hd44780_dev * dev, uint8_t b, bool rs,
		bool bl) {
	int ret;

	ret = hd44780_write_nibble(dev, b >> 4, rs, bl);	/* Low nibble */

	if(ret < 0) {
		return ret;
	}

	ret = hd44780_write_nibble(dev, b >> 0, rs, bl);	/* High nibble */

	return ret;
}

static void hd44780_dev_control(struct hd44780_dev * dev, bool on, bool cursor,
		bool cursor_blink) {
	hd44780_write_byte(dev, HD44780_CMD_DISPLAY_CTRL |
			(on ? HD44780_ARG_DC_DISPLAY_ON : 0) |
			(cursor ? HD44780_ARG_DC_CURSOR_ON : 0) |
			(cursor_blink ? HD44780_ARG_DC_CURSOR_BLINK : 0),
			false, true);

		short_delay();
}

static void hd44780_dev_clear(struct hd44780_dev * dev) {
	hd44780_write_byte(dev, HD44780_CMD_CLEAR, false, true);
	long_delay();
}

static void hd44780_dev_gotoxy(struct hd44780_dev * dev,
		uint8_t col, uint8_t row) {
	hd44780_write_byte(dev, HD44780_CMD_DDRAM_ADDR + line_addr[row] + col,
			false, true);
	short_delay();
}

static void hd44780_dev_init(struct hd44780_dev * dev) {
	/* Switch to 4 bit mode */
	uint8_t i = 0;
	for(i; i < 3; i++) {
		hd44780_write_nibble(dev, (HD44780_CMD_FUNC_SET |
				HD44780_ARG_FS_8_BIT) >> 4, false, true);
		init_delay();
	}

	hd44780_write_nibble(dev, HD44780_CMD_FUNC_SET >> 4, false, true);
	short_delay();

    /* Specify the number of display lines and character font */
    hd44780_write_byte(dev, HD44780_CMD_FUNC_SET |
    		(LCD_LINES > 1 ? HD44780_ARG_FS_2_LINES : 0) |
			(LCD_FONT == HD44780_FONT_5X10 ? HD44780_ARG_FS_FONT_5X10 : 0),
			false, true);
    short_delay();

    /* Display off */
    hd44780_dev_control(dev, false, false, false);

    /* Display clear*/
    hd44780_dev_clear(dev);

    /* Entry mode set */
    hd44780_write_byte(dev, HD44780_CMD_ENTRY_MODE | HD44780_ARG_EM_INCREMENT,
    		false, false);
    short_delay();

    /* Display on */
    hd44780_dev_control(dev,true, true, true);
}

static int hd44780_dev_putc(struct hd44780_dev * dev, char c) {
	int ret;

	ret = hd44780_write_byte(dev, c, true, true);
    short_delay();

    return ret;
}

static int hd44780_dev_puts(struct hd44780_dev * dev, const char *s) {
	int ret;

    while(* s) {
    	ret = hd44780_dev_putc(dev, * s);

    	if(ret < 0) {
    		return ret;
    	}

        s++;
    }

    return ret;
}

/* Writing from the terminal command line, \n is added */
static ssize_t lcd2004_write_file(struct file *file, const char __user *userbuf,
                                   size_t count, loff_t *ppos) {
	int ret;
	unsigned long val;
	char buf[16];
	struct hd44780_dev * lcd2004;
	char cmd[3];

	lcd2004 = container_of(file->private_data,
			     struct hd44780_dev,
			     lcd2004_miscdevice);

	dev_info(&lcd2004->client->dev,
		 "lcd2004_write_file entered on %s\n", lcd2004->name);

	dev_info(&lcd2004->client->dev,
		 "we have written %zu characters\n", count);

	if(copy_from_user(buf, userbuf, count)) {
		dev_err(&lcd2004->client->dev, "Bad copied value\n");
		return -EFAULT;
	}

	buf[count-1] = '\0';

	if(strstr(buf, "___")) {
		cmd[0] = buf[2];
		cmd[0] = buf[3];

		ret = kstrtoul(cmd, 0, &val);

		if(ret) {
			return -EINVAL;
		}

		/**/
		if(val == 0) {
			hd44780_dev_clear(lcd2004);
		}
		else if(val > 0 && val <= 4) {
			hd44780_dev_gotoxy(lcd2004, 0, val - 1);
		}
		else {
			ret = hd44780_dev_puts(lcd2004, "Invalid command");
			if (ret < 0)
				dev_err(&lcd2004->client->dev, "the device is not found\n");
		}
	}

	else {
		ret = hd44780_dev_puts(lcd2004, buf);
	}

	if (ret < 0)
		dev_err(&lcd2004->client->dev, "the device is not found\n");

	dev_info(&lcd2004->client->dev,
		 "lcd2004_write_file exited on %s\n", lcd2004->name);

	return count;
}

static const struct file_operations lcd2004_fops = {
	.owner = THIS_MODULE,
	.write = lcd2004_write_file,
};

static int lcd2004_probe(struct i2c_client * client,
		const struct i2c_device_id * id)
{
	static int counter = 0;

	struct hd44780_dev * lcd2004;

	/* Allocate new structure representing device */
	lcd2004 = devm_kzalloc(&client->dev, sizeof(struct hd44780_dev),
			GFP_KERNEL);

	/* Store pointer to the device-structure in bus device context */
	i2c_set_clientdata(client,lcd2004);

	/* Store pointer to I2C device/client */
	lcd2004->client = client;

	/* Initialize the misc device, lcd2004 incremented after each probe call */
	sprintf(lcd2004->name, "%s-%02d", DEV_NAME, counter++);
	dev_info(&client->dev,
		 "lcd2004_probe is entered on %s\n", lcd2004->name);

	lcd2004->lcd2004_miscdevice.name = lcd2004->name;
	lcd2004->lcd2004_miscdevice.minor = MISC_DYNAMIC_MINOR;
	lcd2004->lcd2004_miscdevice.fops = &lcd2004_fops;

	/**/
	hd44780_dev_init(lcd2004);
	hd44780_dev_gotoxy(lcd2004, 0, 0);
	hd44780_dev_puts(lcd2004, "hola");

	/* Register misc device */
	return misc_register(&lcd2004->lcd2004_miscdevice);

	dev_info(&client->dev,
		 "lcd2004_probe is exited on %s\n", lcd2004->name);

	return 0;
}

static int lcd2004_remove(struct i2c_client * client)
{
	struct hd44780_dev * lcd2004;

	/* Get device structure from bus device context */
	lcd2004 = i2c_get_clientdata(client);

	dev_info(&client->dev,
		 "lcd2004_remove is entered on %s\n", lcd2004->name);

	/* Deregister misc device */
	misc_deregister(&lcd2004->lcd2004_miscdevice);

	dev_info(&client->dev,
		 "lcd2004_remove is exited on %s\n", lcd2004->name);

	return 0;
}

static const struct of_device_id lcd2004_dt_ids[] = {
	{ .compatible = "mbarroso,lcd2004",},
	{ }
};
MODULE_DEVICE_TABLE(of, lcd2004_dt_ids);

static const struct i2c_device_id i2c_ids[] = {
	{ .name = "lcd2004", },
	{/* Sentinel */}
};
MODULE_DEVICE_TABLE(i2c, i2c_ids);

static struct i2c_driver lcd2004_driver = {
	.driver = {
		.name = "lcd2004",
		.owner = THIS_MODULE,
		.of_match_table = lcd2004_dt_ids,
	},
	.probe = lcd2004_probe,
	.remove = lcd2004_remove,
	.id_table = i2c_ids,
};

module_i2c_driver(lcd2004_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mauricio Barroso Benavides <mauriciobarroso1990@gmail.com>");
MODULE_DESCRIPTION("This is a driver that controls a 20x4 LCD");
