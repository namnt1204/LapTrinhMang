#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT 8084
#define NUM_THREADS 4
#define BUFFER_SIZE 2048
#define QUEUE_SIZE 50

// Cấu trúc dữ liệu Hàng đợi vòng (Circular Queue) lưu trữ Socket ID
int client_queue[QUEUE_SIZE];
int queue_head = 0;
int queue_tail = 0;
int queue_count = 0;

pthread_mutex_t queue_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t queue_cond = PTHREAD_COND_INITIALIZER;

// Hàm thêm socket vào hàng đợi
void enqueue(int sock) {
    pthread_mutex_lock(&queue_lock);
    if (queue_count < QUEUE_SIZE) {
        client_queue[queue_tail] = sock;
        queue_tail = (queue_tail + 1) % QUEUE_SIZE;
        queue_count++;
        // Đánh thức một luồng Worker đang ngủ
        pthread_cond_signal(&queue_cond);
    } else {
        close(sock); // Hàng đợi quá tải thì từ chối kết nối
    }
    pthread_mutex_unlock(&queue_lock);
}

// Hàm lấy socket ra khỏi hàng đợi
int dequeue() {
    pthread_mutex_lock(&queue_lock);
    while (queue_count == 0) {
        // Hàng đợi rỗng, luồng rơi vào trạng thái chờ (block) an toàn
        pthread_cond_wait(&queue_cond, &queue_lock);
    }
    int sock = client_queue[queue_head];
    queue_head = (queue_head + 1) % QUEUE_SIZE;
    queue_count--;
    pthread_mutex_unlock(&queue_lock);
    return sock;
}

// Hàm xử lý HTTP giao thức thô trong từng luồng làm việc
void handle_http_request(int client_socket, int thread_idx) {
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);
    
    int bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_received <= 0) {
        close(client_socket);
        return;
    }

    printf("[Worker Thread %d] Đang xử lý yêu cầu HTTP...\n", thread_idx);

    // Phân tích dòng đầu tiên của HTTP Request (Ví dụ: GET /index.html HTTP/1.1)
    char method[16], url[256], protocol[16];
    sscanf(buffer, "%s %s %s", method, url, protocol);

    // Chuẩn bị nội dung trang HTML phản hồi đơn giản
    char *html_content = "<html><head><meta charset='utf-8'></head><body><h1>Chào mừng đến với Bách Khoa HTTP Server!</h1><p>Yêu cầu xử lý thành công qua Thread Pool dựng sẵn.</p></body></html>";
    char response[BUFFER_SIZE];

    if (strcmp(method, "GET") == 0) {
        sprintf(response,
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/html; charset=UTF-8\r\n"
                "Content-Length: %d\r\n"
                "Connection: close\r\n\r\n"
                "%s",
                (int)strlen(html_content), html_content);
        send(client_socket, response, strlen(response), 0);
    } else {
        char *bad_request = "HTTP/1.1 400 Bad Request\r\nConnection: close\r\n\r\n";
        send(client_socket, bad_request, strlen(bad_request), 0);
    }
    close(client_socket);
}

// Hàm khởi chạy của Worker Thread
void *worker_function(void *arg) {
    int thread_idx = *(int *)arg;
    free(arg);
    while (1) {
        int client_socket = dequeue();
        handle_http_request(client_socket, thread_idx);
    }
    return NULL;
}

int main() {
    int server_fd;
    struct sockaddr_in address;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    listen(server_fd, 20);
    printf("HTTP Prethreading Server đang chạy trên port %d với %d luồng...\n", PORT, NUM_THREADS);

    // Tạo sẵn nhóm luồng làm việc (Thread Pool)
    pthread_t workers[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; i++) {
        int *idx = malloc(sizeof(int));
        *idx = i;
        pthread_create(&workers[i], NULL, worker_function, (void *)idx);
    }

    // Luồng chính phụ trách liên tục tiếp nhận kết nối và đẩy vào hàng đợi
    while (1) {
        int client_socket = accept(server_fd, NULL, NULL);
        if (client_socket >= 0) {
            enqueue(client_socket);
        }
    }
    return 0;
}