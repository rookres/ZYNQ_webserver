#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>

#define PORT 8080
#define MAX_REQUEST_SIZE 1024

void handle_request(int client_socket) {
    char request[MAX_REQUEST_SIZE];
    ssize_t request_size = recv(client_socket, request, sizeof(request), 0);
    if (request_size < 0) {
        perror("Error reading request");
        return;
    }

    // 获取当前系统时间
    time_t current_time;
    time(&current_time);
    struct tm* time_info = localtime(&current_time);
    char time_str[80];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", time_info);

    // 构造 HTTP 响应
    char response[1024];
    snprintf(response, sizeof(response),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: text/html\r\n"
             "\r\n"
             "<html>"
             "<head><title>HTTP Response</title></head>"
             "<body>"
             "<h1>Current Time: %s</h1>"
             "</body>"
             "</html>",
             time_str);

    // 发送 HTTP 响应
    ssize_t response_size = send(client_socket, response, strlen(response), 0);
    if (response_size < 0) {
        perror("Error sending response");
    }

    // 关闭客户端连接
    close(client_socket);
}

int main() {
//   while(1)  
// {    // 获取当前系统时间
    time_t current_time;
    time(&current_time);
    struct tm* time_info = localtime(&current_time);
    char time_str[80];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", time_info);

    // 构造 HTTP 响应
    char response[1024];
    snprintf(response, sizeof(response),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "\r\n"
        "<html>"
        "<head>"
        "<title>HTTP Response</title>"
        "<style>"
        "body {"
        "   display: flex;"
        "   justify-content: center;"
        "   align-items: center;"
        "   height: 100vh;"
        "   background-color: #f1f1f1;"
        "}"
        "h1 {"
        "   color: #ff0000;"
        "}"
        "</style>"
        "</head>"
        "<body>"
        "<h1 id='content'>Current Time: %s</h1>"
        "</body>"
        "</html>"

,
             time_str);

    // 发送 HTTP 响应
    ssize_t response_size = send(STDOUT_FILENO, response, strlen(response), 0);
    if (response_size < 0) {
        perror("Error sending response");
    }
//     sleep(1);
// }

    return 0;
}
