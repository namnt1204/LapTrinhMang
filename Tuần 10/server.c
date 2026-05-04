#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>

#define PORT 9000
#define MAX_CLIENTS 100
#define MAX_SUBS 1000
#define BUFFER_SIZE 1024

// Cấu trúc lưu trữ việc đăng ký chủ đề
struct Subscription {
    int client_fd;
    char topic[64];
};

struct Subscription subs[MAX_SUBS];
int sub_count = 0;

// Hàm thêm đăng ký (SUB)
void add_subscription(int fd, const char* topic) {
    // Kiểm tra xem đã đăng ký chưa
    for (int i = 0; i < sub_count; i++) {
        if (subs[i].client_fd == fd && strcmp(subs[i].topic, topic) == 0) return;
    }
    // Thêm mới
    if (sub_count < MAX_SUBS) {
        subs[sub_count].client_fd = fd;
        strncpy(subs[sub_count].topic, topic, 63);
        subs[sub_count].topic[63] = '\0';
        sub_count++;
    }
}

// Hàm hủy đăng ký (UNSUB)
void remove_subscription(int fd, const char* topic) {
    for (int i = 0; i < sub_count; i++) {
        if (subs[i].client_fd == fd && strcmp(subs[i].topic, topic) == 0) {
            // Xóa bằng cách ghi đè phần tử cuối lên phần tử hiện tại
            subs[i] = subs[sub_count - 1];
            sub_count--;
            return;
        }
    }
}

// Hàm dọn dẹp khi client ngắt kết nối
void remove_all_subscriptions(int fd) {
    int i = 0;
    while (i < sub_count) {
        if (subs[i].client_fd == fd) {
            subs[i] = subs[sub_count - 1];
            sub_count--;
        } else {
            i++;
        }
    }
}

int main() {
    int master_socket, addrlen, new_socket, client_socket[MAX_CLIENTS], max_clients = MAX_CLIENTS;
    int activity, i, valread, sd, max_sd;
    struct sockaddr_in address;
    char buffer[BUFFER_SIZE];
    fd_set readfds; // Tập hợp các file descriptor cho select()

    // Khởi tạo mảng client_socket = 0
    for (i = 0; i < max_clients; i++) {
        client_socket[i] = 0;
    }

    // 1. Tạo master socket
    if ((master_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    // 2. Cấu hình để tái sử dụng port (tránh lỗi "Address already in use")
    int opt = 1;
    if (setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // 3. Bind socket vào cổng 9000
    if (bind(master_socket, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // 4. Lắng nghe kết nối
    if (listen(master_socket, 5) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    addrlen = sizeof(address);
    printf("[*] Server dang lang nghe tren cong %d...\n", PORT);

    // 5. Vòng lặp chính sử dụng select()
    while (1) {
        FD_ZERO(&readfds);
        FD_SET(master_socket, &readfds);
        max_sd = master_socket;

        // Thêm các socket client hiện tại vào tập readfds
        for (i = 0; i < max_clients; i++) {
            sd = client_socket[i];
            if (sd > 0) FD_SET(sd, &readfds);
            if (sd > max_sd) max_sd = sd;
        }

        // Đợi một hoạt động trên một trong các socket
        activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
        if (activity < 0) {
            perror("Select error");
            continue;
        }

        // Xử lý kết nối mới
        if (FD_ISSET(master_socket, &readfds)) {
            if ((new_socket = accept(master_socket, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
                perror("Accept error");
                exit(EXIT_FAILURE);
            }
            printf("[+] Client moi ket noi: fd %d, IP %s, port %d\n", new_socket, inet_ntoa(address.sin_addr), ntohs(address.sin_port));

            // Thêm socket mới vào mảng
            for (i = 0; i < max_clients; i++) {
                if (client_socket[i] == 0) {
                    client_socket[i] = new_socket;
                    break;
                }
            }
        }

        // Xử lý dữ liệu từ client cũ
        for (i = 0; i < max_clients; i++) {
            sd = client_socket[i];
            if (FD_ISSET(sd, &readfds)) {
                memset(buffer, 0, BUFFER_SIZE);
                valread = read(sd, buffer, BUFFER_SIZE - 1);
                
                // Client ngắt kết nối
                if (valread == 0) {
                    getpeername(sd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
                    printf("[-] Client ngat ket noi: IP %s, port %d\n", inet_ntoa(address.sin_addr), ntohs(address.sin_port));
                    close(sd);
                    client_socket[i] = 0;
                    remove_all_subscriptions(sd);
                } 
                // Xử lý lệnh từ client
                else {
                    buffer[valread] = '\0';
                    char cmd[10] = {0}, topic[64] = {0}, msg[800] = {0};
                    
                    // Phân tách chuỗi (parse command)
                    int parsed = sscanf(buffer, "%9s %63s %[^\n]", cmd, topic, msg);
                    
                    if (strcmp(cmd, "SUB") == 0 && parsed >= 2) {
                        add_subscription(sd, topic);
                        char reply[100];
                        snprintf(reply, sizeof(reply), "-> [SERVER] Da dang ky thanh cong: %s\n", topic);
                        send(sd, reply, strlen(reply), 0);
                        printf("[*] Client %d SUB: %s\n", sd, topic);
                    } 
                    else if (strcmp(cmd, "UNSUB") == 0 && parsed >= 2) {
                        remove_subscription(sd, topic);
                        char reply[100];
                        snprintf(reply, sizeof(reply), "-> [SERVER] Da huy dang ky: %s\n", topic);
                        send(sd, reply, strlen(reply), 0);
                        printf("[*] Client %d UNSUB: %s\n", sd, topic);
                    } 
                    else if (strcmp(cmd, "PUB") == 0 && parsed >= 3) {
                        int count = 0;
                        char fwd_msg[1024];
                        snprintf(fwd_msg, sizeof(fwd_msg), "\n[TIN NHAN TU %s]: %s\n> ", topic, msg);
                        
                        // Duyệt qua mảng đăng ký để gửi tin
                        for (int j = 0; j < sub_count; j++) {
                            if (strcmp(subs[j].topic, topic) == 0) {
                                send(subs[j].client_fd, fwd_msg, strlen(fwd_msg), 0);
                                count++;
                            }
                        }
                        
                        char reply[100];
                        snprintf(reply, sizeof(reply), "-> [SERVER] Da gui tin nhan toi %d nguoi dang ky.\n", count);
                        send(sd, reply, strlen(reply), 0);
                        printf("[*] Client %d PUB vao %s: %s\n", sd, topic, msg);
                    } 
                    else {
                        char *reply = "-> [SERVER] Sai cu phap. Dung: SUB <topic> | UNSUB <topic> | PUB <topic> <msg>\n";
                        send(sd, reply, strlen(reply), 0);
                    }
                }
            }
        }
    }
    return 0;
}