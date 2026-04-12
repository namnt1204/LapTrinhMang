/*******************************************************************************
 * @file    telnet_client.c
 * @brief   Client kết nối đến Telnet Server để thực thi lệnh từ xa
 *******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/select.h>

#define SERVER_PORT 9000
#define SERVER_IP "127.0.0.1"

int main() {
    int client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (client == -1) {
        perror("socket() failed");
        return 1;
    }
    
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    addr.sin_port = htons(SERVER_PORT);
    
    if (connect(client, (struct sockaddr *)&addr, sizeof(addr))) {
        perror("connect() failed");
        close(client);
        return 1;
    }
    
    printf("Da ket noi den Telnet Server tai %s:%d...\n", SERVER_IP, SERVER_PORT);
    
    fd_set fdread;
    struct timeval tv;
    char buf[1024]; // Tăng kích thước buffer để nhận kết quả lệnh dài

    while (1) {
        FD_ZERO(&fdread);
        FD_SET(STDIN_FILENO, &fdread); 
        FD_SET(client, &fdread);       

        // Thiết lập timeout
        tv.tv_sec = 5;
        tv.tv_usec = 0;

        int ret = select(client + 1, &fdread, NULL, NULL, &tv);
        if (ret < 0) {
            perror("select() failed");
            break;
        }

        if (ret == 0) {
            continue; 
        }
        
        // Sự kiện: Người dùng nhập lệnh từ bàn phím
        if (FD_ISSET(STDIN_FILENO, &fdread)) {
            if (fgets(buf, sizeof(buf), stdin) != NULL) {
                // Xóa ký tự \n ở cuối chuỗi
                buf[strcspn(buf, "\n")] = 0;
                send(client, buf, strlen(buf), 0);
            }
        }

        // Sự kiện: Server trả về thông báo hoặc kết quả của lệnh
        if (FD_ISSET(client, &fdread)) {
            ret = recv(client, buf, sizeof(buf) - 1, 0);
            if (ret <= 0) {
                printf("\nMat ket noi voi Telnet Server.\n");
                break;
            }
            buf[ret] = 0;
            
            // In kết quả trực tiếp ra màn hình mà không cần xuống dòng dư thừa
            // vì bản thân kết quả lệnh hệ thống đã có sẵn \n
            printf("%s", buf);
            fflush(stdout); // Ép xuất ra màn hình ngay lập tức (quan trọng cho dấu nhắc lệnh "> ")
        }
    }

    close(client);
    return 0;
}