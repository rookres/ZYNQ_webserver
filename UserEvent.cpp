#include "UserEvent.hpp"
#include <fcntl.h>
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
const char *doc_root = "/home/tronlong/test/webserver";


int UserEvent::m_user_count = 0;
int UserEvent::m_epollfd = -1;

int setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
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

void removefd(int epollfd, int fd)
{
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

void modfd(int epollfd, int fd, int ev,UserEvent *Uev=NULL)
{
    epoll_event event=Uev->event;
    Uev->fd=fd;
    // if(!Uev)Uev->fd=fd;
    // else event.data.fd = fd;
    
    event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

// int UserEvent::m_user_count = 0;
// int UserEvent::m_epollfd = -1;

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

    memset(m_read_buf, '\0', READ_BUFFER_SIZE);
    memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
    memset(m_real_file, '\0', FILENAME_LEN);
}


/* 循环读取客户数据，直到无数据可读或者对方关闭连接 */
bool UserEvent::read()          /*此函数在main函数里面调用*/
{
    printf("read 111\n");
    if (m_read_idx >= READ_BUFFER_SIZE)
        return false;

    int bytes_read = 0;
    while(true)
    {
         printf("bytes_read before\n");
        bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
        printf("bytes_read %d\n",bytes_read);
        if (bytes_read == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            printf("recv error:eason %s\n",strerror(errno));
            return false;
        }
        else if (bytes_read == 0)       /*此处应该返回true吧?答：不应返回true，这个对方已经关闭了,main函数里面是要读完后加入线程池的*/
        {
            printf("the client is closed\n");
            return false;
        }

        m_read_idx += bytes_read;
    }
    return true;
}

/* 写HTTP响应 */
bool UserEvent::write()       /*此函数在main函数里面调用, main函数会根据返回值决定是否关闭连接*/
{
    return true;/*暂时*/
    int temp = 0;
    int bytes_have_send = 0;
    int bytes_to_send = m_write_idx;

    if (bytes_to_send == 0)
    {
        modfd(m_epollfd, m_sockfd, EPOLLIN,this);
        init();
        return true;
    }

    while(true)
    {
        temp = writev(m_sockfd, m_iv, m_iv_count);
        if (temp <= -1)
        {
            /* 如果TCP写缓冲区没有空间，则等待下一轮EPOLLOUT事件。虽然在此期间，
             * 服务器无法立即接收到同一客户的下一个请求，但这可以保证连接的完整性 */
            if (errno == EAGAIN)
            {
                modfd(m_epollfd, m_sockfd, EPOLLOUT,this);
                return true;
            }

            unmap();
            return false;
        }

        bytes_to_send -= temp;
        bytes_have_send += temp;
        if (bytes_to_send <= bytes_have_send)
        {
            /* 发送HTTP响应成功，根据HTTP请求中的Connection字段决定是否理解关闭连接 */
            unmap();
            if (m_linger)
            {
                init();
                modfd(m_epollfd, m_sockfd, EPOLLIN,this);
                return true;
            }
            else
            {
                modfd(m_epollfd, m_sockfd, EPOLLIN,this);    /*main函数会根据返回值决定是否关闭连接*/
                return false;
            }
        }
    }
}

/* 由线程池中的工作线程调用，这是处理HTTP请求的入口函数 */
void UserEvent::process()
{
    HTTP_CODE read_ret = process_read();
    printf("read_ret=%s\n",read_ret==NO_REQUEST ? "NO_REQUEST":"-1");
    if (read_ret == NO_REQUEST)
    {
        modfd(m_epollfd, m_sockfd, EPOLLIN,this);
        return ;
    }

    bool write_ret = process_write(read_ret);
    if (!write_ret)
    {
        close_conn();
    }

     modfd(m_epollfd, m_sockfd, EPOLLOUT,this);
}

/* 主状态机,(看书第八章,也可观看my word文档的有限状态机分析http协议实例) */
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
        printf("got 1 http line: %s\n", text);

        switch (m_checked_state)
        {
            case CHECK_STATE_REQUESTLINE:            /*第一个状态，分析请求*/
            {
                ret = parse_request_line(text);
                if (ret == BAD_REQUEST)
                {
                    return BAD_REQUEST;
                }
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
                add_headers(m_file_stat.st_size);
                m_iv[0].iov_base = m_write_buf;
                m_iv[0].iov_len = m_write_idx;
                m_iv[1].iov_base = m_file_address;
                m_iv[1].iov_len = m_file_stat.st_size;
                m_iv_count = 2;
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
    if (strncasecmp(m_url, "http://", 7) == 0)  /*检查URL是否合法*/
    {
        m_url += 7;
        m_url = strchr(m_url, '/');
    }

    if (!m_url || m_url[0] != '/')
    {
        return BAD_REQUEST;
    }

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
    else
    {
        printf("oop! unkown header %s\n", text);
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
    strncpy(m_real_file + len, m_url, FILENAME_LEN - len - 1);
    if (stat(m_real_file, &m_file_stat) < 0)
    {
        return NO_RESOURCE;
    }

    if (!(m_file_stat.st_mode & S_IROTH))
    {
        return FORBIDDEN_REQUEST;
    }

    if (S_ISDIR(m_file_stat.st_mode))
    {

        return BAD_REQUEST;
    }

    int fd = open(m_real_file, O_RDONLY);
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

bool UserEvent::add_headers(int content_len)
{
    add_content_length(content_len);
    add_linger();
    add_blank_line();
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