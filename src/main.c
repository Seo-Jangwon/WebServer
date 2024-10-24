#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "http_parser.h"
#include "file_handler.h"

// ws2_32.lib 라이브러리 링크
#pragma comment(lib, "ws2_32.lib")

#define PORT 8080           // 서버 포트 번호
#define BUFFER_SIZE 1024    // 버퍼 크기 (1KB)
#ifdef _WIN32
#define strcasecmp _stricmp
#define STATIC_FILE_PATH ".\\static"
#else
#define STATIC_FILE_PATH "./static"
#endif

// 에러 처리
void error_handling(const char* message) {
    fprintf(stderr, "%s: %d\n", message, WSAGetLastError());
    exit(1);  // 프로그램 종료
}

// 정적 파일 처리
void handle_static_file(SOCKET client_socket, const char* request_path) {
    // 파일 읽기
    file_result file = read_file(STATIC_FILE_PATH, request_path);

    // HTTP 응답 헤더
    char header[1024];
    if (file.status_code == 200) {
        snprintf(header, sizeof(header),
            "HTTP/1.1 200 OK\r\n"
            "Content-Length: %zu\r\n"
            "Connection: close\r\n"
            "\r\n",
            file.size);
    } else {
        const char* error_message = file.status_code == 404 ?
            "404 Not Found" : "500 Internal Server Error";

        snprintf(header, sizeof(header),
            "HTTP/1.1 %d %s\r\n"
            "Content-Length: %zu\r\n"
            "Connection: close\r\n"
            "\r\n",
            file.status_code, error_message, strlen(error_message));

        // 에러 메시지를 응답 본문으로 사용
        file.data = (char*)error_message;
        file.size = strlen(error_message);
    }

    // 헤더 전송
    send(client_socket, header, strlen(header), 0);

    // 파일 데이터 전송
    if (file.data) {
        send(client_socket, file.data, file.size, 0);
    }

    // 리소스 정리
    if (file.status_code == 200) { // 에러 시 file.data가 정적 문자열
        free_file_result(&file);
    }
}

// 클라이언트 요청 처리
void handle_client(SOCKET client_socket) {
    char buffer[BUFFER_SIZE] = {0};  // 버퍼 초기화
    int str_len;

    // 클라이언트로부터 메시지 수신
    str_len = recv(client_socket, buffer, BUFFER_SIZE, 0);
    if (str_len == SOCKET_ERROR) {
        error_handling("recv() error");
        return;
    }

    // 받은 데이터 출력 (디버깅용)
    printf("Received message:\n%s\n", buffer);

    // HTTP 요청 파싱
    http_request req = parse_http_request(buffer);
    print_http_request(&req);  // 파싱된 요청 정보 출력

    // 정적 파일 요청 처리
    if (req.method == HTTP_GET) {
        handle_static_file(client_socket, req.base_path);
    } else {
        // 기본 HTTP 응답 생성
        char response[4096];
        snprintf(response, sizeof(response),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html\r\n"
            "Connection: close\r\n"
            "\r\n"
            "<html><body>"
            "<h1>Request Parsed</h1>"
            "<p>Method: %s</p>"
            "<p>Path: %s</p>"
            "<p>Version: %s</p>"
            "<p>Host: %s</p>"
            "<h2>Headers:</h2>"
            "<ul>",
            get_method_string(req.method),
            req.path,
            req.version,
            req.host
        );

        // 헤더 목록 추가
        char temp[1024];
        char full_response[8192] = {0};  // 더 큰 버퍼
        strcpy(full_response, response);

        for (int i = 0; i < req.header_count; i++) {
            snprintf(temp, sizeof(temp),
                "<li><strong>%s:</strong> %s</li>",
                req.headers[i].name,
                req.headers[i].value
            );
            strcat(full_response, temp);
        }
        strcat(full_response, "</ul></body></html>");

        // 응답 전송
        send(client_socket, full_response, strlen(full_response), 0);
    }

    closesocket(client_socket);
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

