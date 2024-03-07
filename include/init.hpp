#ifndef __INIT_H_
#define __INIT_H_
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <assert.h>
#include <string.h>
#include <signal.h>
#include <dirent.h>
#include <ifaddrs.h>
#include "pub.h"
#include "UserEvent.hpp"
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/epoll.h>
// #define USE_DEBUG //在makefile已经指定了USE_DEBUG宏

#ifdef USE_DEBUG
#define printf_DB(fmt, ...) printf(fmt, ##__VA_ARGS__)  // 定义一个宏PRINT，用于打印
#else
#define printf_DB(fmt, ...)  // 如果宏开关未定义，则将PRINT定义为空
#endif

#define UDP_BUFFER_SIZE 1024
/*TCP UDP something related*/
void err_exit(const char *reason,bool ExitFlag=true);

int Accept(int fd,char* ip,uint16_t* port);
int Accept(int fd,sockaddr_in &sa);
/****************************************************************************/
/**
* 获取本地所有的IP地址.
*
* @param	InstancePtr is a pointer to the XGpioPs instance.
* @param	Pin is the pin number to which the Data is to be written.
*		Valid values are 0-117 in Zynq and 0-173 in Zynq Ultrascale+ MP.
* @param	Data is the data to be written to the specified pin (0 or 1).
*
* @return	None.
*
* @note		This function does a masked write to the specified pin of
*		the specified GPIO bank. The previous state of other pins
*		is maintained.
*
*****************************************************************************/
/*----------------------------上面用来写其他函数介绍------------------------------------------------*/

/****************************************************************************/
/**
* 
* @param	void 无.

* @return   None.
*
* @note		获取本地所有的IP地址.
*
*****************************************************************************/
void get_local_ip_addresses();
int Bind(int fd, const struct sockaddr *sa, socklen_t salen);
int Connect(int fd, const struct sockaddr *sa, socklen_t salen);
int Listen(int fd, int backlog);
int Socket(int family, int type, int protocol);
ssize_t Read(int fd, void *ptr, size_t nbytes);
ssize_t Write(int fd, const void *ptr, size_t nbytes);
int Close(int fd);
ssize_t Readn(int fd, void *vptr, size_t n);
ssize_t Writen(int fd, const void *vptr, size_t n);
 ssize_t ReadOneChar(int fd, char *ptr);
ssize_t Readline(int fd, void *vptr, size_t maxlen);
int tcp4init(short port,const char *IP,short backlog,bool reuseAddr=true);
int udp4init(short port,const char *IP);
void udp_write();
void UDP_Handle(int udpfd);

void Change_Dir(char*pwd_path);



/*epoll something related*/
int setnonblocking( int fd );
void epoll_addfd( int &epollfd, int &fd );
void epoll_rmfd(int &epollfd,int fd);



/*Handle Client Something*/
void read_client_request(UserEvent *Uev);
void send_header(int cfd, int code,const char *info,const char *filetype,int length);
void send_file(int epfd,int cfd,const char *path,bool flag=false);
// void Get_Handle(int epfd,UserEvent *Uev) ;
#endif