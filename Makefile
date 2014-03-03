obj-m += kyouko2.o

default:
	$(MAKE) -C /usr/src/linux  M=$(PWD) modules;

clean:
	rm -rf *.ko *.o *.mod.c
