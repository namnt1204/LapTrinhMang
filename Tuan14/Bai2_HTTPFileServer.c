#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT 8080
#define BUFFER_SIZE 8192
#define ROOT_DIR "./shared_folder"

const char* get_mime_type(const char *filename) {
    if (strstr(filename, ".txt") || strstr(filename, ".c") || strstr(filename, ".h")) 
        return "text/plain; charset=UTF-8";
    if (strstr(filename, ".png")) return "image/png";
    if (strstr(filename, ".jpg") || strstr(filename, ".jpeg")) return "image/jpeg";
    if (strstr(filename, ".mp3")) return "audio/mpeg";
    if (strstr(filename, ".mp4")) return "video/mp4";
    return "application/octet-stream";
}

void send_directory_listing(int client_fd, const char *current_subpath) {
    char target_dir[2048];
    if (strlen(current_subpath) == 0) {
        sprintf(target_dir, "%s", ROOT_DIR);
    } else {
        sprintf(target_dir, "%s/%s", ROOT_DIR, current_subpath);
    }

    DIR *d = opendir(target_dir);
    char response_body[65536] = "";
    
    sprintf(response_body, 
        "<html><head><meta charset='UTF-8'><title>HTTP File Server in C</title>"
        "<style>body{font-family:Arial, sans-serif; margin:40px;} li{margin:10px 0; font-size:13pt;} a{text-decoration:none; color:#0066cc;}</style></head>"
        "<body><h2>Danh sách tệp tin và thư mục: /%s</h2><ul>", current_subpath);

    if (strlen(current_subpath) > 0) {
        strcat(response_body, "<li><a href='/'><b>[..] Quay về Thư mục gốc</b></a></li>");
    }

    if (d) {
        struct dirent *dir;
        while ((dir = readdir(d)) != NULL) {
            if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) continue;

            char full_item_path[4096];
            sprintf(full_item_path, "%s/%s", target_dir, dir->d_name);
            
            struct stat st;
            stat(full_item_path, &st);

            char item_link[8192];
            char web_path[2048];
            if (strlen(current_subpath) == 0) {
                sprintf(web_path, "%s", dir->d_name);
            } else {
                sprintf(web_path, "%s/%s", current_subpath, dir->d_name);
            }

            if (S_ISDIR(st.st_mode)) {
                sprintf(item_link, "<li><a href=\"/files/%s\"><b>%s/</b></a></li>", web_path, dir->d_name);
            } else if (S_ISREG(st.st_mode)) {
                sprintf(item_link, "<li><a href=\"/files/%s\"><i>%s</i></a></li>", web_path, dir->d_name);
            } else {
                continue;
            }
            strcat(response_body, item_link);
        }
        closedir(d);
    } else {
        strcat(response_body, "<p style='color:red;'>Lỗi: Không thể truy cập hoặc thư mục không tồn tại.</p>");
    }

    strcat(response_body, "</ul></body></html>");

    char response_header[1024];
    sprintf(response_header, 
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html\r\n"
            "Content-Length: %d\r\n"
            "Connection: close\r\n\r\n", (int)strlen(response_body));
    
    send(client_fd, response_header, strlen(response_header), 0);
    send(client_fd, response_body, strlen(response_body), 0);
}

void send_file_content(int client_fd, const char *file_relative_path) {
    char full_file_path[4096];
    sprintf(full_file_path, "%s/%s", ROOT_DIR, file_relative_path);

    FILE *f = fopen(full_file_path, "rb");
    if (!f) {
        char *not_found = "HTTP/1.1 404 Not Found\r\nContent-Length: 29\r\nConnection: close\r\n\r\nTập tin không tồn tại trên hệ thống.";
        send(client_fd, not_found, strlen(not_found), 0);
        return;
    }

    struct stat st;
    stat(full_file_path, &st);
    long file_size = st.st_size;

    char header[1024];
    sprintf(header, 
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: %s\r\n"
            "Content-Length: %ld\r\n"
            "Connection: close\r\n\r\n", get_mime_type(file_relative_path), file_size);
    send(client_fd, header, strlen(header), 0);

    char file_buffer[2048];
    int bytes_read;
    while ((bytes_read = fread(file_buffer, 1, sizeof(file_buffer), f)) > 0) {
        send(client_fd, file_buffer, bytes_read, 0);
    }
    fclose(f);
}

int main() {
    int server_fd, client_fd;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    char buffer[BUFFER_SIZE] = {0};

#ifdef _WIN32
    mkdir(ROOT_DIR);
#else
    mkdir(ROOT_DIR, 0777);
#endif

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Khởi tạo Socket thất bại");
        exit(EXIT_FAILURE);
    }
    
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Lỗi gán cổng Bind thất bại");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 10) < 0) {
        perror("Lỗi thiết lập Listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    printf("[SERVER] Máy chủ File Server HTTP hoạt động tại cổng: %d...\n", PORT);

    while (1) {
        client_fd = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        if (client_fd < 0) continue;

        memset(buffer, 0, BUFFER_SIZE);
        int bytes_recv = read(client_fd, buffer, BUFFER_SIZE - 1);
        if (bytes_recv <= 0) { 
            close(client_fd); 
            continue; 
        }

        char method[16] = "", url[1024] = "", version[16] = "";
        sscanf(buffer, "%s %s %s", method, url, version);

        if (strcmp(url, "/") == 0 || strcmp(url, "/files") == 0 || strcmp(url, "/files/") == 0) {
            send_directory_listing(client_fd, "");
        } 
        else if (strncmp(url, "/files/", 7) == 0) {
            char *subpath = url + 7;
            
            char full_check_path[4096];
            sprintf(full_check_path, "%s/%s", ROOT_DIR, subpath);
            
            struct stat st;
            if (stat(full_check_path, &st) == 0) {
                if (S_ISDIR(st.st_mode)) {
                    send_directory_listing(client_fd, subpath);
                } else if (S_ISREG(st.st_mode)) {
                    send_file_content(client_fd, subpath);
                }
            } else {
                char *not_found = "HTTP/1.1 404 Not Found\r\nContent-Length: 20\r\n\r\nĐường dẫn không đúng";
                send(client_fd, not_found, strlen(not_found), 0);
            }
        } else {
            char *bad_req = "HTTP/1.1 400 Bad Request\r\nContent-Length: 15\r\n\r\nYêu cầu sai lỗi";
            send(client_fd, bad_req, strlen(bad_req), 0);
        }

        close(client_fd);
    }

    close(server_fd);
    return 0;
}