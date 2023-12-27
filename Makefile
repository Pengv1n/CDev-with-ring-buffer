EXTRA_CFLAGS += -DDEBUG
PATH_MODULES := "/lib/modules/$(shell uname -r)/build/"
obj-m += circ_cdev.o

all:
	make -C $(PATH_MODULES) M=${PWD} modules
install:
	make -C $(PATH_MODULES) M=${PWD} modules_install
clean:
	make -C $(PATH_MODULES) M=${PWD} clean
