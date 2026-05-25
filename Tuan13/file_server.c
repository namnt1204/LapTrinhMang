#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <sys/stat.h>
#include <signal.h>
#include <sys/wait.h>

#define PORT 8080
#define DIR_PATH "./files" // Cần tạo thư mục này trước khi chạy
#define BUFFER_SIZE 1024

// Xử lý tiến trình con (tránh zombie process)
void sigchld_handler(int s) {
    while(waitpid(-1, NULL, WNOHANG) > 0);
}

void handle_client(int client_socket) {
    DIR *d;
    struct dirent *dir;
    int file_count = 0;
    char buffer[BUFFER_SIZE];
    char file_list[4096] = "";

    // 1. Quét thư mục
    d = opendir(DIR_PATH);
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            // Chỉ lấy file thông thường, bỏ qua thư mục . và ..
            if (dir->d_type == DT_REG) { 
                file_count++;
                strcat(file_list, dir->d_name);
                strcat(file_list, "\r\n");
            }
        }
        closedir(d);
    }

    // 2. Gửi danh sách file
    if (file_count == 0) {
        char *err_msg = "ERROR No files to download\r\n";
        send(client_socket, err_msg, strlen(err_msg), 0);
        close(client_socket);
        exit(0);
    } else {
        // [FIX 1]: Tạo một chuỗi response đủ lớn để chứa file_list (4096 bytes) + header
        char response[4500];
        snprintf(response, sizeof(response), "OK %d\r\n%s\r\n", file_count, file_list);
        send(client_socket, response, strlen(response), 0);
    }

    // 3. Xử lý yêu cầu tải file
    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_received <= 0) break; // Client ngắt kết nối

        // Xóa các ký tự xuống dòng ở cuối tên file
        buffer[strcspn(buffer, "\r\n")] = 0;
        
        // [FIX 2]: Tăng kích thước filepath để chứa đủ nội dung tối đa của buffer
        char filepath[BUFFER_SIZE + 64];
        snprintf(filepath, sizeof(filepath), "%s/%s", DIR_PATH, buffer);

        FILE *fp = fopen(filepath, "rb");
        if (fp == NULL) {
            // File không tồn tại, gửi lỗi và lặp lại để client gửi tên khác
            char *not_found_msg = "ERROR File not found\r\n";
            send(client_socket, not_found_msg, strlen(not_found_msg), 0);
        } else {
            // File tồn tại, lấy kích thước file
            fseek(fp, 0, SEEK_END);
            long file_size = ftell(fp);
            fseek(fp, 0, SEEK_SET);

            // Gửi header kích thước
            char header[128];
            snprintf(header, sizeof(header), "OK %ld\r\n", file_size);
            send(client_socket, header, strlen(header), 0);

            // Gửi nội dung file nhị phân
            int bytes_read;
            while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, fp)) > 0) {
                send(client_socket, buffer, bytes_read, 0);
            }
            fclose(fp);
            break; // Gửi xong thì đóng kết nối
        }
    }
    
    close(client_socket);
    exit(0); // Kết thúc tiến trình con
}

int main() {
    int server_fd, client_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    // Xử lý signal để dọn dẹp các tiến trình con đã hoàn thành
    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGCHLD, &sa, NULL);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    
    // Thêm cấu hình SO_REUSEADDR để tránh lỗi "Address already in use" khi chạy lại server liên tục
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    listen(server_fd, 10);
    printf("File Server đang chạy trên port %d...\n", PORT);

    while (1) {
        client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        if (client_socket < 0) continue;

        if (fork() == 0) {
            // Tiến trình con
            close(server_fd); 
            handle_client(client_socket);
        }
        // Tiến trình cha
        close(client_socket); 
    }
    return 0;
}