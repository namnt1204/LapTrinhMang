#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <poll.h>

#define MAX_CLIENTS 100
#define BUFFER_SIZE 1024
#define PORT 9090

struct ClientTelnet {
    int fd;
    int logged_in;
};

// Hàm kiểm tra đăng nhập từ file cơ sở dữ liệu
int check_login(const char *user_pass) {
    FILE *file = fopen("database.txt", "r");
    if (!file) {
        perror("Khong the mo file database.txt");
        return 0;
    }

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\r\n")] = 0; // Xóa ký tự xuống dòng
        if (strcmp(line, user_pass) == 0) {
            fclose(file);
            return 1;
        }
    }
    fclose(file);
    return 0;
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    struct pollfd fds[MAX_CLIENTS];
    struct ClientTelnet clients[MAX_CLIENTS];
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

    printf("Telnet Server dang chay tren cong %d...\n", PORT);

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
                    printf("Client FD %d ket noi.\n", new_socket);
                    
                    fds[nfds].fd = new_socket;
                    fds[nfds].events = POLLIN;
                    clients[nfds].fd = new_socket;
                    clients[nfds].logged_in = 0;
                    nfds++;

                    char *msg = "=== TELNET SERVER ===\nVui long dang nhap (cu phap: user pass):\n> ";
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

                        // Nếu rỗng thì bỏ qua
                        if (strlen(buffer) == 0) continue; 

                        if (!clients[i].logged_in) {
                            // Xử lý đăng nhập
                            if (check_login(buffer)) {
                                clients[i].logged_in = 1;
                                char *succ_msg = "Dang nhap thanh cong!\nNhap lenh:\n> ";
                                send(fds[i].fd, succ_msg, strlen(succ_msg), 0);
                                printf("Client FD %d dang nhap thanh cong.\n", fds[i].fd);
                            } else {
                                char *err_msg = "Sai tai khoan hoac mat khau. Thu lai:\n> ";
                                send(fds[i].fd, err_msg, strlen(err_msg), 0);
                            }
                        } else {
                            // Xử lý thực thi lệnh
                            printf("Client FD %d yeu cau lenh: %s\n", fds[i].fd, buffer);
                            
                            char cmd[BUFFER_SIZE + 30];
                            // Thêm 2>&1 để bắt cả thông báo lỗi nếu client gõ sai lệnh
                            snprintf(cmd, sizeof(cmd), "%s > out.txt 2>&1", buffer);
                            system(cmd);

                            // Đọc file out.txt và gửi lại kết quả
                            FILE *f = fopen("out.txt", "r");
                            if (f) {
                                char file_buf[BUFFER_SIZE];
                                size_t bytes_read;
                                int has_output = 0;
                                
                                send(fds[i].fd, "\n--- KET QUA ---\n", 17, 0);
                                while ((bytes_read = fread(file_buf, 1, sizeof(file_buf) - 1, f)) > 0) {
                                    file_buf[bytes_read] = '\0';
                                    send(fds[i].fd, file_buf, bytes_read, 0);
                                    has_output = 1;
                                }
                                fclose(f);
                                
                                if (!has_output) {
                                    char *empty_msg = "(Lenh khong tra ve ket qua hien thi)\n";
                                    send(fds[i].fd, empty_msg, strlen(empty_msg), 0);
                                }
                                
                                send(fds[i].fd, "\n> ", 3, 0); // Dấu nhắc lệnh mới
                            } else {
                                char *err = "Loi: Khong the doc ket qua thuc thi.\n> ";
                                send(fds[i].fd, err, strlen(err), 0);
                            }
                        }
                    }
                }
            }
        }
    }
    return 0;
}