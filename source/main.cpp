#include "init.hpp"

#define BUFFER_SIZE 1024
#define MAX_EVENT_NUMBER 1024
#define TCP_BUFFER_SIZE 1024


int epollfd;
int pipefd[2];

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("usage: %s ip_address port_number\n", basename(argv[0])); 
        return -1;
    }
    const char *IP = argv[1];
    short PORT = atoi(argv[2]);
    // const char *IP =NULL;
    // short PORT = 8888;

    printf_DB("hello,world,the program start 2024年2月21日19点40分\n" );
    printf_DB("The main pid is %d\n",getpid());
    get_local_ip_addresses();                                           /*获取本地的ens33 ip地址,以便用于浏览器快速访问*/
    printf("pwd_path:%s\n",Change_Dir(getenv("PWD"),"/Resource"));/*获取当前目录的工作路径,并切换需要的工作Resource目录*/
    threadpool <UserEvent> *pool = new threadpool<UserEvent>; /* 创建线程池 */
    /* 设置(注册)一些信号的处理函数 */
    if(add_sig(SIGINT,sig_handler) < 0)
        err_exit("add sig error");
    if(add_sig(SIGTERM,sig_handler) < 0)
        err_exit("add sig error"); 
   /*在Linux等类Unix系统中，默认情况下，当一个进程尝试向已经关闭了读端的TCP套接字写数据时，内核会向该进程发送一个SIGPIPE信号。
    如果进程没有捕获并处理这个信号，而是默认行为（终止进程），则服务端进程会被立即结束,一般是忽略*/
    if(add_sig(SIGPIPE,SIG_IGN) < 0)
        err_exit("add sig error");
    if(add_sig(SIGHUP,sig_handler) < 0)
        err_exit("add sig error");
    if(add_sig(SIGCHLD,sig_handler) < 0)
        err_exit("add sig error");
    if(add_sig(SIGQUIT,sig_handler) < 0)
        err_exit("add sig error");
    
    if(socketpair(AF_UNIX, SOCK_STREAM, 0, pipefd) < 0)
        err_exit("socketpair error");
    
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

    UserEvent::m_epollfd = epollfd;

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
    printf_DB( "------ Create TimerContainer ------\n");
    ITimerContainer<UserEvent> *htc = new HeapTimerContainer<UserEvent>;
    printf_DB(  "------ Create TimerContainer over ------\n");
    epoll_event events[MAX_EVENT_NUMBER];//最大监听数目
    int timeout = 10000;      //如果没有连接，则设置超时值默认为10秒
    char sigbuf[64] = {0};
    bool running = true;

    while(running)
    { 
        // 将定时容器中定时时间最短的时长作为epoll_wait的最大等待时间
        auto min_expire = htc->getMinExpire();
        /*其实下面这个减，并不是获取之前设定的timeout，而是获得当前定时器还剩多长时间，
        并设置到epoll里面，epoll超时未检测到，则必有定时器过期，然后执行tick()函数,也就是说将超时时间作为心动间隔*/
        int epolltimeout = (min_expire == -1) ? timeout : min_expire - getMSec();
        int epoll_number = epoll_wait(epollfd,events,MAX_EVENT_NUMBER,epolltimeout);
        /*其实这个应该不判断，或者判断完不退出，否则无法统一SINGINT或SIGTERM,
        因为程序大部分时间都消耗在epoll_wait上,此函数会返回-1，虽然调用了sig_handler函数写进pipefd里面，但来不及判断pipefd就退出了*/
        if(epoll_number < 0) 
        { err_exit("\nepoll failure",false);}
        else if(epoll_number > 0)
        {
            for(int i=0;i<epoll_number;i++)
            {
                UserEvent *Uev =  (UserEvent *) events[i].data.ptr;
                /*其实这个最好放最前面来判断，如果放在最后或者中间，当cfd关闭时,删除用户Event时，会造成内存泄漏*/
                if(Uev->fd == pipefd[0] && (Uev->event.events & EPOLLIN))
                {
                    pipefd_handle(pipefd,sigbuf,sizeof(sigbuf),running);
                }
                //判断是否sfd,并建立新连接
                else if(Uev->fd == sfd && Uev->event.events & EPOLLIN)
                {   
                    acceptConn(Uev, htc);
                }
                /*UDP连接暂时只做回射信息*/
                else if(Uev->fd == udpfd &&Uev->event.events & EPOLLIN)
                {   
                    printf_DB("something connect in UDP \n");
                    UDP_Handle(Uev->fd);
                }
                /*TCP连接,即cfd可读变化,写和读都是主线程在做，http分析和响应由线程池做*/
                else if(Uev->event.events & EPOLLIN)
                {
                     Uev->read_cb(Uev, htc,pool);
                }
                /*TCP连接,即cfd可写变化*/
                else if(Uev->event.events & EPOLLOUT)
                {
                    Uev->write_cb(Uev, htc);
                }
                /* 如果有异常，直接关闭客户连接 */
                else if(Uev->event.events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
                {  
                        Uev->close_conn();
                        htc->delTimer((Timer<UserEvent> *)Uev->timer);
                        delete Uev;
                }
            }
        }
        else
        {
              htc->tick();
        }
}

    return 0;
}
