KDIR = /lib/modules/`uname -r`/build

all: kbuild test


kbuild:
	make -C $(KDIR) M=`pwd`

clean:
	make -C $(KDIR) M=`pwd` clean
	rm test 

test: test.c
	gcc test.c -o test