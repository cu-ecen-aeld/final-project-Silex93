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
#include <stdbool.h>
#include "mpl3115a2_driver.h"

#define MAX_REG_MPL3115A2 0x2D
#define SETUP_REG_MPL3115A2 0xF
#define MPL3115A2_ADDRESS 0x60


struct mpl3115a2 {
    struct i2c_client *client;
    struct regmap *regmap;
};

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
	
	
    if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
        PDEBUG("Adapter doesn;t support I2C_SMBUS byte transactions");
        return -EIO;
    }

    mpl3115a2 = devm_kzalloc(dev, sizeof(*mpl3115a2), GFP_KERNEL);
    if (!mpl3115a2) {
        return -ENOMEM;
    }
    i2c_set_clientdata(client, mpl3115a2);

    mpl3115a2->regmap = devm_regmap_init_i2c(client, &mpl3115a2_regmap_config);
    if (IS_ERR(mpl3115a2->regmap)){
        PDEBUG("Failed to initialize register map");
        return PTR_ERR(mpl3115a2->regmap);
    }
    PDEBUG("Reg map initalized!");
    /* Set to Altimeter with an OSR = 128 */
    err = regmap_write(mpl3115a2->regmap, 0x26, 0xB8);
    if (err < 0) {
        PDEBUG("error writing to configuration register");
        return err;
    }
    //Read the ID just to ensure we can communicate
    err = regmap_read(mpl3115a2->regmap, 0xC, &regval);
    if (err < 0) {
        dev_err(dev, "error reading config register\n");
        return err;
    }
    PDEBUG("Sensor ID read and is %X",regval);
    //Enable data flags in PT_DATA_CFG
    err = regmap_write(mpl3115a2->regmap, 0x13, 0x07);
    if (err < 0) {
        PDEBUG("error enabling data flags");
        return err;
    }
    //Set it to active
    err = regmap_write(mpl3115a2->regmap, 0x26, 0xB9);
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

static int __init mpl3115a2_init(void)
{
	PDEBUG("Init function running");
	struct i2c_adapter *mpl3115a2_i2c_adapter  = i2c_get_adapter(1);
	/* Registering driver with Kernel */
	i2c_new_device(mpl3115a2_i2c_adapter, &mpl3115a2_board_info);
	
	i2c_add_driver(&mpl3115a2_driver);
	i2c_put_adapter(mpl3115a2_i2c_adapter);
 return 0;
}

static void __exit mpl3115a2_remove (void)
{

	//follow the dregistration stuff here
	//https://github.com/nitanshnagpal/BMP280_Linux_Driver/blob/8fdd95b46092b6836b819cd4143068f3dd4e0bcf/driver.c#L1138C4-L1144C36
 /* Unregistering from Kernel */

}
module_init(mpl3115a2_init);
module_exit(mpl3115a2_remove);


MODULE_AUTHOR("Daniel Mendez <dame8475@colorado.edu>");
MODULE_DESCRIPTION("MPL3115A2 driver");
MODULE_LICENSE("Dual BSD/GPL");

//manually load device
//sudo bash -c 'echo mpl3115a2 0x60 > /sys/bus/i2c/devices/i2c-1/new_device'

//manually unload device

//sudo bash -c 'echo 0x60 > /sys/bus/i2c/devices/i2c-1/delete_device'