#include <stdio.h>
#include <stdlib.h>

int main(void) {
    // 设置HTTP头部内容类型为HTML
   printf("HTTP/1.1 200 OK\r\n");
   printf("Content-Type: text/html\r\n");
    printf("\r\n");
    // 发送HTML响应正文
    printf("<html>\n");
    printf("<head><title>Hello World CGI Program</title></head>\n");
    printf("<body>\n");
    printf("<h1>Hello, World! This is a simple CGI program.</h1>\n");
    printf("</body>\n");
    printf("</html>\n");

    // 返回成功状态
    return EXIT_SUCCESS;
}
