#include "init.hpp"

void err_exit(const char *reason,bool ExitFlag)
{
	char err[128];
	// char *strerror_r(int errnum, char *buf, size_t buflen);
	// cout << reason << ":" << strerror(errno) << endl;
	printf("%s:%s\n",reason,strerror_r(errno,err,128));
    // exit(1);
	// perror(s);
	if(ExitFlag)
		exit(-1);
}

int Accept(int fd,char* ip,uint16_t* port)
{
	int n;
	struct sockaddr_in sa;
	socklen_t salenptr = sizeof(sa);
again:
	if ((n = accept(fd, (struct sockaddr*)&sa, &salenptr)) < 0) {
		if ((errno == ECONNABORTED) || (errno == EINTR))//如果是被信号中断和软件层次中断,不能退出
			goto again;
		else
			err_exit("accept error");
	}
	inet_ntop(AF_INET,&sa.sin_addr.s_addr,ip,16);
	*port=ntohs(sa.sin_port);
	// printf_DB("new client ip=%s port=%d\n",inet_ntop(AF_INET,&sa.sin_addr.s_addr,ips,16),ntohs(sa.sin_port));
	return n;
}


void get_local_ip_addresses() {
    struct ifaddrs *ifap, *ifa;

    if (getifaddrs(&ifap) == -1) {
        perror("getifaddrs");
        return;
    }

    for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET) {
            struct sockaddr_in *sa = (struct sockaddr_in *)ifa->ifa_addr;
            char ip[INET_ADDRSTRLEN];

            if (inet_ntop(AF_INET, &sa->sin_addr, ip, sizeof(ip))) {
                printf("\n%s IP Address: %s\n", ifa->ifa_name, ip);
            }
        } else if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET6) {
            // 类似处理IPv6地址
        }
    }

    freeifaddrs(ifap);
}

int Accept(int fd,sockaddr_in &sa)
{
	int n;
	// struct sockaddr_in sa;
	socklen_t salenptr = sizeof(sa);
again:
	if ((n = accept(fd, (struct sockaddr*)&sa, &salenptr)) < 0) {
		if ((errno == ECONNABORTED) || (errno == EINTR))//如果是被信号中断和软件层次中断,不能退出
			goto again;
		else
			err_exit("accept error");
	}

	// printf_DB("new client ip=%s port=%d\n",inet_ntop(AF_INET,&sa.sin_addr.s_addr,ips,16),ntohs(sa.sin_port));
	return n;
}

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


void Change_Dir(char*pwd_path)
{
	char * path = getenv("PWD");//获取当前目录的工作路径
	strcpy(pwd_path,path);
	strcat(pwd_path,"/Resource");
	if (chdir(pwd_path) != 0) 
	{
		perror("Failed to change directory\n");
		// 处理错误，例如退出程序或采取其他恢复措施
		exit(EXIT_FAILURE);
	}
}

void  udp_write()
{
	//udp写的话需要ip和端口地址，最好用sendto,收也要是recvfrom等等，先放着,
	//看LinuxServerCodesLinux(源码)高性能服务器编程游双9-8multi_port.cpp,有读写
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

void epoll_addfd( int &epollfd, int &fd )
{
    epoll_event event;
    event.data.fd = fd;
    //event.events = EPOLLIN | EPOLLET;
    event.events = EPOLLIN;
    epoll_ctl( epollfd, EPOLL_CTL_ADD, fd, &event );
    setnonblocking( fd );
}

void epoll_rmfd(int &epollfd,int fd)
{
		epoll_ctl(epollfd,EPOLL_CTL_DEL,fd,NULL);
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

void read_client_request(UserEvent *Uev)
{
		//读取请求(先读取第一行)
	char buf[1024]="";
	char tmp[1024]="";
	Readline(Uev->fd, buf, sizeof(buf));
	printf_DB("The first line:%s",buf);
	int ret =0;
	 /*把其他行读取,暂时扔掉，之后加有限状态机解析后面的行*/
	 while( (ret = Readline(Uev->fd, tmp, sizeof(tmp))) >0)
	 {
		printf_DB("%s",tmp);  
	 }
	 //解析请求行  GET /a.txt  HTTP/1.1\R\N
	//  char method[256]="",content[256]="", protocol[256]="";
	 sscanf(buf,"%[^ ] %[^ ] %[^ \r\n]",Uev->method,Uev->content,Uev->protocol);
	 printf_DB("method:%s\ncontent:%s\nprotocol:%s\n",Uev->method,Uev->content,Uev->protocol);

}

// void Get_Handle(int epfd,UserEvent *Uev) 
// {
// 	//[GET]  [/%E8%8B%A6%E7%93%9C.txt]  [HTTP/1.1]
// 	 		char *strfile = Uev->content+1;
// 			if (strfile[0] == '%' && isxdigit(strfile[1]) && isxdigit(strfile[2]))
// 			{
// 				strdecode(strfile,strfile);/*%E8%8B%A6%E7%93%9C格式乱码转换*/
// 			}
// 	 		 //GET / HTTP/1.1\R\N
// 	 		if(*strfile == 0)		//如果没有请求文件,默认请求当前目录
// 	 			strfile= "./";
// 				// strfile= "./index.html";
// 	 		//判断请求的文件是否存在
// 	 		struct stat s;
			
// 	 		if(stat(strfile,&s) < 0)//文件不存在
// 	 		{
// 	 			printf_DB("file not found\n");
// 	 			//先发送 报头(状态行  消息头  空行)
// 	 			send_header(Uev->fd, 404,"NOT FOUND",get_mime_type("*.html"),0);
// 	 			//发送文件 error.html
// 	 			send_file(epfd,Uev->fd,"error.html");

// 	 		}
// 	 		else
// 	 		{	 //请求的是一个普通的文件
// 	 			if(S_ISREG(s.st_mode))
// 	 			{
// 	 				printf_DB("file\n");
// 	 				//先发送 报头(状态行  消息头  空行)
// 	 				send_header(Uev->fd, 200,"OK",get_mime_type(strfile),s.st_size);
// 	 				//发送文件
// 	 				send_file(epfd,Uev->fd,strfile);
// 	 			}
// 	 			else if(S_ISDIR(s.st_mode))//请求的是一个目录
// 	 			{
// 						printf_DB("dir\n");
// 						//发送一个列表  网页
// 						send_header(Uev->fd, 200,"OK",get_mime_type("*.html"),0);
// 						//发送header.html
// 						send_file(epfd,Uev->fd,"dir_header.html");

// 						struct dirent **mylist=NULL;
// 						char buf[1024]="";
// 						int len =0;
// 						int n = scandir(strfile,&mylist,NULL,alphasort);
// 						for(int i=0;i<n;i++)
// 						{
// 							//printf("%s\n", mylist[i]->d_name);
// 							if(mylist[i]->d_type == DT_DIR)//如果是目录
// 							{
// 								len = sprintf(buf,"<li><a href=%s/ >%s</a></li>",mylist[i]->d_name,mylist[i]->d_name);
// 							}
// 							else
// 							{
// 								len = sprintf(buf,"<li><a href=%s >%s</a></li>",mylist[i]->d_name,mylist[i]->d_name);
// 							}

// 							send(Uev->fd,buf,len ,0);
// 							free(mylist[i]);
// 						}
// 						free(mylist);
// 						send_file(epfd,Uev->fd,"dir_tail.html");
// 	 			}
// 	 		}
// }			