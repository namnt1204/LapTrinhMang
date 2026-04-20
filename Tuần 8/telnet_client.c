#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <poll.h>

#define PORT 9090
#define BUFFER_SIZE 1024

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\nLoi tao Socket \n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        printf("\nDia chi khong hop le hoac khong duoc ho tro \n");
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nKet noi toi Server that bai \n");
        return -1;
    }

    struct pollfd fds[2];
    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;
    fds[1].fd = sock;
    fds[1].events = POLLIN;

    while (1) {
        if (poll(fds, 2, -1) < 0) {
            perror("Loi poll");
            break;
        }

        // Nhận từ bàn phím và gửi đi
        if (fds[0].revents & POLLIN) {
            if (fgets(buffer, BUFFER_SIZE, stdin) != NULL) {
                send(sock, buffer, strlen(buffer), 0);
            }
        }

        // Nhận phản hồi từ server và in ra màn hình
        if (fds[1].revents & POLLIN) {
            memset(buffer, 0, BUFFER_SIZE);
            int valread = recv(sock, buffer, BUFFER_SIZE - 1, 0);
            if (valread <= 0) {
                printf("\nServer da ngat ket noi.\n");
                break;
            }
            printf("%s", buffer);
            fflush(stdout); // Ép in ra màn hình ngay lập tức (quan trọng cho dấu nhắc lệnh "> ")
        }
    }
    close(sock);
    return 0;
}