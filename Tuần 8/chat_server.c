#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <poll.h>
#include <time.h>

#define MAX_CLIENTS 100
#define BUFFER_SIZE 1024
#define PORT 8080

struct Client {
    int fd;
    char name[128]; // Lưu cả cụm "client_id: client_name"
    int registered;
};

// Hàm lấy thời gian hiện tại
void get_current_time(char *time_str, size_t size) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(time_str, size, "%Y/%m/%d %I:%M:%S%p", t);
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    struct pollfd fds[MAX_CLIENTS];
    struct Client clients[MAX_CLIENTS];
    int nfds = 1;

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Lỗi tạo socket"); exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Lỗi bind"); exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("Lỗi listen"); exit(EXIT_FAILURE);
    }

    printf("Chat Server dang chay tren cong %d...\n", PORT);

    memset(fds, 0, sizeof(fds));
    fds[0].fd = server_fd;
    fds[0].events = POLLIN;

    while (1) {
        if (poll(fds, nfds, -1) < 0) {
            perror("Lỗi poll"); break;
        }

        for (int i = 0; i < nfds; i++) {
            if (fds[i].revents & POLLIN) {
                if (fds[i].fd == server_fd) {
                    // Chấp nhận kết nối mới
                    if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
                        continue;
                    }
                    printf("Client FD %d da ket noi.\n", new_socket);

                    fds[nfds].fd = new_socket;
                    fds[nfds].events = POLLIN;
                    clients[nfds].fd = new_socket;
                    clients[nfds].registered = 0;
                    nfds++;

                    char *msg = "Vui long nhap ten theo cu phap: client_id: client_name\n";
                    send(new_socket, msg, strlen(msg), 0);
                } else {
                    // Xử lý dữ liệu từ client
                    char buffer[BUFFER_SIZE];
                    memset(buffer, 0, BUFFER_SIZE);
                    int valread = recv(fds[i].fd, buffer, BUFFER_SIZE, 0);

                    if (valread <= 0) {
                        printf("Client FD %d ngat ket noi.\n", fds[i].fd);
                        close(fds[i].fd);
                        fds[i] = fds[nfds - 1];
                        clients[i] = clients[nfds - 1];
                        nfds--;
                        i--;
                    } else {
                        buffer[strcspn(buffer, "\r\n")] = 0; // Xóa ký tự xuống dòng

                        if (!clients[i].registered) {
                            // Kiểm tra cú pháp đăng ký
                            if (strchr(buffer, ':') != NULL) {
                                strncpy(clients[i].name, buffer, sizeof(clients[i].name) - 1);
                                clients[i].registered = 1;
                                char *success = "Dang ky thanh cong! Ban co the bat dau chat.\n";
                                send(fds[i].fd, success, strlen(success), 0);
                                printf("Client FD %d dang ky la: %s\n", fds[i].fd, clients[i].name);
                            } else {
                                char *err = "Sai cu phap. Vui long nhap lai (VD: 001: Alice)\n";
                                send(fds[i].fd, err, strlen(err), 0);
                            }
                        } else {
                            // Format tin nhắn và broadcast
                            char time_str[64];
                            get_current_time(time_str, sizeof(time_str));
                            
                            char broadcast_msg[BUFFER_SIZE + 200];
                            // Ví dụ: 2023/05/06 11:00:00PM abc: xin chao
                            snprintf(broadcast_msg, sizeof(broadcast_msg), "%s %s: %s\n", time_str, clients[i].name, buffer);

                            // Gửi cho tất cả client khác đã đăng ký
                            for (int j = 1; j < nfds; j++) {
                                if (j != i && clients[j].registered) {
                                    send(fds[j].fd, broadcast_msg, strlen(broadcast_msg), 0);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return 0;
}