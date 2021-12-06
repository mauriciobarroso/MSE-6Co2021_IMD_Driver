obj-m := lcd2004.o

KERNEL_DIR ?= /home/mauricio/buildroot/output/build/linux-custom

all:
	make -C $(KERNEL_DIR) ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- M=$(PWD) modules

clean:
	make -C $(KERNEL_DIR) ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- M=$(PWD) clean

deploy:
	sshpass -p "mauricio" scp *.ko root@192.168.1.124:/root