# How to run

1. Compile with `make all`
2. Add the kernel module with `sudo insmod driver.ko`
3. Run `sudo mknod /dev/skibidi c 300 0`
4. Run `make test` to compile the test program
5. Run `./test` to test the skibidi