PWD := $(shell pwd)
obj-m += circ_cdev.o

all:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules
	cp write.sh /tmp
install:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules_install
clean:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) clean
	rm /tmp/write.sh
