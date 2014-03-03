obj-m += mymod.o

default:
	$(MAKE) -C /usr/src/linux  M=$(PWD) modules;

clean:
	rm -rf *.ko *.o *.mod.c a.out Module.symvers modules.order\
		.mymod.ko.cmd  .mymod.mod.o.cmd  .mymod.o.cmd .tmp_versions

