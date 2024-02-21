obj-m += framework-hwmon.o

all:
	make -C /lib/modules/`uname -r`/build M=$(PWD) modules