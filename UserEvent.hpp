#ifndef	__USEREVENT_HPP_
#define	__USEREVENT_HPP_
#include <iostream>
#include <sys/epoll.h>
#include "heap_timer.hpp"

#define MAX_BUF_SIZE 1024

struct UserEvent;//前置声明
using std::cout;
using std::endl;
using readHandle = void(*)(UserEvent *, ITimerContainer<UserEvent> *);
using writeHandle = void(*)(UserEvent *, ITimerContainer<UserEvent> *);
// 自定义结构体，用来保存一个连接的相关数据
struct UserEvent
{
    int fd;
    char ip[32];
    uint16_t port;
    epoll_event event; 
    
    void *timer;

    char buf[MAX_BUF_SIZE];
    int buf_size;
    char method[256]="",content[256]="", protocol[256]="";
    readHandle read_cb;
    writeHandle write_cb;
};
#endif