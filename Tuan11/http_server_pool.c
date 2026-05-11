#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT 8080
#define THREAD_POOL_SIZE 5  // Số lượng luồng tạo sẵn (tương đương preforking)
#define QUEUE_SIZE 100      // Kích thước hàng đợi tối đa
#define BUFFER_SIZE 2048

// --- XÂY DỰNG HÀNG ĐỢI (QUEUE) ĐỂ LƯU TRỮ SOCKET ---
int client_queue[QUEUE_SIZE];
int queue_count = 0;
int queue_front = 0;
int queue_rear = 0;

pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t queue_cond = PTHREAD_COND_INITIALIZER;

// Lấy socket ra khỏi hàng đợi
int dequeue() {
    if (queue_count == 0) return -1;
    int client_sock = client_queue[queue_front];
    queue_front = (queue_front + 1) % QUEUE_SIZE;
    queue_count--;
    return client_sock;
}

// Thêm socket vào hàng đợi
void enqueue(int client_sock) {
    if (queue_count == QUEUE_SIZE) {
        printf("Canh bao: Hang doi da day!\n");
        return; 
    }
    client_queue[queue_rear] = client_sock;
    queue_rear = (queue_rear + 1) % QUEUE_SIZE;
    queue_count++;
}

// --- HÀM XỬ LÝ CỦA CÁC WORKER THREAD ---
void *worker_thread(void *arg) {
    int thread_id = *(int *)arg;
    free(arg);
    
    while (1) {
        int client_sock;
        
        // 1. Khóa Mutex và kiểm tra hàng đợi
        pthread_mutex_lock(&queue_mutex);
        
        // Nếu không có việc, luồng sẽ ngủ (wait) để nhường CPU
        while (queue_count == 0) {
            pthread_cond_wait(&queue_cond, &queue_mutex);
        }
        
        // Có việc -> Lấy socket ra để xử lý
        client_sock = dequeue();
        pthread_mutex_unlock(&queue_mutex);

        if (client_sock < 0) continue;

        // 2. Xử lý HTTP Request
        char buffer[BUFFER_SIZE];
        memset(buffer, 0, BUFFER_SIZE);
        recv(client_sock, buffer, BUFFER_SIZE - 1, 0);

        // Chỉ phản hồi nếu request có nội dung (tránh các request rỗng do trình duyệt tạo)
        if (strlen(buffer) > 0) {
            printf("[Thread %d] Dang phuc vu mot HTTP Request...\n", thread_id);
            
            char http_response[BUFFER_SIZE];
            snprintf(http_response, sizeof(http_response),
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/html; charset=UTF-8\r\n"
                "Connection: close\r\n\r\n"
                "<!DOCTYPE html>"
                "<html><head><title>Test Server</title></head>"
                "<body>"
                "<h1>Xin chao tu HTTP Server!</h1>"
                "<p>Request cua ban dang duoc xu ly boi <b>Worker Thread so %d</b></p>"
                "</body></html>", thread_id);

            send(client_sock, http_response, strlen(http_response), 0);
        }
        
        // Đóng kết nối (HTTP/1.1 quy định close sau khi phục vụ nếu header Connection: close)
        close(client_sock);
    }
    return NULL;
}

// --- LUỒNG CHÍNH (MASTER THREAD) ---
int main() {
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    // 1. Khởi tạo Thread Pool ngay từ đầu (Tương tự ý tưởng Preforking)
    pthread_t threads[THREAD_POOL_SIZE];
    for (int i = 0; i < THREAD_POOL_SIZE; i++) {
        int *thread_id = malloc(sizeof(int));
        *thread_id = i + 1;
        pthread_create(&threads[i], NULL, worker_thread, thread_id);
    }

    // 2. Thiết lập Socket mạng
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
    listen(server_sock, 10);
    
    printf("HTTP Server (Pre-threading Pool) dang chay tren port %d...\n", PORT);

    // 3. Vòng lặp chỉ làm nhiệm vụ Accept và giao việc
    while (1) {
        client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_len);
        if (client_sock < 0) continue;

        // Đẩy công việc vào hàng đợi
        pthread_mutex_lock(&queue_mutex);
        enqueue(client_sock);
        
        // Phát tín hiệu (signal) đánh thức 1 worker thread đang ngủ
        pthread_cond_signal(&queue_cond);
        pthread_mutex_unlock(&queue_mutex);
    }

    close(server_sock);
    return 0;
}