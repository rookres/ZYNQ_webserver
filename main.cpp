#include "init.hpp"
#include "heap_timer.hpp"


#define PORT 8888
#define BUFFER_SIZE 1024
#define MAX_EVENT_NUMBER 1024
#define TCP_BUFFER_SIZE 1024
#define MAX_BUF_SIZE 1024

const char* IP=NULL;
int epollfd;
int pipefd[2];

struct UserEvent;//前置声明
using std::cout;
using std::endl;
using readHandle = void(*)(UserEvent *, ITimerContainer<UserEvent> *);
using writeHandle = void(*)(UserEvent *, ITimerContainer<UserEvent> *);
// 自定义结构体，用来保存一个连接的相关数据
struct UserEvent
{
    int fd;
    char ip[64];
    uint16_t port;
    epoll_event event; 

    void *timer;

    char buf[MAX_BUF_SIZE];
    int buf_size;

    readHandle read_cb;
    writeHandle write_cb;
};

void sig_handler(int signum)
{
    char sig = (char) signum;
    write(pipefd[1], &sig, 1);
}

int add_sig(int signum)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sig_handler;
    sa.sa_flags |= SA_RESTART;     /*为了确保当信号发生时，被信号打断的系统调用能够从被打断的地方重新开始，而不是直接返回错误并终止系统调用的执行*/
    sigfillset(&sa.sa_mask);

    return sigaction(signum, &sa, nullptr);

}

// 超时处理的回调函数
void timeout_handle(UserEvent *cli)
{
    if(cli == nullptr)
    { return ;}
    cout << "Connection time out " << " ip:[" << cli->ip << ":" << cli->port << "]" << endl;
    char reply[]="Sorry,Your Connection TimeOut,You Will Be Kick Out,!_!\nSee Ya Soon!!!!\n";
    Write(cli->fd,reply,sizeof(reply));
    epoll_ctl(epollfd, EPOLL_CTL_DEL, cli->fd, NULL);
    close(cli->fd);
    delete cli;
}

void readData(UserEvent *ev, ITimerContainer<UserEvent> *htc)
{
    struct epoll_event evnoptr;
    evnoptr.data.fd=ev->fd;
    read_client_request(epollfd,&evnoptr);
    // ev->buf_size = read(ev->fd, ev->buf, MAX_BUF_SIZE - 1);
    // if(ev->buf_size == 0)
    // {
    //     close(ev->fd);
    //     htc->delTimer((Timer<UserEvent> *)ev->timer);
    //     epoll_ctl(epollfd, EPOLL_CTL_DEL, ev->fd, &ev->event);
    //     cout << "Remote Connection has been closed, fd:" << ev->fd << " ip:[" << ev->ip << ":" << ev->port << "]" << endl;
    //     delete ev;
        
    //     return;
    // }

    // ev->event.events = EPOLLOUT;
    // epoll_ctl(epollfd, EPOLL_CTL_MOD, ev->fd, &ev->event);
    cout << "重新设置定时器"<< endl;
    // 重新设置定时器
    htc->resetTimer((Timer<UserEvent> *)ev->timer, 15000);
}

void writeData(UserEvent *ev, ITimerContainer<UserEvent> *htc)
{
    write(ev->fd, ev->buf, ev->buf_size);
    //cout << "write,:fd:" << ev->fd << " ip:[" << ev->ip << ":" << ev->port << "]" <<"content:"<<ev->buf<< endl;
    
    ev->event.events = EPOLLIN;
    epoll_ctl(epollfd, EPOLL_CTL_MOD, ev->fd, &ev->event);
    cout << "重新设置定时器"<< endl;
    // 重新设置定时器
    htc->resetTimer((Timer<UserEvent> *)ev->timer, 15000);
}



// 接收连接回调函数
void acceptConn(UserEvent *ev, ITimerContainer<UserEvent> *htc)
{
    UserEvent *cli = new UserEvent;
    struct sockaddr_in fdinfo;
    int newcfd = Accept(ev->fd,cli->ip,&cli->port);//接受连接并获取客户端ip和端口信息
    setnonblocking(newcfd);
    cli->fd = newcfd;
    cli->read_cb = readData;
    cli->write_cb = writeData;

    auto timer = htc->addTimer(15000);      //设置客户端超时值15秒
    timer->setUserData(cli);
    timer->setCallBack(timeout_handle);
    cli->timer = (void *)timer;

    cli->event.events = EPOLLIN;
    cli->event.data.ptr = (void *) cli;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, newcfd, &cli->event);

    cout << "New Connection, ip:[" << cli->ip << ":" << cli->port << "]" << endl;
    // printf_DB("new client ip=%s port=%u\n",cli->ip,cli->port);
}


int main()
{
    printf_DB("hello,world,the program start 2024年2月21日19点40分\n" );

    signal(SIGPIPE,SIG_IGN); /*在Linux等类Unix系统中，默认情况下，当一个进程尝试向已经关闭了读端的TCP套接字写数据时，
    内核会向该进程发送一个SIGPIPE信号。如果进程没有捕获并处理这个信号，而是默认行为（终止进程），则服务端进程会被立即结束。*/

	char pwd_path[256]="";
    Change_Dir(pwd_path);//切换需要的工作Resource目录
    printf_DB("pwd_path:%s\n",pwd_path);

    // 信号处理
    int ret = add_sig(SIGINT);
    if(ret < 0)
    {
        err_exit("add sig error");
    }
    ret = socketpair(AF_UNIX, SOCK_STREAM, 0, pipefd);
    if(ret < 0)
    {
        err_exit("socketpair error");
    }


    int sfd=tcp4init(PORT,IP,128); //创建TCP套接字并绑定监听
    int udpfd=udp4init(PORT,IP); //创建UDP套接字并绑定

    UserEvent TCPServer,UDPServer;
    UserEvent SigEvent;

    TCPServer.fd=sfd;
    UDPServer.fd=udpfd;
    SigEvent.fd = pipefd[0];
    /*使用epoll监听，并上树*/
    epollfd=epoll_create(1);
    if(epollfd<0) {err_exit("epollfd");}

    /*赋值初始化*/
    TCPServer.event.events = EPOLLIN;
    TCPServer.event.data.ptr = (void *)&TCPServer;

    UDPServer.event.events = EPOLLIN;
    UDPServer.event.data.ptr = (void *)&UDPServer;

    SigEvent.event.events = EPOLLIN;
    SigEvent.event.data.ptr = (void *) &SigEvent;;


    //上树监听
    epoll_ctl(epollfd, EPOLL_CTL_ADD, sfd, &TCPServer.event);
    epoll_ctl(epollfd, EPOLL_CTL_ADD, udpfd, &UDPServer.event);
    epoll_ctl(epollfd, EPOLL_CTL_ADD, pipefd[0], &SigEvent.event);
    cout << "------ Create TimerContainer ------" << endl;

    ITimerContainer<UserEvent> *htc = new HeapTimerContainer<UserEvent>;

    cout << "------ Create TimerContainer over ------" << endl;
    epoll_event events[MAX_EVENT_NUMBER];//最大监听数目
    int timeout = 10000;      //如果没有连接，则设置超时值默认为10秒
    char sigbuf[1024] = {0};
    bool running = true;
    // epoll_addfd(epollfd,sfd);
    // epoll_addfd(epollfd,udpfd);

    while(running)
    { 
        // 将定时容器中定时时间最短的时长作为epoll_wait的最大等待时间
        auto min_expire = htc->getMinExpire();
        /*其实下面这个减，并不是获取之前设定的timeout，而是获得当前定时器还剩多长时间，
        并设置到epoll里面，epoll超时未检测到，则必有定时器过期，然后执行tick()函数。也就是说将超时时间作为心动间隔*/
        timeout = (min_expire == -1) ? timeout : min_expire - getMSec();

        int epoll_number = epoll_wait(epollfd,events,MAX_EVENT_NUMBER,timeout);
        if(epoll_number < 0)
            { err_exit("epoll failure\n");}
        else if(epoll_number>0)
        {
            for(int i=0;i<epoll_number;i++)
            {
                UserEvent *Uev =  (UserEvent *) events[i].data.ptr;
                if(Uev->fd == pipefd[0] && Uev->event.events & EPOLLIN)/*其实这个最好放最前面来判断，如果放在最后或者中间，
                                                                        当cfd关闭时,删除用户Event时，会造成内存泄漏*/
                {
                    memset(sigbuf,'\0',sizeof(sigbuf));
                    int n = read(pipefd[0], sigbuf, sizeof(sigbuf));
                    if(n < 0)
                    {
                        cout << "deal read signal error:" << strerror(errno) << endl;
                        continue; 
                    }
                    else if(n > 0)
                    {
                        for(int i = 0; i < n; i++)
                        {
                            switch (sigbuf[i])
                            {
                                case SIGINT:
                                {                            
                                    running = false;
                                    printf_DB("\nRecive sigint signal,the server STOP!!!\n");
                                    break;
                                }
                            }
                        }
                    }
                }
                //判断是否是sfd
                else if(Uev->fd == sfd && Uev->event.events & EPOLLIN)
                {   
                    acceptConn(Uev, htc);
                }
                else if(events[i].data.fd == udpfd && events[i].events & EPOLLIN)
                {   /*UDP连接暂时只做回射信息*/
                    printf_DB("something connect in UDP \n");
                    UDP_Handle(events[i].data.fd);

                }
                else if(events[i].events & EPOLLIN)//TCP连接,即cfd变化
                {
                    // int cfd=events[i].data.fd;
                        Uev->read_cb(Uev, htc);
                        //Uev->write_cb(Uev, htc);
                    // read_client_request(epollfd,&events[i]);
                    // Uev->read_cb(epollfd,Uev->event);
                }
                // else if(Uev->fd == pipefd[0] && Uev->event.events & EPOLLIN)/*其实这个最好放最前面，如果放在最后或者中间，当cfd关闭时,删除用户Event时，会造成内存泄漏*/
                // {
                //     memset(sigbuf,'\0',sizeof(sigbuf));
                //     int n = read(pipefd[0], sigbuf, sizeof(sigbuf));
                //     if(n < 0)
                //     {
                //         cout << "deal read signal error:" << strerror(errno) << endl;
                //         continue; 
                //     }
                //     else if(n > 0)
                //     {
                //         for(int i = 0; i < n; i++)
                //         {
                //             switch (sigbuf[i])
                //             {
                //             case SIGINT:
                //                 running = false;
                //                 break;
                //             }
                //         }
                //     }
                // }
            }
        }
        else
        {
              htc->tick();
        }
}















    return 0;
}
