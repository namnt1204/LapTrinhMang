/*******************************************************************************
 * @file    telnet_server.c
 * @brief   Telnet Server thực thi lệnh hệ thống dùng I/O Multiplexing
 *******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/select.h>

#define MAX_CLIENTS 100
#define PORT 9000 // Chạy port khác với Chat Server để không bị đụng độ

struct Client {
    int fd;
    int is_logged_in; // 0: Chưa đăng nhập, 1: Đã đăng nhập
};

// Hàm kiểm tra tài khoản từ file database.txt
int check_login(char *credentials) {
    FILE *f = fopen("database.txt", "r");
    if (f == NULL) {
        printf("Khong tim thay file database.txt!\n");
        return 0; // Coi như đăng nhập thất bại nếu mất file
    }

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        // Xóa ký tự xuống dòng ở cuối chuỗi đọc từ file
        line[strcspn(line, "\r\n")] = 0;
        
        // So sánh trực tiếp chuỗi client gửi với dòng trong file
        if (strcmp(credentials, line) == 0) {
            fclose(f);
            return 1; // Đúng tài khoản
        }
    }

    fclose(f);
    return 0; // Sai tài khoản
}

int main() {
    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == -1) {
        perror("socket() failed");
        return 1;
    }

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

    printf("Telnet Server dang cho ket noi tai cong %d...\n", PORT);

    struct Client clients[MAX_CLIENTS];
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].fd = 0;
        clients[i].is_logged_in = 0;
    }

    fd_set fdread;
    char buf[256];

    while (1) {
        FD_ZERO(&fdread);
        FD_SET(listener, &fdread);
        int max_fd = listener;

        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].fd > 0) {
                FD_SET(clients[i].fd, &fdread);
                if (clients[i].fd > max_fd) {
                    max_fd = clients[i].fd;
                }
            }
        }

        int ret = select(max_fd + 1, &fdread, NULL, NULL, NULL);
        if (ret < 0) {
            perror("select() failed");
            break;
        }

        // Sự kiện: Client mới kết nối
        if (FD_ISSET(listener, &fdread)) {
            int new_client = accept(listener, NULL, NULL);
            if (new_client >= 0) {
                printf("=> Co client moi ket noi (FD: %d)\n", new_client);
                int i;
                for (i = 0; i < MAX_CLIENTS; i++) {
                    if (clients[i].fd == 0) {
                        clients[i].fd = new_client;
                        clients[i].is_logged_in = 0;
                        break;
                    }
                }
                if (i == MAX_CLIENTS) {
                    close(new_client);
                } else {
                    char *msg = "Vui long dang nhap (user pass): ";
                    send(new_client, msg, strlen(msg), 0);
                }
            }
        }

        // Sự kiện: Client gửi dữ liệu (tài khoản hoặc lệnh)
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].fd > 0 && FD_ISSET(clients[i].fd, &fdread)) {
                ret = recv(clients[i].fd, buf, sizeof(buf) - 1, 0);
                
                if (ret <= 0) {
                    printf("=> Client %d da ngat ket noi.\n", clients[i].fd);
                    close(clients[i].fd);
                    clients[i].fd = 0;
                    continue;
                }

                buf[ret] = 0;
                buf[strcspn(buf, "\r\n")] = 0; // Cắt bỏ khoảng trắng dư thừa
                if (strlen(buf) == 0) continue;

                // Trạng thái 1: Chưa đăng nhập
                if (clients[i].is_logged_in == 0) {
                    if (check_login(buf)) {
                        clients[i].is_logged_in = 1;
                        char *msg = "Dang nhap thanh cong! Ban co the nhap lenh:\n";
                        send(clients[i].fd, msg, strlen(msg), 0);
                        printf("Client %d dang nhap thanh cong.\n", clients[i].fd);
                    } else {
                        char *msg = "Loi dang nhap. Vui long nhap lai (user pass): ";
                        send(clients[i].fd, msg, strlen(msg), 0);
                    }
                } 
                // Trạng thái 2: Đã đăng nhập -> Xử lý lệnh
                else {
                    printf("Client %d yeu cau thuc thi: %s\n", clients[i].fd, buf);
                    
                    char sys_cmd[512];
                    // Nối lệnh client gửi với "> out.txt" để đẩy kết quả vào file
                    // Thêm 2>&1 để bắt cả lỗi (stderr) vào file out.txt nếu client gõ sai lệnh
                    snprintf(sys_cmd, sizeof(sys_cmd), "%s > out.txt 2>&1", buf);
                    
                    // Thực thi lệnh
                    system(sys_cmd);

                    // Đọc nội dung file out.txt và gửi lại cho client
                    FILE *fout = fopen("out.txt", "r");
                    if (fout == NULL) {
                        char *msg = "Loi: Khong the doc file ket qua.\n";
                        send(clients[i].fd, msg, strlen(msg), 0);
                    } else {
                        char file_buf[1024];
                        int bytes_read;
                        // Đọc theo từng khối dữ liệu lớn gửi về cho an toàn
                        while ((bytes_read = fread(file_buf, 1, sizeof(file_buf) - 1, fout)) > 0) {
                            file_buf[bytes_read] = 0;
                            send(clients[i].fd, file_buf, strlen(file_buf), 0);
                        }
                        fclose(fout);
                    }
                    
                    // Gửi thêm dấu nhắc lệnh để báo hiệu đã thực thi xong
                    send(clients[i].fd, "\n> ", 3, 0);
                }
            }
        }
    }

    close(listener);
    return 0;
}