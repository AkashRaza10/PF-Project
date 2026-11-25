/* Multi-client LAN chat CLIENT (Windows, Winsock, C)
   Linker: -lws2_32 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>

#define PORT     8080
#define BUF_SIZE 1024

/* Worker thread: listens for incoming server messages and displays them */
DWORD WINAPI receive_thread(LPVOID arg) {
    SOCKET fd = *(SOCKET *)arg;
    char buf[BUF_SIZE];
    int n;
    while ((n = recv(fd, buf, BUF_SIZE - 1, 0)) > 0) {
        buf[n] = '\0';
        printf("\r%s", buf);
    }
    printf("\nLost connection to the chat server.\n");
    exit(0);
    return 0;
}

int main(void) {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("WSAStartup failed\n");
        return 1;
    }

    char ip[64], name[32], buf[BUF_SIZE];

    printf("Host IP address (default 127.0.0.1 for localhost): ");
    fgets(ip, sizeof(ip), stdin);
    ip[strcspn(ip, "\r\n")] = '\0';
    if (ip[0] == '\0') strcpy(ip, "127.0.0.1");

    printf("Your display name: ");
    fgets(name, sizeof(name), stdin);
    name[strcspn(name, "\r\n")] = '\0';

    SOCKET fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == INVALID_SOCKET) {
        printf("socket failed: %d\n", WSAGetLastError());
        return 1;
    }

    struct sockaddr_in server;
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    server.sin_addr.s_addr = inet_addr(ip);

    if (connect(fd, (struct sockaddr *)&server, sizeof(server)) == SOCKET_ERROR) {
        printf("connect failed: %d\n", WSAGetLastError());
        closesocket(fd);
        WSACleanup();
        return 1;
    }

    send(fd, name, (int)strlen(name), 0);      /* send username right after connecting */

    HANDLE h = CreateThread(NULL, 0, receive_thread, &fd, 0, NULL);
    if (h) CloseHandle(h);

    printf("Connected! Enter text and hit Enter to send (type 'exit' to disconnect).\n\n");

    while (fgets(buf, sizeof(buf), stdin)) {
        if (strncmp(buf, "exit", 4) == 0) break;
        send(fd, buf, (int)strlen(buf), 0);
    }

    closesocket(fd);
    WSACleanup();
    return 0;
}