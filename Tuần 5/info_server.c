#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdint.h>

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

    while (1) {
        int new_socket = accept(server_fd, NULL, NULL);
        if (new_socket < 0) continue;

        uint16_t net_dir_len;
        if (recv(new_socket, &net_dir_len, sizeof(net_dir_len), 0) <= 0) {
            close(new_socket);
            continue;
        }
        uint16_t dir_len = ntohs(net_dir_len);
        
        char dir_name[1024] = {0};
        recv(new_socket, dir_name, dir_len, 0);
        
        printf("%s\n", dir_name);

        uint16_t net_file_count;
        recv(new_socket, &net_file_count, sizeof(net_file_count), 0);
        uint16_t file_count = ntohs(net_file_count);

        for (int i = 0; i < file_count; i++) {
            uint16_t net_name_len;
            if (recv(new_socket, &net_name_len, sizeof(net_name_len), 0) <= 0) break;
            uint16_t name_len = ntohs(net_name_len);

            char file_name[256] = {0};
            recv(new_socket, file_name, name_len, 0);

            uint32_t high, low;
            recv(new_socket, &high, sizeof(high), 0);
            recv(new_socket, &low, sizeof(low), 0);
            
            uint64_t file_size = ((uint64_t)ntohl(high) << 32) | ntohl(low);

            printf("%s - %llu bytes\n", file_name, (unsigned long long)file_size);
        }
        close(new_socket);
    }
    
    close(server_fd);
    return 0;
}