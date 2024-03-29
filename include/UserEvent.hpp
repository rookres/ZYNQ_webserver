#ifndef	USEREVENT_HPP_
#define	USEREVENT_HPP_
#include <iostream>
#include <sys/epoll.h>
#include "heap_timer.hpp"
#include "threadpool_adjust.hpp"
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/uio.h>
// #include "locker.h"     /*第十四章介绍的线程同步机制的包装类*/

#define MAX_BUF_SIZE 512

class UserEvent;//前置声明

using std::cout;
using std::endl;
using readHandle = void(*)(UserEvent *, ITimerContainer<UserEvent> *,threadpool <UserEvent>*);
using writeHandle = void(*)(UserEvent *, ITimerContainer<UserEvent> *);
void readData(UserEvent *Uev, ITimerContainer<UserEvent> *htc,threadpool <UserEvent> *pool);
void writeData(UserEvent *Uev, ITimerContainer<UserEvent> *htc);
// 自定义结构体，用来保存一个连接的相关数据
class UserEvent
{
public:  
    int fd;
    char ip[32];
    uint16_t port;
    epoll_event event; 
    void *timer;
    readHandle read_cb;
    writeHandle write_cb;

 public:
    /* 文件名的最大长度 */
    static const int FILENAME_LEN = 512;
    /* 读缓冲区的大小 */
    static const int READ_BUFFER_SIZE = 2048;
    /* 写缓冲区的大小 */
    static const int WRITE_BUFFER_SIZE = 4096;
    /* HTTP请求方法,此代码只支持GET请求*/
    enum METHOD {GET = 0, POST, HEAD, PUT, DELETE,TRACE, OPTIONS, CONNECT, PATCH};
    /* 解析客户请求时，主状态机所处的状态(看书第八章,也可观看my word文档的有限状态机分析http协议实例) */
    enum CHECK_STATE {CHECK_STATE_REQUESTLINE = 0, CHECK_STATE_HEADER, CHECK_STATE_CONTENT};
    /* 服务器处理HTTP请求的可能结果 */
    enum HTTP_CODE {NO_REQUEST, GET_REQUEST, BAD_REQUEST, NO_RESOURCE, FORBIDDEN_REQUEST, FILE_REQUEST,DIR_REQUEST,CGI_REQUEST,INTERNAL_ERROR, CLOSED_CONNECTION};
    /* 行的读状态 */
    enum LINE_STATUS {LINE_OK = 0, LINE_BAD, LINE_OPEN};
public:
    UserEvent() {}
    ~UserEvent() {}
public:
    /* 初始化新接受的链接 */
    void init(int newcfd, const sockaddr_in &sa,readHandle readData,writeHandle writeData);
    /* 关闭连接 */
    void close_conn(bool real_close = true);
    /* 处理客户请求 */
    void process();
    /* 非阻塞读操作 */
    bool read();
    /* 非阻塞写操作 */
    bool write();
private:
    /* 初始化连接 */
    void init();
    /* 解析HTTP请求 */
    HTTP_CODE process_read();
    /* 填充HTTP应答 */
    bool process_write(HTTP_CODE ret);

    /* 下面这一组函数被process_read调用以分析HTTP请求 */
    HTTP_CODE parse_request_line(char *text);
    HTTP_CODE parse_headers(char *text);
    HTTP_CODE parse_content(char *text);
    HTTP_CODE do_request();
    char *get_line() { return m_read_buf + m_start_line; }
    LINE_STATUS parse_line();

    /* 下面这一组函数被process_write调用以填充HTTP应答 */
    void unmap();
    bool add_response(const char *format, ...);
    bool add_content(const char *content);
    bool add_status_line(int status, const char *title);
    bool add_headers(int content_length,const char *filetype=NULL);
    bool add_content_type(const char *filetype);
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();

public:
    /* 所有socket上的时间都被注册到同一个epoll内核事件表中，所以被epoll文件描述符设置为静态 */
     static int m_epollfd;
     /* 统计用户数量 */
     static int m_user_count;
         /* 获得读缓冲区,作反射服务器用 */
    char* get_m_read_buf(){return m_read_buf;}
        /* 读HTTP连接的socket和对方的socket地址 */
    int m_sockfd;
    sockaddr_in m_address;
private:
    /* 读缓冲区 */
    char m_read_buf[READ_BUFFER_SIZE];
    /* 标识读缓冲区中已经读入的客户数据的最后一个字节的下一个位置 */
    int m_read_idx;
    /* 当前正在分析的字符在缓冲区中的位置 */
    int m_checked_idx;
    /* 当前正在解析的行的起始位置 */
    int m_start_line;
    /* 写缓冲区 */
    char m_write_buf[WRITE_BUFFER_SIZE];
    /* 写缓冲区待发送的字节数 */
    int m_write_idx;
    /* 写缓冲区已经发送的字节数 */
    int bytes_have_send; //by le
    /* 主状态机当前所处的状态 */
    CHECK_STATE m_checked_state;
    /* 请求方法 */
    METHOD m_method;

    /* 客户请求的目标文件的完整路径，其内容等于doc_root + m_url, doc_root是网站根目录 */
    char m_real_file[FILENAME_LEN];
    /* 客户请求的目标文件的文件名 */
    char *m_url;
    /* HTTP协议版本号，我们仅支持HTTP/1.1 */
    char *m_version;
    /* 主机名 */
    char *m_host;
    /* HTTP请求的消息体的长度 */
    int m_content_length;
    /* HTTP请求是否要求保持连接 */
    bool m_linger;
    /* HTTP请求Upgrade-Insecure-Requests */
    /*Upgrade-Insecure-Requests: 1 是HTTP请求头部的一个字段，它的存在是为了提升网页的安全性，
    特别是对于混合内容场景下从HTTP站点迁移到HTTPS站点的过渡期支持。*/
    bool m_update_Insecure;
    char* m_user_agent;
    char* m_accept;
    char* m_referer;
    char* m_accept_encodeing;
    char* m_accept_language;

    /* 客户请求的目标文件被mmap到内存的起始位置 */
    char *m_file_address;
    /* 目标文件的状态。通过它我们可以判断文件是否存在、是否为目录、是否可读，并获取文件大小等信息 */
    struct stat m_file_stat;
     /* 我们将采用writev来执行写操作，所以定义下面两个成员，其中m_iv_count表示被写内存块的数量 */
    struct iovec m_iv[2];
    int m_iv_count;

};


#endif
