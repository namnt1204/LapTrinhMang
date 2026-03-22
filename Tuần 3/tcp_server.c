#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Cach su dung: %s <cong> <tep_loi_chao> <tep_luu_du_lieu>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int port = atoi(argv[1]);
    char *greeting_file = argv[2];
    char *output_file = argv[3];

    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Loi tao socket");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Loi bind");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, 5) < 0) {
        perror("Loi listen");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("Server dang lang nghe tren cong %d...\n", port);

    client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len);
    if (client_socket < 0) {
        perror("Loi accept");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("Da ket noi voi client: %s\n", inet_ntoa(client_addr.sin_addr));

    FILE *f_greeting = fopen(greeting_file, "r");
    if (f_greeting != NULL) {
        size_t bytes_read = fread(buffer, 1, BUFFER_SIZE - 1, f_greeting);
        buffer[bytes_read] = '\0';
        send(client_socket, buffer, bytes_read, 0);
        fclose(f_greeting);
    } else {
        char *default_msg = "Xin chao tu server!\n";
        send(client_socket, default_msg, strlen(default_msg), 0);
    }

    FILE *f_output = fopen(output_file, "a");
    if (f_output == NULL) {
        perror("Loi mo file ghi du lieu");
        close(client_socket);
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        ssize_t bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_received <= 0) {
            printf("Client da ngat ket noi.\n");
            break;
        }
        
        fprintf(f_output, "%s\n", buffer);
        fflush(f_output); 
        printf("Nhan duoc tu client: %s\n", buffer);
    }

    fclose(f_output);
    close(client_socket);
    close(server_socket);

    return 0;
}