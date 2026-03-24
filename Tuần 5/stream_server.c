#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

int main(int argc, char *argv[]) {
    if (argc != 2) exit(EXIT_FAILURE);

    int port = atoi(argv[1]);
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) exit(EXIT_FAILURE);

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) exit(EXIT_FAILURE);
    if (listen(server_fd, 5) < 0) exit(EXIT_FAILURE);

    printf("Server dang lang nghe tren cong %d...\n", port);

    while (1) {
        int new_socket = accept(server_fd, NULL, NULL);
        if (new_socket < 0) continue;

        char leftover[10] = {0};
        int total_count = 0;
        char buffer[1024];
        int valread;

        while ((valread = recv(new_socket, buffer, sizeof(buffer) - 1, 0)) > 0) {
            buffer[valread] = '\0';
            
            char temp_buf[1050];
            strcpy(temp_buf, leftover);
            strcat(temp_buf, buffer);

            int temp_len = strlen(temp_buf);
            char *ptr = temp_buf;
            
            while ((ptr = strstr(ptr, "0123456789")) != NULL) {
                total_count++;
                ptr += 10;
            }

            printf("\nNhan duoc: %s\n", buffer);
            printf("=> Tong so lan xuat hien hien tai: %d\n", total_count);

            if (temp_len >= 9) {
                strcpy(leftover, temp_buf + temp_len - 9);
            } else {
                strcpy(leftover, temp_buf);
            }
        }
        
        close(new_socket);
        printf("\nClient da ngat ket noi.\n");
    }
    
    close(server_fd);
    return 0;
}