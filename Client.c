#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#define TELNET_PORT 23
#define HOST "telehack.com"
#define BUFFER_SIZE 4096
#define TIMEOUT_SEC 2

static int read_telnet_byte(SOCKET sockfd) {
    unsigned char ch;
    while (1) {
        int res = recv(sockfd, (char*)&ch, 1, 0);
        if (res <= 0) return -1;
        if (ch != 0xFF) return ch;
        unsigned char cmd, opt;
        if (recv(sockfd, (char*)&cmd, 1, 0) <= 0) return -1;
        if (recv(sockfd, (char*)&opt, 1, 0) <= 0) return -1;
        switch (cmd) {
            case 0xFB: cmd = 0xFE; break;
            case 0xFD: cmd = 0xFC; break;
            case 0xFC: case 0xFE: continue;
            case 0xFA: {
                unsigned char b;
                while (1) {
                    if (recv(sockfd, (char*)&b, 1, 0) <= 0) return -1;
                    if (b == 0xFF) {
                        if (recv(sockfd, (char*)&b, 1, 0) <= 0) return -1;
                        if (b == 0xF0) break;
                    }
                }
                continue;
            }
            default: continue;
        }
        unsigned char resp[3] = {0xFF, cmd, opt};
        if (send(sockfd, (char*)resp, 3, 0) <= 0) return -1;
    }
}

static char* read_all_with_timeout(SOCKET sockfd, int timeout_sec) {
    int timeout_ms = timeout_sec * 1000;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout_ms, sizeof(timeout_ms));

    size_t capacity = BUFFER_SIZE;
    size_t len = 0;
    char* data = malloc(capacity);
    if (!data) return NULL;

    while (1) {
        int c = read_telnet_byte(sockfd);
        if (c == -1) break;
        if (len + 1 >= capacity) {
            capacity *= 2;
            char* new = realloc(data, capacity);
            if (!new) { free(data); return NULL; }
            data = new;
        }
        data[len++] = (char)c;
    }
    data[len] = '\0';
    return data;
}

static struct in_addr get_ip(const char* host) {
    struct in_addr addr = {0};
    struct hostent* he = gethostbyname(host);
    if (!he) {
        fprintf(stderr, "Cannot resolve hostname: %s\n", host);
        return addr;
    }
    memcpy(&addr, he->h_addr_list[0], sizeof(addr));
    return addr;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <font> <text>\n", argv[0]);
        return EXIT_FAILURE;
    }

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        fprintf(stderr, "WSAStartup failed\n");
        return EXIT_FAILURE;
    }

    struct in_addr ip = get_ip(HOST);
    if (ip.s_addr == 0) {
        WSACleanup();
        return EXIT_FAILURE;
    }

    SOCKET sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == INVALID_SOCKET) {
        fprintf(stderr, "Socket creation failed\n");
        WSACleanup();
        return EXIT_FAILURE;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(TELNET_PORT);
    server_addr.sin_addr = ip;

    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        fprintf(stderr, "Connection failed\n");
        closesocket(sockfd);
        WSACleanup();
        return EXIT_FAILURE;
    }

    // Читаем приветственный баннер (не более 2 секунд)
    char* banner = read_all_with_timeout(sockfd, 2);
    if (banner) free(banner);

    // Даём серверу время подготовиться
    Sleep(1000);

    // Отправляем команду figlet
    char command[BUFFER_SIZE];
    snprintf(command, sizeof(command), "figlet %s %s\r\n", argv[1], argv[2]);
    if (send(sockfd, command, strlen(command), 0) <= 0) {
        fprintf(stderr, "Failed to send command\n");
        closesocket(sockfd);
        WSACleanup();
        return EXIT_FAILURE;
    }

    // Читаем ответ с таймаутом 10 секунд
    char* response = read_all_with_timeout(sockfd, 10);
    if (response) {
        printf("%s", response);
        free(response);
    } else {
        fprintf(stderr, "Failed to read response (timeout or error)\n");
    }

    closesocket(sockfd);
    WSACleanup();
    return 0;
}