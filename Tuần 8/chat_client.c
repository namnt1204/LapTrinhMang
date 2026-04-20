#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <poll.h>

#define PORT 8080
#define BUFFER_SIZE 1024

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\nLỗi tạo Socket \n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        printf("\nĐịa chỉ không hợp lệ hoặc không được hỗ trợ \n");
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nKết nối tới Server thất bại \n");
        return -1;
    }

    struct pollfd fds[2];
    fds[0].fd = STDIN_FILENO; // File descriptor của bàn phím
    fds[0].events = POLLIN;
    fds[1].fd = sock;         // File descriptor của socket
    fds[1].events = POLLIN;

    while (1) {
        if (poll(fds, 2, -1) < 0) {
            perror("Lỗi poll");
            break;
        }

        // Nếu người dùng nhập từ bàn phím
        if (fds[0].revents & POLLIN) {
            if (fgets(buffer, BUFFER_SIZE, stdin) != NULL) {
                send(sock, buffer, strlen(buffer), 0);
            }
        }

        // Nếu có tin nhắn gửi về từ Server
        if (fds[1].revents & POLLIN) {
            memset(buffer, 0, BUFFER_SIZE);
            int valread = recv(sock, buffer, BUFFER_SIZE, 0);
            if (valread <= 0) {
                printf("\nServer đã ngắt kết nối.\n");
                break;
            }
            printf("%s", buffer);
        }
    }
    close(sock);
    return 0;
}