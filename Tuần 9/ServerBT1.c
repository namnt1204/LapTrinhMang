#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <ctype.h>

#define PORT 8080
#define MAX_CLIENTS 100
#define BUFFER_SIZE 1024

void encode_string(char *str) {
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] == '\n' || str[i] == '\r') {
            continue;
        }
        
        if (str[i] >= 'A' && str[i] <= 'Z') {
            str[i] = (str[i] == 'Z') ? 'A' : str[i] + 1;
        } else if (str[i] >= 'a' && str[i] <= 'z') {
            str[i] = (str[i] == 'z') ? 'a' : str[i] + 1;
        } else if (str[i] >= '0' && str[i] <= '9') {
            str[i] = '9' - (str[i] - '0');
        }
    }
}

int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    char buffer[BUFFER_SIZE];
    fd_set master_fds, read_fds;
    int max_fd;
    int client_count = 0;

    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, MAX_CLIENTS) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server đang lắng nghe trên cổng %d...\n", PORT);

    FD_ZERO(&master_fds);
    FD_SET(server_socket, &master_fds);
    max_fd = server_socket;

    while (1) {
        read_fds = master_fds; 

        if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) < 0) {
            perror("Select error");
            exit(EXIT_FAILURE);
        }

        for (int i = 0; i <= max_fd; i++) {
            if (FD_ISSET(i, &read_fds)) {
                
                if (i == server_socket) {
                    client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
                    if (client_socket < 0) {
                        perror("Accept error");
                    } else {
                        FD_SET(client_socket, &master_fds);
                        if (client_socket > max_fd) max_fd = client_socket;
                        
                        client_count++;
                        printf("Client mới kết nối! Cập nhật: %d clients.\n", client_count);

                        char welcome_msg[256];
                        snprintf(welcome_msg, sizeof(welcome_msg), "Xin chào. Hiện có %d clients đang kết nối.\n", client_count);
                        send(client_socket, welcome_msg, strlen(welcome_msg), 0);
                    }
                } 
                else {
                    memset(buffer, 0, BUFFER_SIZE);
                    int bytes_received = recv(i, buffer, BUFFER_SIZE - 1, 0);

                    if (bytes_received <= 0) {
                        printf("Client (socket %d) đã ngắt kết nối.\n", i);
                        close(i);
                        FD_CLR(i, &master_fds);
                        client_count--;
                    } else {
                        if (strncmp(buffer, "exit", 4) == 0) {
                            char *goodbye_msg = "Tạm biệt!\n";
                            send(i, goodbye_msg, strlen(goodbye_msg), 0);
                            
                            printf("Client (socket %d) đã chủ động thoát.\n", i);
                            close(i);
                            FD_CLR(i, &master_fds);
                            client_count--;
                        } else {
                            encode_string(buffer);
                            send(i, buffer, strlen(buffer), 0);
                        }
                    }
                }
            }
        }
    }

    return 0;
}