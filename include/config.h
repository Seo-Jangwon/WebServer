#ifndef CONFIG_H
#define CONFIG_H

#include <stddef.h>

typedef struct {
  int port; // 포트 번호
  char document_root[1024]; // 정적 파일 경로
  size_t buffer_size; // 버퍼 크기
  int max_connections; // 최대 연결 수
  int backlog_size; // 연결 대기열 크기
  char server_name[64]; // 서버 이름
} server_config;

// 기본 설정
server_config load_default_config(void);

// 설정 값 검증
int validate_config(const server_config *config);

// 디버깅용 설정 출력
void print_config(const server_config *config);

#endif // CONFIG_H
