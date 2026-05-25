#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>

#define PORT 8083
#define BUFFER_SIZE 1024

// Hàm helper dịch định dạng thô của người dùng sang chuẩn strftime
void convert_format(const char *src, char *dest, size_t dest_len) {
    size_t i = 0, j = 0;
    size_t src_len = strlen(src);
    
    memset(dest, 0, dest_len);

    while (i < src_len && j < dest_len - 1) {
        // Kiểm tra yyyy -> %Y (Năm 4 chữ số)
        if (i + 3 < src_len && strncmp(&src[i], "yyyy", 4) == 0) {
            if (j + 2 < dest_len) { strcat(dest, "%Y"); j += 2; }
            i += 4;
        }
        // Kiểm tra dd -> %d (Ngày)
        else if (i + 1 < src_len && strncmp(&src[i], "dd", 2) == 0) {
            if (j + 2 < dest_len) { strcat(dest, "%d"); j += 2; }
            i += 2;
        }
        // Kiểm tra ss -> %S (Giây)
        else if (i + 1 < src_len && strncmp(&src[i], "ss", 2) == 0) {
            if (j + 2 < dest_len) { strcat(dest, "%S"); j += 2; }
            i += 2;
        }
        // Kiểm tra hh -> %H (Giờ 24h)
        else if (i + 1 < src_len && strncmp(&src[i], "hh", 2) == 0) {
            if (j + 2 < dest_len) { strcat(dest, "%H"); j += 2; }
            i += 2;
        }
        // Kiểm tra mm -> %m (Tháng) hoặc %M (Phút)
        else if (i + 1 < src_len && strncmp(&src[i], "mm", 2) == 0) {
            // Quy ước đơn giản: Nếu trước đó có dấu ':' hoặc chữ 'hh' gần đó thì là phút, ngược lại là tháng.
            // Để tổng quát hóa theo đúng tư duy hệ thống: ta ánh xạ linh hoạt hoặc giữ nguyên logic tách.
            // Ở đây, nếu chuỗi chứa ':' trước vị trí này, ta coi là Phút (%M), ngược lại là Tháng (%m)
            int is_minute = 0;
            for(size_t k = (i > 5 ? i - 5 : 0); k < i; k++) {
                if(src[k] == ':' || src[k] == 'h') { is_minute = 1; break; }
            }
            
            if (is_minute) {
                if (j + 2 < dest_len) { strcat(dest, "%M"); j += 2; }
            } else {
                if (j + 2 < dest_len) { strcat(dest, "%m"); j += 2; }
            }
            i += 2;
        }
        // Các ký tự phân tách như / , : - hoặc khoảng trắng giữ nguyên
        else {
            dest[j++] = src[i++];
        }
    }
}

void *time_handler(void *arg) {
    int client_socket = *(int *)arg;
    free(arg);
    char buffer[BUFFER_SIZE];

    char *welcome = "Chào mừng đến với Time Server. Cú pháp lệnh: GET_TIME [định_dạng]\r\nVí dụ: GET_TIME hh:mm:ss dd/mm/yyyy\r\n";
    send(client_socket, welcome, strlen(welcome), 0);

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_received <= 0) break;

        buffer[strcspn(buffer, "\r\n")] = 0;

        if (strncmp(buffer, "GET_TIME ", 9) == 0) {
            char *format_req = buffer + 9;

            time_t rawtime;
            struct tm *timeinfo;
            time(&rawtime);
            timeinfo = localtime(&rawtime);

            // Chuyển đổi an toàn chuỗi định dạng
            char c_format[BUFFER_SIZE];
            convert_format(format_req, c_format, sizeof(c_format));
            
            char output[BUFFER_SIZE];
            memset(output, 0, BUFFER_SIZE);

            // Sinh chuỗi thời gian dựa trên format đã chuẩn hóa
            strftime(output, sizeof(output), c_format, timeinfo);
            strcat(output, "\r\n");
            send(client_socket, output, strlen(output), 0);
        } else {
            char *err_cmd = "Sai cú pháp lệnh. Vui lòng dùng: GET_TIME [định_dạng]\r\n";
            send(client_socket, err_cmd, strlen(err_cmd), 0);
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
    printf("Time Server đang chạy trên port %d...\n", PORT);

    while (1) {
        int client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        if (client_socket < 0) continue;

        pthread_t thread_id;
        new_sock = malloc(sizeof(int));
        *new_sock = client_socket;

        pthread_create(&thread_id, NULL, time_handler, (void *)new_sock);
        pthread_detach(thread_id);
    }
    return 0;
}