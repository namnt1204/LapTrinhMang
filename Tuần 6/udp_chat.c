#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>

int main(int argc, char *argv[]) {
    // Kiểm tra 3 tham số dòng lệnh theo yêu cầu
    if (argc != 4) {
        printf("Cach su dung: ./udp_chat port_s ip_d port_d\n");
        return 1;
    }

    int port_s = atoi(argv[1]);
    char *ip_d = argv[2];
    int port_d = atoi(argv[3]);

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in server_addr, dest_addr;

    // Cấu hình cổng nhận (port_s)
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port_s);
    bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));

    // Cấu hình đích đến (ip_d, port_d)
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_addr.s_addr = inet_addr(ip_d);
    dest_addr.sin_port = htons(port_d);

    fd_set readfds;
    char buffer[1024];

    printf("UDP Chat khoi tao tai cong %d.\nSan sang gui den %s:%d\n\n", port_s, ip_d, port_d);

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds); // Theo dõi bàn phím
        FD_SET(sockfd, &readfds);       // Theo dõi socket nhận UDP
        
        int max_fd = sockfd > STDIN_FILENO ? sockfd : STDIN_FILENO;

        select(max_fd + 1, &readfds, NULL, NULL, NULL);

        // Nếu người dùng nhập từ bàn phím
        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            fgets(buffer, sizeof(buffer), stdin);
            sendto(sockfd, buffer, strlen(buffer), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        }

        // Nếu có tin nhắn gửi tới
        if (FD_ISSET(sockfd, &readfds)) {
            int n = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0, NULL, NULL);
            buffer[n] = '\0';
            printf(">> Nhan: %s", buffer);
        }
    }
    
    close(sockfd);
    return 0;
}