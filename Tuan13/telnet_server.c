#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT 8082
#define BUFFER_SIZE 1024

void *client_handler(void *arg) {
    int client_socket = *(int *)arg;
    free(arg);
    char buffer[BUFFER_SIZE];
    int authenticated = 0;

    char *welcome = "Chào mừng đến với Telnet Server. Vui lòng đăng nhập theo cú pháp 'username password'\r\n";
    send(client_socket, welcome, strlen(welcome), 0);

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_received <= 0) break;

        // Xóa ký tự xuống dòng \r\n
        buffer[strcspn(buffer, "\r\n")] = 0;

        if (!authenticated) {
            // Kiểm tra thông tin đăng nhập (Cố định: admin admin123)
            if (strcmp(buffer, "admin admin123") == 0) {
                authenticated = 1;
                char *auth_ok = "Đăng nhập thành công! Hãy nhập lệnh hệ thống (gõ 'exit' để thoát):\r\n";
                send(client_socket, auth_ok, strlen(auth_ok), 0);
            } else {
                char *auth_fail = "Sai tài khoản hoặc mật khẩu. Vui lòng thử lại:\r\n";
                send(client_socket, auth_fail, strlen(auth_fail), 0);
            }
        } else {
            // Khi đã đăng nhập thành công
            if (strcmp(buffer, "exit") == 0) {
                char *bye = "Tạm biệt!\r\n";
                send(client_socket, bye, strlen(bye), 0);
                break;
            }

            // Thực thi lệnh hệ thống bằng popen
            FILE *fp = popen(buffer, "r");
            if (fp == NULL) {
                char *exec_err = "Lỗi không thể thực thi lệnh.\r\n";
                send(client_socket, exec_err, strlen(exec_err), 0);
                continue;
            }

            char result_buf[BUFFER_SIZE];
            int has_output = 0;
            while (fgets(result_buf, sizeof(result_buf), fp) != NULL) {
                send(client_socket, result_buf, strlen(result_buf), 0);
                has_output = 1;
            }
            pclose(fp);

            if (!has_output) {
                char *no_output = "[Lệnh thực thi không trả về kết quả]\r\n";
                send(client_socket, no_output, strlen(no_output), 0);
            }
            // Gửi ký tự nhắc lệnh mới cho client
            send(client_socket, "\n$ ", 3, 0);
        }
    }

    close(client_socket);
    pthread_exit(NULL);
}

int main() {
    int server_fd, *new_sock;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    listen(server_fd, 10);
    printf("Telnet Server đang chạy trên port %d...\n", PORT);

    while (1) {
        int client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        if (client_socket < 0) continue;

        printf("Client kết nối thành công.\n");
        pthread_t thread_id;
        new_sock = malloc(sizeof(int));
        *new_sock = client_socket;

        pthread_create(&thread_id, NULL, client_handler, (void *)new_sock);
        pthread_detach(thread_id);
    }
    return 0;
}