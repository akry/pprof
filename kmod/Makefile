obj-m += pprof_mod.o

DIR := ../linux-2.6.39.4
PWD := $(shell pwd)

default:
	make -C $(DIR) M=$(PWD) modules

clean:
	rm -rf *.ko *.o *.order *.symvers *.mod.c
