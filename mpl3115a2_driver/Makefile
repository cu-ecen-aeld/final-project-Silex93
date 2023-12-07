obj-m := mpl3115a2.o

SRC := $(shell pwd)

all:
     $(MAKE) -C $(KERNEL_SRC) M=$(SRC)

modules_install:
     $(MAKE) -C $(KERNEL_SRC) M=$(SRC) modules_install

clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions