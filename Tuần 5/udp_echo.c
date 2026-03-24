#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    if (argc != 2) exit(EXIT_FAILURE);

    int port = atoi(argv[1]);
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) exit(EXIT_FAILURE);

    struct sockaddr_in servaddr, cliaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    memset(&cliaddr, 0, sizeof(cliaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(port);

    if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("UDP Echo Server dang lang nghe tren cong %d...\n", port);

    char buffer[BUFFER_SIZE];
    socklen_t len;
    
    while (1) {
        len = sizeof(cliaddr);
        int n = recvfrom(sockfd, (char *)buffer, BUFFER_SIZE, 0, (struct sockaddr *)&cliaddr, &len);
        if (n > 0) {
            buffer[n] = '\0';
            printf("Nhan tu client %s:%d: %s", inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port), buffer);
            
            sendto(sockfd, (const char *)buffer, n, 0, (const struct sockaddr *)&cliaddr, len);
        }
    }

    close(sockfd);
    return 0;
}