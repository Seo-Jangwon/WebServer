#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>
#include <ws2tcpip.h>

// ws2_32.lib 라이브러리 링크
#pragma comment(lib, "ws2_32.lib")

#define PORT 8080           // 서버 포트 번호
#define BUFFER_SIZE 1024    // 버퍼 크기 (1KB)

// 에러 처리
void error_handling(const char* message) {
    fprintf(stderr, "%s: %d\n", message, WSAGetLastError());
    exit(1);  // 프로그램 종료
}

// 클라이언트 요청 처리
void handle_client(SOCKET client_socket) {
    char buffer[BUFFER_SIZE];
    int str_len;

    // 클라이언트로부터 메시지 수신
    str_len = recv(client_socket, buffer, BUFFER_SIZE, 0);
    if (str_len == SOCKET_ERROR) {
        error_handling("recv() error");
    }

    // 디버깅용 수신 메시지 출력
    printf("Received message: %s\n", buffer);

    // HTTP 응답 메시지 생성
    const char* response = "HTTP/1.1 200 OK\r\n"              // 상태 라인
                          "Content-Type: text/plain\r\n"      // 헤더 - 컨텐츠 타입
                          "Content-Length: 13\r\n"            // 헤더 - 컨텐츠 길이
                          "\r\n"                              // 빈 줄로 헤더와 바디 구분
                          "Hello, World!";                    // 응답 바디

    // 응답 발신
    send(client_socket, response, strlen(response), 0);
    closesocket(client_socket);  // 클라이언트 소켓 종료
}

int main() {
    WSADATA wsaData;
    SOCKET server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    int client_addr_size;

    // Windows Socket 초기화
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        error_handling("WSAStartup() error");
    }

    // 서버 소켓 생성 (TCP)
    server_socket = socket(PF_INET, SOCK_STREAM, 0);
    if (server_socket == INVALID_SOCKET) {
        error_handling("socket() error");
    }

    // 서버 주소 구조체 초기화
    memset(&server_addr, 0, sizeof(server_addr));               // 구조체를 0으로 초기화
    server_addr.sin_family = AF_INET;                                     // IPv4 주소체계 사용
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);                      // 모든 IP에서의 연결 허용
    server_addr.sin_port = htons(PORT);                         // 포트 번호 설정

    // 소켓에 주소 할당
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        error_handling("bind() error");
    }

    // 연결 요청 대기상태 시작
    if (listen(server_socket, 5) == SOCKET_ERROR) {          // 대기 큐의 크기: 5
        error_handling("listen() error");
    }

    printf("Server started on port %d...\n", PORT);

    // 무한 루프로 클라이언트 요청 처리
    while (1) {
        client_addr_size = sizeof(client_addr);
        // 클라이언트 연결 수락
        client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_addr_size);
        if (client_socket == INVALID_SOCKET) {
            error_handling("accept() error");
        }

        printf("New client connected...\n");
        handle_client(client_socket);  // 클라이언트 요청 처리
    }

    // 서버 소켓 종료 및 윈속 정리
    closesocket(server_socket);
    WSACleanup();
    return 0;
}