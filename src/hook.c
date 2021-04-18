#include "include/task.h"

#include <dlfcn.h>
#include <poll.h>
#include <fcntl.h> 
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/socket.h>
#include <stdarg.h>  // 可变函数参数相关
#include <errno.h>
#include <stdio.h>

    struct fd_attributes{
        int domain;                 // AF_LOCAL->域套接字 , AF_INET->IP
        int fd_flag;                 // 记录套接字状态
        int read_timeout;           // 读超时时间;sleep空转超时时间；
        int wirte_timeout;          // 写超时时间
    };

    // 确保以下两项相同；此处应该与tid位图长度一致；
    static const int attributes_length = 125100*8;
    // 记得后面要在主协程结束的时候进行清除
    static struct fd_attributes* fd_2_attributes[125100*8];

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

    typedef unsigned int (*sleep_t)(unsigned int seconds);

    // Since glibc 2.12:
    // https://man7.org/linux/man-pages/man3/usleep.3.html
    #ifdef _XOPEN_SOURCE >= 500 && ! _POSIX_C_SOURCE >= 200809L || /* Glibc since 2.19: */ _DEFAULT_SOURCE || /* Glibc <= 2.19: */ _BSD_SOURCE
    typedef int (*usleep_t)(useconds_t usec);
    #endif 
    // Before glibc 2.12: 不知道怎么用宏查看glibc版本
    //           _BSD_SOURCE || _XOPEN_SOURCE >= 500

    #ifdef _POSIX_C_SOURCE >= 199309L   // https://man7.org/linux/man-pages/man2/nanosleep.2.html
    typedef int (*nanosleep_t)(const struct timespec *req, struct timespec *rem);
    #endif

    // ------------------------------------
    // 对函数进行hook,并存储hook前的函数,因为在封装以后还是要调用
    // 函数dlsym的参数RTLD_NEXT可以在对函数实现所在动态库名称未知的情况下完成对库函数的替代
    // 格式为{Hook_##name##_t}

/*     static read_t Hook_read_t = (read_t)dlsym(RTLD_NEXT, "read");
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
    static recv_t Hook_recv_t = (recv_t)dlsym(RTLD_NEXT, "recv_t"); */

    #define HOOK_SYS_FUNC(name) if( !Hook_##name##_t ) { name##t Hook_##name##_t = (name##_t)dlsym(RTLD_NEXT,#name); }

    static inline struct fd_attributes* create_fd_attributes(int fd){
        if(fd < 0 && fd >= attributes_length) return NULL;
        
        struct fd_attributes* Temp = NULL;

        if(fd_2_attributes[fd] != NULL){
            Temp = fd_2_attributes[fd]; // 本身存在的时候直接返回
        } else {
            Temp = (struct fd_attributes*)malloc(sizeof(struct fd_attributes));
            memset(Temp, 0, sizeof(struct fd_attributes));
            Temp->read_timeout  = 1000; // 设定默认超时时间为1s
            Temp->wirte_timeout = 1000;
            fd_2_attributes[fd] = Temp;
        }

        return Temp;
    }

    int fcntl(int fd, int cmd, ...){
        //printf("进入hook的fcntl\n");
        fcntl_t Hook_fcntl_t = (fcntl_t)dlsym(RTLD_NEXT, "fcntl");

        if(fd < 0){ // 当函数返回非零值的时候都是非正常返回
            return __LINE__;
        }

        va_list arg_list;
        va_start( arg_list,cmd );
        struct fd_attributes* attributes = create_fd_attributes(fd);

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

    int socket(int domain, int type, int protocol){
        socket_t Hook_socket_t = (socket_t)dlsym(RTLD_NEXT, "socket");
        //printf("进入hook的socket\n");
        // TODO：这里需要就协程的运行任务不同而做出不同的修改
        if(__builtin_expect(!current_is_hook(), 1)){        // 可能有些协程不需要hook
            return Hook_socket_t(domain, type, protocol);
        }
        int fd = Hook_socket_t(domain, type, protocol);
        if(fd < 0){
            printf("ERROR: error in create a socket in socket().\n");
            return fd;
        }
        struct fd_attributes* attributes = create_fd_attributes(fd);
        attributes->domain = domain;

        // TODO 套接字状态和地址还未设定

        // F_SETFL会把套接字设置成非阻塞的
        fcntl_t Hook_fcntl_t = (fcntl_t)dlsym(RTLD_NEXT, "fcntl");
        fcntl(fd, F_SETFL, Hook_fcntl_t(fd, F_GETFL, 0));

        return fd;
    }

    unsigned int sleep(unsigned int seconds){
        //printf("进入hook的sleep second : %d, name : %s\n", seconds, current_task->name);
        sleep_t Hook_sleep_t = (sleep_t)dlsym(RTLD_NEXT, "sleep");
        if(!current_is_hook()){
            return Hook_sleep_t(seconds);
        }
        current_task->is_collaborative_schedule = true;     // 标记这个任务虽然时间片没跑完，但是仍然需要被调度
        current_task->sleep_millisecond = seconds * 1000;
        
        while(current_task->is_collaborative_schedule){    // 会在空转一个时间片以后进入信号处理函数，
        }

        // 执行流会在至少seconds秒后回到这里继续执行
        // 因为是用户态的实现，所以return返回值不会出现错误的情况，一定会在seconds秒后返回
        return 0;
    }

    void co_enable_hook_sys(){
        current_task->is_hook = true;
    }