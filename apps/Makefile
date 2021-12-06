all: lcd2004_app

CC = aarch64-none-linux-gnu-gcc 

app: ioctl_test.c
	$(CC)s -o $@ $^s

clean:
	rm lcd2004_app

deploy: lcd2004_app
	sshpass -p "mauricio" scp $^ root@192.168.1.124:/root