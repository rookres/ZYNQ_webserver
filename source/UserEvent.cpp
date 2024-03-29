#include "UserEvent.hpp"
#include <fcntl.h>
#include "init.hpp"
/* 定义HTTP响应的一些状态信息 */
const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to satisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file from this server.\n";
const char *error_404_title = "Not Found";
const char *error_404_form = "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form = "There was an unusual problem serving the requested file.\n";

/* 网站的根目录 */
// const char *doc_root = "/home/tronlong/test/webserver/Resource";
const char *doc_root = Change_Dir(getenv("PWD"));//切换需要的工作Resource目录;

int UserEvent::m_user_count = 0;
int UserEvent::m_epollfd = -1;

void UserEvent::close_conn(bool real_close)     /*声明 函数参数已经有默认参数void close_conn(bool real_close = true;*/
{
    if (real_close && (m_sockfd != -1))
    {
        removefd(m_epollfd, m_sockfd);          /*只是移走监听，没有删除吗？ 删除了，在removefd函数里面*/
        m_sockfd = -1;
        m_user_count--; /* 关闭一个连接时，将客户总量减1 */
    }
}

void UserEvent::init(int newcfd, const sockaddr_in &sa,readHandle readData,writeHandle writeData)
{
    fd = newcfd;
    m_sockfd=newcfd;
    m_address = sa;
    inet_ntop(AF_INET,&sa.sin_addr.s_addr,ip,32);
	port=ntohs(sa.sin_port);
    read_cb = readData;
    write_cb = writeData;
    /* 如下两行是为了避免TIME_WAIT状态, 仅用于调试，实际使用时应该去掉 */
    int reuse = 1;
    setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    m_user_count++;

    init();
}

void UserEvent::init()
{
    m_checked_state = CHECK_STATE_REQUESTLINE;
    m_linger = false;

    m_method = GET;
    m_url = 0;
    m_version = 0;
    m_content_length = 0;
    m_host = 0;
    m_start_line = 0;
    m_checked_idx = 0;
    m_read_idx = 0;
    m_write_idx = 0;
    bytes_have_send=0;//le

    memset(m_read_buf, '\0', READ_BUFFER_SIZE);
    memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
    memset(m_real_file, '\0', FILENAME_LEN);
}


/* 循环读取客户数据，直到无数据可读或者对方关闭连接 */
bool UserEvent::read()          /*此函数在main函数里面调用*/
{
    if (m_read_idx >= READ_BUFFER_SIZE)
        return false;

    int bytes_read = 0;
    while(true)
    {
        bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
        if (bytes_read == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            err_exit("recv error:eason",false);
            return false;
        }
        else if (bytes_read == 0)       /*此处应该返回true吧?答：不应返回true，这个对方已经关闭了,main函数里面是要读完后加入线程池的*/
        {
            printf_DB("the client is closed\n");
            return false;
        }

        m_read_idx += bytes_read;
    }
    return true;
}
/* 写HTTP响应,此处逻辑不对，
（1）如果这次循环只把一部分数据写到socket缓冲区然后缓冲区满了，writev系统调用返回-1（socket是非阻塞的），循环退出，那下次再次调用write时会不会把这部分数据重复写呢。
（2）而且这里通过循环把数据集中写到socket缓冲区里的，它是怎么记录下次从用户缓冲区的哪个地方开始写的。
（3）另外bytes_to_send <= bytes_have_send这个条件也看的不太懂，结束条件不应该是bytes_to_send等于0吗。
    要是小文件之类的,暂时看不出问题，但是要是gif大几M的文件就会出现卡顿。
 解决方法有两种：
 1.在UserEvent::write()函数里重新设置为阻塞,int old_option = fcntl( fd, F_GETFL );int new_option = old_option & ~O_NONBLOCK;fcntl( fd, F_SETFL, new_option );
 让内核去操作等待，但此时主线程会阻塞,影响客户响应。放在之前world文档源代码while循环前就可以。
 2.在在循环体内，首先调用writev尝试写入数据，然后根据返回值和错误码判断是否需要继续写入，
 同时更新已写入的数据量以及iov数组中的偏移量，以便下一次调用时从上次写入的位置开始继续尝试。
*/
/* 写HTTP响应 */
bool UserEvent::write()       /*此函数在main函数里面调用, main函数会根据返回值决定是否关闭连接*/
{
    int bytes_to_send = m_write_idx;
    if (bytes_to_send == 0)
    {
        printf_DB("  if (bytes_to_send == 0),***return true\n");
        modfd(EPOLLIN,this);
        init();
        return true;
    }
    while (bytes_have_send < m_write_idx)  // 当循环结束时，total_len等于iov_len_sum表示所有数据都已成功写入
    { // iov_len_sum是所有iov缓冲区的总长度
        ssize_t  written_bytes  =  writev(m_sockfd, m_iv, m_iv_count);
        if (written_bytes == -1) {
            if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) 
            {
                // 没有足够的缓冲区空间或资源暂时不可用，可以稍后再试
                // 此处可以结合select/poll/epoll等I/O多路复用技术等待套接字变为可写
                modfd(EPOLLOUT,this);
                printf_DB(" errno == EAGAIN,***return true\n");
                return true;
                // continue;
            }else if (errno == EPIPE || errno == ECONNRESET) 
            {
            // 发生了Broken pipe（连接已关闭）或连接重置错误
            // 这种情况下通常停止写入，处理错误或关闭连接
                printf_DB("errno == EPIPE  the client has close connection,can't write\n");
                unmap();
                return false;
            }else 
            {
                // 其他错误，打印错误日志并处理
                err_exit("writev failed");
                break; 
            }   
        }
        else if (written_bytes == 0) 
        {
        // 写入0字节，通常表示连接已经关闭
                printf_DB("written_bytes == 0  the client has close connection,can't write\n");
                break;
        }
        bytes_have_send += written_bytes;
        // 移动iov数组中已写入部分，准备下一轮写入,牛逼！！！！！！！！！(破音),可惜不是我写出来的
        for (int i = 0; i < m_iv_count; ++i) {
            if (m_iv[i].iov_len <= (size_t)written_bytes)
            {
                written_bytes -= m_iv[i].iov_len;   /*当写入字节大于某块的字节长度时,减去是为了下一个块的比较*/
                m_iv[i].iov_base = (char*)m_iv[i].iov_base + m_iv[i].iov_len;
                m_iv[i].iov_len = 0;
            } else {
                m_iv[i].iov_base = (char*)m_iv[i].iov_base + written_bytes;
                m_iv[i].iov_len -= written_bytes;
                written_bytes = 0;
                break;
            }
        }                
    }
    /* 发送HTTP响应成功，根据HTTP请求中的Connection字段决定是否理解关闭连接 */
        unmap();
        if (m_linger)
        {
            init();
            modfd(EPOLLIN,this);
            printf_DB("******************m_linger true***************\n");
            return true;
        }
        else
        {
            modfd(EPOLLIN,this);    /*main函数会根据返回值决定是否关闭连接*/
            // init();
            printf_DB("******************m_linger  false***************\n");
            return false;//return true/*此处暂时不断开，因为没解析完整的http请求,暂时不断掉连接*/
        }
}

/* 由线程池中的工作线程调用，这是处理HTTP请求的入口函数 */
void UserEvent::process()
{
    HTTP_CODE read_ret = process_read();
    if (read_ret == NO_REQUEST)
    {
        modfd(EPOLLIN,this);
        return ;
    }
    bool write_ret = process_write(read_ret);
    if (!write_ret)
    {
        close_conn();
        read_cb=nullptr;
        write_cb=nullptr;
    }
    if(read_ret==DIR_REQUEST)
    {
        /*在process_write里面已经处理写过了了，此处暂不操作,如果此处再设置EPOLLOUT,会导致无法监听EPOLLIN,但必须调整定时器时长*/
        Timer<UserEvent> * timer=(Timer<UserEvent> *)this->timer;
        timer->setTimeout(15000);
    }
    else
    {modfd(EPOLLOUT,this);}
}

/* 主状态机,也可观看my word文档的有限状态机分析http协议实例) */
UserEvent::HTTP_CODE UserEvent::process_read()
{
    LINE_STATUS line_status = LINE_OK;          /*记录当前的读取状态*/
    HTTP_CODE ret = NO_REQUEST;                 /*记录HTTP请求的处理结果*/
    char *text = 0;
    /*用于从m_read_buffer中取出完整的行*/
    while (((m_checked_state == CHECK_STATE_CONTENT) && (line_status == LINE_OK))
        || ((line_status = parse_line()) == LINE_OK))
    {
        text = get_line();
        m_start_line = m_checked_idx;       /*记录下一行的真实位置*/
        printf_DB("%s\n", text);
        switch (m_checked_state)
        {
            case CHECK_STATE_REQUESTLINE:            /*第一个状态，分析请求*/
            {
                ret = parse_request_line(text);
                if (ret == BAD_REQUEST)
                {
                    return BAD_REQUEST;
                }

                // return do_request();/*放在这是为了只解析Get行，之后为了分析完整的http请求，这个要去掉*/
                break;
            }
            case CHECK_STATE_HEADER:             /*第二个状态，分析头部字段*/
            {
                ret = parse_headers(text);
                if (ret == BAD_REQUEST)
                {
                    return BAD_REQUEST;
                }
                else if (ret == GET_REQUEST)
                {
                    return do_request();
                }
                break;
            }
            case CHECK_STATE_CONTENT:           /*第三个状态，分析请求包体*/
            {
                ret = parse_content(text);
                if (ret == GET_REQUEST)
                {
                    return do_request();
                }
                line_status = LINE_OPEN;
                break;
            }
            default:
            {
                return INTERNAL_ERROR;
            }
        }
    }

    return NO_REQUEST;
}

/* 根据服务器处理HTTP请求的结果，决定返回给客户端的内容 */
bool UserEvent::process_write(HTTP_CODE ret)
{
    switch(ret)
    {
        case INTERNAL_ERROR:
        {
            add_status_line(500, error_500_title);
            add_headers(strlen(error_500_form));
            if (!add_content(error_500_form))
            {
                return false;
            }
            break;
        }
        case BAD_REQUEST:
        {
            add_status_line(400, error_400_title);
            add_headers(strlen(error_400_form));
            if (!add_content(error_400_form))
            {
                return false;
            }
            break;
        }
        case NO_RESOURCE:
        {
            add_status_line(404, error_404_title);
            add_headers(strlen(error_404_form));
            if (!add_content(error_404_form))
            {
                return false;
            }
            break;
        }
        case FORBIDDEN_REQUEST:
        {
            add_status_line(403, error_403_title);
            add_headers(strlen(error_403_form));
            if (!add_content(error_403_form))
            {
                return false;
            }
            break;
        }
        case FILE_REQUEST:
        {
            add_status_line(200, ok_200_title);
            if (m_file_stat.st_size != 0)
            {
                add_headers(m_file_stat.st_size,get_mime_type(m_url+1));
                m_iv[0].iov_base = m_write_buf;
                m_iv[0].iov_len = m_write_idx;
                m_iv[1].iov_base = m_file_address;
                m_iv[1].iov_len = m_file_stat.st_size;
                m_iv_count = 2;
                m_write_idx=m_write_idx+ m_file_stat.st_size;//le
                return true;
            }
            else
            {
                const char *ok_string = "<html><body></body></html>";
                add_headers(strlen(ok_string));
                if (!add_content(ok_string))
                {
                    return false;
                }
            }
            break;
        }
        case DIR_REQUEST:
        {
            send_header(this->fd, 200,"OK",get_mime_type("*.html"),0);
            //发送header.html
            send_file(this->m_epollfd,this->fd,"dir_header.html",0);

            struct dirent **mylist=NULL;
            char buf[1024]="";
            int len =0;
            int n = scandir(m_real_file,&mylist,NULL,alphasort);
            for(int i=0;i<n;i++)
            {
                if(mylist[i]->d_type == DT_DIR)//如果是目录
                {
                    // len = sprintf(buf,"<li><a href=%s/ >%s</a></li>",mylist[i]->d_name,mylist[i]->d_name);
                    len = sprintf(buf,
                    "<li style=\"color: white; font-size: 20px;\"><a href=\"%s/\">%s</a></li>",
                    mylist[i]->d_name, mylist[i]->d_name);
                }
                else
                {
                    // len = sprintf(buf,"<li><a href=%s >%s</a></li>",mylist[i]->d_name,mylist[i]->d_name);
                    len = sprintf(buf,
                    "<li style=\"color: white; font-size: 20px;\"><a href=\"%s\">%s</a></li>",
                    mylist[i]->d_name, mylist[i]->d_name);
                }
                send(this->fd,buf,len ,0);

                free(mylist[i]);
            }
            free(mylist);
            send_file(this->m_epollfd,this->fd,"dir_tail.html",false);
            return true;/*此处设为fasle,而不是true,浏览器就不会转圈了*/
        }
        case CGI_REQUEST:
        {
            /* 判断客户要运行的cgi程序是否存在 */
            if (access(m_url+1, F_OK) == -1)
            {
                removefd(this->m_epollfd, this->m_sockfd);
                printf_DB("cgi no exits,false!\n");
                return false;
            }
            /* 创建子进程来执行CGI程序 */
            int ret = fork();
            if (ret == -1)
            {
                // removefd(this->m_epollfd, this->m_sockfd);
                err_exit("fork fail!");
                return false;
            }
            else if (ret == 0)
            {
                /* 子进程将标准输出定制到m_sockfd, 并执行CGI程序 */
                close(STDOUT_FILENO);
                if(dup(this->m_sockfd)<0)
                {
                    err_exit("dup error");
                }
                else
                {
                    if(strcasecmp(get_mime_type(m_real_file),"sh")==0)
                    {
                        send_header(this->fd, 200,"OK",get_mime_type("*.html"),0);/*不知道为什么执行cgi程序之前必须先给浏览器以响应,如果在cgi程序里面写的也有不行嘛？不理解--2024年3月7日18点08分*/
                        execl(m_real_file,"cgi",NULL);
                    }else
                    {
                         execl(m_real_file,"cgi",NULL);
                    }                                                          /*其实是可以的,可能程序编写的时候http响应不符合要求.。要么提前发响应,要么在cgi程序里写--2024年3月7日18点32分*/
                }
                // execl("/bin/ls", "ls", "-l", NULL);  // 执行 ls -l 命令
                exit(0);

            }
            else
            {   
                /* 父进程只需要关闭连接,不关闭的话用nc命令可以再次请求,不过最好关闭*/
                removefd(this->m_epollfd, this->m_sockfd);
                return false;
            }
        }
        default:
        {
            return false;
        }
    }
    m_iv[0].iov_base = m_write_buf;
    m_iv[0].iov_len = m_write_idx;
    m_iv_count = 1;
    return true;
}

/* 解析HTTP请求行，获得请求方法、目标URL，以及HTTP版本号 */
UserEvent::HTTP_CODE UserEvent::parse_request_line(char *text)
{
    m_url = strpbrk(text, " \t");   
    if (!m_url)             /*如果请求行中没有空白字符或“\t”，则HTTP请求必有问题*/
    {
        return BAD_REQUEST;
    }
    *m_url++ = '\0';

    char *method = text;
    if (strcasecmp(method, "GET") == 0)
    {
        m_method = GET;
    }
    else
    {
        return BAD_REQUEST;
    }

    m_url += strspn(m_url, " \t");
    m_version = strpbrk(m_url, " \t");
    if (!m_version)
    {
        return BAD_REQUEST;
    }
    *m_version++ = '\0';
    m_version += strspn(m_version, " \t");
    if (strcasecmp(m_version, "HTTP/1.1") != 0)  /*仅支持HTTP/1.1*/
    {
        return BAD_REQUEST;
    }
    // if (strncasecmp(m_url, "http://", 7) == 0)  /*检查URL是否合法*/
    // {
    //     m_url += 7;
    //     m_url = strchr(m_url, '/');
    // }

    // if (!m_url || m_url[0] != '/')
    // {
    //     return BAD_REQUEST;
    // }
    /*下面是自己改的，没用http://,从这开始*/
    if (strncasecmp(m_url, "/", 1) == 0)  /*检查URL是否合法*/
    {
        // m_url += 1;
        m_url = strchr(m_url, '/');
    }
    if (!m_url || m_url[0] != '/')
    {
        return BAD_REQUEST;
    }
    /*到这结束*/
    m_checked_state = CHECK_STATE_HEADER;    /*HTTP请求行处理完毕，状态转移到头部字段的分析*/
    return NO_REQUEST;
}

/* 解析HTTP请求的一个头部信息 */
UserEvent::HTTP_CODE UserEvent::parse_headers(char *text)
{
    /* 遇到空行，表示头部字段解析完毕 */
    if (text[0] == '\0')
    {
        /* 如果HTTP请求有消息体，则还需要读取m_content_length字节的消息体，状态机转移到CHECK_STATE_CONTENT状态 */
        if (m_content_length != 0)
        {
            m_checked_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }

        /* 否则说明我们已经得到了一个完整的HTTP请求 */
        return GET_REQUEST;
    }
    /* 处理Connection头部字段 */
    else if (strncasecmp(text, "Connection:", 11) == 0)
    {
        text += 11;
        text += strspn(text, " \t");
        if (strcasecmp(text, "keep-alive") == 0)
        {
            m_linger = true;
        }
    }
    /* 处理Content-Length头部字段 */
    else if (strncasecmp(text, "Content-Length:", 15) == 0)
    {
        text += 15;
        text += strspn(text, " \t");
        m_content_length = atol(text);

    }
    /* 处理Host头部字段 */
    else if (strncasecmp(text, "Host:", 5) == 0)
    {
        text += 5;
        text += strspn(text, " \t");
        m_host = text;
    }
     /* 处理User-Agent头部字段 */
    else if(strncasecmp(text, "User-Agent:", 11))
    {
        text += 11;
        text += strspn(text,  " \t");
        m_user_agent=text;

    }
     /* 处理Accept:头部字段 */
    else if(strncasecmp(text, "Accept:",7))
    {
        text += 7;
        text += strspn(text,  " \t");
        m_accept=text;
    }
    /* 处理Referer:头部字段 */
    else if(strncasecmp(text, "Referer:",8))
    {
        text += 8;
        text += strspn(text,  " \t");
        m_referer=text;
    }
    /* 处理Accept-Encoding:头部字段 */
    else if(strncasecmp(text, "Accept-Encoding:",16))
    {
        text += 16;
        text += strspn(text,  " \t");
        m_accept_encodeing=text;
    }
    /* 处理Accept-Language:头部字段 */
    else if(strncasecmp(text, "Accept-Language:",16))
    {
        text += 16;
        text += strspn(text,  " \t");
        m_accept_language=text;
    }
     /* 处理Upgrade-Insecure-Requests::头部字段 */
    else if(strncasecmp(text, "Upgrade-Insecure-Requests:",26))
    {
        text += 26;
        text += strspn(text,  " \t");
        if (strcasecmp(text, "1") == 0)
        {
            m_update_Insecure = true;
        }
    }
    else
    {
        printf_DB("********************** \n");
        printf_DB("oop! unkown header %s\n", text);
        printf_DB("********************** \n");
    }

    return NO_REQUEST;
}

/* 没有真正解析HTTP请求的消息体，只是判断它是否被完整读入 */
UserEvent::HTTP_CODE UserEvent::parse_content(char *text)
{
    if (m_read_idx >= (m_content_length + m_checked_idx))
    {
        text[m_content_length] = '\0';
        return GET_REQUEST;
    }

    return NO_REQUEST;
}

/* 当得到一个完整、正确的HTTP请求时，我们就分析目标文件的属性。如果目标文件存在、
 * 对所有用户可读，且不是目录，则使用mmap将其映射到内存地址m_file_address处，
 * 并告诉调用者获取文件成功 */
UserEvent::HTTP_CODE UserEvent::do_request()
{
    strcpy(m_real_file, doc_root);
    int len = strlen(doc_root);
    char *strfile = m_url+1;    /*注意这个,如果请求的不是文件而是目录的话,会访问非法地址,导致段错误,所有判断return DIR_REQUEST;要在前面*/
    char *strtemp=strchr(strfile,'%');
    printf_DB("strtemp ==************%s\n",strtemp);
    if(strtemp==NULL)
    {

    }
    else
    {    
        if (strtemp[0] == '%' && isxdigit(strtemp[1]) && isxdigit(strtemp[2]))
        {
            strdecode(strtemp,strtemp);/*%E8%8B%A6%E7%93%9C格式乱码转换*/
        }
    }
    strncpy(m_real_file + len, m_url, FILENAME_LEN - len - 1);
    printf_DB("**stat before***The m_real_file is %s\n",m_real_file);
    if (stat(m_real_file, &m_file_stat) < 0)
    {
        printf_DB(" return NO_RESOURCE;\n");
        return NO_RESOURCE;
    }
 
    if (S_ISDIR(m_file_stat.st_mode))
    {
        return DIR_REQUEST;
    }

    printf_DB("*****The m_real_file is %s\n",m_real_file);
    if (strcasecmp(strrchr(strfile,'.'),".cgi")==0 || strcasecmp(strrchr(strfile,'.'),".sh")==0 )
    {
        return CGI_REQUEST;
    }

    if (!(m_file_stat.st_mode & S_IROTH))
    {
        return FORBIDDEN_REQUEST;
    }

    int fd = open(m_real_file, O_RDONLY);
     printf_DB("*****m_file_stat.st_size %ld\n",m_file_stat.st_size);
    m_file_address = (char *)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    return FILE_REQUEST;
}

/* 从状态机,(看书第八章,也可观看my word文档的有限状态机分析http协议实例) */
UserEvent::LINE_STATUS UserEvent::parse_line()
{
    char temp;

     /*m_checked_index指向m_read_buf中当前正在分析的字节，m_read_idx指向m_read_buffer中客户数据的
    尾部的下一字节。m_read_buffer中第0~m_checked_index字节都已经分析完毕，第m_checked_index~（m_read_index-1）
    字节由下面的循环挨个分析*/
    for (; m_checked_idx < m_read_idx; ++m_checked_idx)
    {
        temp = m_read_buf[m_checked_idx];    /*获得当前要分析的字节*/
        if (temp == '\r')                   /*如果当前的字节是"\r"，即回车符，则说明可能读取到一个完整的行*/
        {   
                                             /*如果“\r”字符碰巧是目前m_read_buffer中最后一个已经被读入的客户数据，那么这次分析
                                                没有读取到一个完整的行，返回LINE_OPEN以表示还需要继续去客户数据才能进一步分析*/
            if ((m_checked_idx + 1) == m_read_idx)
            {
                return LINE_OPEN;
            }
            else if (m_read_buf[m_checked_idx + 1] == '\n')     /*如果下一个字符时"\n"，则说明我们成功读取一个完整的行*/
            {
                m_read_buf[m_checked_idx++] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;                     /*否则的话，说明客户发送的HTTP请求存在语法问题*/
        }
        else if( temp ==  '\n' )            /*如果当前的字节是"\n"，即换行符，则也说明可能读取到一个完整的行*/
        {
             if( ( m_checked_idx >  1 ) && ( m_read_buf[ m_checked_idx -  1 ] == '\r' ) )
            {
                m_read_buf[ m_checked_idx-1 ] =  '\0';
                m_read_buf[ m_checked_idx++ ] =  '\0';
                return LINE_OK;
            }
             return LINE_BAD;
        }
    }
     /*如果所有内容都分析完毕也没有遇到"\r"字符，则返回LINE_OPEN，表示还需要继续读取客户数据才能进一步分析*/
    return LINE_OPEN;
}

/* 对内存映射区执行munmap操作 */
void UserEvent::unmap()
{
    if (m_file_address)
    {
        munmap(m_file_address, m_file_stat.st_size);
        m_file_address = 0;
    }
}

/* 往写缓冲中写入待发送的数据 */
bool UserEvent::add_response(const char *format, ...)
{
    if (m_write_idx >= WRITE_BUFFER_SIZE)
    {
        return false;
    }
    va_list arg_list;
    va_start(arg_list, format);
    int len = vsnprintf(m_write_buf + m_write_idx, WRITE_BUFFER_SIZE - 1 - m_write_idx, format, arg_list);
    if (len >= (WRITE_BUFFER_SIZE - 1 -m_write_idx))/*这是判断是否缓存区写满，写满则返回*/
    {
        return false;
    }
    m_write_idx += len;
    va_end(arg_list);
    return true;
}

bool UserEvent::add_status_line(int status, const char *title)
{
    return add_response("%s %d %s\r\n", "HTTP/1.1", status, title);
}

bool UserEvent::add_headers(int content_len,const char *filetype)
{
    if(filetype)
        add_content_type(filetype);
    add_content_length(content_len);
    add_linger();
    add_blank_line();
    return true;
}

bool UserEvent::add_content_type(const char *filetype)
{
    return add_response("Content-Type: %s\r\n", filetype);

}

bool UserEvent::add_content_length(int content_len)
{
    return add_response("Content-Length: %d\r\n", content_len);
}

bool UserEvent::add_linger()
{
    return add_response("Connection: %s\r\n", (m_linger == true) ? "keep-alive" : "close");
}

bool UserEvent::add_blank_line()
{
    return add_response("%s", "\r\n");
}

bool UserEvent::add_content(const char *content)
{
    return add_response("%s", content);
}


void readData(UserEvent *Uev, ITimerContainer<UserEvent> *htc,threadpool <UserEvent> *pool)
{
     /* 根据读的结果，决定是将任务加到线程池，还是关闭连接 */
    if(Uev->read())
    {
         pool->append(Uev);         
    }
    else
    {
        Uev->close_conn();            //当关闭连接时,应该删除定时器,与时间关联起来
        htc->delTimer((Timer<UserEvent> *)Uev->timer);
        delete Uev;
    }

}

void writeData(UserEvent *Uev, ITimerContainer<UserEvent> *htc)
{
    /* 根据写的结果，决定是否关闭连接 */
    if (!Uev->write())
    {
        printf_DB("Uev->write Fasle\n");
        Uev->close_conn();            //当关闭连接时,应该删除定时器,与时间关联起来
        htc->delTimer((Timer<UserEvent> *)Uev->timer);
        delete Uev;
    }
    else
    {  
        printf_DB("resetTimer\n");
        // 重新设置定时器
        htc->resetTimer((Timer<UserEvent> *)Uev->timer, 15000);
    }
}