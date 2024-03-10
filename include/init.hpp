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
#include <sys/wait.h>
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
#define MAX_USER_CLIENT 100
/*TCP UDP something related*/
void err_exit(const char *reason,bool ExitFlag=true);
void get_local_ip_addresses();
int Bind(int fd, const struct sockaddr *sa, socklen_t salen);
int Accept(int fd,sockaddr_in &sa);
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
void UDP_Handle(int udpfd);

char* Change_Dir(char*pwd_path,const char*resource_path=nullptr);

void show_error(int connfd, const char *info);
void sig_handler(int signum);
int add_sig(int sig, void ( handler ) (int), bool restart = true) ;
/*epoll something related*/
int setnonblocking( int fd );
void addfd(UserEvent *Uev, bool one_shot);
void modfd(int ev,UserEvent *Uev=NULL);
void removefd(int &epollfd, int &fd);
void pipefd_handle(int *pipefd,char*sigbuf,short sigbuf_len,bool &running );
void acceptConn(UserEvent *ev, ITimerContainer<UserEvent> *htc);
void timeout_handle(UserEvent *cli);
/*Handle Client Something*/
void send_header(int cfd, int code,const char *info,const char *filetype,int length);
void send_file(int epfd,int cfd,const char *path,bool flag=false);
#endif