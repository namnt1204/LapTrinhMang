#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdint.h>

int main(int argc, char *argv[]) {
    if (argc != 3) exit(EXIT_FAILURE);

    char *server_ip = argv[1];
    int server_port = atoi(argv[2]);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) exit(EXIT_FAILURE);

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(server_port);
    if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0) exit(EXIT_FAILURE);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) exit(EXIT_FAILURE);

    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) == NULL) exit(EXIT_FAILURE);

    uint16_t dir_len = strlen(cwd);
    uint16_t net_dir_len = htons(dir_len);
    send(sock, &net_dir_len, sizeof(net_dir_len), 0);
    send(sock, cwd, dir_len, 0);

    DIR *dir = opendir(".");
    if (!dir) exit(EXIT_FAILURE);

    struct dirent *entry;
    struct stat file_stat;
    uint16_t file_count = 0;

    while ((entry = readdir(dir)) != NULL) {
        if (stat(entry->d_name, &file_stat) == 0 && S_ISREG(file_stat.st_mode)) {
            file_count++;
        }
    }
    
    uint16_t net_file_count = htons(file_count);
    send(sock, &net_file_count, sizeof(net_file_count), 0);

    rewinddir(dir);

    while ((entry = readdir(dir)) != NULL) {
        if (stat(entry->d_name, &file_stat) == 0 && S_ISREG(file_stat.st_mode)) {
            uint16_t name_len = strlen(entry->d_name);
            uint16_t net_name_len = htons(name_len);
            uint64_t file_size = file_stat.st_size;
            
            uint32_t high = htonl((uint32_t)(file_size >> 32));
            uint32_t low = htonl((uint32_t)(file_size & 0xFFFFFFFFLL));

            send(sock, &net_name_len, sizeof(net_name_len), 0);
            send(sock, entry->d_name, name_len, 0);
            send(sock, &high, sizeof(high), 0);
            send(sock, &low, sizeof(low), 0);
        }
    }

    closedir(dir);
    close(sock);
    return 0;
}