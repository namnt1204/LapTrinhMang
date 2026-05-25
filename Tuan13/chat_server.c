#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/select.h>

#define PORT 8081
#define BUFFER_SIZE 1024

// Biến toàn cục để lưu socket client đang chờ ghép cặp
int waiting_client = -1;
// Mutex để bảo vệ biến toàn cục khỏi Race Condition
pthread_mutex_t queue_lock = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
    int sock1;
    int sock2;
} Pair;

// Luồng xử lý giao tiếp song song cho 1 cặp
void *session_handler(void *arg) {
    Pair *p = (Pair *)arg;
    int sock1 = p->sock1;
    int sock2 = p->sock2;
    free(p); // Giải phóng bộ nhớ đã cấp phát

    char buffer[BUFFER_SIZE];
    fd_set readfds;
    int max_fd = (sock1 > sock2) ? sock1 : sock2;

    char *connected_msg = "Ghép cặp thành công! Bắt đầu chat.\n";
    send(sock1, connected_msg, strlen(connected_msg), 0);
    send(sock2, connected_msg, strlen(connected_msg), 0);

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(sock1, &readfds);
        FD_SET(sock2, &readfds);

        // Chờ dữ liệu từ 1 trong 2 socket
        if (select(max_fd + 1, &readfds, NULL, NULL, NULL) < 0) {
            break; 
        }

        // Nếu client 1 gửi tin nhắn
        if (FD_ISSET(sock1, &readfds)) {
            int valread = recv(sock1, buffer, BUFFER_SIZE, 0);
            if (valread <= 0) break; // Client 1 ngắt kết nối
            send(sock2, buffer, valread, 0); // Chuyển tiếp sang client 2
        }

        // Nếu client 2 gửi tin nhắn
        if (FD_ISSET(sock2, &readfds)) {
            int valread = recv(sock2, buffer, BUFFER_SIZE, 0);
            if (valread <= 0) break; // Client 2 ngắt kết nối
            send(sock1, buffer, valread, 0); // Chuyển tiếp sang client 1
        }
    }

    // Khi vòng lặp break (nghĩa là 1 trong 2 client đã rớt mạng), đóng cả 2 socket
    printf("Một phiên chat đã kết thúc. Đóng kết nối của cả 2 clients.\n");
    close(sock1);
    close(sock2);
    pthread_exit(NULL);
}

int main() {
    int server_fd, client_socket;
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
    printf("Chat Server đang chờ kết nối trên port %d...\n", PORT);

    while (1) {
        client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        if (client_socket < 0) continue;

        printf("Client mới kết nối (Socket ID: %d)\n", client_socket);

        // Khóa mutex khi thao tác với biến toàn cục
        pthread_mutex_lock(&queue_lock);
        
        if (waiting_client == -1) {
            // Chưa có ai đợi, đưa client này vào hàng đợi
            waiting_client = client_socket;
            char *wait_msg = "Đang chờ đối tác ghép cặp...\n";
            send(client_socket, wait_msg, strlen(wait_msg), 0);
            pthread_mutex_unlock(&queue_lock);
        } else {
            // Đã có 1 người đợi, tiến hành ghép cặp
            Pair *p = malloc(sizeof(Pair));
            p->sock1 = waiting_client;
            p->sock2 = client_socket;
            
            // Reset hàng đợi về trạng thái trống
            waiting_client = -1; 
            pthread_mutex_unlock(&queue_lock);

            // Tạo một luồng mới (session thread) để xử lý cặp này
            pthread_t thread_id;
            pthread_create(&thread_id, NULL, session_handler, (void*)p);
            
            // Tách luồng để hệ thống tự thu hồi tài nguyên khi luồng kết thúc
            pthread_detach(thread_id); 
        }
    }
    return 0;
}