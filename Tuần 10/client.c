#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>

#define PORT 9000
#define SERVER_IP "127.0.0.1"
#define BUFFER_SIZE 1024

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE];
    fd_set readfds;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Khong the tao socket \n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        printf("\n Dia chi IP khong hop le \n");
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\n Ket noi that bai \n");
        return -1;
    }

    printf("[+] Da ket noi toi Server.\n");
    printf("Cac lenh: SUB <topic> | UNSUB <topic> | PUB <topic> <msg>\n");
    printf("----------------------------------------------------------\n> ");
    fflush(stdout);

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds); // Lắng nghe bàn phím
        FD_SET(sock, &readfds);         // Lắng nghe server

        // Select chờ dữ liệu
        int activity = select(sock + 1, &readfds, NULL, NULL, NULL);
        if (activity < 0) {
            perror("Select error");
            break;
        }

        // 1. Nếu có dữ liệu từ Server gửi về
        if (FD_ISSET(sock, &readfds)) {
            memset(buffer, 0, BUFFER_SIZE);
            int valread = read(sock, buffer, BUFFER_SIZE - 1);
            if (valread == 0) {
                printf("\n[!] Mat ket noi toi server.\n");
                break;
            }
            buffer[valread] = '\0';
            printf("%s", buffer);
            fflush(stdout); // In ngay lập tức
        }

        // 2. Nếu người dùng nhập từ bàn phím
        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            memset(buffer, 0, BUFFER_SIZE);
            if (fgets(buffer, BUFFER_SIZE, stdin) != NULL) {
                // Xóa ký tự newline ở cuối chuỗi
                buffer[strcspn(buffer, "\n")] = 0; 
                if (strlen(buffer) > 0) {
                    send(sock, buffer, strlen(buffer), 0);
                }
                printf("> "); // In lại dấu nhắc lệnh
                fflush(stdout);
            }
        }
    }

    close(sock);
    return 0;
}