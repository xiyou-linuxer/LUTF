#include "include/task.h"

#include <dlfcn.h>
#include <poll.h>
#include <fcntl.h> 
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstdarg>  // 可变函数参数相关
#include <errno.h>
#include <stdio.h>

    // ------------------------------------ 
    // hook前的函数声明
    // 格式为{name##_t}
    typedef int(*poll_t)(struct pollfd fds[], nfds_t nfds, int timeout);

    typedef ssize_t (*read_t)(int fd, void *buf, size_t nbyte);
    typedef ssize_t (*write_t)(int fd, const void *buf, size_t nbyte);

    typedef int (*socket_t)(int domain, int type, int protocol);
    typedef int (*connect_t)(int fd, const struct sockaddr *address, socklen_t address_len);
    typedef int (*close_t)(int fd);

    typedef int (*fcntl_t)(int fd, int cmd, ...);   // 最后一个参数是可变参

    typedef int (*setenv_t)(const char *name, const char *value, int overwrite);
    typedef int (*unsetenv_t)(const char *name);
    typedef char *(*getenv_t)(const char *name);

    // sendto, recvfrom与UDP相关
    typedef ssize_t (*sendto_t)(int fd, const void *message, size_t length,
                                int flags, const struct sockaddr *dest_addr,
                                socklen_t dest_len);

    typedef ssize_t (*recvfrom_t)(int fd, void *buffer, size_t length,
                                  int flags, struct sockaddr *address,
                                  socklen_t *address_len);

    typedef ssize_t (*send_t)(int fd, const void *buffer, size_t length, int flags);
    typedef ssize_t (*recv_t)(int fd, void *buffer, size_t length, int flags);

    // ------------------------------------
    // 对函数进行hook,并存储hook前的函数,因为在封装以后还是要调用
    // 函数dlsym的参数RTLD_NEXT可以在对函数实现所在动态库名称未知的情况下完成对库函数的替代
    // 格式为{Hook_##name##_t}

    static read_t Hook_read_t = (read_t)dlsym(RTLD_NEXT, "read");
    static write_t Hook_write_t = (write_t)dlsym(RTLD_NEXT, "write");
    static poll_t Hook_poll_t = (poll_t)dlsym(RTLD_NEXT, "poll");

    static socket_t Hook_socket_t = (socket_t)dlsym(RTLD_NEXT, "socket");
    static connect_t Hook_connect_t = (connect_t)dlsym(RTLD_NEXT, "connect");
    static close_t Hook_close_t = (close_t)dlsym(RTLD_NEXT, "close");

    static fcntl_t Hook_fcntl_t = (fcntl_t)dlsym(RTLD_NEXT, "fcntl");

    static setenv_t Hook_setenv_t = (setenv_t)dlsym(RTLD_NEXT, "setenv");
    static unsetenv_t Hook_unsetenv_t = (unsetenv_t)dlsym(RTLD_NEXT, "unsetenv");
    static getenv_t Hook_getenv_t = (getenv_t)dlsym(RTLD_NEXT, "getenv");

    static sendto_t Hook_sendto_t = (sendto_t)dlsym(RTLD_NEXT, "sendto");
    static recvfrom_t Hook_recvfrom_t = (recvfrom_t)dlsym(RTLD_NEXT, "recvfrom");
    static send_t Hook_send_t = (send_t)dlsym(RTLD_NEXT, "send_t");
    static recv_t Hook_recv_t = (recv_t)dlsym(RTLD_NEXT, "recv_t");

    #define HOOK_SYS_FUNC(name) if( !Hook_##name##_t ) { Hook_##name##_t = (name##_t)dlsym(RTLD_NEXT,#name); }

    struct fd_attributes{
        struct sockaddr_in addr;    // 套接字目标地址
        int domain;                 // AF_LOCAL->域套接字 , AF_INET->IP
        int fd_flag;                 // 记录套接字状态
        int read_timeout;           // 读超时时间
        int wirte_timeout;          // 写超时时间
    };

    static const int attributes_length = 8196;
    // 记得后面要在主协程结束的时候进行清除
    static fd_attributes* fd_2_attributes[attributes_length];

    static inline fd_attributes* create_fd_attributes(int fd){
        if(fd < 0 && fd >= attributes_length) return nullptr;
        
        fd_attributes* Temp = nullptr;

        if(fd_2_attributes[fd] != nullptr){
            Temp = fd_2_attributes[fd];
        } else {
            Temp = (fd_attributes*)malloc(sizeof(fd_attributes));
            memset(Temp, 0, sizeof(fd_attributes));
            Temp->read_timeout  = 1000; // 设定默认超时时间为1s
            Temp->wirte_timeout = 1000;
            fd_2_attributes[fd] = Temp;
        }

        return Temp;
    }

    int socket(int domain, int type, int protocol){
        HOOK_SYS_FUNC(socket);
        printf("进入hook的socket\n");
        // TODO：这里需要就协程的运行任务不同而做出不同的修改
        if(__builtin_expect(current_is_hook(), 1)){        // 可能有些协程不需要hook
            return Hook_socket_t(domain, type, protocol);
        }
        int fd = Hook_socket_t(domain, type, protocol);
        if(fd < 0){
            printf("ERROR: error in create a socket in socket().\n");
            return fd;
        }
        fd_attributes* attributes = create_fd_attributes(fd);
        attributes->domain = domain;

        // TODO 套接字状态和地址还未设定

        // F_SETFL会把套接字设置成非阻塞的
        fcntl(fd, F_SETFL, Hook_fcntl_t(fd, F_GETFL, 0));

        return fd;
    }

    int fcntl(int fd, int cmd, ...){
        printf("进入hook的fcntl\n");
        HOOK_SYS_FUNC(fcntl);

        if(fd < 0){ // 当函数返回非零值的时候都是非正常返回
            return __LINE__;
        }

        va_list arg_list;
        va_start( arg_list,cmd );
        fd_attributes* attributes = create_fd_attributes(fd);

        int ret = 0;
        // https://man7.org/linux/man-pages/man2/fcntl.2.html
        // https://blog.csdn.net/bical/article/details/3014731
        switch (cmd){
            case F_GETFD:   // 忽略第三个参数
            {
                ret = Hook_fcntl_t(fd, cmd);
                // 用户创建的套接字如果不包含O_NONBLOCK的话,返回值也不包含
                if(attributes && !(attributes->fd_flag & O_NONBLOCK)){
                    ret = ret & (~O_NONBLOCK);
                }
                break;
            }
            case F_SETFD:
            {
                int param = va_arg(arg_list,int);
                Hook_fcntl_t(fd, cmd, param);
                break;
            }
            case F_GETFL:
            {
                ret = Hook_fcntl_t(fd, cmd);
                break;
            }
            case F_SETFL:   // 忽略第三个参数
            {   // 只允许O_APPEND、O_NONBLOCK和O_ASYNC位的改变
                int param = va_arg(arg_list, int);
                int flag = param;
                if(current_is_hook() && attributes){
                    flag |= O_NONBLOCK;
                }
                ret = Hook_fcntl_t(fd, cmd, flag);  // 一定是非阻塞的
                // fcntl正常返回0
                if(!ret && attributes){
                    attributes->fd_flag = param;     // attributes中存储原来的信息
                }
                break;
            }
            case F_GETLK:
            {
                struct flock *param = va_arg(arg_list,struct flock *);
                ret = Hook_fcntl_t(fd, cmd, param);
                break;
            }
            case F_SETLK:
            {
                struct flock *param = va_arg(arg_list,struct flock *);
                ret = Hook_fcntl_t(fd, cmd, param);
                break;
            }
            case F_SETLKW:
            {
                struct flock *param = va_arg(arg_list,struct flock *);
                ret = Hook_fcntl_t(fd, cmd, param);
                break;
            }
            case F_DUPFD: // 查找大于或等于参数arg的最小且仍未使用的文件描述词
            {
                int param = va_arg(arg_list,int); // 指向arg_list当前元素的下一个
                ret = Hook_fcntl_t(fd , cmd, param);
                break;
            }
        }
        va_end( arg_list );
        return ret;
    }

    void co_enable_hook_sys(){
        printf("nihao\n");
        current_task->is_hook = true;
    }