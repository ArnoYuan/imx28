obj-m	:= button.o
PWD		:= $(shell pwd)
KDIR	:= ../../linux-2.6.35.3-v1.13/linux-2.6.35.3

all:
	$(MAKE) -C $(KDIR) M=$(PWD)

clean:
	rm -rf *.ko *.order *.symvers *.cmd *.o *.mod.c *.tmp_versions