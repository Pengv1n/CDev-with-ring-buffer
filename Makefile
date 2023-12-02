PWD := $(shell pwd)
obj-m += circ_cdev.o

all:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules
	gcc user_write.c -o write
	gcc user_read.c -o read
install:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules_install
clean:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) clean
	rm -f read
	rm -f write
