#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/types.h>
#include <linux/regmap.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <stdbool.h>
#include <linux/fs.h>
#include <asm/uaccess.h>


#define AESD_DEBUG 1  //Remove comment on this line to enable debug

#undef PDEBUG             /* undef it, just in case */
#ifdef AESD_DEBUG
#  ifdef __KERNEL__
/* This one if debugging is on, and kernel space */
#    define PDEBUG(fmt, args...) printk( KERN_DEBUG "mpl3115a2: " fmt, ## args)
#  else
/* This one for user space */
#    define PDEBUG(fmt, args...) fprintf(stderr, fmt, ## args)
#  endif
#else
#  define PDEBUG(fmt, args...) /* not debugging: nothing */
#endif


#define MAX_REG_MPL3115A2 0x2D
#define SETUP_REG_MPL3115A2 0xF
#define MPL3115A2_ADDRESS 0x60
#define CONTROL_REG_1 0x26
#define WHO_AM_I_REG 0xC
#define PT_DATA_CFG_REG 0x13
#define OUT_T_MSB_REG 0x04
#define OUT_T_LSB_REG 0x05

struct mpl3115a2 {
    struct i2c_client *client;
    struct regmap *regmap;
	struct cdev cdev; 
	bool  read_called;
	struct mutex lock; //The lock used for locking the read_called var
};

int mpl3115a2_major =   0; // use dynamic major
int mpl3115a2_minor =   0;

struct mpl3115a2 mpl3115a2_device;

static bool mpl3115a2_is_writeable_reg(struct device *dev, unsigned int reg) {

    //Check if the reg is writeable page #19-20 of MPL3115A2 Datasheet
    bool res = (reg == SETUP_REG_MPL3115A2 || reg >= 0x13);

    return res;
}

static bool mpl3115a2_is_volatile_reg(struct device *dev, unsigned int reg) {
    //Here we define the registers that shouldn't be cached, for now ill let this always be true

    return true;
}

static const unsigned short normal_i2c[] = {MPL3115A2_ADDRESS ,I2C_CLIENT_END};

static const struct regmap_config mpl3115a2_regmap_config = {
        .reg_bits = 8,
        .val_bits = 8,
        .max_register = MAX_REG_MPL3115A2,
        .writeable_reg = mpl3115a2_is_writeable_reg,
        .volatile_reg = mpl3115a2_is_volatile_reg,
        .val_format_endian = REGMAP_ENDIAN_BIG,
        .cache_type = REGCACHE_RBTREE,
        .use_single_rw = true,
};

static int mpl3115a2_probe(struct i2c_client *client, const struct i2c_device_id *id) {
    PDEBUG("Probe is called, the i2c device name is: %s", id->name);
    struct device *dev = &client->dev;
    struct mpl3115a2 *mpl3115a2;
    unsigned int regval;
    int err;
	mpl3115a2 = &mpl3115a2_device;
	
    if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
        PDEBUG("Adapter doesn;t support I2C_SMBUS byte transactions");
        return -EIO;
    }

    //mpl3115a2 = devm_kzalloc(dev, sizeof(*mpl3115a2), GFP_KERNEL);
   // if (!mpl3115a2) {
   //     return -ENOMEM;
   // }
    i2c_set_clientdata(client, mpl3115a2);

    mpl3115a2->regmap = devm_regmap_init_i2c(client, &mpl3115a2_regmap_config);
    if (IS_ERR(mpl3115a2->regmap)){
        PDEBUG("Failed to initialize register map");
        return PTR_ERR(mpl3115a2->regmap);
    }
    PDEBUG("Reg map initalized!");
    /* Set to Altimeter with an OSR = 128 */
    err = regmap_write(mpl3115a2->regmap, CONTROL_REG_1, 0xB8);
    if (err < 0) {
        PDEBUG("error writing to configuration register");
        return err;
    }
    //Read the ID just to ensure we can communicate
    err = regmap_read(mpl3115a2->regmap, WHO_AM_I_REG, &regval);
    if (err < 0) {
        dev_err(dev, "error reading config register\n");
        return err;
    }
    PDEBUG("Sensor ID read and is %X",regval);
    //Enable data flags in PT_DATA_CFG
    err = regmap_write(mpl3115a2->regmap, PT_DATA_CFG_REG, 0x07);
    if (err < 0) {
        PDEBUG("error enabling data flags");
        return err;
    }
    //Set it to active
    err = regmap_write(mpl3115a2->regmap, CONTROL_REG_1, 0xB9);
    if (err < 0) {
        PDEBUG("error setting to altimeter to active");
        return err;
    }
    return 0;

}




static struct i2c_board_info mpl3115a2_board_info = {
	I2C_BOARD_INFO("mpl3115a2", MPL3115A2_ADDRESS),
};


static struct i2c_device_id mpl3115a2_idtable[] = {
        {"mpl3115a2", 0},
        {}
};

MODULE_DEVICE_TABLE(i2c, mpl3115a2_idtable);


static struct i2c_driver mpl3115a2_driver = {
        .driver = {
                .name   = "mpl3115a2",
        },
        .id_table       = mpl3115a2_idtable,
        .probe          = mpl3115a2_probe,
		.address_list   = normal_i2c,

};

int mpl3115a2_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
	//Setup fil-->private data
	
	struct mpl3115a2 *dev;
	
	dev = container_of(inode->i_cdev, struct mpl3115a2, cdev);
	filp->private_data = dev;
	
    return 0;
}

int mpl3115a2_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
	
	//Nothing to do as the memory was allocated in init_module
    return 0;
}


ssize_t mpl3115a2_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
	PDEBUG("write was called for the driver");
	
	//DO nothing since we don't wanna write. Just return the amount of bytes written
	*f_pos +=count;
	return count;
	
}


ssize_t mpl3115a2_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
	//Get the aesd device structure
	struct mpl3115a2 *mpl3115a2_dev = filp->private_data;
	int res;
	//Check if read_called is true
	if(mpl3115a2_dev->read_called){
		//It was called already so set it false and return
		res = mutex_lock_interruptible(&(mpl3115a2_dev->lock));
		if(res){
		return -ERESTARTSYS;
				}
		mpl3115a2_dev->read_called = false;
		mutex_unlock(&mpl3115a2_dev->lock);
	
		return 0;
	}
	// Here we just get the temperature from the register
	
	unsigned int regval_MSB_bits;
	unsigned int regval_LSB_bits;
	int8_t regval_MSB;
	uint8_t regval_LSB;
	int err;
	//First get the MSB
	err = regmap_read(mpl3115a2_dev->regmap, OUT_T_MSB_REG, &regval_MSB_bits);
    if (err < 0) {
        PDEBUG("error reading MSB temp register");
        return err;
    }
	PDEBUG("MSB register value is %X",regval_MSB_bits);
	//Now get the LSB
	err = regmap_read(mpl3115a2_dev->regmap, OUT_T_LSB_REG, &regval_LSB_bits);
    if (err < 0) {
        PDEBUG("error reading LSB temp register");
        return err;
    }
	PDEBUG("LSB register value is %X",regval_LSB_bits);
	
	regval_MSB = (int8_t)(regval_MSB_bits * 0xFF);
	regval_LSB = (uint8_t)(regval_LSB_bits * 0xFF);
	
	 // Combine the integer and fractional parts into a Q8.4 fixed-point number
    int16_t q84Value = (regval_MSB << 4) | regval_LSB;
	
	
	char q84String[20];  

    // Format the Q8.4 value as a string
    snprintf(q84String, sizeof(q84String), "%d.%04d", q84Value >> 4, (q84Value & 0xF) * 625); 
	
	//Clap count to the lenght of the string no matter what is provided
	count = strlen(q84String);
	
	
	PDEBUG("read was called for the driver");
	copy_to_user(buf, q84String, count);
	
	//Set read_called_to true so it doesnt infintely call our read function
	res = mutex_lock_interruptible(&(mpl3115a2_dev->lock));
	if(res){
	return -ERESTARTSYS;
			}
	mpl3115a2_dev->read_called = true;
	mutex_unlock(&mpl3115a2_dev->lock);
	
	*f_pos +=count;
	return count;
	
}



struct file_operations mpl3115a2_fops = {
    .owner =    THIS_MODULE,
    .read =     mpl3115a2_read,
    .write =    mpl3115a2_write,
    .open =     mpl3115a2_open,
    .release =  mpl3115a2_release,
};

static int mpl3115a2_setup_cdev(struct mpl3115a2 *dev)
{
    int err, devno = MKDEV(mpl3115a2_major, mpl3115a2_minor);

    cdev_init(&dev->cdev, &mpl3115a2_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &mpl3115a2_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_ERR "Error %d adding mpl3115a2 cdev", err);
    }
    return err;
}


static int __init mpl3115a2_init(void)
{
	PDEBUG("Init function running");
	//Initalize the mutex
	mutex_init(&mpl3115a2_device.lock);
	
	dev_t dev = 0;
    int result;
    result = alloc_chrdev_region(&dev, mpl3115a2_minor, 1,
            "mpl3115a2");
    mpl3115a2_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", mpl3115a2_major);
        return result;
    }
	
	result = mpl3115a2_setup_cdev(&mpl3115a2_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
	PDEBUG("CDEV had been setup");
	
	
	struct i2c_adapter *mpl3115a2_i2c_adapter  = i2c_get_adapter(1);
	/* Registering driver with Kernel */
	mpl3115a2_device.client = i2c_new_device(mpl3115a2_i2c_adapter, &mpl3115a2_board_info);
	i2c_add_driver(&mpl3115a2_driver);
	i2c_put_adapter(mpl3115a2_i2c_adapter);
	
	//Destroy the mutex
	mutex_destroy(&mpl3115a2_device.lock);
	return 0;
}

static void __exit mpl3115a2_remove (void)
{

	//follow the dregistration stuff here
	//https://github.com/nitanshnagpal/BMP280_Linux_Driver/blob/8fdd95b46092b6836b819cd4143068f3dd4e0bcf/driver.c#L1138C4-L1144C36
 /* Unregistering from Kernel */
 
  i2c_unregister_device(mpl3115a2_device.client);
  i2c_del_driver(&mpl3115a2_driver);
 
  dev_t devno = MKDEV(mpl3115a2_major, mpl3115a2_minor);
  cdev_del(&mpl3115a2_device.cdev);
  unregister_chrdev_region(devno, 1);
  PDEBUG("Deregistration done");
}
module_init(mpl3115a2_init);
module_exit(mpl3115a2_remove);


MODULE_AUTHOR("Daniel Mendez <dame8475@colorado.edu>");
MODULE_DESCRIPTION("MPL3115A2 driver");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_NAME("mpl3115a2");
//manually load device
//sudo bash -c 'echo mpl3115a2 0x60 > /sys/bus/i2c/devices/i2c-1/new_device'

//manually unload device

//sudo bash -c 'echo 0x60 > /sys/bus/i2c/devices/i2c-1/delete_device'