

obj-m := led.o
PWD = $(shell pwd)

KDIR := ../../linux-2.6.35.3-v1.13/linux-2.6.35.3

all:
	$(MAKE) -C $(KDIR) M=$(PWD)

clean:
	rm -rf *.order *.symvers *.tmp_versions *.cmd *.o *.mod.c *.ko
