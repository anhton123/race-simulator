obj-m += my_kernel.o 

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
	gcc my_user.c -o my_user

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm my_user