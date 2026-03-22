#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Cach su dung: %s <dia_chi_IP> <cong>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *server_ip = argv[1];
    int server_port = atoi(argv[2]);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) exit(EXIT_FAILURE);

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(server_port);
    if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0) exit(EXIT_FAILURE);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Ket noi that bai");
        exit(EXIT_FAILURE);
    }

    char mssv[20], name[100], dob[20], gpa[10];
    char buffer[BUFFER_SIZE];

    printf("Nhap MSSV: ");
    fgets(mssv, sizeof(mssv), stdin);
    mssv[strcspn(mssv, "\n")] = 0;

    printf("Nhap Ho Ten: ");
    fgets(name, sizeof(name), stdin);
    name[strcspn(name, "\n")] = 0;

    printf("Nhap Ngay Sinh (YYYY-MM-DD): ");
    fgets(dob, sizeof(dob), stdin);
    dob[strcspn(dob, "\n")] = 0;

    printf("Nhap Diem TB: ");
    fgets(gpa, sizeof(gpa), stdin);
    gpa[strcspn(gpa, "\n")] = 0;

    snprintf(buffer, sizeof(buffer), "%s %s %s %s", mssv, name, dob, gpa);

    send(sock, buffer, strlen(buffer), 0);
    
    printf("Da gui thong tin thanh cong!\n");
    close(sock);
    return 0;
}