#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    if (argc != 3) exit(EXIT_FAILURE);

    char *ip = argv[1];
    int port = atoi(argv[2]);

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) exit(EXIT_FAILURE);

    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &servaddr.sin_addr) <= 0) exit(EXIT_FAILURE);

    char buffer[BUFFER_SIZE];
    socklen_t len;

    printf("Nhap tin nhan de gui (go 'exit' de thoat):\n");

    while (1) {
        printf("> ");
        if (fgets(buffer, BUFFER_SIZE, stdin) == NULL) break;
        
        if (strncmp(buffer, "exit", 4) == 0) break;

        sendto(sockfd, (const char *)buffer, strlen(buffer), 0, (const struct sockaddr *)&servaddr, sizeof(servaddr));

        struct sockaddr_in fromaddr;
        len = sizeof(fromaddr);
        int n = recvfrom(sockfd, (char *)buffer, BUFFER_SIZE, 0, (struct sockaddr *)&fromaddr, &len);
        
        if (n > 0) {
            buffer[n] = '\0';
            printf("[Echo tu server]: %s", buffer);
        }
    }

    close(sockfd);
    return 0;
}