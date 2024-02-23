#include "init.hpp"

#define PORT 8888
#define MAX_EVENT_NUMBER 1024
#define BUFFER_SIZE 1024
#define TCP_BUFFER_SIZE 1024

const char* IP=NULL;

int main()
{
    printf_DB("hello,world,the program start 2024年2月21日19点40分\n" );

    signal(SIGPIPE,SIG_IGN);
    /*在Linux等类Unix系统中，默认情况下，当一个进程尝试向已经关闭了读端的TCP套接字写数据时，
    内核会向该进程发送一个SIGPIPE信号。如果进程没有捕获并处理这个信号，而是默认行为（终止进程），则服务端进程会被立即结束。*/

	char pwd_path[256]="";
    Change_Dir(pwd_path);//切换需要的工作Resource目录
    printf_DB("pwd_path:%s\n",pwd_path);

    int sfd=tcp4init(PORT,IP,128); //创建TCP套接字并绑定监听
    int udpfd=udp4init(PORT,IP); //创建UDP套接字并绑定
    
    /*使用epoll监听，并上树*/
    int epollfd=epoll_create(1);
    if(epollfd<0) {perr_exit("epollfd");}
    epoll_event events[MAX_EVENT_NUMBER];
    epoll_addfd(epollfd,sfd);
    epoll_addfd(epollfd,udpfd);

    while(1)
    { 
        int epoll_number = epoll_wait(epollfd,events,MAX_EVENT_NUMBER,-1);
        if(epoll_number < 0)
            { perr_exit("epoll failure\n");}
        for(int i=0;i<epoll_number;i++)
        {
            //判断是否是lfd
            if(events[i].data.fd == sfd && events[i].events & EPOLLIN)
            {   
	            int newcfd = Accept(sfd);//接受连接并显示客户端信息
                epoll_addfd(epollfd,newcfd);  //上树监听，并设置newfd非阻塞
            }
            else if(events[i].data.fd == udpfd && events[i].events & EPOLLIN)
            {   /*UDP连接暂时只做回射信息*/
                printf_DB("something connect in UDP \n");
                UDP_Handle(events[i].data.fd);

            }
            else if(events[i].events & EPOLLIN)//TCP连接,即cfd变化
            {
                //int cfd=events[i].data.fd;
                read_client_request(epollfd,&events[i]);
            }
        }
}















    return 0;
}
