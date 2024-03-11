#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

int main() {

        // 构造 HTTP 响应
    const char* response = "HTTP/1.1 200 OK\r\n"
                           "Content-Type: text/html\r\n"
                           "\r\n"
                           "<html>"
                           "<head><title>HTTP Response</title></head>"
                           "<body>"
                           "<h1>Hello, World!</h1>"
                           "<img src=\"suya.jpg\">"
                           "</body>"
                           "</html>";
        // 发送 HTTP 响应
       ssize_t response_size = send(STDOUT_FILENO, response, strlen(response), 0);
        if (response_size < 0) {

        }

    return 0;
}
