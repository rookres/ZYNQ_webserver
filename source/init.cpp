#include "init.hpp"
extern void addfd(UserEvent *Uev, bool one_shot);
extern int pipefd[2];

/*用来判断函数调用返回值,错误则退出,默认退出结束进程*/
void err_exit(const char *reason,bool ExitFlag)
{
	char err[128];
	/*strerror_r用于一般用于多线程中,防止竞态条件*/
	printf("%s:%s\n",reason,strerror_r(errno,err,128));
	if(ExitFlag)
		exit(-1);
}

void sig_handler(int signum)
{
    /* 保留原来的 errno，在函数最后恢复，以保证函数的可重入性 */
    int save_errno = errno;
    char sig = (char) signum;
    Write(pipefd[1], &sig, 1); /* 将信号值写入管道，以通知主循环 */
    errno = save_errno;

}

int add_sig(int sig, void ( handler ) (int), bool restart )   /*void (handler)(int)和void (*handler)(int)是同一个函数指针类型的声明。*/
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if (restart)
    {
        sa.sa_flags |= SA_RESTART;  /*为了确保当信号发生时，被信号打断的系统调用能够从被打断的地方重新开始，而不是直接返回错误并终止系统调用的执行*/
    }
    sigfillset(&sa.sa_mask);
    return sigaction(sig, &sa, nullptr);
}

/*获取本地所有的IP地址,只显示了ipv4的，ipv6的没做处理.*/
void get_local_ip_addresses() {
    struct ifaddrs *ifap, *ifa;

    if (getifaddrs(&ifap) == -1) {
		err_exit("getifaddrs");
        return;
    }

    for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET) {
            struct sockaddr_in *sa = (struct sockaddr_in *)ifa->ifa_addr;
            char ip[INET_ADDRSTRLEN];

            if (inet_ntop(AF_INET, &sa->sin_addr, ip, sizeof(ip))) {
                printf_DB("%s IP Address: %s\n", ifa->ifa_name, ip);
            }
        } else if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET6) {
            // 类似处理IPv6地址
        }
    }
    freeifaddrs(ifap);
}
/*接受连接并通过sockaddr_in &sa引用返回端口和ip*/
int Accept(int fd,sockaddr_in &sa)
{
	int n;
	socklen_t salenptr = sizeof(sa);
again:
	if ((n = accept(fd, (struct sockaddr*)&sa, &salenptr)) < 0) {
		if ((errno == ECONNABORTED) || (errno == EINTR))//如果是被信号中断和软件层次中断,不能退出
			goto again;
		else
			err_exit("accept error");
	}

	return n;
}
/*绑定端口和地址*/
int Bind(int fd, const struct sockaddr *sa, socklen_t salen)
{
    int n;

	if ((n = bind(fd, sa, salen)) < 0)
		err_exit("bind error");

    return n;
}

int Connect(int fd, const struct sockaddr *sa, socklen_t salen)
{
    int n;

	if ((n = connect(fd, sa, salen)) < 0)
		err_exit("connect error");

    return n;
}

int Listen(int fd, int backlog)
{
    int n;

	if ((n = listen(fd, backlog)) < 0)
		err_exit("listen error");

    return n;
}

int Socket(int family, int type, int protocol)
{
	int n;

	if ((n = socket(family, type, protocol)) < 0)
		err_exit("socket error");

	return n;
}

ssize_t Read(int fd, void *ptr, size_t nbytes)
{
	ssize_t n;

again:
	if ( (n = read(fd, ptr, nbytes)) == -1) {
		if (errno == EINTR)//如果是被信号中断,不应该退出
			goto again;
		else
			return -1;
	}
	return n;
}

ssize_t Write(int fd, const void *ptr, size_t nbytes)
{
	ssize_t n;

again:
	if ( (n = write(fd, ptr, nbytes)) == -1) {
		if (errno == EINTR)
			goto again;
		else
			return -1;
	}
	return n;
}

int Close(int fd)
{
    int n;
	if ((n = close(fd)) == -1)
		err_exit("close error");

    return n;
}

/*参三: 应该读取固定的字节数数据*/
ssize_t Readn(int fd, void *vptr, size_t n)
{
	size_t  nleft;              //usigned int 剩余未读取的字节数
	ssize_t nread;              //int 实际读到的字节数
	char   *ptr;

	ptr =(char*)vptr;
	nleft = n;

	while (nleft > 0) {
		if ((nread = read(fd, ptr, nleft)) < 0) {
			if (errno == EINTR)
				nread = 0;
			else
				return -1;
		} else if (nread == 0)
			break;

		nleft -= nread;
		ptr += nread;
	}
	return n - nleft;
}
/*:固定的字节数数据*/
ssize_t Writen(int fd, const void *vptr, size_t n)
{
	size_t nleft;
	ssize_t nwritten;
	const char *ptr;

	ptr = (const char *)vptr;
	nleft = n;
	while (nleft > 0) {
		if ( (nwritten = write(fd, ptr, nleft)) <= 0) {
			if (nwritten < 0 && errno == EINTR)
				nwritten = 0;
			else
				return -1;
		}

		nleft -= nwritten;
		ptr += nwritten;
	}
	return n;
}

 ssize_t ReadOneChar(int fd, char *ptr)
{
	static int read_cnt;
	static char *read_ptr;
	static char read_buf[100];

	if (read_cnt <= 0) {
again:
		if ( (read_cnt = read(fd, read_buf, sizeof(read_buf))) < 0) {
			if (errno == EINTR)
				goto again;
			return -1;
		} else if (read_cnt == 0)
			return 0;
		read_ptr = read_buf;
	}
	read_cnt--;
	*ptr = *read_ptr++;

	return 1;
}

ssize_t Readline(int fd, void *vptr, size_t maxlen)
{
	size_t n, rc;
	char    c, *ptr;

	ptr = (char*)vptr;
	for (n = 1; n < maxlen; n++) {
		if ( (rc = ReadOneChar(fd, &c)) == 1) {
			*ptr++ = c;
			if (c  == '\n')
				break;
		} else if (rc == 0) {
			*ptr = 0;
			return n - 1;
		} else
			return -1;
	}
	*ptr  = 0;

	return n;
}

int tcp4init(short port,const char *IP,short backlog,bool reuseAddr)
{
    struct sockaddr_in serv_addr;
    int lfd = Socket(AF_INET,SOCK_STREAM,0);
    bzero(&serv_addr,sizeof(serv_addr));
    if(IP == NULL){
        //如果这样使用 0.0.0.0,任意ip将可以连接
        serv_addr.sin_addr.s_addr = INADDR_ANY;
    }else{
        if(inet_pton(AF_INET,IP,&serv_addr.sin_addr.s_addr) <= 0){
            perror(IP);//转换失败
            exit(1);
        }
    }
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port   = htons(port);
	/*设置端口复用*/
	if(reuseAddr)
	{    int opt = 1;
		setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));}
    Bind(lfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr));
	Listen(lfd,backlog);
    return lfd;
}

int udp4init(short port,const char *IP)
{
	/*创建UDP socket，并将其绑定到端口port上*/
	struct sockaddr_in address;
    bzero( &address, sizeof( address ) );
    address.sin_family = AF_INET;
	if(IP == NULL){
        //如果这样使用 0.0.0.0,任意ip将可以连接
        address.sin_addr.s_addr = INADDR_ANY;
    }else{
        if(inet_pton(AF_INET,IP,&address.sin_addr.s_addr) <= 0){
            perror(IP);//转换失败
            exit(1);
        }
    }
	int udpfd = socket( AF_INET, SOCK_DGRAM, 0 );
    address.sin_port = htons( port );
	/*设置端口复用*/
    int opt = 1;
	setsockopt(udpfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    Bind( udpfd, ( struct sockaddr* )&address, sizeof( address ) );

	return udpfd;
}

char* Change_Dir(char*pwd_path,const char*resource_path)
{
	if(resource_path==nullptr)
	{
		return pwd_path;
	}
	else
	{
		strcat(pwd_path,resource_path);
		if (chdir(pwd_path) != 0) 
		{
			err_exit("Failed to change directory");
			// 处理错误，例如退出程序或采取其他恢复措施
			exit(EXIT_FAILURE);
		}
		return pwd_path;
	}
}

void UDP_Handle(int udpfd)
{

	char buf[ UDP_BUFFER_SIZE ],serverbuf[UDP_BUFFER_SIZE];
	memset( buf, '\0', UDP_BUFFER_SIZE );
	memset( serverbuf, '\0', UDP_BUFFER_SIZE );
	char ip[64];
	struct sockaddr_in client_address;
	socklen_t client_addrlength = sizeof( client_address );
		/*UDP read，in order to get addr and ip*/
	recvfrom( udpfd, buf, UDP_BUFFER_SIZE-1, 0, ( struct sockaddr* )&client_address, &client_addrlength );
	inet_ntop(AF_INET, &client_address.sin_addr, ip, client_addrlength);
	printf_DB( "new UDP connect,ip:%s ,port:%d\n",ip,ntohs(client_address.sin_port));
	/*return same message*/
	sprintf(serverbuf,"The server reply:%s",buf);
	sendto( udpfd, serverbuf, UDP_BUFFER_SIZE-1, 0, ( struct sockaddr* )&client_address, client_addrlength );
}

int setnonblocking( int fd )
{
    int old_option = fcntl( fd, F_GETFL );
    int new_option = old_option | O_NONBLOCK;
    fcntl( fd, F_SETFL, new_option );
    return old_option;
}


void addfd(UserEvent *Uev, bool one_shot)
{

     Uev->event.data.ptr = (void *) Uev;
    Uev->event.events=EPOLLIN | EPOLLET | EPOLLRDHUP;
    if (one_shot)
    {
         Uev->event.events |= EPOLLONESHOT;
    }
    epoll_ctl(UserEvent::m_epollfd, EPOLL_CTL_ADD, Uev->fd,&Uev->event);
    setnonblocking(Uev->fd);
}

void modfd(int ev,UserEvent *Uev)
{
    Uev->event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    epoll_ctl(UserEvent::m_epollfd, EPOLL_CTL_MOD, Uev->m_sockfd, &Uev->event);
}

void removefd(int &epollfd, int &fd)
{
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}


void send_header(int cfd, int code,const char *info,const char *filetype,int length)
{	//发送状态行
	char buf[1024]="";
	int len =0;
	len = sprintf(buf,"HTTP/1.1 %d %s\r\n",code,info);
	send(cfd,buf,len,0);
	//发送消息头
	len = sprintf(buf,"Content-Type:%s\r\n",filetype);
	send(cfd,buf,len,0);
	if(length > 0)
	{
			//发送消息头
		len = sprintf(buf,"Content-Length:%d\r\n",length);
		send(cfd,buf,len,0);

	}
	//空行
	send(cfd,"\r\n",2,0);
}

void send_file(int epfd,int cfd,const char *path,bool flag)
{
		int fd = open(path,O_RDONLY);
		if(fd <0)
		{
			perror("open");
			return ;
		}
		char buf[1024]="";
		int len =0;
		 while((len=Read(fd,buf,sizeof(buf)))>0)
		{
			 send(cfd,buf,len,0);
		}

		close(fd);
		//关闭cfd,下树
		if(flag)
		{
			close(cfd);
			epoll_ctl(epfd,EPOLL_CTL_DEL,cfd,NULL);
		}
}

void pipefd_handle(int *pipefd,char*sigbuf,short sigbuf_len,bool &running )
{
    memset(sigbuf,'\0',sigbuf_len);
    int n = read(pipefd[0], sigbuf, sigbuf_len);
    if(n < 0)
    {
		err_exit( "deal read signal error:",false);
        return;
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
                    break;
                }
                case SIGHUP:
                {
                    printf_DB("\nRecive SIGHUP\n");
                    break;
                }
                case SIGCHLD: /*此处回收的主要是由主进程fork产生的子进程执行excel的*/
                {
                    printf_DB("\nRecive SIGCHLD\n");                                    
                    pid_t pid;
                    int stat;
                    while ((pid = waitpid(-1, &stat, WNOHANG)) > 0)/*回收子进程资源*/
                    {
                        printf_DB("Handle SIGCHLD signal over\n"); 
                        continue;
                    }
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
// 超时处理的回调函数
void timeout_handle(UserEvent *cli)
{
    if(cli == nullptr)
    { return ;}

    send_header(cli->fd, 200,"OK",get_mime_type("*.html"),0);
    send_file(cli->m_epollfd,cli->fd,"timeout.html",true);
	cli->read_cb=nullptr;
	cli->write_cb=nullptr;
    UserEvent::m_user_count--;
    delete cli;
}

void show_error(int connfd, const char *info)
{
    printf("%s", info);
    send(connfd, info, strlen(info), 0);
    close(connfd);
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
    cli->init(newcfd,sa,readData,writeData);

    auto timer = htc->addTimer(15000);      //设置客户端超时值15秒
    timer->setUserData(cli);
    timer->setCallBack(timeout_handle);
    cli->timer = (void *)timer;
    addfd(cli, true);
    printf_DB("New Connection, ip:[%s:port=%u]\n",cli->ip,cli->port);

}