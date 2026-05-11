#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>

#define PORT 9003
#define BUFFER_SIZE 256

void *handle_client(void *arg) {
    // Lấy socket descriptor và giải phóng bộ nhớ
    int client_sock = *(int *)arg;
    free(arg);
    
    // Tách luồng để hệ thống tự thu hồi tài nguyên khi kết thúc
    pthread_detach(pthread_self());

    char buffer[BUFFER_SIZE];

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(client_sock, buffer, BUFFER_SIZE - 1, 0);
        
        // Client ngắt kết nối hoặc có lỗi
        if (bytes_received <= 0) break;

        // Xóa bỏ các ký tự xuống dòng (\r, \n) có thể do telnet/nc sinh ra
        buffer[strcspn(buffer, "\r\n")] = 0;
        
        if (strlen(buffer) == 0) continue;

        // Kiểm tra tiền tố lệnh GET_TIME
        if (strncmp(buffer, "GET_TIME ", 9) == 0) {
            char *format = buffer + 9; // Lấy phần chuỗi format phía sau
            
            // Xử lý trường hợp client copy y nguyên dấu $ trong slide
            if (format[0] == '$') format++;
            int len = strlen(format);
            if (len > 0 && format[len - 1] == '$') format[len - 1] = '\0';

            // Lấy thời gian hiện tại (Sử dụng localtime_r để đảm bảo Thread-safe)
            time_t t = time(NULL);
            struct tm tm_info;
            localtime_r(&t, &tm_info); 
            
            char time_str[100];
            int is_valid_format = 1;

            // Xử lý chặt chẽ theo đúng các test case trong tài liệu
            if (strcmp(format, "dd/mm/yy") == 0) {
                // Ví dụ slide: 30/01/2023 -> Ngày/Tháng/Năm (4 chữ số)
                strftime(time_str, sizeof(time_str), "%d/%m/%Y\n", &tm_info);
            } 
            else if (strcmp(format, "dd/mm/y") == 0) {
                // Ví dụ slide: 30/01/23 -> Ngày/Tháng/Năm (2 chữ số)
                strftime(time_str, sizeof(time_str), "%d/%m/%y\n", &tm_info);
            } 
            else if (strcmp(format, "mm/dd/yyy") == 0) {
                // Ví dụ slide: 01/30/2023 -> Tháng/Ngày/Năm (4 chữ số)
                strftime(time_str, sizeof(time_str), "%m/%d/%Y\n", &tm_info);
            } 
            else if (strcmp(format, "mm/dd/yy") == 0) {
                // Ví dụ slide: 01/30/23 -> Tháng/Ngày/Năm (2 chữ số)
                strftime(time_str, sizeof(time_str), "%m/%d/%y\n", &tm_info);
            } 
            else {
                is_valid_format = 0;
            }

            // Gửi phản hồi
            if (is_valid_format) {
                send(client_sock, time_str, strlen(time_str), 0);
            } else {
                char *err_msg = "Format khong hop le. Cac format ho tro: dd/mm/yy, dd/mm/y, mm/dd/yyy, mm/dd/yy\n";
                send(client_sock, err_msg, strlen(err_msg), 0);
            }
        } else {
            char *err_msg = "Sai cu phap. Hay dung: GET_TIME [format]\n";
            send(client_sock, err_msg, strlen(err_msg), 0);
        }
    }

    close(client_sock);
    return NULL;
}

int main() {
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    // Khởi tạo socket
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    
    // Tùy chọn tái sử dụng port nhanh
    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
    listen(server_sock, 10);
    
    printf("Time Server (Đa luồng) đang chạy trên port %d...\n", PORT);

    // Vòng lặp chính chấp nhận kết nối
    while (1) {
        client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_len);
        if (client_sock < 0) continue;

        // Cấp phát bộ nhớ cho socket descriptor để truyền vào luồng
        int *new_sock = malloc(sizeof(int));
        *new_sock = client_sock;

        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, handle_client, (void *)new_sock) != 0) {
            perror("Khong the tao luong");
            free(new_sock);
            close(client_sock);
        }
    }

    close(server_sock);
    return 0;
}