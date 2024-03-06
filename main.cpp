#include "init.hpp"
#include "threadpool_adjust.hpp"
#define PORT 8888
#define BUFFER_SIZE 1024
#define MAX_EVENT_NUMBER 1024
#define TCP_BUFFER_SIZE 1024
#define MAX_BUF_SIZE 1024
#define MAX_USER_CLIENT 100

const char* IP=NULL;
int epollfd;
int pipefd[2];
// threadpool <UserEvent> *pool=NULL;
extern void addfd(UserEvent *Uev, bool one_shot);

    /* 创建线程池 */
threadpool <UserEvent> *pool = new threadpool<UserEvent>;//放在这不会产生段错误,放在main函数里面就会产生，暂时不知道为什么。2024年3月6日11点08分
    // if(!pool){err_exit("new");}

void sig_handler(int signum)
{
    /* 保留原来的 errno，在函数最后恢复，以保证函数的可重入性 */
    int save_errno = errno;
    char sig = (char) signum;
    Write(pipefd[1], &sig, 1); /* 将信号值写入管道，以通知主循环 */
    errno = save_errno;

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

void show_error(int connfd, const char *info)
{
    printf("%s", info);
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

// 超时处理的回调函数
void timeout_handle(UserEvent *cli)
{
    if(cli == nullptr)
    { return ;}

    // send_header(cli->fd, 200,"OK",get_mime_type("*.html"),0);
    send_file(cli->m_epollfd,cli->fd,"timeout.html",0);

    // cout << "Connection time out " << " ip:[" << cli->ip << ":" << cli->port << "]" << endl;
    // char reply[]="Sorry,Your Connection TimeOut,You Will Be Kick Out,!_!\nSee Ya Soon!!!!\n";
    // Write(cli->fd,reply,sizeof(reply));
    epoll_ctl(epollfd, EPOLL_CTL_DEL, cli->fd, NULL);
    close(cli->fd);
    UserEvent::m_user_count--;
    delete cli;
}

void readData(UserEvent *Uev, ITimerContainer<UserEvent> *htc)
{
    // char peekbuf[1]={0};
    // short pbf_size=recv(Uev->fd, peekbuf, sizeof(peekbuf),MSG_PEEK);
    // if(pbf_size == 0)
    // {
    //     close(Uev->fd);
    //     htc->delTimer((Timer<UserEvent> *)Uev->timer);
    //     epoll_ctl(epollfd, EPOLL_CTL_DEL, Uev->fd, &Uev->event);
    //     cout << "Remote Connection has been closed, fd:" << Uev->fd << " ip:[" << Uev->ip << ":" << Uev->port << "]" << endl;
    //     delete Uev;
    //     return;
    // }
    // else if(pbf_size<0) /*其实对这个报错不知道到底如何处理才好,看下那个epolloneshot,epoll LT,ET模式*/
    //                     /*EAGAIN 或 EWOULDBLOCK：在非阻塞模式下，如果没有数据可读且套接字没有被设置为等待数据就绪，
    //                     则返回-1并设置errno为EAGAIN或EWOULDBLOCK（对于非阻塞socket而言）。*/
    // {
    //     close(Uev->fd);
    //     htc->delTimer((Timer<UserEvent> *)Uev->timer);
    //     epoll_ctl(epollfd, EPOLL_CTL_DEL, Uev->fd, &Uev->event);
    //     cout << "readData err" << endl;
    //     delete Uev;
    //     return;
    // }
    // else
    // {
    //    read_client_request(epollfd,Uev);
    //     Uev->event.events = EPOLLOUT;
    //     epoll_ctl(epollfd, EPOLL_CTL_MOD, Uev->fd, &Uev->event);
    // }
    if(Uev->read())
    {
         pool->append(Uev);         
    }
    else
    {
        close(Uev->fd);
        htc->delTimer((Timer<UserEvent> *)Uev->timer);

        Uev->m_user_count--;
        delete Uev;
    }

}

void writeData(UserEvent *Uev, ITimerContainer<UserEvent> *htc)
{
    // //判断是否为get请求  get   GET
	// if(strcasecmp(Uev->method,"get") == 0)
	//  {
    //     cout << "strcasecmp"<< endl;
	// 	Get_Handle(epollfd,Uev);
	//  }
    // else
    // {
    //     char buf[512]={'\0'};
    //     sprintf(buf,"No Get Request,Only Reply Same Message\nThe Server Reply:%s",Uev->method);
    //     Write(Uev->fd, buf, sizeof(buf));
    // }


        /* 根据写的结果，决定是否关闭连接 */
    if (!Uev->write())
    {
        cout << "\nUev->write Fasle\n"<< endl;
        Uev->close_conn();            //当关闭连接时,应该删除定时器,与时间关联起来
        htc->delTimer((Timer<UserEvent> *)Uev->timer);
        delete Uev;
    }
    // send_file(Uev->m_epollfd,Uev->fd,Uev->real_file,0);
    // Uev->event.events = EPOLLIN;
    // epoll_ctl(epollfd, EPOLL_CTL_MOD, Uev->fd, &Uev->event);
    else
    {    cout << "resetTimer\n"<< endl;
    // // 重新设置定时器
        htc->resetTimer((Timer<UserEvent> *)Uev->timer, 15000);
    }
}



// 接收连接回调函数
void acceptConn(UserEvent *ev, ITimerContainer<UserEvent> *htc)
{
    struct sockaddr_in sa;
    int newcfd = Accept(ev->fd,sa);//接受连接并获取客户端ip和端口信息
    if (UserEvent::m_user_count >= MAX_USER_CLIENT)
    {
        show_error(newcfd, "Internal server busy,m_user_count >= MAX_USER_CLIENT\n");
        return;
    }
    UserEvent *cli = new UserEvent;
    // setnonblocking(newcfd);          //设置非阻塞
    cli->init(newcfd,sa,readData,writeData);

    auto timer = htc->addTimer(300000);      //设置客户端超时值300秒
    timer->setUserData(cli);
    timer->setCallBack(timeout_handle);
    cli->timer = (void *)timer;

    addfd(cli, true);
    // cli->event.events = EPOLLIN;
    // cli->event.data.ptr = (void *) cli;
    // epoll_ctl(epollfd, EPOLL_CTL_ADD, newcfd, &cli->event);

    cout << "New Connection, ip:[" << cli->ip << ":" << cli->port << "]" << endl;
    // printf_DB("new client ip=%s port=%u\n",cli->ip,cli->port);

}


int main()
{
    printf_DB("hello,world,the program start 2024年2月21日19点40分\n" );
    printf_DB("The main pid is %d\n",getpid());
    // signal(SIGPIPE,SIG_IGN); /*在Linux等类Unix系统中，默认情况下，当一个进程尝试向已经关闭了读端的TCP套接字写数据时，
    // 内核会向该进程发送一个SIGPIPE信号。如果进程没有捕获并处理这个信号，而是默认行为（终止进程），则服务端进程会被立即结束。*/
    get_local_ip_addresses();
	char pwd_path[256]="";
    Change_Dir(pwd_path);//切换需要的工作Resource目录
    printf_DB("pwd_path:%s\n",pwd_path);

    //     /* 创建线程池 */
    // threadpool <UserEvent> *pool = new threadpool<UserEvent>;
    /* 设置(注册)一些信号的处理函数 */
    // if(add_sig(SIGINT) < 0)
    //     err_exit("add sig error");
    if(add_sig(SIGTERM) < 0)
        err_exit("add sig error"); 
    if(add_sig(SIGPIPE) < 0)
        err_exit("add sig error");
    if(add_sig(SIGHUP) < 0)
        err_exit("add sig error");
    if(add_sig(SIGCHLD) < 0)
        err_exit("add sig error");
    if(add_sig(SIGQUIT) < 0)
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
    cout << "------ Create TimerContainer ------" << endl;

    ITimerContainer<UserEvent> *htc = new HeapTimerContainer<UserEvent>;

    cout << "------ Create TimerContainer over ------" << endl;
    epoll_event events[MAX_EVENT_NUMBER];//最大监听数目
    int timeout = 10000;      //如果没有连接，则设置超时值默认为10秒
    char sigbuf[128] = {0};
    bool running = true;
    // epoll_addfd(epollfd,sfd);
    // epoll_addfd(epollfd,udpfd);

    while(running)
    { 
        // 将定时容器中定时时间最短的时长作为epoll_wait的最大等待时间
        auto min_expire = htc->getMinExpire();
        /*其实下面这个减，并不是获取之前设定的timeout，而是获得当前定时器还剩多长时间，
        并设置到epoll里面，epoll超时未检测到，则必有定时器过期，然后执行tick()函数。也就是说将超时时间作为心动间隔*/
        // printf("min_expire:%ld\n",min_expire);
        // cout<<"min_expire - getMSec()"<<min_expire - getMSec()<<endl;
        int epolltimeout = (min_expire == -1) ? timeout : min_expire - getMSec();
        // printf("epolltimeout:%d\n",epolltimeout);
        int epoll_number = epoll_wait(epollfd,events,MAX_EVENT_NUMBER,epolltimeout);
        if(epoll_number < 0) /*其实这个应该不判断，或者判断完不退出，否则无法统一SINGINT或SIGTERM,
                            因为程序大部分时间都消耗在epoll_wait上,此函数会返回-1，虽然调用了sig_handler函数写进pipefd里面，但来不及判断pipefd就退出了*/
        { err_exit("\nepoll failure",false);}
        else if(epoll_number > 0)
        {
            for(int i=0;i<epoll_number;i++)
            {
                UserEvent *Uev =  (UserEvent *) events[i].data.ptr;
                if(Uev->fd == pipefd[0] && (Uev->event.events & EPOLLIN))/*其实这个最好放最前面来判断，如果放在最后或者中间，
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
                                    printf_DB("\nRecive SIGINT,the server STOP!!!\n");
                                    break;
                                }
                                case SIGTERM:
                                {
                                    printf_DB("\nRecive SIGTERM\n");
                                }
                                case SIGPIPE:
                                {                            
                                    //running = false;
                                    printf_DB("\nRecive SIGPIPE\n");
                                    break;
                                }
                                case SIGHUP:
                                {
                                    printf_DB("\nRecive SIGHUP\n");
                                    break;
                                }
                                case SIGCHLD:
                                {
                                    printf_DB("\nRecive SIGCHLD\n");
                                    break;
                                }
                                case SIGQUIT:
                                {
                                    printf_DB("\nRecive SIGQUIT\n");
                                    break;
                                }
                            }
                        }
                    }
                }
                //判断是否sfd,并建立新连接
                else if(Uev->fd == sfd && Uev->event.events & EPOLLIN)
                {   
                    acceptConn(Uev, htc);

                }
                else if(Uev->fd == udpfd &&Uev->event.events & EPOLLIN)/*UDP连接暂时只做回射信息*/
                {   
                    printf_DB("something connect in UDP \n");
                    UDP_Handle(Uev->fd);

                }
                else if(Uev->event.events & EPOLLIN)//TCP连接,即cfd可读变化,写和读都是主线程在做，http分析和响应由线程池做*/
                {
                     Uev->read_cb(Uev, htc);
                    /* 根据读的结果，决定是将任务加到线程池，还是关闭连接 */
                    // if (Uev->read())
                    // {
                    //     pool->append(Uev);          /*这里+????，会不会是 pool->append(users[sockfd])???但是好像+,也不是不行啊,users是数组首地址*/
                    // }
                    // else
                    // {
                    //     Uev->close_conn();
                    // }


                }
                else if(Uev->event.events & EPOLLOUT)//TCP连接,即cfd可写变化
                {
                        Uev->write_cb(Uev, htc);
                        /* 根据写的结果，决定是否关闭连接 */
                        // if (!Uev->write())
                        // {
                        //     Uev->close_conn();
                        // }
                }

                else if(Uev->event.events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
                {
                     /* 如果有异常，直接关闭客户连接 */
                        Uev->close_conn();
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
