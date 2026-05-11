#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>

#define PORT 9002
#define MAX_CLIENTS 100
#define BUFFER_SIZE 2048

// Cấu trúc lưu trữ thông tin client
typedef struct {
    int socket;
    char id[50];
    char name[50];
} Client;

Client clients[MAX_CLIENTS];
int client_count = 0;

// Mutex để đồng bộ hóa việc truy cập mảng clients
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

void *handle_client(void *arg) {
    int client_sock = *(int *)arg;
    free(arg);
    pthread_detach(pthread_self());

    char buffer[BUFFER_SIZE];
    char client_id[50] = {0};
    char client_name[50] = {0};

    // 1. Vòng lặp xác thực client
    while (1) {
        char *prompt = "Vui long nhap ten theo cu phap 'client_id: client_name':\n";
        send(client_sock, prompt, strlen(prompt), 0);
        
        memset(buffer, 0, sizeof(buffer));
        int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
        if (bytes <= 0) {
            close(client_sock);
            return NULL;
        }

        buffer[strcspn(buffer, "\r\n")] = 0; // Xóa ký tự newline

        // Parse cú pháp: yêu cầu dấu ':' và client_name viết liền (không chứa dấu cách)
        if (sscanf(buffer, "%49[^:]: %49s", client_id, client_name) == 2) {
            char success_msg[100];
            snprintf(success_msg, sizeof(success_msg), "Dang nhap thanh cong voi ID: %s\n", client_id);
            send(client_sock, success_msg, strlen(success_msg), 0);
            break;
        } else {
            char *err_msg = "Sai cu phap. client_name phai viet lien. Thu lai!\n";
            send(client_sock, err_msg, strlen(err_msg), 0);
        }
    }

    // 2. Thêm client vào danh sách quản lý chung
    pthread_mutex_lock(&clients_mutex);
    clients[client_count].socket = client_sock;
    strcpy(clients[client_count].id, client_id);
    strcpy(clients[client_count].name, client_name);
    client_count++;
    pthread_mutex_unlock(&clients_mutex);

    // 3. Vòng lặp nhận và Broadcast tin nhắn
    while (1) {
        memset(buffer, 0, sizeof(buffer));
        int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
        if (bytes <= 0) break; // Client ngắt kết nối

        buffer[strcspn(buffer, "\r\n")] = 0;
        if (strlen(buffer) == 0) continue;

        // Lấy thời gian thực
        time_t t = time(NULL);
        struct tm *tm_info = localtime(&t);
        char time_str[50];
        // Format: YYYY/MM/DD HH:MM:SSPM
        strftime(time_str, sizeof(time_str), "%Y/%m/%d %I:%M:%S%p", tm_info);

        // Đóng gói tin nhắn theo chuẩn test case trong slide
        char broadcast_msg[BUFFER_SIZE + 150];
        snprintf(broadcast_msg, sizeof(broadcast_msg), "%s %s: %s\n", time_str, client_id, buffer);

        // Gửi cho tất cả các client CÒN LẠI
        pthread_mutex_lock(&clients_mutex);
        for (int i = 0; i < client_count; i++) {
            if (clients[i].socket != client_sock) {
                send(clients[i].socket, broadcast_msg, strlen(broadcast_msg), 0);
            }
        }
        pthread_mutex_unlock(&clients_mutex);
    }

    // 4. Xóa client khỏi danh sách khi ngắt kết nối
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < client_count; i++) {
        if (clients[i].socket == client_sock) {
            for (int j = i; j < client_count - 1; j++) {
                clients[j] = clients[j + 1];
            }
            client_count--;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
    
    close(client_sock);
    return NULL;
}

int main() {
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
    listen(server_sock, 10);
    printf("Chat Server (Đa luồng) đang chạy trên port %d...\n", PORT);

    while (1) {
        client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_len);
        if (client_sock < 0) continue;

        int *new_sock = malloc(sizeof(int));
        *new_sock = client_sock;

        pthread_t thread_id;
        pthread_create(&thread_id, NULL, handle_client, (void *)new_sock);
    }

    close(server_sock);
    return 0;
}