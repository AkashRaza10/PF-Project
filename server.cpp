/* Multi-client LAN chat SERVER (Windows, Winsock, C)
   Linker: -lws2_32 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>

#define PORT        8080
#define MAX_CLIENTS 20
#define BUF_SIZE    1024

SOCKET clients[MAX_CLIENTS];
int client_count = 0;
CRITICAL_SECTION lock;          /* clients[] ko threads se bachane ke liye */

/* msg ko sender ke ilawa sab clients ko bhejo */
void broadcast(const char *msg, SOCKET sender) {
    EnterCriticalSection(&lock);
    for (int i = 0; i < client_count; i++) {
        if (clients[i] != sender)
            send(clients[i], msg, (int)strlen(msg), 0);
    }
    LeaveCriticalSection(&lock);
}

int add_client(SOCKET s) {
    int ok = 0;
    EnterCriticalSection(&lock);
    if (client_count < MAX_CLIENTS) {
        clients[client_count++] = s;
        ok = 1;
    }
    LeaveCriticalSection(&lock);
    return ok;
}

void remove_client(SOCKET s) {
    EnterCriticalSection(&lock);
    for (int i = 0; i < client_count; i++) {
        if (clients[i] == s) {
            clients[i] = clients[client_count - 1];   /* aakhri ko is jagah rakho */
            client_count--;
            break;
        }
    }
    LeaveCriticalSection(&lock);
}

/* Har client ke liye ek alag thread */
DWORD WINAPI handle_client(LPVOID arg) {
    SOCKET s = *(SOCKET *)arg;
    free(arg);

    char name[32], buf[BUF_SIZE], msg[BUF_SIZE + 64];
    int n;

    /* Pehla message = client ka naam */
    n = recv(s, name, sizeof(name) - 1, 0);
    if (n <= 0) { closesocket(s); return 0; }
    name[n] = '\0';
    name[strcspn(name, "\r\n")] = '\0';

    if (!add_client(s)) {
        send(s, "Server full hai.\n", 17, 0);
        closesocket(s);
        return 0;
    }

    snprintf(msg, sizeof(msg), "*** %s chat mein shamil hua ***\n", name);
    printf("%s", msg);
    broadcast(msg, s);

    /* Chat loop */
    while ((n = recv(s, buf, BUF_SIZE - 1, 0)) > 0) {
        buf[n] = '\0';
        snprintf(msg, sizeof(msg), "%s: %s", name, buf);
        printf("%s", msg);
        broadcast(msg, s);
    }

    /* recv 0 ya error = client chala gaya */
    remove_client(s);
    snprintf(msg, sizeof(msg), "*** %s chala gaya ***\n", name);
    printf("%s", msg);
    broadcast(msg, s);
    closesocket(s);
    return 0;
}

int main(void) {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("WSAStartup fail\n");
        return 1;
    }
    InitializeCriticalSection(&lock);

    SOCKET server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == INVALID_SOCKET) {
        printf("socket fail: %d\n", WSAGetLastError());
        return 1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR) {
        printf("bind fail: %d\n", WSAGetLastError());
        return 1;
    }
    if (listen(server_fd, 5) == SOCKET_ERROR) {
        printf("listen fail: %d\n", WSAGetLastError());
        return 1;
    }

    printf("Chat server port %d par chal raha hai...\n", PORT);

    while (1) {                       /* hamesha naye clients ka intezar */
        struct sockaddr_in caddr;
        int len = sizeof(caddr);
        SOCKET c = accept(server_fd, (struct sockaddr *)&caddr, &len);
        if (c == INVALID_SOCKET) continue;

        SOCKET *p = (SOCKET *)malloc(sizeof(SOCKET));
        *p = c;
        HANDLE h = CreateThread(NULL, 0, handle_client, p, 0, NULL);
        if (h) CloseHandle(h);
    }

    DeleteCriticalSection(&lock);
    closesocket(server_fd);
    WSACleanup();
    return 0;
}