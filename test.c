#include <sys/ioctl.h>
#include <linux/net.h>
#include <unistd.h>
#include <fcntl.h>

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "driver.h"

#define LOOPBACK 0x7f000001


int make_socket(uint32_t dst_addr, uint16_t dst_port) {
    int fd = open("/dev/skibidi", O_RDWR);
    if (fd < 0) {
        return fd;
    }

    bool ok = 
        ioctl(fd, IOCTL_SET_DEST_ADDR, dst_addr) == 0 &&
        ioctl(fd, IOCTL_SET_DEST_PORT, dst_port) == 0;
    
    if (!ok) {
        return -1;
    } 

    return fd;
}


int main() {
    int sock1 = make_socket(LOOPBACK, 6600);
    int sock2 = make_socket(LOOPBACK, 4444);

    if (sock1 < 0 || sock2 < 0) {
        perror("Can't initialize sockets");
        return 1;
    }

    char *string = "Hello kernel module";
    char long_buffer[2048] = {};
    memset(long_buffer, 0xb3, sizeof(long_buffer));

    ssize_t w = write(sock1, string, strlen(string));
    if (w >= 0) {
        printf("Wrote %zi bytes to 127.0.0.1:6600, check in tcpdump\n", w);
    } else {
        perror("Can't send to 127.0.0.1:6000");
        return 1;
    }

    w = write(sock2, long_buffer, sizeof(long_buffer));
    if (w >= 0) {
        printf("Wrote %zi bytes to 127.0.0.1:4444, check in tcpdump\n", w);
    } else {
        perror("Can't send to 127.0.0.1:6000");
        return 1;
    }


    close(sock1);
    close(sock2);

    return 0;
}