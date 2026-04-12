/*******************************************************************************
 * @file    chat_client.c
 * @brief   Client kết nối đến Chat Server
 *******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/select.h>

int main() {
    int client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (client == -1) {
        perror("socket() failed");
        return 1;
    }
    
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1"); // Kết nối localhost
    addr.sin_port = htons(8080);
    
    if (connect(client, (struct sockaddr *)&addr, sizeof(addr))) {
        perror("connect() failed");
        close(client);
        return 1;
    }
    
    printf("Da ket noi den server tren cong 8080...\n");
    
    fd_set fdread;
    struct timeval tv;
    char buf[512];

    while (1) {
        FD_ZERO(&fdread);
        FD_SET(STDIN_FILENO, &fdread); // Theo dõi bàn phím
        FD_SET(client, &fdread);       // Theo dõi socket nhận từ server

        tv.tv_sec = 5;
        tv.tv_usec = 0;

        int ret = select(client + 1, &fdread, NULL, NULL, &tv);
        if (ret < 0) {
            perror("select() failed");
            break;
        }

        if (ret == 0) {
            continue; // Hết timeout thì lặp lại, không làm gì cả
        }
        
        // Sự kiện: Người dùng gõ phím
        if (FD_ISSET(STDIN_FILENO, &fdread)) {
            fgets(buf, sizeof(buf), stdin);
            // Xóa ký tự \n ở cuối chuỗi do fgets sinh ra
            buf[strcspn(buf, "\n")] = 0;
            send(client, buf, strlen(buf), 0);
        }

        // Sự kiện: Server gửi tin nhắn xuống
        if (FD_ISSET(client, &fdread)) {
            ret = recv(client, buf, sizeof(buf) - 1, 0);
            if (ret <= 0) {
                printf("Da ngat ket noi khoi server.\n");
                break;
            }
            buf[ret] = 0; // Chặn đuôi chuỗi
            printf("%s\n", buf);
        }
    }

    close(client);
    return 0;
}