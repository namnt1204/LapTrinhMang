#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <ctype.h>

#define PORT 8080
#define MAX_CLIENTS 10
#define BUFFER_SIZE 256

typedef struct {
    int fd;
    int state; // 0: rỗng, 1: chờ Họ tên, 2: chờ MSSV
    char name[BUFFER_SIZE];
    char mssv[BUFFER_SIZE];
} Client;

Client clients[MAX_CLIENTS];

// Hàm tạo email theo cấu trúc ĐHBK
void generate_hust_email(char* name, char* mssv, char* email) {
    name[strcspn(name, "\r\n")] = 0;
    mssv[strcspn(mssv, "\r\n")] = 0;

    char first_name[50] = {0}, initials[50] = {0};
    char *token = strtok(name, " ");
    char words[10][50];
    int count = 0;

    while(token && count < 10) {
        strcpy(words[count++], token);
        token = strtok(NULL, " ");
    }

    if(count > 0) {
        strcpy(first_name, words[count-1]);
        for(int i = 0; i < count - 1; i++) initials[i] = words[i][0];
        
        // Chuyển thành chữ thường
        for(int i = 0; first_name[i]; i++) first_name[i] = tolower(first_name[i]);
        for(int i = 0; initials[i]; i++) initials[i] = tolower(initials[i]);
        
        sprintf(email, "%s.%s%s@sis.hust.edu.vn", first_name, initials, mssv);
    } else {
        sprintf(email, "sv.%s@sis.hust.edu.vn", mssv);
    }
}

int main() {
    int server_fd, new_socket, max_sd;
    struct sockaddr_in address;
    fd_set readfds;

    for (int i = 0; i < MAX_CLIENTS; i++) clients[i].fd = 0;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    listen(server_fd, 5);

    printf("Server dang lang nghe tren port %d...\n", PORT);

    while(1) {
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        max_sd = server_fd;

        for (int i = 0; i < MAX_CLIENTS; i++) {
            int sd = clients[i].fd;
            if(sd > 0) FD_SET(sd, &readfds);
            if(sd > max_sd) max_sd = sd;
        }

        select(max_sd + 1, &readfds, NULL, NULL, NULL);

        // Xử lý kết nối mới
        if (FD_ISSET(server_fd, &readfds)) {
            new_socket = accept(server_fd, NULL, NULL);
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if(clients[i].fd == 0) {
                    clients[i].fd = new_socket;
                    clients[i].state = 1;
                    char *msg = "Nhap Ho ten: ";
                    send(new_socket, msg, strlen(msg), 0);
                    break;
                }
            }
        }

        // Xử lý dữ liệu từ client
        for (int i = 0; i < MAX_CLIENTS; i++) {
            int sd = clients[i].fd;
            if (FD_ISSET(sd, &readfds)) {
                char buffer[BUFFER_SIZE] = {0};
                int valread = read(sd, buffer, BUFFER_SIZE);
                
                if (valread <= 0) {
                    close(sd);
                    clients[i].fd = 0;
                } else {
                    if (clients[i].state == 1) {
                        strcpy(clients[i].name, buffer);
                        clients[i].state = 2;
                        char *msg = "Nhap MSSV: ";
                        send(sd, msg, strlen(msg), 0);
                    } else if (clients[i].state == 2) {
                        strcpy(clients[i].mssv, buffer);
                        char email[BUFFER_SIZE];
                        generate_hust_email(clients[i].name, clients[i].mssv, email);
                        
                        char response[BUFFER_SIZE + 50];
                        sprintf(response, "Email cua ban: %s\n", email);
                        send(sd, response, strlen(response), 0);
                        
                        close(sd); // Đóng kết nối sau khi gửi phản hồi
                        clients[i].fd = 0;
                    }
                }
            }
        }
    }
    return 0;
}