#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT 5000
#define BUFFER_SIZE 4096

void get_param(const char *src, const char *key, char *dest) {
    char *start = strstr(src, key);
    if (start) {
        start += strlen(key);
        char *end = strchr(start, '&');
        if (!end) end = strchr(start, ' ');
        if (end) {
            strncpy(dest, start, end - start);
            dest[end - start] = '\0';
        } else {
            strcpy(dest, start);
        }
    } else {
        dest[0] = '\0';
    }
}

void do_calculation(char *a_str, char *b_str, char *op_str, char *result_str) {
    if (strlen(a_str) == 0 || strlen(b_str) == 0 || strlen(op_str) == 0) {
        strcpy(result_str, "Chưa nhận dữ liệu phép tính hợp lệ.");
        return;
    }
    double a = atof(a_str);
    double b = atof(b_str);
    
    if (strcmp(op_str, "cong") == 0 || strcmp(op_str, "%2B") == 0) 
        sprintf(result_str, "Kết quả: %.2f + %.2f = %.2f", a, b, a + b);
    else if (strcmp(op_str, "tru") == 0) 
        sprintf(result_str, "Kết quả: %.2f - %.2f = %.2f", a, b, a - b);
    else if (strcmp(op_str, "nhan") == 0) 
        sprintf(result_str, "Kết quả: %.2f * %.2f = %.2f", a, b, a * b);
    else if (strcmp(op_str, "chia") == 0) {
        if (b != 0) sprintf(result_str, "Kết quả: %.2f / %.2f = %.2f", a, b, a / b);
        else strcpy(result_str, "Lỗi: Không thể thực hiện phép chia cho giá trị 0!");
    } else {
        sprintf(result_str, "Toán tử '%s' không được hỗ trợ hệ thống.", op_str);
    }
}

int main() {
    int server_fd, client_fd;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    char buffer[BUFFER_SIZE] = {0};

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    listen(server_fd, 10);
    printf("[SERVER] Máy tính HTTP phục vụ hoạt động tại cổng: %d...\n", PORT);

    while (1) {
        client_fd = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_recv = read(client_fd, buffer, BUFFER_SIZE - 1);
        if(bytes_recv < 0) { close(client_fd); continue; }

        char a_str[32] = "", b_str[32] = "", op_str[32] = "", result[256] = "";

        if (strncmp(buffer, "GET /calc", 9) == 0) {
            get_param(buffer, "a=", a_str);
            get_param(buffer, "b=", b_str);
            get_param(buffer, "op=", op_str);
            do_calculation(a_str, b_str, op_str, result);
        } 
        else if (strncmp(buffer, "POST /calc", 10) == 0) {
            char *body = strstr(buffer, "\r\n\r\n");
            if (body) {
                body += 4;
                get_param(body, "a=", a_str);
                get_param(body, "b=", b_str);
                get_param(body, "op=", op_str);
                do_calculation(a_str, b_str, op_str, result);
            }
        }

        char html[BUFFER_SIZE];
        char body_content[1024];
        
        sprintf(body_content,
            "<h2>MÁY TÍNH HTTP - NGÔN NGỮ C</h2>"
            "<form method='POST' action='/calc'>"
            "  <input type='number' step='any' name='a' placeholder='Số thứ nhất' required> "
            "  <select name='op'>"
            "    <option value='cong'>+ (Cộng)</option>"
            "    <option value='tru'>- (Trừ)</option>"
            "    <option value='nhan'>* (Nhân)</option>"
            "    <option value='chia'>/ (Chia)</option>"
            "  </select> "
            "  <input type='number' step='any' name='b' placeholder='Số thứ hai' required> "
            "  <button type='submit'>Tính toán (POST)</button>"
            "</form>"
            "<div style='margin-top:20px; padding:10px; background:#e8f4fd; border-radius:4px;'>"
            "  <strong>%s</strong>"
            "</div>"
            "<p><small>Thử nghiệm lệnh GET trực tiếp qua URL: <a href='/calc?a=12.5&b=4&op=nhan'>/calc?a=12.5&b=4&op=nhan</a></small></p>",
            result);

        sprintf(html, 
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html; charset=UTF-8\r\n"
            "Connection: close\r\n\r\n" // Bỏ Content-Length, giữ nguyên cặp \r\n\r\n ở cuối Header
            "<!DOCTYPE html><html><head><title>HTTP Calculator in C</title><style>body{font-family:Arial;margin:40px;}</style></head><body>%s</body></html>",
            body_content);

        send(client_fd, html, strlen(html), 0);
        close(client_fd);
    }
    return 0;
}