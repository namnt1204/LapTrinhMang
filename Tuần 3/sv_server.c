#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>

#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Cach su dung: %s <cong> <ten_file_log>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int port = atoi(argv[1]);
    char *log_file = argv[2];

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) exit(EXIT_FAILURE);

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Loi bind");
        exit(EXIT_FAILURE);
    }
    
    if (listen(server_fd, 5) < 0) {
        perror("Loi listen");
        exit(EXIT_FAILURE);
    }

    printf("Server dang lang nghe tren cong %d...\n", port);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int new_socket = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (new_socket < 0) continue;

        char buffer[BUFFER_SIZE] = {0};
        int valread = read(new_socket, buffer, BUFFER_SIZE - 1);
        if (valread > 0) {
            char *client_ip = inet_ntoa(client_addr.sin_addr);
            
            time_t t = time(NULL);
            struct tm tm = *localtime(&t);
            char time_str[50];
            sprintf(time_str, "%04d-%02d-%02d %02d:%02d:%02d", 
                    tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, 
                    tm.tm_hour, tm.tm_min, tm.tm_sec);

            char log_entry[BUFFER_SIZE + 100];
            snprintf(log_entry, sizeof(log_entry), "%s %s %s\n", client_ip, time_str, buffer);

            printf("Ban ghi moi: %s", log_entry);

            FILE *fp = fopen(log_file, "a");
            if (fp != NULL) {
                fputs(log_entry, fp);
                fclose(fp);
            } else {
                perror("Loi mo file log");
            }
        }
        close(new_socket);
    }
    
    close(server_fd);
    return 0;
}