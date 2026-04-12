/*******************************************************************************
 * @file    chat_server.c
 * @brief   Server quản lý nhiều Client sử dụng I/O Multiplexing (select)
 *******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/select.h>
#include <time.h>

#define MAX_CLIENTS 100
#define PORT 8080

// Cấu trúc quản lý trạng thái từng client
struct Client {
    int fd;                 // Socket FD
    int is_logged_in;       // 0: Chưa đăng nhập, 1: Đã đăng nhập đúng cú pháp
    char id[32];            // client_id
    char name[64];          // client_name
};

int main() {
    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == -1) {
        perror("socket() failed");
        return 1;
    }

    // Tùy chọn chống lỗi "Address already in use" khi khởi động lại server
    int opt = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(PORT);

    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr))) {
        perror("bind() failed");
        return 1;
    }

    if (listen(listener, 5)) {
        perror("listen() failed");
        return 1;
    }

    printf("Server dang cho ket noi tai cong %d...\n", PORT);

    // Khởi tạo danh sách Client trống
    struct Client clients[MAX_CLIENTS];
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].fd = 0;
        clients[i].is_logged_in = 0;
    }

    fd_set fdread;
    char buf[256];

    while (1) {
        // Cài đặt lại tập fdread trước mỗi lần select()
        FD_ZERO(&fdread);
        FD_SET(listener, &fdread);
        int max_fd = listener;

        // Đưa các client đang kết nối vào tập theo dõi
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].fd > 0) {
                FD_SET(clients[i].fd, &fdread);
                if (clients[i].fd > max_fd) {
                    max_fd = clients[i].fd;
                }
            }
        }

        // Chờ sự kiện
        int ret = select(max_fd + 1, &fdread, NULL, NULL, NULL);
        if (ret < 0) {
            perror("select() failed");
            break;
        }

        // 1. Sự kiện có Client mới kết nối
        if (FD_ISSET(listener, &fdread)) {
            int new_client = accept(listener, NULL, NULL);
            if (new_client >= 0) {
                printf("=> Co client moi ket noi (FD: %d)\n", new_client);

                // Tìm vị trí trống trong mảng
                int i;
                for (i = 0; i < MAX_CLIENTS; i++) {
                    if (clients[i].fd == 0) {
                        clients[i].fd = new_client;
                        clients[i].is_logged_in = 0;
                        break;
                    }
                }

                if (i == MAX_CLIENTS) {
                    close(new_client); // Đầy
                } else {
                    char *msg = "Vui long nhap ten (client_id: client_name): ";
                    send(new_client, msg, strlen(msg), 0);
                }
            }
        }

        // 2. Sự kiện có dữ liệu từ các Client đã kết nối
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].fd > 0 && FD_ISSET(clients[i].fd, &fdread)) {
                ret = recv(clients[i].fd, buf, sizeof(buf) - 1, 0);
                
                // Ngắt kết nối
                if (ret <= 0) {
                    printf("=> Client %d da ngat ket noi.\n", clients[i].fd);
                    close(clients[i].fd);
                    clients[i].fd = 0;
                    continue;
                }

                buf[ret] = 0;
                // Xóa ký tự xuống dòng ở cuối nếu có
                buf[strcspn(buf, "\r\n")] = 0;

                if (strlen(buf) == 0) continue; 

                // Xử lý logic đăng nhập
                if (clients[i].is_logged_in == 0) {
                    char temp_id[32];
                    char temp_name[64];
                    
                    // Kiểm tra chuỗi theo định dạng "id: name" (name không chứa dấu cách)
                    if (sscanf(buf, "%31[^:]: %63s", temp_id, temp_name) == 2) {
                        strcpy(clients[i].id, temp_id);
                        strcpy(clients[i].name, temp_name);
                        clients[i].is_logged_in = 1;
                        
                        char *msg = "Dang nhap thanh cong. Ban co the bat dau chat!\n";
                        send(clients[i].fd, msg, strlen(msg), 0);
                        printf("=> Client %d dang nhap thanh cong voi id: %s\n", clients[i].fd, clients[i].id);
                    } else {
                        char *msg = "Sai cu phap. Vui long nhap lai (client_id: client_name): ";
                        send(clients[i].fd, msg, strlen(msg), 0);
                    }
                } 
                // Xử lý logic chat (Broadcast)
                else {
                    time_t t = time(NULL);
                    struct tm *tm_info = localtime(&t);
                    char time_str[64];
                    // Format thời gian: YYYY/MM/DD HH:MM:SSPM
                    strftime(time_str, sizeof(time_str), "%Y/%m/%d %I:%M:%S%p", tm_info);

                    char send_buf[512];
                    // Nối chuỗi thời gian, id và nội dung chat
                    snprintf(send_buf, sizeof(send_buf), "%s %s: %s", time_str, clients[i].id, buf);

                    // Gửi tin nhắn cho toàn bộ client khác (đã đăng nhập)
                    for (int j = 0; j < MAX_CLIENTS; j++) {
                        if (clients[j].fd > 0 && clients[j].is_logged_in == 1 && j != i) {
                            send(clients[j].fd, send_buf, strlen(send_buf), 0);
                        }
                    }
                    // In ra màn hình console của Server để dễ theo dõi
                    printf("Broadcast: %s\n", send_buf);
                }
            }
        }
    }

    close(listener);
    return 0;
}