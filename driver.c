#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/blkdev.h>
#include <linux/uaccess.h>
#include <linux/socket.h>
#include <linux/net.h>
#include <linux/nsproxy.h>
#include <uapi/linux/in.h>

#include "driver.h"


MODULE_DESCRIPTION("UDP Module");
MODULE_AUTHOR("Barak");
MODULE_LICENSE("GPL");


#define SRC_ADDR (INADDR_LOOPBACK)
#define SRC_PORT 3300
#define DEFAULT_DST_ADDR SRC_ADDR
#define DEFAULT_DST_PORT 3301


#define DEV_NAME "skibidi"
#define DEV_MAJOR 300

#define MAX_CONNECTIONS 128




typedef struct client_t {
    struct file *file;
    uint32_t address;
    uint16_t port;
} client_t;

static struct dev_t {
    client_t clients[MAX_CONNECTIONS];
    struct socket *socket;
    size_t nclients;
} dev = {
    .nclients = 0,
    .socket = NULL,
    .clients = {}
};


/* 
    Finds the corresponding `client_t` for the given file descriptor
    Returns NULL if not found
*/
static client_t *find_client(struct file *file) {
    for (size_t i = 0; i < dev.nclients; i++) {
        client_t *client = &dev.clients[i];
        if (client->file == file) {
            return client;
        }
    }
    return NULL;
}

/*
    Adds the client to the client list
    Returns:
        0           if no error occured
        -ENOMEM     if the client list is full
*/
static int add_client(struct file *file) {
    if (dev.nclients >= MAX_CONNECTIONS) {
        return -ENOMEM;
    }

    client_t client = {
        .file = file,
        .address = DEFAULT_DST_ADDR,
        .port = DEFAULT_DST_PORT
    };

    dev.clients[dev.nclients] = client;
    dev.nclients += 1;

    return 0;
}

/*
    Removes a client from the client list
*/
static void close_client(client_t *client) {
    dev.nclients -= 1;
    memcpy(client, &dev.clients[dev.nclients], sizeof(client_t));
}


/*
    Initializes the UDP socket
    Returns 0 if no error occured
*/
static int init_socket(struct socket **out) {
    *out = NULL;

    struct socket *socket;
    struct net *net = current->nsproxy->net_ns;

    int err = sock_create_kern(net, AF_INET, SOCK_DGRAM, 0, &socket);

    if (err) {
        return err;
    }

    struct sockaddr_in src = {
        .sin_family = AF_INET,
        .sin_port = htons(SRC_PORT),
        .sin_addr = (struct in_addr) {
            .s_addr = htonl(SRC_ADDR)
        }
    };


    err = kernel_bind(socket, (struct sockaddr*)(&src), sizeof(src));
    if (err) {
        return err;
    }

    *out = socket;
    return 0;
}

/*
    Implements the open function of the UDP device
*/
static int udp_open(struct inode *ino, struct file *file) {

    int err = add_client(file);
    if (err) {
        return err;
    }

    printk(KERN_INFO "Hello client %zu\n", dev.nclients - 1);

    return 0;
}

/*
    Implements the write function of the UDP device
*/
static ssize_t udp_write(struct file *file, const char __user *buffer, size_t buffer_size, loff_t *off) {
    
    client_t *client = find_client(file);
    if (client == NULL) {
        return EBADF;
    }

    struct sockaddr_in dest = {
        .sin_family = AF_INET,
        .sin_port = htons(client->port),
        .sin_addr = (struct in_addr) {
            .s_addr = htonl(client->address)
        }
    };
    struct iov_iter iter;

    int err = import_ubuf(/* source */ true, (void*)buffer, buffer_size, &iter);
    if (err < 0) {
        return err;
    }

    struct msghdr msg = {
        .msg_name = (struct sockaddr*)(&dest),
        .msg_namelen = sizeof(dest),

        .msg_iter = iter,
        .msg_inq = 0,

        .msg_control = NULL,
        .msg_control_is_user = false,
        .msg_controllen = 0,
        .msg_get_inq = false,
        .msg_flags = 0,

        .msg_iocb = NULL,
        .msg_ubuf = NULL,
        .sg_from_iter = NULL,
    };


    // We use `sock_sendmsg` here and not `kernel_sendmsg` here because
    // there is no point to copy to kernel where there is a function that does it 
    // for user space
    return sock_sendmsg(dev.socket, &msg);
}

/*
    Implements the release function of the UDP device
*/
static int udp_release(struct inode *ino, struct file *file) {

    client_t *client = find_client(file);
    if (client == NULL) {
        return -EBADF;
    }    

    printk(KERN_INFO "Bye client %zi\n", client - dev.clients);
    close_client(client);

    return 0;
}

/*
    Implements the unlocked_ioctl function of the UDP device
*/
static long udp_ioctl(struct file * file, unsigned int a, unsigned long b) {

    client_t *client = find_client(file);
    if (client == NULL) {
        return -EBADF;
    }

    switch (a) {
        case IOCTL_SET_DEST_ADDR: {
            client->address = b;
            return 0;
        }
        case IOCTL_SET_DEST_PORT: {
            client->port = b;
            return 0;
        }
        default: {
            return -EINVAL;
        }
    }

    return 0;
}


static struct file_operations ops = {
    .open = udp_open,
    .write = udp_write,
    .release = udp_release,
    .unlocked_ioctl = udp_ioctl,
 };

/*
    Initializies the UDP module
*/
static int udp_init(void)
{
    printk(KERN_INFO "Initializing\n");

    struct socket *socket;
    int err = init_socket(&socket);
    if (err || socket == NULL) {
        printk(KERN_ERR "unable to create udp socket\n");
        return err ? err : EBUSY;
    }

    dev.socket = socket;
    
    err = register_chrdev(DEV_MAJOR, DEV_NAME, &ops);
    if (err < 0) {
        printk(KERN_ERR "unable to register udp char device\n");
        return err;
    }

    return 0;
}

/*
    Destroys the UDP module
*/
static void udp_exit(void)
{
    unregister_chrdev(DEV_MAJOR, DEV_NAME);
    sock_release(dev.socket);
    printk(KERN_INFO "Exiting\n");
}

module_init(udp_init);
module_exit(udp_exit);
