#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

int main(int argc, char *argv[]) {
    if (argc != 3) exit(EXIT_FAILURE);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) exit(EXIT_FAILURE);

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[2]));
    
    if (inet_pton(AF_INET, argv[1], &serv_addr.sin_addr) <= 0) exit(EXIT_FAILURE);
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) exit(EXIT_FAILURE);

    char *chunks[] = {
        "SOICTSOICT012345678901234567890123456789012345",
        "6789SOICTSOICTSOICT012345678901234567890123456",
        "7890123456789012345678901234567890123456789012",
        "3456789SOICTSOICT01234567890123456789012345678"
    };

    for (int i = 0; i < 4; i++) {
        send(sock, chunks[i], strlen(chunks[i]), 0);
        printf("Da gui lan %d: %s\n", i + 1, chunks[i]);
        sleep(1);
    }

    close(sock);
    return 0;
}